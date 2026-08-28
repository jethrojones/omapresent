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
        QVERIFY(saveButton);
        QVERIFY(openButton);

        QSignalSpy saveDialogSpy(&backend, &Backend::saveDialogRequested);
        QVERIFY(QMetaObject::invokeMethod(saveButton, "clicked"));
        QCOMPARE(saveDialogSpy.count(), 1);

        QSignalSpy openDialogSpy(&backend, &Backend::openDialogRequested);
        QVERIFY(QMetaObject::invokeMethod(openButton, "clicked"));
        QCOMPARE(openDialogSpy.count(), 1);
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

private:
    QTemporaryDir m_settingsDirectory;
};

OMAPRESENT_TEST_SUITE(OmapresentTest)
#include "tst_omapresent.moc"
