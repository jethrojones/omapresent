#include "backend.h"

#include <QClipboard>
#include <QColor>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QMimeData>
#include <QProcess>
#include <QPrintDialog>
#include <QPrinter>
#include <QQuickTextDocument>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QSaveFile>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextStream>
#include <QUrl>
#include <QVariantMap>
#include <QWindow>

#include <algorithm>
#include <limits>
#include <utility>

#include "markdownhighlighter.h"

constexpr qreal typoraLineHeightPercent = 140;
const QString lastSaveDirectorySetting = QStringLiteral("file/lastSaveDirectory");
const QString welcomeShownSetting = QStringLiteral("firstRun/welcomeShown");
const QString installedSkillPath = QStringLiteral("/usr/share/omapresent/skill");
const QString installedWelcomeDeck = QStringLiteral("/usr/share/omapresent/welcome.md");

// A keystroke must not reparse a 200-slide deck, so edits collect for a beat
// before the preview and any running presentation are handed a new document.
constexpr int deckRebuildDelayMs = 150;
// The renderer reports every scroll; the session file is only interesting once
// the reader has settled somewhere.
constexpr int sessionWriteDelayMs = 2000;
constexpr int maxRememberedDecks = 200;

// Spec §9 asks the user before an upload; a pipe that cannot answer is a no.
static bool confirmPublishOnStdin(const QString &slug) {
    QTextStream out(stdout);
    out << QStringLiteral("Publish \"%1\" to an external host? [y/N] ").arg(slug);
    out.flush();

    QTextStream in(stdin);
    const QString answer = in.readLine().trimmed().toLower();
    return answer == QStringLiteral("y") || answer == QStringLiteral("yes");
}

static bool pathIsInside(const QString &path, const QString &root) {
    const QString relative = QDir(root).relativeFilePath(path);
    return relative != QStringLiteral("..")
        && !relative.startsWith(QStringLiteral("../"))
        && !QDir::isAbsolutePath(relative);
}

QString Backend::normalizedLinkUrl(const QString &clipboardText) {
    QString candidate = clipboardText.trimmed();
    static const QRegularExpression lineBreakRe(QStringLiteral("[\\r\\n]"));
    const int lineBreak = candidate.indexOf(lineBreakRe);
    if (lineBreak >= 0)
        candidate = candidate.left(lineBreak).trimmed();

    if (candidate.isEmpty())
        return {};

    if (candidate.startsWith(QStringLiteral("www."), Qt::CaseInsensitive))
        candidate.prepend(QStringLiteral("https://"));

    static const QRegularExpression schemeRe(
        QStringLiteral("^[A-Za-z][A-Za-z0-9+.-]*:"));
    if (!schemeRe.match(candidate).hasMatch())
        return {};

    const QUrl url(candidate);
    if (!url.isValid() || url.scheme().isEmpty())
        return {};

    const QString scheme = url.scheme().toLower();
    const bool webUrl = scheme == QStringLiteral("http")
        || scheme == QStringLiteral("https")
        || scheme == QStringLiteral("ftp");
    if (webUrl && url.host().isEmpty())
        return {};

    if (!webUrl && scheme != QStringLiteral("mailto"))
        return {};

    return url.toString();
}

Backend::Backend(QObject *parent) : QObject(parent) {
    const QString stateDirectory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(stateDirectory);
    // Claim an orphaned snapshot before taking an empty slot. This ensures a
    // crash in window 2 is still recovered even if window 1 exited normally.
    for (int pass = 0; pass < 2 && !m_recoveryLock; ++pass) {
        for (int slot = 0; slot < 100; ++slot) {
            const QString base = QDir(stateDirectory).filePath(
                QStringLiteral("recovery-%1").arg(slot));
            const bool snapshotExists = QFileInfo::exists(base + QStringLiteral(".json"));
            if ((pass == 0) != snapshotExists)
                continue;
            auto lock = std::make_unique<QLockFile>(base + QStringLiteral(".lock"));
            if (lock->tryLock()) {
                m_recoveryPath = base + QStringLiteral(".json");
                m_recoveryLock = std::move(lock);
                break;
            }
        }
    }
    m_wordCountTimer.setSingleShot(true);
    m_wordCountTimer.setInterval(120);
    connect(&m_wordCountTimer, &QTimer::timeout, this, &Backend::refreshWordCount);
    m_recoveryTimer.setSingleShot(true);
    m_recoveryTimer.setInterval(750);
    connect(&m_recoveryTimer, &QTimer::timeout, this, &Backend::writeRecovery);
    connect(&m_fileWatcher, &QFileSystemWatcher::fileChanged, this,
            [this](const QString &path) {
                if (path != m_fileUrl.toLocalFile())
                    return;

                const bool deleted = !QFileInfo::exists(path);
                if (!deleted && m_hasKnownFileContents) {
                    QFile file(path);
                    if (file.open(QIODevice::ReadOnly)
                            && file.readAll() == m_lastKnownFileContents) {
                        // Atomic saves can replace the watched inode. Re-arm the
                        // watcher, but do not report our own save as an outside edit.
                        watchCurrentFile();
                        return;
                    }
                }

                emit externalChangeDetected(deleted, m_modified);
            });

    loadOmarchyTheme();
    watchOmarchyTheme();
    connect(&m_themeWatcher, &QFileSystemWatcher::fileChanged, this, [this]() {
        loadOmarchyTheme();
        watchOmarchyTheme();
    });
    connect(&m_themeWatcher, &QFileSystemWatcher::directoryChanged, this, [this]() {
        loadOmarchyTheme();
        watchOmarchyTheme();
    });

    m_deckTimer.setSingleShot(true);
    m_deckTimer.setInterval(deckRebuildDelayMs);
    connect(&m_deckTimer, &QTimer::timeout, this, &Backend::rebuildDeck);

    m_sessionTimer.setSingleShot(true);
    m_sessionTimer.setInterval(sessionWriteDelayMs);
    connect(&m_sessionTimer, &QTimer::timeout, this, &Backend::writeSessionPosition);

    // How C++ learns where the reader is: the renderer reports it through the
    // `omapresentHost` channel object (docs/renderer-contract.md §2).
    connect(&m_renderHost, &RenderHost::stateChanged, this, [this]() {
        m_slideIndex = m_renderHost.slideIndex();
        m_scrollFraction = m_renderHost.scrollFraction();
        m_hasRendererState = true;
        m_sessionTimer.start();
    });

    // A theme change repaints every open surface. It goes out through the same
    // update() path as an edit, which is what keeps slide and scroll position.
    connect(&m_theme, &OmarchyTheme::themeChanged, this, [this]() {
        loadOmarchyTheme();
        rebuildDeck();
    });
    connect(&m_assets, &AssetIndex::indexChanged, this, &Backend::scheduleDeckRebuild);
    connect(&m_media, &VideoCache::cacheChanged, this, &Backend::scheduleDeckRebuild);

    connect(&m_publisher, &Publisher::published, this,
            [this](const QString &liveUrl, const QString &) {
                setStatus(QStringLiteral("Published to %1").arg(liveUrl));
            });
    connect(&m_publisher, &Publisher::failed, this, [this](const QString &message) {
        setStatus(QStringLiteral("Could not publish: %1").arg(message));
    });
    connect(&m_pdfExport, &PdfExport::finished, this,
            [this](bool ok, const QString &path, const QString &message) {
                setStatus(ok ? QStringLiteral("Exported %1").arg(QFileInfo(path).fileName())
                             : message);
            });
}

