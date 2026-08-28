#include "assetindex.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QPointer>
#include <QRegularExpression>
#include <QRunnable>
#include <QThreadPool>
#include <QTimer>
#include <QUrl>

#include <algorithm>

static constexpr int kMaxWatchedDirectories = 2048;

struct AssetIndex::Private {
    QFileSystemWatcher *watcher = nullptr;
    QTimer *debounceTimer = nullptr;
    QStringList watchedDirs;
    quint64 generation = 0;
    bool scanInProgress = false;
    bool limitWarned = false;
};

class AssetIndexScanWorker : public QRunnable {
public:
    AssetIndexScanWorker(AssetIndex *receiver, quint64 generation, const QString &rootPath)
        : m_receiver(receiver), m_generation(generation), m_rootPath(rootPath)
    {
        setAutoDelete(true);
    }

    void run() override
    {
        QHash<QString, QStringList> newByName;
        QStringList allDirs;

        if (!m_rootPath.isEmpty()) {
            QDir rootDir(m_rootPath);
            if (rootDir.exists()) {
                const QString rootClean = QDir::cleanPath(rootDir.absolutePath());
                allDirs.append(rootClean);

                QDirIterator it(m_rootPath,
                                QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable,
                                QDirIterator::Subdirectories);

                while (it.hasNext()) {
                    it.next();
                    QFileInfo fi = it.fileInfo();
                    if (fi.isDir()) {
                        if (fi.isReadable()) {
                            allDirs.append(QDir::cleanPath(fi.absoluteFilePath()));
                        }
                    } else if (fi.isFile() && fi.exists() && fi.isReadable()) {
                        const QString absPath = QDir::cleanPath(fi.absoluteFilePath());
                        const QString lowerName = fi.fileName().toLower();
                        newByName[lowerName].append(absPath);
                    }
                }

                // Sort candidates in shortest-path-first order
                for (auto itMap = newByName.begin(); itMap != newByName.end(); ++itMap) {
                    QStringList &paths = itMap.value();
                    std::sort(paths.begin(), paths.end(), [&rootClean](const QString &a, const QString &b) {
                        QString relA = QDir(rootClean).relativeFilePath(a);
                        QString relB = QDir(rootClean).relativeFilePath(b);
                        int depthA = relA.count('/') + relA.count('\\');
                        int depthB = relB.count('/') + relB.count('\\');
                        if (depthA != depthB)
                            return depthA < depthB;
                        if (relA.length() != relB.length())
                            return relA.length() < relB.length();
                        return a < b;
                    });
                }

                // Sort directories by depth (closest to root first)
                allDirs.removeDuplicates();
                std::sort(allDirs.begin(), allDirs.end(), [&rootClean](const QString &a, const QString &b) {
                    QString relA = QDir(rootClean).relativeFilePath(a);
                    QString relB = QDir(rootClean).relativeFilePath(b);
                    int depthA = relA.count('/') + relA.count('\\');
                    int depthB = relB.count('/') + relB.count('\\');
                    if (depthA != depthB)
                        return depthA < depthB;
                    return relA.length() < relB.length();
                });
            }
        }

        if (m_receiver) {
            QMetaObject::invokeMethod(m_receiver, [receiver = m_receiver,
                                                  gen = m_generation,
                                                  map = std::move(newByName),
                                                  dirs = std::move(allDirs)]() mutable {
                if (receiver) {
                    receiver->applyScanResult(gen, std::move(map), std::move(dirs));
                }
            }, Qt::QueuedConnection);
        }
    }

private:
    QPointer<AssetIndex> m_receiver;
    quint64 m_generation;
    QString m_rootPath;
};

