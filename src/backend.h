#pragma once

#include <QObject>
#include <QPointer>
#include <QByteArray>
#include <QFileSystemWatcher>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QTemporaryDir>
#include <QVariantList>
#include <memory>

#include "assetindex.h"
#include "deckmodel.h"
#include "omarchytheme.h"
#include "presentation.h"
#include "publisher.h"
#include "renderhost.h"
#include "videocache.h"
#include "webbundle.h"

class MarkdownHighlighter;
class QTextDocument;
class QWindow;
class QLockFile;

class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QUrl fileUrl READ fileUrl NOTIFY fileUrlChanged)
    Q_PROPERTY(QString fileName READ fileName NOTIFY fileUrlChanged)
    Q_PROPERTY(bool modified READ modified NOTIFY modifiedChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(int wordCount READ wordCount NOTIFY wordCountChanged)
    Q_PROPERTY(bool darkMode READ darkMode WRITE setDarkMode NOTIFY darkModeChanged)
    Q_PROPERTY(qreal textScale READ textScale WRITE setTextScale NOTIFY textScaleChanged)
    Q_PROPERTY(QString themeBackground READ themeBackground NOTIFY themeColorsChanged)
    Q_PROPERTY(QString themeForeground READ themeForeground NOTIFY themeColorsChanged)
    Q_PROPERTY(QString themeAccent READ themeAccent NOTIFY themeColorsChanged)
    Q_PROPERTY(QString themeSelection READ themeSelection NOTIFY themeColorsChanged)
    // False in the unit-test harness, which loads Main.qml without ever calling
    // QtWebEngineQuick::initialize(); the preview pane is left out there.
    Q_PROPERTY(bool webEngineReady READ webEngineReady WRITE setWebEngineReady
                   NOTIFY webEngineReadyChanged)
    Q_PROPERTY(DeckModel *deck READ deck CONSTANT)
    Q_PROPERTY(AssetIndex *assets READ assets CONSTANT)
    Q_PROPERTY(OmarchyTheme *theme READ theme CONSTANT)
    Q_PROPERTY(VideoCache *media READ media CONSTANT)
    Q_PROPERTY(Publisher *publisher READ publisher CONSTANT)
    Q_PROPERTY(Presentation *presentation READ presentation CONSTANT)
    Q_PROPERTY(RenderHost *renderHost READ renderHost CONSTANT)

public:
    // One invocation of the `omapresent` command (spec §11).
    struct CommandLine {
        enum Command { Edit, Present, ExportPdf, Publish };
        Command command = Edit;
        QString file;
        QString provider;
        // `publish` uploads the deck to someone else's server, so it asks first
        // unless the caller has already said yes.
        bool assumeYes = false;
        QString error;
        bool needsWindow() const { return command == Edit || command == Present; }
    };

    explicit Backend(QObject *parent = nullptr);
    ~Backend() override;

    void setParentWindow(QWindow *window);

    QUrl fileUrl() const { return m_fileUrl; }
    QString fileName() const;

    bool modified() const { return m_modified; }
    QString status() const { return m_status; }
    int wordCount() const { return m_wordCount; }
    bool darkMode() const { return m_darkMode; }
    void setDarkMode(bool darkMode);
    qreal textScale() const { return m_textScale; }
    void setTextScale(qreal textScale);
    QString themeBackground() const { return m_themeBackground; }
    QString themeForeground() const { return m_themeForeground; }
    QString themeAccent() const { return m_themeAccent; }
    QString themeSelection() const { return m_themeSelection; }
    bool webEngineReady() const { return m_webEngineReady; }
    void setWebEngineReady(bool ready);

    DeckModel *deck() { return &m_deck; }
    AssetIndex *assets() { return &m_assets; }
    OmarchyTheme *theme() { return &m_theme; }
    VideoCache *media() { return &m_media; }
    Publisher *publisher() { return &m_publisher; }
    Presentation *presentation() { return &m_presentation; }
    RenderHost *renderHost() { return &m_renderHost; }

    static int countWords(const QString &text);
    static QString normalizedLinkUrl(const QString &clipboardText);
    static QString suggestedFileName(const QString &text);

    // Arguments after the program name, e.g. {"export", "--pdf", "talk.md"}.
    // Never throws and never exits: a rejected command line comes back with
    // `error` set (spec §11).
    static CommandLine parseCommandLine(const QStringList &arguments);
    static QString usage();
    // Runs a command that needs no window (export, publish) and emits
    // commandFinished() with the process exit code.
    void runCommand(const CommandLine &command);

    // Spec §7 and §11: link the bundled agent skill into this user's skill
    // directories, and on the very first launch open the welcome deck. Both are
    // best-effort; neither may keep the app from starting.
    void completeFirstRun();
    // The agent skill directories that exist for this user, in Omarchy's own
    // order. Omarchy links system skills into every one of them.
    static QStringList agentSkillDirectories(const QString &homeDirectory);
    // Symlinks `skillSource` into each directory as `name`. Idempotent, and it
    // never replaces a directory or a link the user pointed somewhere else.
    // Returns the links that now exist.
    static QStringList installAgentSkill(const QString &skillSource, const QString &name,
                                         const QStringList &skillDirectories);
    // Local paths out of a `text/uri-list` drop (spec §4.5). Wayland sends
    // percent-encoded file:// URIs, one per line, possibly with `#` comments.
    static QStringList pathsFromUriList(const QString &uriListText);

    Q_INVOKABLE void attachDocument(QObject *textDocument);
    Q_INVOKABLE void openDialog();
    Q_INVOKABLE void open(const QUrl &url);
    Q_INVOKABLE void save();
    Q_INVOKABLE void saveForClose();
    Q_INVOKABLE void saveAsDialog();
    Q_INVOKABLE void saveAs(const QUrl &url);
    Q_INVOKABLE void fileDialogCanceled();
    Q_INVOKABLE void discardRecovery();
    Q_INVOKABLE void reloadFromDisk();
    Q_INVOKABLE void keepExternalVersion();
    Q_INVOKABLE void printDocument();
    Q_INVOKABLE void newWindow();
    Q_INVOKABLE QString clipboardUrl() const;
    Q_INVOKABLE QString clipboardText() const;
    Q_INVOKABLE bool editorTextChanged();
    Q_INVOKABLE QVariantList hiddenRangesAt(int position) const;
    Q_INVOKABLE void setSearchHighlight(const QString &query, int currentMatchStart);
    Q_INVOKABLE void openExternalUrl(const QUrl &url);
    Q_INVOKABLE QVariantMap windowGeometry() const;
    Q_INVOKABLE void saveWindowGeometry(int x, int y, int width, int height, bool maximized);

    // `![[shortest-unambiguous-name]]` for every file in a drop, newline
    // separated, ready to insert at the cursor (spec §4.5).
    Q_INVOKABLE QString imageEmbedsForDrop(const QString &uriListText) const;
    // The slide the editing caret sits in, for "present from here" (spec §10).
    Q_INVOKABLE int slideIndexForCursor(int cursorPosition) const;

    // The first script the preview page runs once it has loaded: a full
    // render(), plus the slide and scroll position this deck was left at.
    Q_INVOKABLE QString previewRenderScript();
    // The host bridge of docs/renderer-contract.md §2, injected into the page.
    Q_INVOKABLE QString bridgeScript() const { return RenderHost::bridgeScript(); }

    Q_INVOKABLE void presentFrom(int slideIndex);
    Q_INVOKABLE void exportPdfDialog();
    Q_INVOKABLE void exportPdf(const QUrl &url);
    // Uploads the deck. Only ever called after the user has confirmed. An empty
    // `provider` uses the deck's own `publish.provider`, else the configured
    // default. False means the upload never started, and no Publisher signal
    // is coming.
    Q_INVOKABLE bool publishDeck(const QString &provider = QString());

signals:
    void fileUrlChanged();
    void modifiedChanged();
    void statusChanged();
    void wordCountChanged();
    void darkModeChanged();
    void textScaleChanged();
    void themeColorsChanged();
    void closeAfterSave();
    void openDialogRequested();
    void saveDialogRequested(const QUrl &suggestedUrl);
    void saveSucceeded();
    void externalChangeDetected(bool deleted, bool locallyModified);
    void webEngineReadyChanged();
    // A `window.omapresent.update(...)` call for every open preview page.
    void previewUpdate(const QString &script);
    void pdfDialogRequested(const QUrl &suggestedUrl);
    void commandFinished(int exitCode);

private:
    void loadDocumentText(const QString &text);
    void setFileUrl(const QUrl &url);
    void setModified(bool modified);
    void setStatus(const QString &status);
    void saveTo(const QUrl &url);
    QUrl suggestedSaveUrl() const;
    QString currentDocumentText() const;
    void setWordCount(int words);
    void refreshWordCount();
    void scheduleWordCount();
    void applyDocumentTypography();
    void reapplyTypographyToChange();
    void scheduleRecovery();
    void writeRecovery();
    void restoreRecovery();
    void clearRecovery();
    QString recoveryPath() const;
    void watchCurrentFile();
    void loadOmarchyTheme();
    void watchOmarchyTheme();
    void ensureDocument();
    void scheduleDeckRebuild();
    void rebuildDeck();
    void applyFrontmatter();
    QJsonObject deckDocument(const QString &mode) const;
    QString deckSlug() const;
    QString buildWebBundle();
    void restoreSessionPosition();
    void writeSessionPosition();
    static QString sessionStatePath();

    QUrl m_fileUrl;
    bool m_modified = false;
    QString m_status;
    int m_wordCount = 0;
    bool m_darkMode = true;
    qreal m_textScale = 1.0;
    bool m_loading = false;
    bool m_closeAfterSave = false;
    bool m_formattingTypography = false;
    int m_formattedBlockCount = 0;
    int m_lastChangePos = 0;
    int m_lastChangeAdded = 0;
    QTimer m_wordCountTimer;
    QTimer m_recoveryTimer;
    QFileSystemWatcher m_fileWatcher;
    QPointer<QTextDocument> m_document;
    QPointer<QWindow> m_parentWindow;
    QPointer<MarkdownHighlighter> m_highlighter;
    QString m_lastDocumentText;
    QByteArray m_lastKnownFileContents;
    bool m_hasKnownFileContents = false;
    QString m_recoveryPath;
    std::unique_ptr<QLockFile> m_recoveryLock;

    DeckModel m_deck;
    AssetIndex m_assets;
    OmarchyTheme m_theme;
    VideoCache m_media;
    Publisher m_publisher;
    WebBundle m_webBundle;
    Presentation m_presentation;
    RenderHost m_renderHost;
    PdfExport m_pdfExport;
    bool m_webEngineReady = false;
    QTimer m_deckTimer;
    QTimer m_sessionTimer;
    // Where the open deck was left, and where it should reopen (spec §10).
    int m_slideIndex = 0;
    qreal m_scrollFraction = 0.0;
    bool m_hasRendererState = false;
    // Held for the length of an upload: the provider reads the bundle back off
    // disk while it works (spec §9).
    std::unique_ptr<QTemporaryDir> m_bundle;
    // Stands in for the QML text document when a command line runs without a
    // window.
    std::unique_ptr<QTextDocument> m_ownDocument;

    QString m_themeBackground;
    QString m_themeForeground;
    QString m_themeAccent;
    QString m_themeSelection;
    QFileSystemWatcher m_themeWatcher;
};