Backend::~Backend() {
    writeSessionPosition();
}

void Backend::setWebEngineReady(bool ready) {
    if (m_webEngineReady == ready)
        return;

    m_webEngineReady = ready;
    emit webEngineReadyChanged();
}

void Backend::setParentWindow(QWindow *window) {
    m_parentWindow = window;
}

QString Backend::fileName() const {
    if (!m_fileUrl.isValid() || m_fileUrl.isEmpty())
        return QStringLiteral("Untitled.md");

    if (m_fileUrl.isLocalFile()) {
        const QFileInfo info(m_fileUrl.toLocalFile());
        if (!info.fileName().isEmpty())
            return info.fileName();
    }

    const QString name = m_fileUrl.fileName();
    return name.isEmpty() ? QStringLiteral("Untitled.md") : name;
}

void Backend::setDarkMode(bool darkMode) {
    if (m_darkMode == darkMode)
        return;

    m_darkMode = darkMode;
    loadOmarchyTheme();
    emit darkModeChanged();
}

void Backend::setTextScale(qreal textScale) {
    if (qFuzzyCompare(m_textScale, textScale))
        return;

    m_textScale = textScale;
    // The renderer sizes itself from the deck's textScale, so it needs the new
    // one to reflow without a restart (spec §10).
    scheduleDeckRebuild();
    emit textScaleChanged();
}

void Backend::attachDocument(QObject *textDocument) {
    auto *quickDocument = qobject_cast<QQuickTextDocument *>(textDocument);
    if (!quickDocument || !quickDocument->textDocument()) {
        setStatus(QStringLiteral("Could not attach the Markdown renderer."));
        return;
    }

    if (m_highlighter)
        delete m_highlighter.data();

    m_document = quickDocument->textDocument();
    m_ownDocument.reset();
    m_lastDocumentText = m_document->toPlainText();
    m_highlighter = new MarkdownHighlighter(m_document);
    m_highlighter->setDarkMode(m_darkMode);
    m_highlighter->setColors(m_themeBackground, m_themeForeground, m_themeAccent);

    connect(m_document, &QTextDocument::contentsChange, this,
            [this](int position, int, int charsAdded) {
                if (m_formattingTypography || m_loading)
                    return;
                m_lastChangePos = position;
                m_lastChangeAdded = charsAdded;
            });

    applyDocumentTypography();
    restoreRecovery();
}

void Backend::openDialog() {
    emit openDialogRequested();
}

void Backend::open(const QUrl &url) {
    openLocalFile(url);
}

bool Backend::openCommandFile(const QString &filePath) {
    return openLocalFile(QUrl::fromLocalFile(QFileInfo(filePath).absoluteFilePath()));
}

bool Backend::openLocalFile(const QUrl &url) {
    if (!url.isLocalFile()) {
        setStatus(QStringLiteral("Only local files can be opened."));
        return false;
    }

    const QString path = QFileInfo(url.toLocalFile()).absoluteFilePath();
    const QFileInfo target(path);
    if (!target.exists()) {
        setStatus(QStringLiteral("No such file: %1").arg(path));
        return false;
    }
    if (target.isDir()) {
        setStatus(QStringLiteral("Could not open %1: it is a directory.").arg(path));
        return false;
    }
    if (!target.isFile()) {
        setStatus(QStringLiteral("Could not open %1: it is not a regular file.").arg(path));
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setStatus(QStringLiteral("Could not open %1: %2.")
                      .arg(path, file.errorString()));
        return false;
    }

    const QByteArray contents = file.readAll();
    if (file.error() != QFileDevice::NoError) {
        setStatus(QStringLiteral("Could not read %1: %2.")
                      .arg(path, file.errorString()));
        return false;
    }

    writeSessionPosition();
    ensureDocument();

    loadDocumentText(QString::fromUtf8(contents));
    clearRecovery();
    m_lastKnownFileContents = contents;
    m_hasKnownFileContents = true;
    setFileUrl(QUrl::fromLocalFile(path));
    watchCurrentFile();
    setModified(false);
    setStatus(QStringLiteral("Opened %1").arg(fileName()));

    // Image references and the video cache resolve against the deck's own
    // folder until `root:` says otherwise (spec §4.5).
    const QString deckDir = target.absolutePath();
    m_assets.setDeckDir(deckDir);
    m_media.setDeckDir(deckDir);
    restoreSessionPosition();
    rebuildDeck();
    return true;
}

void Backend::save() {
    if (!m_fileUrl.isValid() || m_fileUrl.isEmpty()) {
        saveAsDialog();
        return;
    }

    saveTo(m_fileUrl);
}

void Backend::saveForClose() {
    if (!m_modified) {
        emit closeAfterSave();
        return;
    }

    m_closeAfterSave = true;
    save();
}

void Backend::saveAsDialog() {
    emit saveDialogRequested(suggestedSaveUrl());
}

void Backend::saveAs(const QUrl &url) {
    saveTo(url);
}

void Backend::fileDialogCanceled() {
    m_closeAfterSave = false;
}

void Backend::discardRecovery() {
    clearRecovery();
}