static QString expandEnvironmentAndTilde(const QString &input)
{
    if (input.isEmpty())
        return {};

    QString str = input;

    // Tilde expansion at the start of the path
    if (str == QStringLiteral("~")) {
        str = QDir::homePath();
    } else if (str.startsWith(QStringLiteral("~/")) || str.startsWith(QStringLiteral("~\\"))) {
        str = QDir::homePath() + str.mid(1);
    }

    // $VAR and ${VAR} environment variable expansion
    static const QRegularExpression envRegex(QStringLiteral(R"(\$\{([A-Za-z0-9_]+)\}|\$([A-Za-z0-9_]+))"));
    QRegularExpressionMatchIterator it = envRegex.globalMatch(str);
    QList<QRegularExpressionMatch> matches;
    while (it.hasNext()) {
        matches.append(it.next());
    }
    for (int i = matches.size() - 1; i >= 0; --i) {
        const QRegularExpressionMatch &m = matches.at(i);
        QString varName = m.captured(1);
        if (varName.isEmpty())
            varName = m.captured(2);
        QString val = qEnvironmentVariable(varName.toLocal8Bit().constData());
        str.replace(m.capturedStart(), m.capturedLength(), val);
    }

    return str;
}

// Blanks out inline code spans, keeping the line the same length so the match
// offsets that order the results still line up with it.
//
// Inline code is documentation *about* the syntax, not a use of it: the welcome
// deck shows readers what `![[figure.png]]` looks like. The renderer never
// draws those, so neither may we — an embed we invent here resolves to nothing
// and paints the missing-image placeholder (spec §4.5 step 5).
static QString withoutInlineCode(const QString &line)
{
    QString masked = line;
    int index = 0;

    while (index < masked.size()) {
        if (masked.at(index) != u'`') {
            ++index;
            continue;
        }

        // A run of N backticks is closed by the next run of exactly N.
        const int openStart = index;
        while (index < masked.size() && masked.at(index) == u'`')
            ++index;
        const int fenceLength = index - openStart;

        int search = index;
        while (search < masked.size()) {
            if (masked.at(search) != u'`') {
                ++search;
                continue;
            }
            const int closeStart = search;
            while (search < masked.size() && masked.at(search) == u'`')
                ++search;
            if (search - closeStart == fenceLength) {
                for (int i = openStart; i < search; ++i)
                    masked[i] = u' ';
                index = search;
                break;
            }
        }
        // An unclosed run is not a code span; leave it and carry on from just
        // past it, so the rest of the line is still read normally.
    }

    return masked;
}

// `![[qr:https://…]]` forces a QR code (spec §4.8); it never names a file. The
// renderer's parseObsidianImage excludes the prefix, so this side must too.
static bool isQrReference(const QString &reference)
{
    return reference.startsWith(QStringLiteral("qr:"), Qt::CaseInsensitive);
}

// The three local-video suffixes of the renderer contract §3a. A bare line
// ending in one of these is a local video and only a video: without this,
// `./clip.webm` is a video by rule 1 and an image by rule 3 at the same time.
static bool hasVideoExtension(const QString &str)
{
    static const QStringList extensions = {
        QStringLiteral(".mp4"), QStringLiteral(".webm"), QStringLiteral(".mov")
    };
    for (const QString &extension : extensions) {
        if (str.endsWith(extension, Qt::CaseInsensitive) && str.length() > extension.length())
            return true;
    }
    return false;
}

// True for a line that is a single token rooted like a path and naming
// something at the end: "~/photos/holiday", "./img/x", "../up/x", "/mnt/x".
// A root is what separates a path from a pair of words joined by a slash, and
// the absence of prose punctuation is what keeps a sentence out.
static bool isRootedPathToken(const QString &str)
{
    const bool rooted = str.startsWith(u'/')
        || str.startsWith(QStringLiteral("./"))
        || str.startsWith(QStringLiteral("../"))
        || str.startsWith(QStringLiteral("~/"));
    if (!rooted)
        return false;

    static const QString prose = QStringLiteral(",;:!?\"'`$*<>|=(){}[]");
    for (const QChar character : str) {
        if (character.isSpace() || prose.contains(character))
            return false;
    }

    // "/", "~/" and "//" are roots that name nothing.
    return !str.endsWith(u'/');
}

