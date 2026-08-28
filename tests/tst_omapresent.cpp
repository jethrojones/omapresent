#include <QtTest>

#include "testrunner.h"
#include <QFont>
#include <QJsonArray>
#include <QPageLayout>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickStyle>

#include "backend.h"
#include "markdownhighlighter.h"
#include "renderhost.h"

class PresentCaptureBackend final : public Backend {
public:
    int launchedSlideIndex = -1;

protected:
    void startPresentation(int slideIndex) override {
        launchedSlideIndex = slideIndex;
    }
};

class OmapresentTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        QVERIFY(m_settingsDirectory.isValid());
        QQuickStyle::setStyle(QStringLiteral("Material"));
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                           m_settingsDirectory.path());
    }

    void countsWords() {
        QCOMPARE(Backend::countWords(QStringLiteral("one two-three don't 42")), 4);
        QCOMPARE(Backend::countWords(QStringLiteral("你好 世界")), 2);
        QCOMPARE(Backend::countWords(QString()), 0);
    }

    void normalizesLinks() {
        QCOMPARE(Backend::normalizedLinkUrl(QStringLiteral("www.example.com/path")),
                 QStringLiteral("https://www.example.com/path"));
        QCOMPARE(Backend::normalizedLinkUrl(QStringLiteral("mailto:writer@example.com")),
                 QStringLiteral("mailto:writer@example.com"));
        QVERIFY(Backend::normalizedLinkUrl(QStringLiteral("example.com")).isEmpty());
        QVERIFY(Backend::normalizedLinkUrl(QStringLiteral("file:///tmp/private")).isEmpty());
    }

    void suggestsSafeNames() {
        QCOMPARE(Backend::suggestedFileName(QStringLiteral("My first draft\nBody")),
                 QStringLiteral("My first draft.md"));
        QCOMPARE(Backend::suggestedFileName(QStringLiteral("A/B")), QStringLiteral("A-B.md"));
        QCOMPARE(Backend::suggestedFileName(QString()), QStringLiteral("Untitled.md"));
        QCOMPARE(Backend::suggestedFileName(QStringLiteral("Already.md")),
                 QStringLiteral("Already.md"));
    }

    void findsInlineMarkdownRanges() {
        const auto markup = MarkdownHighlighter::inlineMarkup(
            QStringLiteral("**bold** and *italic* and [site](https://example.com)"));
        QCOMPARE(markup.size(), 3);
        QCOMPARE(markup.at(0).content.start, 2);
        QCOMPARE(markup.at(0).content.length, 4);
        QCOMPARE(markup.at(2).content.length, 4);
        QCOMPARE(markup.at(2).markers[0].length, 1);
    }

    void loadsCurrentOmarchyTheme() {
        QTemporaryDir homeDirectory;
        QVERIFY(homeDirectory.isValid());

        const QByteArray originalHome = qgetenv("HOME");
        struct HomeRestorer {
            QByteArray value;
            ~HomeRestorer() { qputenv("HOME", value); }
        } restoreHome{originalHome};
        QVERIFY(qputenv("HOME", homeDirectory.path().toUtf8()));

        const QString themeDirectory = homeDirectory.path()
            + QStringLiteral("/.local/state/omarchy/current/theme");
        QVERIFY(QDir().mkpath(themeDirectory));

        QFile colorsFile(themeDirectory + QStringLiteral("/colors.toml"));
        QVERIFY(colorsFile.open(QIODevice::WriteOnly | QIODevice::Text));
        const QByteArray palette(
            "mode = \"light\"\n"
            "accent = \"#112233\"\n"
            "selection = \"#445566\"\n"
            "background = \"#fefefe\"\n"
            "foreground = \"#101010\"\n");
        QCOMPARE(colorsFile.write(palette), qint64(palette.size()));
        colorsFile.close();

        Backend backend;
        QCOMPARE(backend.themeBackground(), QStringLiteral("#fefefe"));
        QCOMPARE(backend.themeForeground(), QStringLiteral("#101010"));
        QCOMPARE(backend.themeAccent(), QStringLiteral("#112233"));
        QCOMPARE(backend.themeSelection(), QStringLiteral("#445566"));
        QVERIFY(!backend.darkMode());
    }

    void ignoresFileWatcherEventsForSavedContents() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        const QString path = directory.filePath(QStringLiteral("first-save.md"));
        Backend backend;
        QSignalSpy externalChangeSpy(&backend, &Backend::externalChangeDetected);

        backend.saveAs(QUrl::fromLocalFile(path));
        QVERIFY(QFileInfo::exists(path));

        QFile sameContents(path);
        QVERIFY(sameContents.open(QIODevice::WriteOnly | QIODevice::Truncate));
        sameContents.close();
        QTest::qWait(100);
        QCOMPARE(externalChangeSpy.count(), 0);

        QFile changedContents(path);
        QVERIFY(changedContents.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(changedContents.write("changed elsewhere"), qint64(17));
        changedContents.close();
        QTRY_COMPARE(externalChangeSpy.count(), 1);
    }

    void keepsCursorAndSelectionStableAcrossInsertions() {
        const QString mutationsPath = QFINDTESTDATA("../src/EditorMutations.js");
        QVERIFY(!mutationsPath.isEmpty());

        QQmlEngine engine;
        QQmlComponent component(&engine);
        const QByteArray harness = R"QML(
            import QtQuick
            import "EditorMutations.js" as EditorMutations

            TextEdit {
                property string insertionText
                property int insertionCursor
                property string wrappedText
                property int wrappedSelectionStart
                property int wrappedSelectionEnd

                Component.onCompleted: {
                    text = "alpha omega";
                    cursorPosition = 5;
                    EditorMutations.replaceRange(this, 5, 5, "one\r\ntwo");
                    insertionText = text;
                    insertionCursor = cursorPosition;

                    text = "alpha beta omega";
                    select(6, 10);
                    EditorMutations.replaceRange(this, selectionStart, selectionEnd,
                                                 "**beta**", 2, 6);
                    wrappedText = text;
                    wrappedSelectionStart = selectionStart;
                    wrappedSelectionEnd = selectionEnd;
                }
            }
        )QML";
        const QUrl harnessUrl = QUrl::fromLocalFile(
            QFileInfo(mutationsPath).absolutePath() + QStringLiteral("/MutationHarness.qml"));
        component.setData(harness, harnessUrl);
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> editor(component.create());
        QVERIFY2(editor, qPrintable(component.errorString()));

        QCOMPARE(editor->property("insertionText").toString(),
                 QStringLiteral("alphaone\ntwo omega"));
        QCOMPARE(editor->property("insertionCursor").toInt(), 12);
        QCOMPARE(editor->property("wrappedText").toString(),
                 QStringLiteral("alpha **beta** omega"));
        QCOMPARE(editor->property("wrappedSelectionStart").toInt(), 8);
        QCOMPARE(editor->property("wrappedSelectionEnd").toInt(), 12);
    }

    void savesAndOpensFromFooterButtons() {
        const QString mainQmlPath = QFINDTESTDATA("../src/Main.qml");
        QVERIFY(!mainQmlPath.isEmpty());

        Backend backend;
        QQmlEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
        QQmlComponent component(&engine, QUrl::fromLocalFile(mainQmlPath));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> window(component.create());
        QVERIFY2(window, qPrintable(component.errorString()));

        QVERIFY(window->findChild<QObject *>(QStringLiteral("sourceEditor")));

        // The preview pane is the one part of the window that needs a web
        // engine, and this suite never initialises one. It must therefore stay
        // unbuilt — if it ever loads here, QtWebEngine aborts the whole binary.
        QObject *previewLoader = window->findChild<QObject *>(QStringLiteral("previewLoader"));
        QVERIFY(previewLoader);
        QVERIFY(!backend.webEngineReady());
        QVERIFY(!previewLoader->property("active").toBool());
        QVERIFY(!window->findChild<QObject *>(QStringLiteral("previewPane")));

        QObject *saveButton = window->findChild<QObject *>(QStringLiteral("saveButton"));
        QObject *openButton = window->findChild<QObject *>(QStringLiteral("openButton"));
        QObject *presentButton =
            window->findChild<QObject *>(QStringLiteral("presentFromCaretButton"));
        QVERIFY(saveButton);
        QVERIFY(openButton);
        QVERIFY(presentButton);
        QCOMPARE(presentButton->property("text").toString(), QStringLiteral("Present"));
        QCOMPARE(presentButton->property("accessibleLabel").toString(),
                 QStringLiteral("Present from current slide"));
        QVERIFY(presentButton->property("toolTipText").toString()
                    .contains(QStringLiteral("Ctrl+Return")));

        QSignalSpy saveDialogSpy(&backend, &Backend::saveDialogRequested);
        QVERIFY(QMetaObject::invokeMethod(saveButton, "clicked"));
        QCOMPARE(saveDialogSpy.count(), 1);

        QSignalSpy openDialogSpy(&backend, &Backend::openDialogRequested);
        QVERIFY(QMetaObject::invokeMethod(openButton, "clicked"));
        QCOMPARE(openDialogSpy.count(), 1);
    }

    void followsTheEditorCaretInTheLivePreview() {
        const QString mainQmlPath = QFINDTESTDATA("../src/Main.qml");
        QVERIFY(!mainQmlPath.isEmpty());

        Backend backend;
        QQmlEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
        QQmlComponent component(&engine, QUrl::fromLocalFile(mainQmlPath));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> window(component.create());
        QVERIFY2(window, qPrintable(component.errorString()));

        QObject *editor = window->findChild<QObject *>(QStringLiteral("sourceEditor"));
        QVERIFY(editor);

        const QString deck = QStringLiteral("# One\n\n---\n\n# Two\n");
        QSignalSpy previewSpy(&backend, &Backend::previewUpdate);
        editor->setProperty("text", deck);
        const int secondSlide = deck.indexOf(QStringLiteral("# Two"));
        QVERIFY(secondSlide > 0);
        editor->setProperty("cursorPosition", secondSlide);

        // The normal debounce remains in force, but its one emitted script
        // updates the deck and then moves to the freshly parsed caret slide.
        QTRY_VERIFY(previewSpy.count() > 0);
        const QString editedScript = previewSpy.constLast().constFirst().toString();
        QVERIFY(editedScript.contains(QStringLiteral("window.omapresent.update(")));
        QVERIFY(!editedScript.contains(QStringLiteral("window.omapresent.render(")));
        QVERIFY(editedScript.contains(QStringLiteral("window.omapresent.goto(1);")));

        // A caret-only move follows immediately and does not wait for a second
        // document render. This is the focused non-WebEngine seam for preview
        // following; PreviewPane runs the emitted script when it exists.
        const int beforeMove = previewSpy.count();
        editor->setProperty("cursorPosition", 0);
        QTRY_COMPARE(previewSpy.count(), beforeMove + 1);
        const QString moveScript = previewSpy.constLast().constFirst().toString();
        QVERIFY(moveScript.contains(QStringLiteral("window.omapresent.goto(0);")));
        QVERIFY(!moveScript.contains(QStringLiteral("window.omapresent.update(")));
    }

    void presentsFromTheFreshCaretSlideAfterASeparatorEdit() {
        const QString mainQmlPath = QFINDTESTDATA("../src/Main.qml");
        QVERIFY(!mainQmlPath.isEmpty());

        PresentCaptureBackend backend;
        QQmlEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
        QQmlComponent component(&engine, QUrl::fromLocalFile(mainQmlPath));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> window(component.create());
        QVERIFY2(window, qPrintable(component.errorString()));

        QObject *editor = window->findChild<QObject *>(QStringLiteral("sourceEditor"));
        QObject *presentButton =
            window->findChild<QObject *>(QStringLiteral("presentFromCaretButton"));
        QVERIFY(editor);
        QVERIFY(presentButton);

        const QString beforeEdit = QStringLiteral("# One\n\n# Two\n");
        QSignalSpy previewSpy(&backend, &Backend::previewUpdate);
        editor->setProperty("text", beforeEdit);
        QTRY_VERIFY(previewSpy.count() > 0);
        const int oldSecondSlide = beforeEdit.indexOf(QStringLiteral("# Two"));
        QCOMPARE(backend.slideIndexForCursor(oldSecondSlide), 0);

        // Do not wait for the debounce after inserting this separator. The
        // old DeckModel has one slide, while the current editor text has two.
        const QString afterEdit = QStringLiteral("# One\n\n---\n\n# Two\n");
        const int newSecondSlide = afterEdit.indexOf(QStringLiteral("# Two"));
        previewSpy.clear();
        editor->setProperty("text", afterEdit);
        editor->setProperty("cursorPosition", newSecondSlide);
        QVERIFY(QMetaObject::invokeMethod(presentButton, "clicked"));

        QCOMPARE(backend.launchedSlideIndex, 1);
        QCOMPARE(backend.slideIndexForCursor(newSecondSlide), 1);
        QCOMPARE(previewSpy.count(), 1);
        const QString script = previewSpy.constFirst().constFirst().toString();
        QVERIFY(script.contains(QStringLiteral("window.omapresent.update(")));
        QVERIFY(script.contains(QStringLiteral("window.omapresent.goto(1);")));
    }

    void scalesTextWithDesktopTextSize() {
        const QString mainQmlPath = QFINDTESTDATA("../src/Main.qml");
        QVERIFY(!mainQmlPath.isEmpty());

        Backend backend;
        QQmlEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
        QQmlComponent component(&engine, QUrl::fromLocalFile(mainQmlPath));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> window(component.create());
        QVERIFY2(window, qPrintable(component.errorString()));

        QObject *editor = window->findChild<QObject *>(QStringLiteral("sourceEditor"));
        QVERIFY(editor);
        QCOMPARE(editor->property("font").value<QFont>().pixelSize(), 20);

        // `omarchy display text size 16` sets the GNOME factor to 16/12.
        backend.setTextScale(16.0 / 12.0);
        QCOMPARE(window->property("editorFontPixelSize").toInt(), 27);
        QCOMPARE(editor->property("font").value<QFont>().pixelSize(), 27);

        backend.setTextScale(9.0 / 12.0);
        QCOMPARE(window->property("editorFontPixelSize").toInt(), 15);
        QCOMPARE(editor->property("font").value<QFont>().pixelSize(), 15);
    }

    void remembersLastSaveDirectory() {
        QTemporaryDir saveDirectory;
        QVERIFY(saveDirectory.isValid());

        const QString savedPath = saveDirectory.filePath(QStringLiteral("first.md"));
        Backend savedDocument;
        savedDocument.saveAs(QUrl::fromLocalFile(savedPath));

        Backend nextDocument;
        QSignalSpy saveDialogSpy(&nextDocument, &Backend::saveDialogRequested);
        nextDocument.saveAsDialog();
        QCOMPARE(saveDialogSpy.count(), 1);

        const QUrl suggestedUrl = saveDialogSpy.takeFirst().constFirst().toUrl();
        QCOMPARE(QFileInfo(suggestedUrl.toLocalFile()).absolutePath(),
                 saveDirectory.path());
        QCOMPARE(QFileInfo(suggestedUrl.toLocalFile()).fileName(),
                 QStringLiteral("Untitled.md"));

        QSettings().setValue(QStringLiteral("file/lastSaveDirectory"),
                             saveDirectory.filePath(QStringLiteral("missing")));
        Backend fallbackDocument;
        QSignalSpy fallbackDialogSpy(&fallbackDocument, &Backend::saveDialogRequested);
        fallbackDocument.saveAsDialog();
        const QUrl fallbackUrl = fallbackDialogSpy.takeFirst().constFirst().toUrl();
        QCOMPARE(QFileInfo(fallbackUrl.toLocalFile()).absolutePath(), QDir::homePath());
    }

    void parsesTheCommandLine() {
        const Backend::CommandLine edit =
            Backend::parseCommandLine({QStringLiteral("talk.md")});
        QVERIFY(edit.error.isEmpty());
        QVERIFY(edit.command == Backend::CommandLine::Edit);
        QCOMPARE(edit.file, QStringLiteral("talk.md"));
        QVERIFY(edit.needsWindow());

        QVERIFY(Backend::parseCommandLine({}).error.isEmpty());

        const Backend::CommandLine present =
            Backend::parseCommandLine({QStringLiteral("present"), QStringLiteral("talk.md")});
        QVERIFY(present.command == Backend::CommandLine::Present);
        QCOMPARE(present.file, QStringLiteral("talk.md"));

        const Backend::CommandLine exported = Backend::parseCommandLine(
            {QStringLiteral("export"), QStringLiteral("--pdf"), QStringLiteral("talk.md")});
        QVERIFY(exported.error.isEmpty());
        QVERIFY(exported.command == Backend::CommandLine::ExportPdf);
        QVERIFY(!exported.needsWindow());

        const Backend::CommandLine published = Backend::parseCommandLine(
            {QStringLiteral("publish"), QStringLiteral("talk.md"),
             QStringLiteral("--provider"), QStringLiteral("mybox"), QStringLiteral("--yes")});
        QVERIFY(published.error.isEmpty());
        QVERIFY(published.command == Backend::CommandLine::Publish);
        QCOMPARE(published.provider, QStringLiteral("mybox"));
        QVERIFY(published.assumeYes);
        QCOMPARE(Backend::parseCommandLine({QStringLiteral("publish"),
                                            QStringLiteral("--provider=s3"),
                                            QStringLiteral("talk.md")}).provider,
                 QStringLiteral("s3"));

        // An upload is never assumed: without --yes something has to ask.
        QVERIFY(!Backend::parseCommandLine({QStringLiteral("publish"),
                                            QStringLiteral("talk.md")}).assumeYes);

        QVERIFY(!Backend::parseCommandLine({QStringLiteral("export"),
                                            QStringLiteral("talk.md")}).error.isEmpty());
        QVERIFY(!Backend::parseCommandLine({QStringLiteral("present")}).error.isEmpty());
        QVERIFY(!Backend::parseCommandLine({QStringLiteral("--wat"),
                                            QStringLiteral("talk.md")}).error.isEmpty());
        QVERIFY(!Backend::parseCommandLine({QStringLiteral("publish"),
                                            QStringLiteral("--provider")}).error.isEmpty());
        QVERIFY(!Backend::parseCommandLine({QStringLiteral("--pdf"),
                                            QStringLiteral("talk.md")}).error.isEmpty());
        QVERIFY(!Backend::parseCommandLine({QStringLiteral("present"), QStringLiteral("a.md"),
                                            QStringLiteral("b.md")}).error.isEmpty());

        // `present` is a subcommand, so a file of that name needs a path.
        QCOMPARE(Backend::parseCommandLine({QStringLiteral("./present")}).file,
                 QStringLiteral("./present"));
    }

    void decodesWaylandUriListDrops() {
        const QString drop = QStringLiteral(
            "# a uri-list may carry comments\r\n"
            "file:///home/jethro/My%20Pictures/q3%20budget.png\r\n"
            "file:///home/jethro/plain.png\r\n"
            "https://example.com/elsewhere.png\r\n");

        const QStringList paths = Backend::pathsFromUriList(drop);
        QCOMPARE(paths.size(), 2);
        QCOMPARE(paths.at(0), QStringLiteral("/home/jethro/My Pictures/q3 budget.png"));
        QCOMPARE(paths.at(1), QStringLiteral("/home/jethro/plain.png"));
        QVERIFY(Backend::pathsFromUriList(QString()).isEmpty());
    }

    void insertsASlideBreakOnTheThirdReturn() {
        const QString mutationsPath = QFINDTESTDATA("../src/EditorMutations.js");
        QVERIFY(!mutationsPath.isEmpty());

        QQmlEngine engine;
        QQmlComponent component(&engine);
        const QByteArray harness = R"QML(
            import QtQuick
            import "EditorMutations.js" as EditorMutations

            QtObject {
                property string brokenText
                property int caret
                property bool secondReturnIsOrdinary
                property bool afterASeparatorIsOrdinary
                property bool midDocumentIsOrdinary
                property bool emptyDocumentIsOrdinary

                Component.onCompleted: {
                    var text = "# One\n\n\n\n";
                    var edit = EditorMutations.slideBreakForReturn(text, text.length);
                    brokenText = text.slice(0, edit.start) + edit.insert + text.slice(edit.end);
                    caret = edit.start + edit.insert.length;

                    secondReturnIsOrdinary =
                        EditorMutations.slideBreakForReturn("# One\n\n", 7) === null;
                    afterASeparatorIsOrdinary =
                        EditorMutations.slideBreakForReturn("# One\n\n---\n\n\n\n", 14) === null;
                    midDocumentIsOrdinary =
                        EditorMutations.slideBreakForReturn("# One\n\n\n\n# Two", 9) === null;
                    emptyDocumentIsOrdinary =
                        EditorMutations.slideBreakForReturn("\n\n\n\n", 4) === null;
                }
            }
        )QML";
        const QUrl harnessUrl = QUrl::fromLocalFile(
            QFileInfo(mutationsPath).absolutePath() + QStringLiteral("/ReturnHarness.qml"));
        component.setData(harness, harnessUrl);
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> harnessObject(component.create());
        QVERIFY2(harnessObject, qPrintable(component.errorString()));

        QCOMPARE(harnessObject->property("brokenText").toString(),
                 QStringLiteral("# One\n\n---\n\n"));
        // The caret lands on the new slide, past the separator.
        QCOMPARE(harnessObject->property("caret").toInt(), 12);
        QVERIFY(harnessObject->property("secondReturnIsOrdinary").toBool());
        QVERIFY(harnessObject->property("afterASeparatorIsOrdinary").toBool());
        QVERIFY(harnessObject->property("midDocumentIsOrdinary").toBool());
        QVERIFY(harnessObject->property("emptyDocumentIsOrdinary").toBool());
    }

    void buildsTheDeckDocumentTheRendererExpects() {
        const QJsonObject parsed{
            {QStringLiteral("frontmatter"),
             QJsonObject{{QStringLiteral("title"), QStringLiteral("Quarterly Review")}}},
            {QStringLiteral("slides"),
             QJsonArray{QJsonObject{{QStringLiteral("index"), 0},
                                    {QStringLiteral("markdown"), QStringLiteral("# Hello\n")}}}}};
        const QJsonObject assets{
            {QStringLiteral("budget.png"), QStringLiteral("file:///pictures/budget.png")},
            {QStringLiteral("missing.png"), QString()}};

        const QJsonObject deck = RenderHost::composeDeck(
            QStringLiteral("preview"), parsed, assets, QJsonObject(),
            QJsonObject{{QStringLiteral("accent"), QStringLiteral("#ff0000")}},
            QStringLiteral("/home/jethro/.local/state/omarchy/current/background"), 1.25);

        QCOMPARE(deck.value(QStringLiteral("mode")).toString(), QStringLiteral("preview"));
        QCOMPARE(deck.value(QStringLiteral("slides")).toArray().size(), 1);
        QCOMPARE(deck.value(QStringLiteral("frontmatter")).toObject()
                     .value(QStringLiteral("title")).toString(),
                 QStringLiteral("Quarterly Review"));
        // An unresolved reference stays in the map as an empty string, which is
        // what tells the renderer to draw the placeholder.
        QVERIFY(deck.value(QStringLiteral("assets")).toObject()
                    .contains(QStringLiteral("missing.png")));
        QCOMPARE(deck.value(QStringLiteral("backgroundImage")).toString(),
                 QStringLiteral("file:///home/jethro/.local/state/omarchy/current/background"));
        QCOMPARE(deck.value(QStringLiteral("textScale")).toDouble(), 1.25);

        // A deck built before anything has been parsed still has to render.
        const QJsonObject empty = RenderHost::composeDeck(
            QStringLiteral("pdf"), QJsonObject(), QJsonObject(), QJsonObject(),
            QJsonObject(), QString(), 1.0);
        QVERIFY(empty.value(QStringLiteral("slides")).isArray());
        QVERIFY(empty.value(QStringLiteral("slides")).toArray().isEmpty());
        QVERIFY(empty.value(QStringLiteral("backgroundImage")).toString().isEmpty());
    }

    void wrapsTheDeckInASafeJavaScriptCall() {
        const QJsonObject deck{
            {QStringLiteral("slides"),
             QJsonArray{QJsonObject{{QStringLiteral("markdown"),
                                     QStringLiteral("a") + QChar(0x2028)
                                         + QStringLiteral("b")}}}}};

        const QString script = RenderHost::callScript(QStringLiteral("update"), deck);
        QVERIFY(script.startsWith(
            QStringLiteral("window.omapresent && window.omapresent.update({")));
        // Legal in JSON, fatal in a JavaScript source literal.
        QVERIFY(!script.contains(QChar(0x2028)));
        QVERIFY(script.contains(QStringLiteral("\\u2028")));
    }

    void injectsAWorkingHostBridge() {
        // Without this script the renderer draws but never reports where the
        // reader is, and it fails by going quiet rather than by crashing.
        const QString bridge = RenderHost::bridgeScript();
        QVERIFY(!bridge.isEmpty());
        QVERIFY(bridge.contains(QStringLiteral("new QWebChannel(")));
        QVERIFY(bridge.contains(QStringLiteral("channel.objects.omapresentHost")));
        QVERIFY(bridge.contains(QStringLiteral("window.omapresent.onState")));
    }

    void recordsWhatTheRendererReports() {
        RenderHost host;
        QSignalSpy stateSpy(&host, &RenderHost::stateChanged);

        host.state(QStringLiteral(
            "{\"slideIndex\":4,\"slideCount\":12,\"scrollFraction\":0.25}"));
        QCOMPARE(stateSpy.count(), 1);
        QCOMPARE(host.slideIndex(), 4);
        QCOMPARE(host.slideCount(), 12);
        QCOMPARE(host.scrollFraction(), 0.25);

        // A page mid-load can say something unhelpful; it must not clear what
        // we already knew.
        host.state(QStringLiteral("not json at all"));
        QCOMPARE(stateSpy.count(), 1);
        QCOMPARE(host.slideIndex(), 4);
    }

    void sizesThePdfCanvasFromTheAspectKey() {
        const QPageLayout wide = RenderHost::pageLayoutFor(QStringLiteral("16:9"));
        QCOMPARE(wide.orientation(), QPageLayout::Landscape);
        QCOMPARE(wide.fullRectPoints().width(), 960);
        QCOMPARE(wide.fullRectPoints().height(), 540);
        // No margins, no scaling: a slide fills its page (spec §8).
        QCOMPARE(wide.margins(), QMarginsF());

        QCOMPARE(RenderHost::pageLayoutFor(QStringLiteral("4:3")).fullRectPoints().height(), 720);
        // Anything unparsable falls back to the default canvas.
        QCOMPARE(RenderHost::pageLayoutFor(QStringLiteral("widescreen"))
                     .fullRectPoints().height(), 540);
        QCOMPARE(RenderHost::pageLayoutFor(QString()).fullRectPoints().height(), 540);
    }

    void linksTheBundledSkillIntoEveryAgentDirectory() {
        QTemporaryDir home;
        QTemporaryDir skill;
        QVERIFY(home.isValid() && skill.isValid());
        QVERIFY(QDir().mkpath(home.filePath(QStringLiteral(".claude/skills"))));
        // An agent that is installed but keeps no skills yet.
        QVERIFY(QDir().mkpath(home.filePath(QStringLiteral(".codex"))));

        const QStringList directories = Backend::agentSkillDirectories(home.path());
        QCOMPARE(directories.size(), 2);
        QVERIFY(directories.contains(home.filePath(QStringLiteral(".claude/skills"))));
        QVERIFY(directories.contains(home.filePath(QStringLiteral(".codex/skills"))));

        const QString link = home.filePath(QStringLiteral(".claude/skills/omapresent"));
        QCOMPARE(Backend::installAgentSkill(skill.path(), QStringLiteral("omapresent"),
                                            directories).size(), 2);
        QVERIFY(QFileInfo(link).isSymLink());
        QCOMPARE(QFileInfo(QFileInfo(link).symLinkTarget()).canonicalFilePath(),
                 QFileInfo(skill.path()).canonicalFilePath());

        // Running again changes nothing.
        QCOMPARE(Backend::installAgentSkill(skill.path(), QStringLiteral("omapresent"),
                                            directories).size(), 2);

        // Something the user put there by hand is left exactly where it is.
        QVERIFY(QFile::remove(link));
        QVERIFY(QDir().mkpath(link));
        QCOMPARE(Backend::installAgentSkill(skill.path(), QStringLiteral("omapresent"),
                                            directories).size(), 1);
        QVERIFY(QFileInfo(link).isDir());
        QVERIFY(!QFileInfo(link).isSymLink());

        // Nothing to link when the package is not installed.
        QVERIFY(Backend::installAgentSkill(QStringLiteral("/nowhere/omapresent/skill"),
                                           QStringLiteral("omapresent"),
                                           directories).isEmpty());
    }

    void reopensADeckWhereItWasLeft() {
        QTemporaryDir stateDirectory;
        QTemporaryDir deckDirectory;
        QVERIFY(stateDirectory.isValid() && deckDirectory.isValid());

        const QByteArray originalStateHome = qgetenv("XDG_STATE_HOME");
        struct StateHomeRestorer {
            QByteArray value;
            ~StateHomeRestorer() {
                if (value.isEmpty())
                    qunsetenv("XDG_STATE_HOME");
                else
                    qputenv("XDG_STATE_HOME", value);
            }
        } restoreStateHome{originalStateHome};
        QVERIFY(qputenv("XDG_STATE_HOME", stateDirectory.path().toUtf8()));

        const QString deckPath = deckDirectory.filePath(QStringLiteral("talk.md"));
        QFile deck(deckPath);
        QVERIFY(deck.open(QIODevice::WriteOnly | QIODevice::Text));
        QVERIFY(deck.write("# One\n\n---\n\n# Two\n") > 0);
        deck.close();

        {
            Backend backend;
            backend.open(QUrl::fromLocalFile(deckPath));
            // What the renderer reports over the omapresentHost channel.
            backend.renderHost()->state(
                QStringLiteral("{\"slideIndex\":3,\"scrollFraction\":0.5}"));
        }

        Backend reopened;
        reopened.open(QUrl::fromLocalFile(deckPath));
        const QString script = reopened.previewRenderScript();
        QVERIFY(script.contains(QStringLiteral(".render({")));
        QVERIFY(script.contains(QStringLiteral(".goto(3)")));
        QVERIFY(script.contains(QStringLiteral(".setScroll(0.5000)")));

        // A deck nobody has opened starts at the top, with no restore calls.
        QVERIFY(QFile::remove(stateDirectory.filePath(
            QStringLiteral("omapresent/sessions.json"))));
        Backend unseen;
        unseen.open(QUrl::fromLocalFile(deckPath));
        QVERIFY(!unseen.previewRenderScript().contains(QStringLiteral(".goto(")));
    }

    void settingsReachTheRunningApplication() {
        QTemporaryDir configDirectory;
        QVERIFY(configDirectory.isValid());
        const QString settingsPath = configDirectory.filePath(QStringLiteral("settings.toml"));

        Backend backend;
        backend.settings()->setPath(settingsPath);

        // The desktop's own preferences, before settings.toml has a say.
        backend.setDarkMode(true);
        backend.setTextScale(1.0);
        QVERIFY(backend.darkMode());
        QCOMPARE(backend.textScale(), 1.0);
        QVERIFY(backend.tripleReturnBreaksSlide());

        QFile file(settingsPath);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        QVERIFY(file.write(
            "[editor]\n"
            "dark_mode = \"light\"\n"
            "text_scale = 1.5\n"
            "auto_break_triple_return = false\n"
            "remember_geometry = false\n") > 0);
        file.close();
        backend.settings()->reload();

        // editor.dark_mode overrules the portal, and text_scale multiplies the
        // desktop scale rather than replacing it.
        QVERIFY(!backend.darkMode());
        QCOMPARE(backend.textScale(), 1.5);
        QVERIFY(!backend.tripleReturnBreaksSlide());

        // A later portal change is still filtered through the setting.
        backend.setDarkMode(true);
        QVERIFY(!backend.darkMode());
        backend.setTextScale(2.0);
        QCOMPARE(backend.textScale(), 3.0);

        // editor.remember_geometry off means the window always opens at the
        // default size, whatever was stored last time.
        backend.saveWindowGeometry(10, 20, 640, 480, false);
        const QVariantMap geometry = backend.windowGeometry();
        QCOMPARE(geometry.value(QStringLiteral("width")).toInt(), 1280);
        QCOMPARE(geometry.value(QStringLiteral("x")).toInt(), -1);
    }

    void settingsFillInWhatTheDeckLeavesUnsaid() {
        QTemporaryDir configDirectory;
        QTemporaryDir deckDirectory;
        QVERIFY(configDirectory.isValid() && deckDirectory.isValid());

        const QString settingsPath = configDirectory.filePath(QStringLiteral("settings.toml"));
        QFile settingsFile(settingsPath);
        QVERIFY(settingsFile.open(QIODevice::WriteOnly | QIODevice::Text));
        QVERIFY(settingsFile.write(
            "[editor]\n"
            "font = \"IBM Plex Sans\"\n"
            "[presentation]\n"
            "default_aspect = \"4:3\"\n"
            "[export]\n"
            "pdf_aspect = \"16:10\"\n") > 0);
        settingsFile.close();

        const QString deckPath = deckDirectory.filePath(QStringLiteral("deck.md"));
        QFile deck(deckPath);
        QVERIFY(deck.open(QIODevice::WriteOnly | QIODevice::Text));
        QVERIFY(deck.write("# One\n") > 0);
        deck.close();

        Backend backend;
        backend.settings()->setPath(settingsPath);
        backend.open(QUrl::fromLocalFile(deckPath));

        const QString preview = backend.previewRenderScript();
        QVERIFY(preview.contains(QStringLiteral("\"font\":\"IBM Plex Sans\"")));
        // On screen the canvas comes from presentation.default_aspect...
        QVERIFY(preview.contains(QStringLiteral("\"aspect\":\"4:3\"")));

        // ...and the PDF has a canvas preference of its own.
        QCOMPARE(RenderHost::pageLayoutFor(
                     backend.settings()->stringValue(QStringLiteral("export.pdf_aspect")))
                     .fullRectPoints().height(),
                 600);
    }

    void aDeckAlwaysOutranksThePreference() {
        QTemporaryDir configDirectory;
        QTemporaryDir deckDirectory;
        QVERIFY(configDirectory.isValid() && deckDirectory.isValid());

        const QString settingsPath = configDirectory.filePath(QStringLiteral("settings.toml"));
        QFile settingsFile(settingsPath);
        QVERIFY(settingsFile.open(QIODevice::WriteOnly | QIODevice::Text));
        QVERIFY(settingsFile.write(
            "[editor]\n"
            "font = \"IBM Plex Sans\"\n"
            "[presentation]\n"
            "default_aspect = \"4:3\"\n") > 0);
        settingsFile.close();

        const QString deckPath = deckDirectory.filePath(QStringLiteral("deck.md"));
        QFile deck(deckPath);
        QVERIFY(deck.open(QIODevice::WriteOnly | QIODevice::Text));
        QVERIFY(deck.write("---\ntitle: Quarterly Review\nfont: Inter\naspect: \"16:9\"\n---\n\n# One\n") > 0);
        deck.close();

        Backend backend;
        backend.settings()->setPath(settingsPath);
        backend.open(QUrl::fromLocalFile(deckPath));

        const QString preview = backend.previewRenderScript();
        QVERIFY(preview.contains(QStringLiteral("\"font\":\"Inter\"")));
        QVERIFY(preview.contains(QStringLiteral("\"aspect\":\"16:9\"")));
        QVERIFY(!preview.contains(QStringLiteral("IBM Plex Sans")));

        // Spec §4.4: the window is named after the deck, not the file.
        QCOMPARE(backend.deckTitle(), QStringLiteral("Quarterly Review"));
    }

    void anUntitledDeckFallsBackToItsFileName() {
        QTemporaryDir deckDirectory;
        QVERIFY(deckDirectory.isValid());
        const QString deckPath = deckDirectory.filePath(QStringLiteral("untitled-talk.md"));
        QFile deck(deckPath);
        QVERIFY(deck.open(QIODevice::WriteOnly | QIODevice::Text));
        QVERIFY(deck.write("# Just a heading\n") > 0);
        deck.close();

        Backend backend;
        backend.open(QUrl::fromLocalFile(deckPath));
        QCOMPARE(backend.deckTitle(), QStringLiteral("untitled-talk.md"));
    }

    void nothingFetchesMediaUntilItIsAskedTo() {
        QTemporaryDir deckDirectory;
        QVERIFY(deckDirectory.isValid());
        const QString deckPath = deckDirectory.filePath(QStringLiteral("media.md"));
        QFile deck(deckPath);
        QVERIFY(deck.open(QIODevice::WriteOnly | QIODevice::Text));
        QVERIFY(deck.write("# Watch\n\nhttps://youtu.be/dQw4w9WgXcQ\n") > 0);
        deck.close();

        Backend backend;
        QSignalSpy finishedSpy(&backend, &Backend::offlinePrefetchFinished);

        // Opening a deck reads the file and nothing else.
        backend.open(QUrl::fromLocalFile(deckPath));
        QCOMPARE(backend.offlineMediaUrls(),
                 QStringList{QStringLiteral("https://youtu.be/dQw4w9WgXcQ")});
        QVERIFY(!backend.offlinePrefetchRunning());
        QCOMPARE(finishedSpy.count(), 0);

        // So does saving it: presentation.auto_prefetch_video is off by default.
        backend.save();
        QTest::qWait(50);
        QVERIFY(!backend.offlinePrefetchRunning());
        QCOMPARE(finishedSpy.count(), 0);
    }

    void aDeckWithNoWebMediaHasNothingToPrepare() {
        QTemporaryDir deckDirectory;
        QVERIFY(deckDirectory.isValid());
        const QString deckPath = deckDirectory.filePath(QStringLiteral("plain.md"));
        QFile deck(deckPath);
        QVERIFY(deck.open(QIODevice::WriteOnly | QIODevice::Text));
        QVERIFY(deck.write("# One\n\nJust prose.\n") > 0);
        deck.close();

        Backend backend;
        backend.open(QUrl::fromLocalFile(deckPath));
        QVERIFY(backend.offlineMediaUrls().isEmpty());

        // It answers immediately rather than starting a fetch of nothing.
        QSignalSpy finishedSpy(&backend, &Backend::offlinePrefetchFinished);
        backend.prepareForOffline();
        QCOMPARE(finishedSpy.count(), 1);
        QVERIFY(finishedSpy.takeFirst().constFirst().toStringList().isEmpty());
        QVERIFY(!backend.offlinePrefetchRunning());
    }

    void publishPreferencesPersistAConfiguredDefaultProvider() {
        QTemporaryDir configHome;
        QVERIFY(configHome.isValid());
        const QByteArray oldConfigHome = qgetenv("XDG_CONFIG_HOME");
        const bool hadConfigHome = qEnvironmentVariableIsSet("XDG_CONFIG_HOME");
        struct ConfigHomeRestorer {
            QByteArray value;
            bool hadValue;
            ~ConfigHomeRestorer() {
                if (hadValue)
                    qputenv("XDG_CONFIG_HOME", value);
                else
                    qunsetenv("XDG_CONFIG_HOME");
            }
        } restoreConfigHome{oldConfigHome, hadConfigHome};
        QVERIFY(qputenv("XDG_CONFIG_HOME", configHome.path().toUtf8()));

        const QString configPath = configHome.filePath(QStringLiteral("omapresent/publish.toml"));
        QVERIFY(QDir().mkpath(QFileInfo(configPath).absolutePath()));
        QFile config(configPath);
        QVERIFY(config.open(QIODevice::WriteOnly | QIODevice::Text));
        QVERIFY(config.write(
            "# Keep this comment.\n"
            "default = \"herenow\"\n"
            "\n"
            "[providers.herenow]\n"
            "type = \"herenow\"\n"
            "\n"
            "[providers.archive]\n"
            "type = \"command\"\n") > 0);
        config.close();

        Backend backend;
        QCOMPARE(backend.publishProvider(), QStringLiteral("herenow"));
        QVERIFY(backend.setPublishProvider(QStringLiteral("archive")));
        QCOMPARE(backend.publishProvider(), QStringLiteral("archive"));
        QVERIFY(!backend.setPublishProvider(QStringLiteral("missing")));
        QCOMPARE(backend.publishProvider(), QStringLiteral("archive"));

        QVERIFY(config.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString written = QString::fromUtf8(config.readAll());
        QVERIFY(written.contains(QStringLiteral("# Keep this comment.\n")));
        QVERIFY(written.contains(QStringLiteral("default = \"archive\"\n")));
    }

    void editingACopyOfTheWelcomeDeckNeverTouchesTheOriginal() {
        // Stands in for /usr/share/omapresent/welcome.md, read-only as installed.
        QTemporaryDir homeDirectory;
        QVERIFY(homeDirectory.isValid());
        const QByteArray originalHome = qgetenv("HOME");
        struct HomeRestorer {
            QByteArray value;
            ~HomeRestorer() { qputenv("HOME", value); }
        } restoreHome{originalHome};
        QVERIFY(qputenv("HOME", homeDirectory.path().toUtf8()));

        const QString welcome = Backend::welcomeDeckPath();
        if (welcome.isEmpty())
            QSKIP("No welcome deck is installed next to this build.");
        const QByteArray before = [&welcome]() {
            QFile file(welcome);
            return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
        }();
        QVERIFY(!before.isEmpty());

        Backend backend;
        const QString copy = backend.editWelcomeCopy();
        QVERIFY(!copy.isEmpty());
        QCOMPARE(QFileInfo(copy).absolutePath(), homeDirectory.path());
        QVERIFY(QFileInfo(copy).isWritable());
        QCOMPARE(backend.fileUrl(), QUrl::fromLocalFile(copy));

        // A second copy never lands on the first.
        const QString second = backend.editWelcomeCopy();
        QVERIFY(!second.isEmpty());
        QVERIFY(second != copy);

        QFile original(welcome);
        QVERIFY(original.open(QIODevice::ReadOnly));
        QCOMPARE(original.readAll(), before);
    }

    void everyNewControlIsReachableAndWired() {
        const QString mainQmlPath = QFINDTESTDATA("../src/Main.qml");
        QVERIFY(!mainQmlPath.isEmpty());

        Backend backend;
        QQmlEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
        QQmlComponent component(&engine, QUrl::fromLocalFile(mainQmlPath));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> window(component.create());
        QVERIFY2(window, qPrintable(component.errorString()));

        // Nothing the menu offers may be a label with no handler behind it.
        const QStringList controls{
            QStringLiteral("menuButton"),          QStringLiteral("actionsMenu"),
            QStringLiteral("exportPdfItem"),       QStringLiteral("prepareOfflineItem"),
            QStringLiteral("publishItem"),         QStringLiteral("publishPreferencesItem"),
            QStringLiteral("howItWorksItem"),      QStringLiteral("editWelcomeCopyItem"),
            QStringLiteral("shortcutsItem"),       QStringLiteral("offlineDialog"),
            QStringLiteral("welcomeCopyDialog"),   QStringLiteral("publishPreferences"),
            QStringLiteral("claimDialog"),          QStringLiteral("publishProviderBox"),
            QStringLiteral("publishAccessBox"),     QStringLiteral("publishPasswordField"),
            QStringLiteral("publishDomainField"),   QStringLiteral("publishEmailField"),
            QStringLiteral("publishSignInButton"),  QStringLiteral("publishCodeField"),
            QStringLiteral("publishVerifyButton"),  QStringLiteral("publishSlugLabel"),
            QStringLiteral("publishNowButton"),     QStringLiteral("republishButton"),
            QStringLiteral("versionsButton"),       QStringLiteral("publishVersionList")};
        for (const QString &name : controls)
            QVERIFY2(window->findChild<QObject *>(name), qPrintable(name));

        // The offline action tells the user what it is about to download.
        QObject *offlineDialog = window->findChild<QObject *>(QStringLiteral("offlineDialog"));
        QVERIFY(QMetaObject::invokeMethod(window.data(), "requestOfflinePreparation"));
        QCOMPARE(offlineDialog->property("videoCount").toInt(), 0);

        // Publish preferences load from the live provider config, not a stub.
        QVERIFY(QMetaObject::invokeMethod(window.data(), "openPublishPreferences"));
        QObject *providerBox = window->findChild<QObject *>(QStringLiteral("publishProviderBox"));
        QVERIFY(providerBox);
        QVERIFY(!providerBox->property("model").toStringList().isEmpty());
        QObject *accessBox = window->findChild<QObject *>(QStringLiteral("publishAccessBox"));
        QVERIFY(accessBox);
        QCOMPARE(accessBox->property("model").toStringList().size(), 4);
    }

private:
    QTemporaryDir m_settingsDirectory;
};

OMAPRESENT_TEST_SUITE(OmapresentTest)
#include "tst_omapresent.moc"