void Backend::reloadFromDisk() {
    if (m_fileUrl.isLocalFile())
        open(m_fileUrl);
}

void Backend::keepExternalVersion() {
    QFile file(m_fileUrl.toLocalFile());
    if (file.open(QIODevice::ReadOnly)) {
        m_lastKnownFileContents = file.readAll();
        m_hasKnownFileContents = true;
    } else {
        m_lastKnownFileContents.clear();
        m_hasKnownFileContents = false;
    }
    setModified(true);
    scheduleRecovery();
    watchCurrentFile();
    setStatus(QStringLiteral("Kept your version"));
}

void Backend::printDocument() {
    if (!m_document) {
        setStatus(QStringLiteral("There is no document to print."));
        return;
    }

    QPrinter printer(QPrinter::HighResolution);
    QPrintDialog dialog(&printer);
    dialog.setWindowTitle(QStringLiteral("Print %1").arg(fileName()));
    dialog.winId();
    if (dialog.windowHandle() && m_parentWindow)
        dialog.windowHandle()->setTransientParent(m_parentWindow);

    if (dialog.exec() == QDialog::Accepted) {
        QTextDocument rendered;
        rendered.setDefaultFont(m_document->defaultFont());
        rendered.setMarkdown(currentDocumentText());
        rendered.print(&printer);
    }
}

void Backend::newWindow() {
    const bool started = QProcess::startDetached(QCoreApplication::applicationFilePath(),
                                                 QStringList());
    if (!started)
        setStatus(QStringLiteral("Could not open a new window."));
}

QString Backend::clipboardUrl() const {
    const QClipboard *clipboard = QGuiApplication::clipboard();
    if (!clipboard)
        return {};

    const QMimeData *mimeData = clipboard->mimeData();
    if (!mimeData)
        return {};

    if (mimeData->hasUrls()) {
        const QList<QUrl> urls = mimeData->urls();
        for (const QUrl &url : urls) {
            const QString normalized = normalizedLinkUrl(url.toString());
            if (!normalized.isEmpty())
                return normalized;
        }
    }

    if (!mimeData->hasText())
        return {};

    return normalizedLinkUrl(mimeData->text());
}

QString Backend::clipboardText() const {
    const QClipboard *clipboard = QGuiApplication::clipboard();
    if (!clipboard)
        return {};

    const QMimeData *mimeData = clipboard->mimeData();
    return mimeData && mimeData->hasText() ? mimeData->text() : QString();
}

bool Backend::editorTextChanged() {
    if (m_loading || m_formattingTypography)
        return false;

    const QString text = currentDocumentText();
    if (text == m_lastDocumentText)
        return false;
    m_lastDocumentText = text;

    if (m_document) {
        const int blockCount = m_document->blockCount();
        if (blockCount > m_formattedBlockCount)
            reapplyTypographyToChange();
        m_formattedBlockCount = blockCount;
    }

    scheduleWordCount();
    scheduleDeckRebuild();
    setModified(true);
    setStatus(QStringLiteral("Unsaved"));
    scheduleRecovery();
    return true;
}

QVariantList Backend::hiddenRangesAt(int position) const {
    QVariantList ranges;
    if (!m_document)
        return ranges;

    const QTextBlock block =
        m_document->findBlock(qBound(0, position, m_document->characterCount() - 1));
    if (!block.isValid())
        return ranges;

    const int lineStart = block.position();
    QList<QPair<int, int>> spans;
    const QList<MarkdownHighlighter::InlineMarkup> markup =
        MarkdownHighlighter::inlineMarkup(block.text());
    for (const MarkdownHighlighter::InlineMarkup &item : markup) {
        for (const MarkdownHighlighter::Span &marker : item.markers) {
            spans.append({lineStart + marker.start,
                          lineStart + marker.start + marker.length});
        }
    }
    std::sort(spans.begin(), spans.end());

    for (const auto &span : spans) {
        ranges.append(QVariantMap{{QStringLiteral("start"), span.first},
                                  {QStringLiteral("end"), span.second}});
    }
    return ranges;
}

void Backend::setSearchHighlight(const QString &query, int currentMatchStart) {
    if (m_highlighter)
        m_highlighter->setSearch(query, currentMatchStart);
}

void Backend::openExternalUrl(const QUrl &url) {
    const QString scheme = url.scheme().toLower();
    if (scheme == QStringLiteral("http") || scheme == QStringLiteral("https")
            || scheme == QStringLiteral("mailto"))
        QDesktopServices::openUrl(url);
}

QVariantMap Backend::windowGeometry() const {
    QSettings settings;
    return {{QStringLiteral("x"), settings.value(QStringLiteral("window/x"), -1)},
            {QStringLiteral("y"), settings.value(QStringLiteral("window/y"), -1)},
            {QStringLiteral("width"), settings.value(QStringLiteral("window/width"), 1280)},
            {QStringLiteral("height"), settings.value(QStringLiteral("window/height"), 820)},
            {QStringLiteral("maximized"), settings.value(QStringLiteral("window/maximized"), false)}};
}

void Backend::saveWindowGeometry(int x, int y, int width, int height, bool maximized) {
    QSettings settings;
    if (!maximized) {
        settings.setValue(QStringLiteral("window/x"), x);
        settings.setValue(QStringLiteral("window/y"), y);
        settings.setValue(QStringLiteral("window/width"), width);
        settings.setValue(QStringLiteral("window/height"), height);
    }
    settings.setValue(QStringLiteral("window/maximized"), maximized);
}

void Backend::loadDocumentText(const QString &text) {
    ensureDocument();
    if (!m_document) {
        setStatus(QStringLiteral("Could not attach the Markdown renderer."));
        return;
    }

    // A new document has no meaningful relationship to the old editor caret
    // or preview target. QML reports its new caret as soon as it owns this text.
    m_editorCursorPosition = 0;
    m_editorSlideIndex = 0;
    m_previewCaretSlideIndex = -1;
    m_hasEditorCaret = false;

    m_loading = true;
    m_document->setPlainText(text);
    m_lastDocumentText = text;
    m_loading = false;

    applyDocumentTypography();
    m_wordCountTimer.stop();
    setWordCount(countWords(text));
}

void Backend::setFileUrl(const QUrl &url) {
    if (m_fileUrl == url)
        return;

    m_fileUrl = url;
    emit fileUrlChanged();
    watchCurrentFile();
}