static bool hasImageExtension(const QString &str)
{
    static const QStringList extensions = {
        QStringLiteral(".png"),  QStringLiteral(".jpg"),  QStringLiteral(".jpeg"),
        QStringLiteral(".gif"),  QStringLiteral(".webp"), QStringLiteral(".svg"),
        QStringLiteral(".svgz"), QStringLiteral(".heic"), QStringLiteral(".heif"),
        QStringLiteral(".tiff"), QStringLiteral(".tif"),  QStringLiteral(".apng"),
        QStringLiteral(".avif"), QStringLiteral(".bmp"),  QStringLiteral(".ico"),
        QStringLiteral(".pdf"),  QStringLiteral(".jfif"), QStringLiteral(".pjpeg"),
        QStringLiteral(".pjp"),  QStringLiteral(".raw"),  QStringLiteral(".cr2"),
        QStringLiteral(".nef")
    };
    for (const QString &ext : extensions) {
        if (str.endsWith(ext, Qt::CaseInsensitive)) {
            if (str.length() > ext.length()) {
                return true;
            }
        }
    }
    return false;
}

AssetIndex::AssetIndex(QObject *parent)
    : QObject(parent)
    , d(new Private)
{
    d->watcher = new QFileSystemWatcher(this);
    d->debounceTimer = new QTimer(this);
    d->debounceTimer->setSingleShot(true);
    d->debounceTimer->setInterval(150);

    connect(d->watcher, &QFileSystemWatcher::directoryChanged, this, [this](const QString &) {
        d->debounceTimer->start();
    });
    connect(d->debounceTimer, &QTimer::timeout, this, [this]() {
        rebuild();
    });
}

AssetIndex::~AssetIndex()
{
    delete d;
}

void AssetIndex::setDeckDir(const QString &deckDir)
{
    if (m_deckDir == deckDir)
        return;
    m_deckDir = deckDir;
    if (m_root.isEmpty()) {
        rebuild();
    }
}

QString AssetIndex::deckDir() const
{
    return m_deckDir;
}

void AssetIndex::setRoot(const QString &root)
{
    if (m_root == root)
        return;
    m_root = root;
    rebuild();
}

QString AssetIndex::root() const
{
    return m_root.isEmpty() ? m_deckDir : m_root;
}

void AssetIndex::rebuild()
{
    if (!d)
        return;

    d->generation++;
    d->scanInProgress = true;

    const QString rootPath = root();
    if (rootPath.isEmpty()) {
        m_byName.clear();
        d->scanInProgress = false;
        if (d->watcher) {
            const QStringList currentWatched = d->watcher->directories();
            if (!currentWatched.isEmpty()) {
                d->watcher->removePaths(currentWatched);
            }
            d->watchedDirs.clear();
        }
        return;
    }

    QDir rootDir(rootPath);
    if (!rootDir.exists()) {
        m_byName.clear();
        d->scanInProgress = false;
        if (d->watcher) {
            const QStringList currentWatched = d->watcher->directories();
            if (!currentWatched.isEmpty()) {
                d->watcher->removePaths(currentWatched);
            }
            d->watchedDirs.clear();
        }
        return;
    }

    QThreadPool::globalInstance()->start(new AssetIndexScanWorker(this, d->generation, rootPath));
}

void AssetIndex::applyScanResult(quint64 generation,
                                 QHash<QString, QStringList> newByName,
                                 QStringList allDirs)
{
    if (!d || d->generation != generation) {
        return;
    }

    m_byName = std::move(newByName);
    d->scanInProgress = false;

    if (d->watcher) {
        const QStringList currentWatched = d->watcher->directories();
        if (!currentWatched.isEmpty()) {
            d->watcher->removePaths(currentWatched);
        }
        d->watchedDirs.clear();

        if (allDirs.size() > kMaxWatchedDirectories) {
            if (!d->limitWarned) {
                d->limitWarned = true;
                qWarning("AssetIndex: watched directory limit (%d) reached for root '%s'; deeper directories will not be actively watched",
                         kMaxWatchedDirectories, qPrintable(root()));
            }
            allDirs = allDirs.mid(0, kMaxWatchedDirectories);
        }

        if (!allDirs.isEmpty()) {
            const QStringList failed = d->watcher->addPaths(allDirs);
            if (!failed.isEmpty()) {
                qWarning("AssetIndex: could not watch %lld directories (inotify limit reached)",
                         static_cast<long long>(failed.size()));
            }
            d->watchedDirs = allDirs;
        }
    }

    emit indexChanged();
}

void AssetIndex::waitForIndex(int timeoutMs) const
{
    if (!d || !d->scanInProgress)
        return;

    QElapsedTimer timer;
    timer.start();
    while (d && d->scanInProgress && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }
}

