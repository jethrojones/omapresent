#include "assetindex.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QRegularExpression>
#include <QTimer>
#include <QUrl>

#include <algorithm>

struct AssetIndex::Private {
    QFileSystemWatcher *watcher = nullptr;
    QTimer *debounceTimer = nullptr;
    QStringList watchedDirs;
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
        emit indexChanged();
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
    m_byName.clear();

    if (d && d->watcher) {
        const QStringList currentWatched = d->watcher->directories();
        if (!currentWatched.isEmpty()) {
            d->watcher->removePaths(currentWatched);
        }
        d->watchedDirs.clear();
    }

    const QString rootPath = root();
    if (rootPath.isEmpty())
        return;

    QDir rootDir(rootPath);
    if (!rootDir.exists())
        return;

    QStringList dirsToWatch;
    dirsToWatch.append(rootDir.absolutePath());

    // Recursively iterate over files and subdirectories.
    // We do NOT follow directory symlinks to avoid cycles.
    QDirIterator it(rootPath,
                    QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable,
                    QDirIterator::Subdirectories);

    while (it.hasNext()) {
        it.next();
        QFileInfo fi = it.fileInfo();
        if (fi.isDir()) {
            if (fi.isReadable()) {
                dirsToWatch.append(fi.absoluteFilePath());
            }
        } else if (fi.isFile() && fi.exists() && fi.isReadable()) {
            const QString absPath = QDir::cleanPath(fi.absoluteFilePath());
            const QString lowerName = fi.fileName().toLower();
            m_byName[lowerName].append(absPath);
        }
    }

    // Sort candidates in shortest-path-first order:
    // 1. Shorter depth (fewer directory separators relative to root)
    // 2. Shorter relative path length
    // 3. Alphabetical tie-breaker
    const QString rootClean = QDir::cleanPath(rootDir.absolutePath());
    for (auto itMap = m_byName.begin(); itMap != m_byName.end(); ++itMap) {
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

    if (d && d->watcher && !dirsToWatch.isEmpty()) {
        dirsToWatch.removeDuplicates();
        d->watcher->addPaths(dirsToWatch);
        d->watchedDirs = dirsToWatch;
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

    if (s.contains('/') || s.contains('\\'))
        return true;

    return hasImageExtension(s);
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

        struct MatchItem {
            int pos;
            QString target;
        };
        QList<MatchItem> matches;

        // Match Obsidian embeds ![[ ... ]]
        QRegularExpressionMatchIterator obsIt = obsidianRegex.globalMatch(rawLine);
        while (obsIt.hasNext()) {
            QRegularExpressionMatch m = obsIt.next();
            QString content = m.captured(1).trimmed();
            if (!content.isEmpty()) {
                QString bare;
                parseSizeHint(content, &bare, nullptr, nullptr);
                if (!bare.isEmpty()) {
                    matches.append({ static_cast<int>(m.capturedStart()), bare });
                }
            }
        }

        // Match Markdown images ![ ... ]( ... )
        QRegularExpressionMatchIterator mdIt = markdownRegex.globalMatch(rawLine);
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
            if (!urlPart.isEmpty()) {
                QString bare;
                parseSizeHint(urlPart, &bare, nullptr, nullptr);
                if (!bare.isEmpty()) {
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
            if (!trimmedLine.startsWith('#') &&
                !trimmedLine.startsWith('>') &&
                !trimmedLine.startsWith("- ") &&
                !trimmedLine.startsWith("* ") &&
                !trimmedLine.startsWith("+ ") &&
                !trimmedLine.startsWith('-') &&
                !trimmedLine.startsWith('*') &&
                !trimmedLine.contains(QRegularExpression(QStringLiteral(R"(^\d+\.\s)")))
            ) {
                QString bare;
                parseSizeHint(trimmedLine, &bare, nullptr, nullptr);
                if (looksLikeImageReference(bare)) {
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