void Backend::setModified(bool modified) {
    if (m_modified == modified)
        return;

    m_modified = modified;
    emit modifiedChanged();
}

void Backend::setStatus(const QString &status) {
    if (m_status == status)
        return;

    m_status = status;
    emit statusChanged();
}

void Backend::saveTo(const QUrl &url) {
    if (!url.isLocalFile()) {
        m_closeAfterSave = false;
        setStatus(QStringLiteral("Only local files can be saved."));
        return;
    }

    const QString targetName = QFileInfo(url.toLocalFile()).fileName();
    QSaveFile file(url.toLocalFile());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        m_closeAfterSave = false;
        setStatus(QStringLiteral("Could not save %1.").arg(targetName));
        return;
    }

    const QByteArray contents = currentDocumentText().toUtf8();
    file.write(contents);

    // QSaveFile commits by replacing the target. Stop watching the old inode
    // before that replacement so our own write is not classified as external.
    const QStringList watched = m_fileWatcher.files();
    if (!watched.isEmpty())
        m_fileWatcher.removePaths(watched);

    // commit() flushes, fsyncs, and atomically renames the temp file into place,
    // returning false (and leaving the original untouched) on any write error.
    if (!file.commit()) {
        watchCurrentFile();
        m_closeAfterSave = false;
        setStatus(QStringLiteral("Could not write %1.").arg(targetName));
        return;
    }

    const bool shouldClose = m_closeAfterSave;
    m_closeAfterSave = false;
    m_lastKnownFileContents = contents;
    m_hasKnownFileContents = true;
    setFileUrl(url);
    watchCurrentFile();
    QSettings().setValue(lastSaveDirectorySetting,
                         QFileInfo(url.toLocalFile()).absolutePath());
    setModified(false);
    setStatus(QStringLiteral("Saved %1").arg(fileName()));
    clearRecovery();
    emit saveSucceeded();

    if (shouldClose)
        emit closeAfterSave();
}

void Backend::scheduleRecovery() {
    m_recoveryTimer.start();
}

QString Backend::recoveryPath() const {
    return m_recoveryPath;
}

void Backend::writeRecovery() {
    if (!m_modified)
        return;
    const QString path = recoveryPath();
    if (path.isEmpty())
        return;
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return;
    const QJsonObject recovery{{QStringLiteral("fileUrl"), m_fileUrl.toString()},
                               {QStringLiteral("text"), currentDocumentText()}};
    file.write(QJsonDocument(recovery).toJson(QJsonDocument::Compact));
    file.commit();
}

void Backend::restoreRecovery() {
    QFile file(recoveryPath());
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QJsonDocument json = QJsonDocument::fromJson(file.readAll());
    if (!json.isObject() || !json.object().contains(QStringLiteral("text")))
        return;
    const QJsonObject recovery = json.object();
    loadDocumentText(recovery.value(QStringLiteral("text")).toString());
    const QUrl recoveredUrl(recovery.value(QStringLiteral("fileUrl")).toString());
    QFile diskFile(recoveredUrl.toLocalFile());
    if (recoveredUrl.isLocalFile() && diskFile.open(QIODevice::ReadOnly)) {
        m_lastKnownFileContents = diskFile.readAll();
        m_hasKnownFileContents = true;
    } else {
        m_lastKnownFileContents.clear();
        m_hasKnownFileContents = false;
    }
    setFileUrl(recoveredUrl);
    setModified(true);
    setStatus(QStringLiteral("Recovered unsaved changes"));
}

void Backend::clearRecovery() {
    m_recoveryTimer.stop();
    QFile::remove(recoveryPath());
}

void Backend::watchCurrentFile() {
    const QStringList watched = m_fileWatcher.files();
    if (!watched.isEmpty())
        m_fileWatcher.removePaths(watched);
    if (m_fileUrl.isLocalFile() && QFileInfo::exists(m_fileUrl.toLocalFile()))
        m_fileWatcher.addPath(m_fileUrl.toLocalFile());
}

void Backend::loadOmarchyTheme() {
    m_themeBackground = m_darkMode ? QStringLiteral("#101010") : QStringLiteral("#ffffff");
    m_themeForeground = m_darkMode ? QStringLiteral("#eeeeee") : QStringLiteral("#222324");
    m_themeAccent = m_darkMode ? QStringLiteral("#5584aa") : QStringLiteral("#2077b2");
    m_themeSelection = m_darkMode ? QStringLiteral("#186a9a") : QStringLiteral("#2077b2");

    QString themeMode;
    const QJsonObject palette = m_theme.palette();
    if (!palette.isEmpty()) {
        // Spec §6: OmarchyTheme resolves the one palette every surface wears,
        // including the `theme:` override a deck can ask for.
        themeMode = palette.value(QStringLiteral("mode")).toString();
        m_themeBackground = palette.value(QStringLiteral("background")).toString(m_themeBackground);
        m_themeForeground = palette.value(QStringLiteral("foreground")).toString(m_themeForeground);
        m_themeAccent = palette.value(QStringLiteral("accent")).toString(m_themeAccent);
        m_themeSelection = palette.value(QStringLiteral("selection")).toString(m_themeSelection);
    }

    const QString colorsPath = QDir::homePath()
        + QStringLiteral("/.local/state/omarchy/current/theme/colors.toml");
    QFile file(colorsPath);
    // Only when OmarchyTheme has nothing: on a machine with no Omarchy theme
    // installed the editor still wants the four colours it draws itself with.
    if (palette.isEmpty() && file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        while (!in.atEnd()) {
            const QString line = in.readLine().trimmed();
            if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
                continue;

            const int equals = line.indexOf(QLatin1Char('='));
            if (equals < 0)
                continue;

            const QString key = line.left(equals).trimmed();
            QString value = line.mid(equals + 1).trimmed();
            if (value.size() >= 2
                    && ((value.front() == QLatin1Char('"') && value.back() == QLatin1Char('"'))
                        || (value.front() == QLatin1Char('\'') && value.back() == QLatin1Char('\''))))
                value = value.mid(1, value.size() - 2);

            if (key == QStringLiteral("mode"))
                themeMode = value;
            else if (key == QStringLiteral("background"))
                m_themeBackground = value;
            else if (key == QStringLiteral("foreground"))
                m_themeForeground = value;
            else if (key == QStringLiteral("accent"))
                m_themeAccent = value;
            else if (key == QStringLiteral("selection"))
                m_themeSelection = value;
        }
    }

    bool themeModeKnown = false;
    bool themeIsDark = m_darkMode;
    if (themeMode == QStringLiteral("dark")) {
        themeIsDark = true;
        themeModeKnown = true;
    } else if (themeMode == QStringLiteral("light")) {
        themeIsDark = false;
        themeModeKnown = true;
    } else {
        const QColor background(m_themeBackground);
        if (background.isValid()) {
            const double luminance = 0.299 * background.redF()
                + 0.587 * background.greenF() + 0.114 * background.blueF();
            themeIsDark = luminance < 0.5;
            themeModeKnown = true;
        }
    }
    if (themeModeKnown && themeIsDark != m_darkMode) {
        m_darkMode = themeIsDark;
        emit darkModeChanged();
    }

    if (m_highlighter) {
        m_highlighter->setDarkMode(m_darkMode);
        m_highlighter->setColors(m_themeBackground, m_themeForeground, m_themeAccent);
    }

    emit themeColorsChanged();
}