QString AssetIndex::resolve(const QString &reference) const
{
    if (reference.trimmed().isEmpty())
        return {};

    const QString refTrimmed = reference.trimmed();

    // URLs (http://, https://, data:) returned unchanged for the cache layer
    if (refTrimmed.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive) ||
        refTrimmed.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive) ||
        refTrimmed.startsWith(QStringLiteral("data:"), Qt::CaseInsensitive)) {
        return refTrimmed;
    }

    QString rawPath = refTrimmed;
    if (rawPath.startsWith(QStringLiteral("file://"), Qt::CaseInsensitive)) {
        rawPath = QUrl(rawPath).toLocalFile();
    }

    // Step 1: Exact path relative to the deck file's directory.
    if (!m_deckDir.isEmpty()) {
        QFileInfo fiDeck(QDir(m_deckDir).filePath(rawPath));
        if (fiDeck.exists() && fiDeck.isFile() && fiDeck.isReadable()) {
            return QDir::cleanPath(fiDeck.absoluteFilePath());
        }
    }

    // Step 2: ~ / $HOME / env-var expansion, then absolute path.
    const QString expanded = expandEnvironmentAndTilde(rawPath);
    QFileInfo fiExpanded(expanded);
    if (fiExpanded.isAbsolute()) {
        if (fiExpanded.exists() && fiExpanded.isFile() && fiExpanded.isReadable()) {
            return QDir::cleanPath(fiExpanded.absoluteFilePath());
        }
    } else if (!m_deckDir.isEmpty()) {
        QFileInfo fiExpDeck(QDir(m_deckDir).filePath(expanded));
        if (fiExpDeck.exists() && fiExpDeck.isFile() && fiExpDeck.isReadable()) {
            return QDir::cleanPath(fiExpDeck.absoluteFilePath());
        }
    }

    // Step 3: Filename search against the asset index (root folder, recursive).
    // Shortest / closest match wins, like Obsidian. Exact case match first.
    const QString targetFileName = QFileInfo(rawPath).fileName();
    const QString lowerFileName = targetFileName.toLower();
    const QStringList candidates = m_byName.value(lowerFileName);

    if (!candidates.isEmpty()) {
        if (rawPath.contains('/') || rawPath.contains('\\')) {
            const QString normRef = QDir::cleanPath(rawPath);
            for (const QString &cand : candidates) {
                if (cand.endsWith(normRef, Qt::CaseSensitive)) {
                    int prefixLen = cand.length() - normRef.length();
                    if (prefixLen == 0 || cand.at(prefixLen - 1) == '/' || cand.at(prefixLen - 1) == '\\') {
                        return cand;
                    }
                }
            }
        } else {
            for (const QString &cand : candidates) {
                if (QFileInfo(cand).fileName() == targetFileName) {
                    return cand;
                }
            }
        }

        // Step 4: Case-insensitive retry of step 3
        if (rawPath.contains('/') || rawPath.contains('\\')) {
            const QString normRef = QDir::cleanPath(rawPath);
            for (const QString &cand : candidates) {
                if (cand.endsWith(normRef, Qt::CaseInsensitive)) {
                    int prefixLen = cand.length() - normRef.length();
                    if (prefixLen == 0 || cand.at(prefixLen - 1) == '/' || cand.at(prefixLen - 1) == '\\') {
                        return cand;
                    }
                }
            }
        } else {
            return candidates.first();
        }
    }

    // Step 5: Not found -> return empty
    return {};
}

QJsonObject AssetIndex::resolveAll(const QStringList &references) const
{
    QJsonObject result;
    for (const QString &ref : references) {
        const QString resolved = resolve(ref);
        if (resolved.isEmpty()) {
            result.insert(ref, QString(""));
        } else if (resolved.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive) ||
                   resolved.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive) ||
                   resolved.startsWith(QStringLiteral("data:"), Qt::CaseInsensitive)) {
            result.insert(ref, resolved);
        } else {
            result.insert(ref, QUrl::fromLocalFile(resolved).toString());
        }
    }
    return result;
}