void Backend::watchOmarchyTheme() {
    const QStringList watched = m_themeWatcher.files() + m_themeWatcher.directories();
    if (!watched.isEmpty())
        m_themeWatcher.removePaths(watched);

    const QString currentDir = QDir::homePath()
        + QStringLiteral("/.local/state/omarchy/current");
    const QString themeDir = currentDir + QStringLiteral("/theme");
    const QString colorsPath = themeDir + QStringLiteral("/colors.toml");

    if (QDir(currentDir).exists())
        m_themeWatcher.addPath(currentDir);
    if (QDir(themeDir).exists())
        m_themeWatcher.addPath(themeDir);
    if (QFile::exists(colorsPath))
        m_themeWatcher.addPath(colorsPath);
}

QUrl Backend::suggestedSaveUrl() const {
    if (m_fileUrl.isLocalFile())
        return m_fileUrl;

    const QString savedDirectory = QSettings().value(lastSaveDirectorySetting).toString();
    const QDir directory = savedDirectory.isEmpty() || !QDir(savedDirectory).exists()
        ? QDir::home()
        : QDir(savedDirectory);
    return QUrl::fromLocalFile(
        directory.filePath(suggestedFileName(currentDocumentText())));
}

QString Backend::currentDocumentText() const {
    return m_document ? m_document->toPlainText() : QString();
}

int Backend::countWords(const QString &text) {
    static const QRegularExpression wordRe(
        QStringLiteral("[\\p{L}\\p{N}]+(?:['-][\\p{L}\\p{N}]+)*"));
    int count = 0;
    QRegularExpressionMatchIterator it = wordRe.globalMatch(text);
    while (it.hasNext()) {
        it.next();
        ++count;
    }
    return count;
}

QString Backend::suggestedFileName(const QString &text) {
    QString name = text.section(QLatin1Char('\n'), 0, 0).trimmed();
    name.replace(QRegularExpression(QStringLiteral("[/\\x00-\\x1f\\x7f]")),
                 QStringLiteral("-"));
    name = name.left(120).trimmed();
    if (name.isEmpty() || name == QStringLiteral(".") || name == QStringLiteral(".."))
        name = QStringLiteral("Untitled");
    if (!name.endsWith(QStringLiteral(".md"), Qt::CaseInsensitive))
        name += QStringLiteral(".md");
    return name;
}

void Backend::setWordCount(int words) {
    if (m_wordCount == words)
        return;

    m_wordCount = words;
    emit wordCountChanged();
}

void Backend::refreshWordCount() {
    setWordCount(countWords(currentDocumentText()));
}

void Backend::scheduleWordCount() {
    m_wordCountTimer.start();
}

void Backend::applyDocumentTypography() {
    if (!m_document)
        return;

    QTextBlockFormat blockFormat;
    blockFormat.setLineHeight(typoraLineHeightPercent, QTextBlockFormat::ProportionalHeight);

    // A full pass is only used for freshly loaded/attached documents, so it is
    // safe to drop undo history here (re-enabling clears the stack anyway).
    const bool undoEnabled = m_document->isUndoRedoEnabled();
    m_document->setUndoRedoEnabled(false);

    m_formattingTypography = true;
    QTextCursor cursor(m_document);
    cursor.select(QTextCursor::Document);
    cursor.mergeBlockFormat(blockFormat);
    m_formattingTypography = false;

    m_document->setUndoRedoEnabled(undoEnabled);

    m_formattedBlockCount = m_document->blockCount();
}

void Backend::reapplyTypographyToChange() {
    if (!m_document)
        return;

    QTextBlockFormat blockFormat;
    blockFormat.setLineHeight(typoraLineHeightPercent, QTextBlockFormat::ProportionalHeight);

    // Format only the block(s) touched by the last edit instead of the whole
    // document, and fold the change into the preceding edit command so a single
    // undo reverts both the text and its formatting.
    const int maxPos = m_document->characterCount() - 1;
    const int start = qBound(0, m_lastChangePos, maxPos);
    const int end = qBound(start, m_lastChangePos + m_lastChangeAdded, maxPos);

    m_formattingTypography = true;
    QTextCursor cursor(m_document);
    cursor.joinPreviousEditBlock();
    cursor.setPosition(start);
    cursor.setPosition(end, QTextCursor::KeepAnchor);
    cursor.mergeBlockFormat(blockFormat);
    cursor.endEditBlock();
    m_formattingTypography = false;
}

void Backend::ensureDocument() {
    if (m_document)
        return;

    // `omapresent export --pdf` and `omapresent publish` run with no window and
    // therefore no QML text document; give the file somewhere to live.
    m_ownDocument = std::make_unique<QTextDocument>();
    m_document = m_ownDocument.get();
}

void Backend::scheduleDeckRebuild() {
    m_deckTimer.start();
}