bool AssetIndex::looksLikeImageReference(const QString &line)
{
    QString bare;
    parseSizeHint(line.trimmed(), &bare, nullptr, nullptr);
    const QString s = bare.trimmed();
    if (s.isEmpty() || s.contains('\n') || s.contains('\r'))
        return false;

    // Local video is decided before images (contract §3a), and a line cannot be
    // both. VideoCache::extractUrls takes these lines instead.
    if (hasVideoExtension(s))
        return false;

    // A known image extension is the strong signal, and the only one that can
    // survive spaces — spec §4.5 is explicit that a path with spaces needs no
    // escaping, so "./img/chart with spaces.png" has to pass.
    if (hasImageExtension(s))
        return true;

    // Without an extension the whole line has to read as one path, not merely
    // contain a slash somewhere. Accepting any slash is what let "and/or",
    // "X/Twitter" and "$$e^{i\pi} + 1 = 0$$" through, and a speaker note read
    // as an image paints the missing-image placeholder over the slide
    // (spec §4.5 step 5).
    return isRootedPathToken(s);
}

void AssetIndex::parseSizeHint(const QString &reference,
                               QString *bareReference,
                               int *maxWidthPx,
                               bool *isMain)
{
    QString bare = reference.trimmed();
    int width = 0;
    bool hero = false;

    int pipeIndex = reference.lastIndexOf('|');
    if (pipeIndex >= 0) {
        bare = reference.left(pipeIndex).trimmed();
        QString hint = reference.mid(pipeIndex + 1).trimmed();

        if (hint.compare(QStringLiteral("main"), Qt::CaseInsensitive) == 0 ||
            hint.compare(QStringLiteral("hero"), Qt::CaseInsensitive) == 0) {
            hero = true;
        } else {
            QString numStr = hint;
            if (numStr.endsWith(QStringLiteral("px"), Qt::CaseInsensitive)) {
                numStr.chop(2);
                numStr = numStr.trimmed();
            }
            bool ok = false;
            int parsed = numStr.toInt(&ok);
            if (ok && parsed > 0) {
                width = parsed;
            }
        }
    }

    if (bareReference)
        *bareReference = bare;
    if (maxWidthPx)
        *maxWidthPx = width;
    if (isMain)
        *isMain = hero;
}

QStringList AssetIndex::extractReferences(const QString &slideMarkdown)
{
    QStringList result;
    if (slideMarkdown.isEmpty())
        return result;

    const QStringList lines = slideMarkdown.split(QRegularExpression(QStringLiteral(R"(\r?\n)")));

    bool inCodeBlock = false;
    QString codeFence;

    static const QRegularExpression obsidianRegex(QStringLiteral(R"(!\[\[(.*?)\]\])"));
    static const QRegularExpression markdownRegex(QStringLiteral(R"(!\[(.*?)\]\((.*?)\))"));

    for (const QString &rawLine : lines) {
        const QString trimmedLine = rawLine.trimmed();

        // Check for fenced code block toggle
        if (trimmedLine.startsWith(QStringLiteral("```")) || trimmedLine.startsWith(QStringLiteral("~~~"))) {
            QString fenceType = trimmedLine.left(3);
            if (!inCodeBlock) {
                inCodeBlock = true;
                codeFence = fenceType;
                continue;
            } else if (trimmedLine.startsWith(codeFence)) {
                inCodeBlock = false;
                codeFence.clear();
                continue;
            }
        }

        if (inCodeBlock)
            continue;

        if (trimmedLine.startsWith(QStringLiteral("---")))
            continue;

        // Everything below reads the line with its inline code spans blanked
        // out, so example syntax inside backticks is not mistaken for a use of
        // that syntax.
        const QString line = withoutInlineCode(rawLine);
        const QString trimmed = line.trimmed();

        struct MatchItem {
            int pos;
            QString target;
        };
        QList<MatchItem> matches;

        // Match Obsidian embeds ![[ ... ]]
        QRegularExpressionMatchIterator obsIt = obsidianRegex.globalMatch(line);
        while (obsIt.hasNext()) {
            QRegularExpressionMatch m = obsIt.next();
            QString content = m.captured(1).trimmed();
            if (!content.isEmpty() && !isQrReference(content)) {
                QString bare;
                parseSizeHint(content, &bare, nullptr, nullptr);
                if (!bare.isEmpty() && !isQrReference(bare)) {
                    matches.append({ static_cast<int>(m.capturedStart()), bare });
                }
            }
        }

        // Match Markdown images ![ ... ]( ... )
        QRegularExpressionMatchIterator mdIt = markdownRegex.globalMatch(line);
        while (mdIt.hasNext()) {
            QRegularExpressionMatch m = mdIt.next();
            QString urlPart = m.captured(2).trimmed();
            if (urlPart.startsWith('<') && urlPart.endsWith('>')) {
                urlPart = urlPart.mid(1, urlPart.length() - 2).trimmed();
            }
            int spaceIdx = urlPart.indexOf(' ');
            if (spaceIdx > 0 && (urlPart.contains('"') || urlPart.contains('\''))) {
                urlPart = urlPart.left(spaceIdx).trimmed();
            }
            if (!urlPart.isEmpty() && !isQrReference(urlPart)) {
                QString bare;
                parseSizeHint(urlPart, &bare, nullptr, nullptr);
                if (!bare.isEmpty() && !isQrReference(bare)) {
                    matches.append({ static_cast<int>(m.capturedStart()), bare });
                }
            }
        }

        if (!matches.isEmpty()) {
            std::sort(matches.begin(), matches.end(), [](const MatchItem &a, const MatchItem &b) {
                return a.pos < b.pos;
            });
            for (const auto &item : matches) {
                result.append(item.target);
            }
        } else {
            // Check for bare image path on its own line
            if (!trimmed.startsWith('#') &&
                !trimmed.startsWith('>') &&
                !trimmed.startsWith("- ") &&
                !trimmed.startsWith("* ") &&
                !trimmed.startsWith("+ ") &&
                !trimmed.startsWith('-') &&
                !trimmed.startsWith('*') &&
                !trimmed.contains(QRegularExpression(QStringLiteral(R"(^\d+\.\s)")))
            ) {
                QString bare;
                parseSizeHint(trimmed, &bare, nullptr, nullptr);
                if (!isQrReference(bare) && looksLikeImageReference(bare)) {
                    result.append(bare);
                }
            }
        }
    }

    return result;
}