void Backend::rebuildDeck() {
    m_deckTimer.stop();
    m_deck.setSource(currentDocumentText());
    applyFrontmatter();

    const QJsonObject document = deckDocument(QStringLiteral("preview"));
    // update(), never render(): the contract says update() is the one that
    // keeps the slide and scroll position across an edit. When an edit changes
    // which slide holds the caret, append a goto() after update() so the old
    // preview state cannot survive in a different slide.
    QString script = RenderHost::callScript(QStringLiteral("update"), document);
    if (m_hasEditorCaret) {
        m_editorSlideIndex = slideIndexForCursor(m_editorCursorPosition);
        if (m_editorSlideIndex != m_previewCaretSlideIndex) {
            script += previewGotoScript(m_editorSlideIndex);
            m_previewCaretSlideIndex = m_editorSlideIndex;
        }
    }
    emit previewUpdate(script);
    m_presentation.setDeck(document);
}

void Backend::applyFrontmatter() {
    const QVariantMap frontmatter = m_deck.frontmatter();

    const QString root = frontmatter.value(QStringLiteral("root")).toString();
    if (root != m_assets.root())
        m_assets.setRoot(root);

    // Guarded because setOverrideTheme() reloads, which comes back round as
    // themeChanged() and another rebuild.
    const QString theme = frontmatter.value(QStringLiteral("theme")).toString();
    if (theme != m_theme.overrideTheme())
        m_theme.setOverrideTheme(theme);
}

QJsonObject Backend::deckDocument(const QString &mode) const {
    QStringList references;
    QStringList urls;
    const QVector<Slide> slides = m_deck.slides();
    for (const Slide &slide : slides) {
        references += AssetIndex::extractReferences(slide.markdown);
        urls += VideoCache::extractUrls(slide.markdown);
    }
    references.removeDuplicates();
    urls.removeDuplicates();

    QJsonObject media;
    for (const QString &url : std::as_const(urls))
        media.insert(url, m_media.describe(url));

    return RenderHost::composeDeck(mode, m_deck.toJson(),
                                   m_assets.resolveAll(references), media,
                                   m_theme.palette(), m_theme.backgroundImagePath(),
                                   m_textScale);
}

QString Backend::previewRenderScript() {
    if (m_deckTimer.isActive())
        rebuildDeck();

    QString script = RenderHost::callScript(QStringLiteral("render"),
                                            deckDocument(QStringLiteral("preview")));
    if (m_hasEditorCaret) {
        // The editor is the authority for an editor preview. Do not restore a
        // saved scroll from a prior slide after choosing the caret's slide.
        m_editorSlideIndex = slideIndexForCursor(m_editorCursorPosition);
        script += previewGotoScript(m_editorSlideIndex);
        m_previewCaretSlideIndex = m_editorSlideIndex;
    } else {
        // Spec §10: a deck reopens where it was left until an editor caret is
        // available to direct the preview.
        if (m_slideIndex > 0) {
            script += previewGotoScript(m_slideIndex);
        }
        m_previewCaretSlideIndex = m_slideIndex;
        if (m_scrollFraction > 0.0) {
            script += QStringLiteral("window.omapresent && window.omapresent.setScroll(%1);")
                          .arg(m_scrollFraction, 0, 'f', 4);
        }
    }
    return script;
}

int Backend::slideIndexForCursor(int cursorPosition) const {
    const QString text = currentDocumentText();
    const int position = qBound(0, cursorPosition, int(text.size()));
    const int line = text.left(position).count(QLatin1Char('\n'));
    return qMax(0, m_deck.slideIndexForLine(line));
}

QString Backend::previewGotoScript(int slideIndex) {
    return QStringLiteral("window.omapresent && window.omapresent.goto(%1);")
        .arg(qMax(0, slideIndex));
}

void Backend::followEditorCaret(int cursorPosition) {
    const int position = qBound(0, cursorPosition, int(currentDocumentText().size()));
    m_editorCursorPosition = position;
    m_hasEditorCaret = true;

    // A typing burst has not been parsed yet. Let its existing 150 ms timer
    // calculate against fresh slides, then update() and goto() together.
    if (m_deckTimer.isActive())
        return;

    m_editorSlideIndex = slideIndexForCursor(position);
    if (m_editorSlideIndex == m_previewCaretSlideIndex)
        return;

    m_previewCaretSlideIndex = m_editorSlideIndex;
    emit previewUpdate(previewGotoScript(m_editorSlideIndex));
}

QStringList Backend::pathsFromUriList(const QString &uriListText) {
    QStringList paths;
    // RFC 2483: CRLF separated, `#` comment lines allowed.
    static const QRegularExpression lineBreakRe(QStringLiteral("[\r\n]+"));
    const QStringList lines = uriListText.split(lineBreakRe, Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const QString candidate = line.trimmed();
        if (candidate.isEmpty() || candidate.startsWith(QLatin1Char('#')))
            continue;

        // toLocalFile() is what undoes the percent-encoding Wayland arrives
        // with, so a path with spaces in it lands intact.
        const QUrl url(candidate);
        if (url.isLocalFile())
            paths.append(url.toLocalFile());
    }
    return paths;
}

QString Backend::imageEmbedsForDrop(const QString &uriListText) const {
    QStringList embeds;
    const QStringList paths = pathsFromUriList(uriListText);
    for (const QString &path : paths)
        embeds.append(QStringLiteral("![[%1]]").arg(m_assets.shortestUniqueReference(path)));
    return embeds.join(QLatin1Char('\n'));
}

void Backend::presentFrom(int slideIndex) {
    if (m_deckTimer.isActive())
        rebuildDeck();

    startPresentation(qMax(0, slideIndex));
}

void Backend::presentFromCaret(int cursorPosition) {
    // The separator that creates a new slide may still be in the 150 ms editor
    // debounce. Rebuild unconditionally, then derive the caret slide from
    // that exact parse before presentation starts.
    rebuildDeck();
    startPresentation(slideIndexForCursor(cursorPosition));
}

void Backend::startPresentation(int slideIndex) {
    m_presentation.setDeck(deckDocument(QStringLiteral("present")));
    m_presentation.start(slideIndex);
}

void Backend::exportPdfDialog() {
    const QFileInfo deckFile(m_fileUrl.toLocalFile());
    const QDir directory = m_fileUrl.isLocalFile() ? deckFile.absoluteDir() : QDir::home();
    const QString base = m_fileUrl.isLocalFile() ? deckFile.completeBaseName()
                                                 : QStringLiteral("Untitled");
    emit pdfDialogRequested(
        QUrl::fromLocalFile(directory.filePath(base + QStringLiteral(".pdf"))));
}

void Backend::exportPdf(const QUrl &url) {
    if (!url.isLocalFile()) {
        setStatus(QStringLiteral("Only local files can be exported."));
        return;
    }

    if (m_deckTimer.isActive())
        rebuildDeck();

    setStatus(QStringLiteral("Exporting %1…")
                  .arg(QFileInfo(url.toLocalFile()).fileName()));
    m_pdfExport.run(deckDocument(QStringLiteral("pdf")), url.toLocalFile());
}

QString Backend::deckSlug() const {
    const QVariantMap frontmatter = m_deck.frontmatter();
    const QVariantMap publish = frontmatter.value(QStringLiteral("publish")).toMap();

    QString source = publish.value(QStringLiteral("slug")).toString();
    if (source.isEmpty())
        source = frontmatter.value(QStringLiteral("title")).toString();
    if (source.isEmpty())
        source = QFileInfo(m_fileUrl.toLocalFile()).completeBaseName();
    return Publisher::slugify(source);
}

QString Backend::buildWebBundle() {
    m_bundle = std::make_unique<QTemporaryDir>();
    if (!m_bundle->isValid()) {
        setStatus(QStringLiteral("Could not make a place to build the bundle."));
        return {};
    }

    m_webBundle.setDeck(deckDocument(QStringLiteral("web")));
    m_webBundle.setDeckDir(QFileInfo(m_fileUrl.toLocalFile()).absolutePath());
    if (!m_webBundle.build(m_bundle->path())) {
        setStatus(QStringLiteral("Could not build the web bundle: %1")
                      .arg(m_webBundle.lastError()));
        return {};
    }
    return m_bundle->path();
}

bool Backend::publishDeck(const QString &provider) {
    if (m_deckTimer.isActive())
        rebuildDeck();

    const QString bundle = buildWebBundle();
    if (bundle.isEmpty())
        return false;

    const QVariantMap publish = m_deck.frontmatter().value(QStringLiteral("publish")).toMap();
    const QString chosen = provider.isEmpty()
        ? publish.value(QStringLiteral("provider")).toString()
        : provider;

    setStatus(QStringLiteral("Publishing %1…").arg(deckSlug()));
    m_publisher.publish(bundle, deckSlug(), chosen,
                        publish.value(QStringLiteral("access")).toString());
    return true;
}

Backend::CommandLine Backend::parseCommandLine(const QStringList &arguments) {
    CommandLine parsed;
    QStringList rest = arguments;

    if (!rest.isEmpty()) {
        const QString first = rest.constFirst();
        if (first == QStringLiteral("present"))
            parsed.command = CommandLine::Present;
        else if (first == QStringLiteral("export"))
            parsed.command = CommandLine::ExportPdf;
        else if (first == QStringLiteral("publish"))
            parsed.command = CommandLine::Publish;

        if (parsed.command != CommandLine::Edit)
            rest.removeFirst();
    }

    bool sawPdfFlag = false;
    for (int i = 0; i < rest.size(); ++i) {
        const QString argument = rest.at(i);
        if (argument == QStringLiteral("--pdf")) {
            sawPdfFlag = true;
        } else if (argument == QStringLiteral("--yes") || argument == QStringLiteral("-y")) {
            parsed.assumeYes = true;
        } else if (argument == QStringLiteral("--provider")) {
            if (i + 1 >= rest.size()) {
                parsed.error = QStringLiteral("--provider needs a provider name.");
                return parsed;
            }
            parsed.provider = rest.at(++i);
        } else if (argument.startsWith(QStringLiteral("--provider="))) {
            parsed.provider = argument.section(QLatin1Char('='), 1);
        } else if (argument.startsWith(QLatin1Char('-')) && argument.size() > 1) {
            parsed.error = QStringLiteral("Unknown option %1.").arg(argument);
            return parsed;
        } else if (parsed.file.isEmpty()) {
            parsed.file = argument;
        } else {
            parsed.error = QStringLiteral("One deck at a time, please.");
            return parsed;
        }
    }

    if (sawPdfFlag && parsed.command != CommandLine::ExportPdf)
        parsed.error = QStringLiteral("--pdf belongs to `omapresent export`.");
    else if (!parsed.provider.isEmpty() && parsed.command != CommandLine::Publish)
        parsed.error = QStringLiteral("--provider belongs to `omapresent publish`.");
    else if (parsed.command == CommandLine::ExportPdf && !sawPdfFlag)
        parsed.error = QStringLiteral("`omapresent export` needs --pdf.");
    else if (parsed.command != CommandLine::Edit && parsed.file.isEmpty())
        parsed.error = QStringLiteral("That command needs a file.");

    return parsed;
}

QString Backend::usage() {
    return QStringLiteral(
        "Usage:\n"
        "  omapresent [<file>]                       edit a deck\n"
        "  omapresent present <file>                 present it\n"
        "  omapresent export --pdf <file>            write <file>.pdf\n"
        "  omapresent publish <file> [--provider <name>] [--yes]\n"
        "                                            upload it (asks first)\n");
}

void Backend::runCommand(const CommandLine &command) {
    const QFileInfo deckFile(command.file);
    if (!openCommandFile(command.file)) {
        QTextStream(stderr) << status() << '\n';
        emit commandFinished(1);
        return;
    }

    if (command.command == CommandLine::ExportPdf) {
        connect(&m_pdfExport, &PdfExport::finished, this,
                [this](bool ok, const QString &path, const QString &message) {
                    if (ok)
                        QTextStream(stdout) << path << '\n';
                    else
                        QTextStream(stderr) << message << '\n';
                    emit commandFinished(ok ? 0 : 1);
                });
        exportPdf(QUrl::fromLocalFile(
            deckFile.absoluteDir().filePath(deckFile.completeBaseName()
                                            + QStringLiteral(".pdf"))));
        return;
    }

    // Publishing hands the deck to someone else's server, so it is never done
    // on the strength of the command line alone (spec §9, §11).
    if (!command.assumeYes && !confirmPublishOnStdin(deckSlug())) {
        QTextStream(stderr) << "Nothing was uploaded.\n";
        emit commandFinished(1);
        return;
    }

    connect(&m_publisher, &Publisher::published, this,
            [this](const QString &liveUrl, const QString &) {
                QTextStream(stdout) << liveUrl << '\n';
                emit commandFinished(0);
            });
    connect(&m_publisher, &Publisher::failed, this, [this](const QString &message) {
        QTextStream(stderr) << message << '\n';
        emit commandFinished(1);
    });
    if (!publishDeck(command.provider)) {
        QTextStream(stderr) << status() << '\n';
        emit commandFinished(1);
    }
}