QString AssetIndex::shortestUniqueReference(const QString &absolutePath) const
{
    const QString target = QDir::cleanPath(absolutePath);
    if (target.isEmpty())
        return {};

    QStringList candidates;

    // 1. Bare filename
    const QString fileName = QFileInfo(target).fileName();
    const QString lowerName = fileName.toLower();
    const QStringList indexedForName = m_byName.value(lowerName);

    if (indexedForName.size() <= 1 && resolve(fileName) == target) {
        candidates.append(fileName);
    }

    // 2. Directory prefix candidates (e.g. sub/photo.png, deep/sub/photo.png)
    QStringList pathParts = target.split('/', Qt::SkipEmptyParts);
    if (pathParts.size() >= 2) {
        QString partial = pathParts.takeLast(); // fileName
        while (!pathParts.isEmpty()) {
            partial = pathParts.takeLast() + QStringLiteral("/") + partial;
            int count = 0;
            for (const QString &cand : indexedForName) {
                if (cand == target || cand.endsWith(QStringLiteral("/") + partial)) {
                    count++;
                }
            }
            if (count == 1 && resolve(partial) == target) {
                candidates.append(partial);
                break;
            }
        }
    }

    // 3. Relative to deckDir (if inside deckDir)
    if (!m_deckDir.isEmpty()) {
        QString relDeck = QDir(m_deckDir).relativeFilePath(target);
        if (!relDeck.startsWith(QStringLiteral("..")) && resolve(relDeck) == target) {
            candidates.append(relDeck);
        }
    }

    // 4. ~ home relative path
    const QString home = QDir::homePath();
    if (target.startsWith(home + QStringLiteral("/"))) {
        QString homeRel = QStringLiteral("~") + target.mid(home.length());
        if (resolve(homeRel) == target) {
            candidates.append(homeRel);
        }
    }

    // 5. Full absolute path
    candidates.append(target);

    // Pick the shortest candidate that resolves to target
    QString best;
    for (const QString &cand : candidates) {
        if (resolve(cand) == target) {
            if (best.isEmpty() || cand.length() < best.length()) {
                best = cand;
            }
        }
    }

    return best.isEmpty() ? target : best;
}