QStringList Backend::agentSkillDirectories(const QString &homeDirectory) {
    // The four Omarchy links its own system skills into, in its order.
    static const QStringList relativePaths{
        QStringLiteral(".claude/skills"), QStringLiteral(".agents/skills"),
        QStringLiteral(".codex/skills"), QStringLiteral(".pi/agent/skills")};

    QStringList directories;
    const QString canonicalHome = QFileInfo(homeDirectory).canonicalFilePath();
    if (canonicalHome.isEmpty() || !QFileInfo(canonicalHome).isDir())
        return directories;

    const QDir home(homeDirectory);
    for (const QString &relative : relativePaths) {
        const QString path = home.filePath(relative);
        const QFileInfo target(path);
        if (target.isSymLink())
            continue;

        if (target.exists()) {
            const QString canonicalTarget = target.canonicalFilePath();
            if (target.isDir() && pathIsInside(canonicalTarget, canonicalHome))
                directories.append(path);
            continue;
        }

        // The agent is installed but keeps no skills yet. Making its skills
        // directory is fair; making a home for an agent that is not here is not.
        const QFileInfo parent(target.absolutePath());
        const QString canonicalParent = parent.canonicalFilePath();
        if (parent.isSymLink() || !parent.isDir()
            || !pathIsInside(canonicalParent, canonicalHome)
            || !QDir().mkpath(path)) {
            continue;
        }

        const QFileInfo created(path);
        if (!created.isSymLink() && created.isDir()
            && pathIsInside(created.canonicalFilePath(), canonicalHome)) {
            directories.append(path);
        }
    }
    return directories;
}

QStringList Backend::installAgentSkill(const QString &skillSource, const QString &name,
                                       const QStringList &skillDirectories) {
    QStringList links;
    const QString canonicalSource = QFileInfo(skillSource).canonicalFilePath();
    if (canonicalSource.isEmpty() || !QFileInfo(skillSource).isDir())
        return links;

    for (const QString &directory : skillDirectories) {
        const QFileInfo directoryInfo(directory);
        if (directoryInfo.isSymLink() || !directoryInfo.isDir()) {
            qWarning() << "Refusing unsafe agent skill directory" << directory;
            continue;
        }

        const QString link = QDir(directory).filePath(name);
        const QFileInfo existing(link);

        if (existing.isSymLink()) {
            if (QFileInfo(existing.symLinkTarget()).canonicalFilePath() == canonicalSource)
                links.append(link);
            else
                qWarning() << "Leaving" << link << "alone: it points at"
                           << existing.symLinkTarget();
            continue;
        }
        if (existing.exists()) {
            qWarning() << "Leaving" << link << "alone: it is not a symlink.";
            continue;
        }
        if (QFile::link(skillSource, link))
            links.append(link);
        else
            qWarning() << "Could not link the Omapresent skill into" << directory;
    }
    return links;
}

void Backend::completeFirstRun() {
    // Idempotent, so it runs every launch rather than only the first: a skills
    // directory that appears later still gets its link, and one that is already
    // there costs four stats. Nothing here may stop the app from starting.
    installAgentSkill(installedSkillPath, QStringLiteral("omapresent"),
                      agentSkillDirectories(QDir::homePath()));

    QSettings settings;
    if (settings.value(welcomeShownSetting, false).toBool())
        return;
    // A first launch that was handed a deck of its own is not the moment.
    if (m_fileUrl.isLocalFile())
        return;

    settings.setValue(welcomeShownSetting, true);
    if (QFileInfo::exists(installedWelcomeDeck))
        open(QUrl::fromLocalFile(installedWelcomeDeck));
}

QString Backend::sessionStatePath() {
    // Spec §10 names this path; XDG_STATE_HOME moves it, as it should.
    const QString stateHome = qEnvironmentVariable(
        "XDG_STATE_HOME", QDir::homePath() + QStringLiteral("/.local/state"));
    return stateHome + QStringLiteral("/omapresent/sessions.json");
}

void Backend::restoreSessionPosition() {
    m_slideIndex = 0;
    m_scrollFraction = 0.0;
    m_hasRendererState = false;
    if (!m_fileUrl.isLocalFile())
        return;

    QFile file(sessionStatePath());
    if (!file.open(QIODevice::ReadOnly))
        return;

    const QJsonObject entry = QJsonDocument::fromJson(file.readAll())
                                  .object()
                                  .value(m_fileUrl.toLocalFile())
                                  .toObject();
    m_slideIndex = entry.value(QStringLiteral("slide")).toInt();
    m_scrollFraction = entry.value(QStringLiteral("scroll")).toDouble();
}

void Backend::writeSessionPosition() {
    // Nothing to remember until a renderer has told us where the reader is.
    if (!m_hasRendererState || !m_fileUrl.isLocalFile())
        return;

    const QString path = sessionStatePath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QJsonObject sessions;
    QFile existing(path);
    if (existing.open(QIODevice::ReadOnly))
        sessions = QJsonDocument::fromJson(existing.readAll()).object();
    existing.close();

    sessions.insert(m_fileUrl.toLocalFile(),
                    QJsonObject{{QStringLiteral("slide"), m_slideIndex},
                                {QStringLiteral("scroll"), m_scrollFraction},
                                {QStringLiteral("seenAt"),
                                 QDateTime::currentSecsSinceEpoch()}});

    // Decks come and go; the file should not grow forever.
    while (sessions.size() > maxRememberedDecks) {
        QString oldestDeck;
        qint64 oldestSeenAt = std::numeric_limits<qint64>::max();
        for (auto it = sessions.constBegin(); it != sessions.constEnd(); ++it) {
            const qint64 seenAt = qint64(it.value().toObject()
                                             .value(QStringLiteral("seenAt")).toDouble());
            if (seenAt <= oldestSeenAt) {
                oldestSeenAt = seenAt;
                oldestDeck = it.key();
            }
        }
        sessions.remove(oldestDeck);
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return;
    file.write(QJsonDocument(sessions).toJson(QJsonDocument::Compact));
    file.commit();
}
