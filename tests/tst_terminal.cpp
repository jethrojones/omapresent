#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QPointer>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include "testrunner.h"
#include "deckmodel.h"
#include "platform.h"
#include "presentation.h"
#include "terminalsession.h"

// Suite for the live terminal: the `--- {terminal}` tag, the PTY session behind
// it, and the rules in Presentation for who has the keyboard. The session
// tests start a real /bin/sh on a real pseudo-terminal, because a terminal that
// is only ever tested against a fake is not known to work. xterm.js and the
// two windows are not reachable from here; what the pages would report is
// called in directly, the same way tst_presentation.cpp drives the keys.
class TerminalTest : public QObject {
    Q_OBJECT

private:
    // $SHELL and $HOME for the length of one test, so the shell under test is
    // /bin/sh with no profile of the developer's own to read.
    struct ShellSandbox {
        ShellSandbox()
            : shell(qgetenv("SHELL")), home(qgetenv("HOME")), env(qgetenv("ENV")) {
            qputenv("SHELL", "/bin/sh");
            qputenv("HOME", directory.path().toUtf8());
            qunsetenv("ENV");
        }
        ~ShellSandbox() {
            qputenv("SHELL", shell);
            qputenv("HOME", home);
            if (!env.isEmpty())
                qputenv("ENV", env);
        }
        QTemporaryDir directory;
        QByteArray shell;
        QByteArray home;
        QByteArray env;
    };

    // One entry per slide: "" plain, "terminal", "t:terminal" (bound to t),
    // "t:terminal:skip", "q" (an ordinary recall slide).
    static QJsonObject deck(const QStringList &specs) {
        QJsonArray slides;
        int flowIndex = 0;
        for (int i = 0; i < specs.size(); ++i) {
            const QStringList parts = specs.at(i).split(QLatin1Char(':'), Qt::SkipEmptyParts);
            QString key;
            bool terminal = false;
            bool skip = false;
            for (const QString &part : parts) {
                if (part == QStringLiteral("terminal"))
                    terminal = true;
                else if (part == QStringLiteral("skip"))
                    skip = true;
                else
                    key = part;
            }
            QJsonObject slide{{QStringLiteral("index"), skip ? -1 : flowIndex++},
                              {QStringLiteral("markdown"), QStringLiteral("# Slide %1\n").arg(i + 1)},
                              {QStringLiteral("recallKey"), key},
                              {QStringLiteral("skip"), skip}};
            if (terminal)
                slide.insert(QStringLiteral("terminal"), true);
            slides.append(slide);
        }
        return QJsonObject{{QStringLiteral("mode"), QStringLiteral("present")},
                           {QStringLiteral("slides"), slides}};
    }

    // A talk that is actually running: terminal slides only exist in one.
    static void begin(Presentation &presentation, const QJsonObject &deckJson) {
        presentation.setEnvironmentPreferences(false, false);
        presentation.setDeck(deckJson);
        presentation.setWindowFactoryForTesting(
            [](const QString &) { return new QQuickWindow; });
        presentation.start(0);
        // What AudienceWindow.qml reports once its page has loaded.
        presentation.viewReady(QStringLiteral("audience"));
    }

    static bool shiftEscape(Presentation &presentation) {
        return presentation.handleKey(Qt::Key_Escape, Qt::ShiftModifier, QString());
    }

    // Every byte the host has sent one role's page, decoded from the
    // `terminalWrite('<base64>')` calls it was asked to run.
    static QByteArray written(const QSignalSpy &scripts, const QString &role) {
        static const QRegularExpression call(
            QStringLiteral("terminalWrite\\('([A-Za-z0-9+/=]*)'\\)"));
        QByteArray bytes;
        for (const QList<QVariant> &emission : scripts) {
            if (emission.at(0).toString() != role)
                continue;
            auto matches = call.globalMatch(emission.at(1).toString());
            while (matches.hasNext())
                bytes += QByteArray::fromBase64(matches.next().captured(1).toLatin1());
        }
        return bytes;
    }

private slots:
    // --- The tag ----------------------------------------------------------

    void terminalTag_data() {
        QTest::addColumn<QString>("line");
        QTest::addColumn<QString>("key");
        QTest::addColumn<bool>("skip");
        QTest::addColumn<bool>("terminal");

        QTest::newRow("plain") << "---" << QString() << false << false;
        QTest::newRow("recall only") << "--- {q}" << "q" << false << false;
        QTest::newRow("terminal") << "--- {terminal}" << QString() << false << true;
        QTest::newRow("bound") << "--- {t, terminal}" << "t" << false << true;
        QTest::newRow("any order") << "--- {terminal, t}" << "t" << false << true;
        QTest::newRow("bound and skipped") << "--- {t, terminal, skip}" << "t" << true << true;
        QTest::newRow("case and space") << "---{ T ,  Terminal }" << "T" << false << true;
        QTest::newRow("a typo is not a terminal") << "--- {t, termnal}" << "t" << false << false;
    }

    void terminalTag() {
        QFETCH(QString, line);
        QFETCH(QString, key);
        QFETCH(bool, skip);
        QFETCH(bool, terminal);

        QString parsedKey = QStringLiteral("dirty");
        bool parsedSkip = !skip;
        bool parsedTerminal = !terminal;
        DeckModel::parseSeparatorTag(line, &parsedKey, &parsedSkip, &parsedTerminal);
        QCOMPARE(parsedKey, key);
        QCOMPARE(parsedSkip, skip);
        QCOMPARE(parsedTerminal, terminal);

        // The original three-argument form still answers the first two.
        DeckModel::parseSeparatorTag(line, &parsedKey, &parsedSkip);
        QCOMPARE(parsedKey, key);
        QCOMPARE(parsedSkip, skip);
    }

    void terminalSlidesReachTheDeckDocument() {
        DeckModel model;
        model.setSource(QStringLiteral(
            "# One\n\n--- {terminal}\n\n# Live\n\nType `ls`.\n\n--- {t, terminal, skip}\n\n# Popped\n"));
        const QVector<Slide> slides = model.slides();
        QCOMPARE(slides.size(), 3);
        QVERIFY(!slides.at(0).terminal);
        QVERIFY(slides.at(1).terminal);
        QVERIFY(slides.at(2).terminal);
        QCOMPARE(slides.at(2).recallKey, QStringLiteral("t"));
        QVERIFY(slides.at(2).skipInFlow);

        const QJsonArray json = model.toJson().value(QStringLiteral("slides")).toArray();
        // Absent, not false, on an ordinary slide: a deck with no terminal
        // produces exactly the document it did before the tag existed.
        QVERIFY(!json.at(0).toObject().contains(QStringLiteral("terminal")));
        QVERIFY(json.at(1).toObject().value(QStringLiteral("terminal")).toBool());
        QVERIFY(json.at(2).toObject().value(QStringLiteral("terminal")).toBool());
    }

    void aTerminalSlideNeedsNoMarkdown() {
        DeckModel model;
        model.setSource(QStringLiteral("# One\n\n--- {terminal}\n\n---\n\n# Three\n"));
        const QVector<Slide> slides = model.slides();
        QCOMPARE(slides.size(), 3);
        QVERIFY(slides.at(1).terminal);
        QVERIFY(slides.at(1).markdown.isEmpty());
        QCOMPARE(model.slideIndexForLine(2), 1);

        // Still true of every other slide: nothing between two separators is
        // no slide at all.
        model.setSource(QStringLiteral("# One\n\n---\n\n---\n\n# Three\n"));
        QCOMPARE(model.slideCount(), 2);
    }

    // --- The session's pure helpers ----------------------------------------

    void theShellComesFromTheEnvironment() {
        const TerminalSession::Shell plain =
            TerminalSession::shellFor(QStringLiteral("/bin/sh"), false);
        QCOMPARE(plain.program, QStringLiteral("/bin/sh"));
        QVERIFY(plain.arguments.isEmpty());

        const TerminalSession::Shell login =
            TerminalSession::shellFor(QStringLiteral("/bin/sh"), true);
        QCOMPARE(login.arguments, QStringList{QStringLiteral("-l")});

        // Unset, relative, or not there at all: never a guess, always /bin/sh.
        for (const QString &bad : {QString(), QStringLiteral("zsh"),
                                   QStringLiteral("/no/such/shell")}) {
            QCOMPARE(TerminalSession::shellFor(bad, false).program, QStringLiteral("/bin/sh"));
        }
    }

    void theEnvironmentNamesThisTerminal() {
        const QStringList environment = TerminalSession::environmentFor(
            {QStringLiteral("PATH=/usr/bin"), QStringLiteral("TERM=dumb"),
             QStringLiteral("COLUMNS=20"), QStringLiteral("LINES=5"),
             QStringLiteral("TERM_PROGRAM=Other")},
            QStringLiteral("en_GB"));

        QVERIFY(environment.contains(QStringLiteral("PATH=/usr/bin")));
        QVERIFY(environment.contains(QStringLiteral("TERM=xterm-256color")));
        QVERIFY(environment.contains(QStringLiteral("COLORTERM=truecolor")));
        QVERIFY(environment.contains(QStringLiteral("TERM_PROGRAM=omapresent")));
        QVERIFY(!environment.contains(QStringLiteral("TERM=dumb")));
        QVERIFY(!environment.contains(QStringLiteral("COLUMNS=20")));
        QVERIFY(!environment.contains(QStringLiteral("LINES=5")));
        // No locale at all is how a Mac app started from Finder runs.
        QVERIFY(environment.contains(QStringLiteral("LANG=en_GB.UTF-8")));
    }

    void anExistingLocaleIsLeftAlone() {
        const QStringList environment = TerminalSession::environmentFor(
            {QStringLiteral("LC_CTYPE=de_DE.UTF-8")}, QStringLiteral("en_US"));
        QVERIFY(environment.contains(QStringLiteral("LC_CTYPE=de_DE.UTF-8")));
        QVERIFY(environment.filter(QStringLiteral("LANG=")).isEmpty());

        // An empty assignment is not a locale, and "C" is not a language.
        const QStringList fallback = TerminalSession::environmentFor(
            {QStringLiteral("LANG=")}, QStringLiteral("C"));
        QVERIFY(fallback.contains(QStringLiteral("LANG=en_US.UTF-8")));
    }

    void scrollbackIsBoundedAndStartsOnALine() {
        QByteArray buffer;
        bool truncated = false;
        TerminalSession::appendScrollback(&buffer, "one\ntwo\n", 64, &truncated);
        QCOMPARE(buffer, QByteArray("one\ntwo\n"));
        QVERIFY(!truncated);

        TerminalSession::appendScrollback(&buffer, QByteArray(60, 'x') + "\nlast\n", 64, &truncated);
        QVERIFY(truncated);
        QVERIFY(buffer.size() <= 64);
        // Cut at the line break after the limit, not in the middle of a line.
        QCOMPARE(buffer, QByteArray("last\n"));
    }

    // --- A real shell on a real PTY -----------------------------------------

    void theSessionIsARealTerminal() {
        ShellSandbox sandbox;
        TerminalSession session;
        QByteArray seen;
        connect(&session, &TerminalSession::output, this,
                [&seen](const QByteArray &bytes) { seen += bytes; });
        QSignalSpy exits(&session, &TerminalSession::exited);

        QVERIFY2(session.start(sandbox.directory.path(), 91, 27),
                 qPrintable(session.lastError()));
        QVERIFY(session.running());

        // Arithmetic in the command, so the answer can only come from a shell
        // that ran it and not from the terminal echoing what was typed.
        session.write("echo op-$((6*7))\r");
        QTRY_VERIFY_WITH_TIMEOUT(seen.contains("op-42"), 5000);

        // stdin is a terminal, of the size asked for, in the directory given.
        session.write("test -t 0 && echo tty-$((1+1)); stty size; pwd -P\r");
        QTRY_VERIFY_WITH_TIMEOUT(seen.contains("tty-2"), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(seen.contains("27 91"), 5000);
        const QString realDirectory = QFileInfo(sandbox.directory.path()).canonicalFilePath();
        QTRY_VERIFY_WITH_TIMEOUT(seen.contains(realDirectory.toUtf8()), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(session.scrollback().contains("op-42"), 5000);

        session.resize(120, 40);
        session.write("stty size; echo $TERM\r");
        QTRY_VERIFY_WITH_TIMEOUT(seen.contains("40 120"), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(seen.contains("xterm-256color"), 5000);

        session.write("exit 3\r");
        QTRY_COMPARE_WITH_TIMEOUT(exits.count(), 1, 5000);
        QCOMPARE(exits.first().first().toInt(), 3);
        QVERIFY(!session.running());
    }

    void stoppingHangsUpTheShell() {
        ShellSandbox sandbox;
        TerminalSession session;
        QVERIFY(session.start(sandbox.directory.path(), 80, 24));
        QTRY_VERIFY_WITH_TIMEOUT(session.running(), 5000);
        session.write("sleep 600\r");
        QTest::qWait(150);

        QElapsedTimer timer;
        timer.start();
        session.stop();
        QVERIFY(!session.running());
        QVERIFY2(timer.elapsed() < 2000, "stop() must not wait on a busy shell");
        session.stop();  // twice is fine
    }

    // --- Who has the keyboard ----------------------------------------------

    void nothingIsATerminalBeforeTheTalkStarts() {
        Presentation presentation;
        presentation.setDeck(deck({QStringLiteral("terminal")}));
        // The editor preview and a deck that is merely loaded never have one.
        QVERIFY(!presentation.terminalVisible());
        QVERIFY(!presentation.terminalFocused());
        QVERIFY(presentation.navigator().showingTerminal());
    }

    void arrivingOnATerminalGivesItTheKeyboard() {
        Presentation presentation;
        begin(presentation, deck({QString(), QStringLiteral("terminal"), QString()}));
        QVERIFY(!presentation.terminalVisible());

        presentation.handleKey(Qt::Key_Right, Qt::NoModifier, QString());
        QCOMPARE(presentation.slideIndex(), 1);
        QVERIFY(presentation.terminalVisible());
        QVERIFY(presentation.terminalFocused());

        // With the keyboard, the arrows are the shell's: the talk stays put.
        QVERIFY(presentation.handleKey(Qt::Key_Right, Qt::NoModifier, QString()));
        QVERIFY(presentation.handleKey(Qt::Key_B, Qt::NoModifier, QStringLiteral("b")));
        QVERIFY(presentation.handleKey(Qt::Key_Escape, Qt::NoModifier, QString()));
        QCOMPARE(presentation.slideIndex(), 1);
        QVERIFY(presentation.blank().isEmpty());
        QVERIFY(presentation.active());

        presentation.stop();
    }

    void shiftEscapeGivesTheKeyboardBackAndEnterTakesIt() {
        Presentation presentation;
        begin(presentation, deck({QString(), QStringLiteral("terminal"), QString()}));
        presentation.gotoSlide(1);
        QVERIFY(presentation.terminalFocused());

        QVERIFY(shiftEscape(presentation));
        // Still on the terminal slide, and it is still up.
        QCOMPARE(presentation.slideIndex(), 1);
        QVERIFY(presentation.terminalVisible());
        QVERIFY(!presentation.terminalFocused());

        QVERIFY(presentation.handleKey(Qt::Key_Return, Qt::NoModifier, QStringLiteral("\r")));
        QVERIFY(presentation.terminalFocused());

        // And once it is back with the talk, the arrows move on as usual.
        shiftEscape(presentation);
        presentation.handleKey(Qt::Key_Right, Qt::NoModifier, QString());
        QCOMPARE(presentation.slideIndex(), 2);
        QVERIFY(!presentation.terminalVisible());
        QVERIFY(!presentation.terminalFocused());

        // Coming back is arriving again.
        presentation.handleKey(Qt::Key_Left, Qt::NoModifier, QString());
        QVERIFY(presentation.terminalFocused());

        presentation.stop();
    }

    void aTerminalPoppedByItsKeyClosesOnShiftEscape() {
        Presentation presentation;
        begin(presentation, deck({QString(), QString(), QStringLiteral("t:terminal:skip")}));
        presentation.gotoSlide(1);

        QVERIFY(presentation.handleKey(Qt::Key_T, Qt::NoModifier, QStringLiteral("t")));
        QCOMPARE(presentation.recall(), QStringLiteral("t"));
        QVERIFY(presentation.terminalVisible());
        QVERIFY(presentation.terminalFocused());

        // `t` is typing now, not a toggle: the overlay stays up.
        QVERIFY(presentation.handleKey(Qt::Key_T, Qt::NoModifier, QStringLiteral("t")));
        QCOMPARE(presentation.recall(), QStringLiteral("t"));

        QVERIFY(shiftEscape(presentation));
        QVERIFY(presentation.recall().isEmpty());
        QCOMPARE(presentation.slideIndex(), 1);
        QVERIFY(!presentation.terminalVisible());
        QVERIFY(!presentation.terminalFocused());

        presentation.stop();
    }

    void withoutATerminalShiftEscapeIsStillEscape() {
        Presentation presentation;
        begin(presentation, deck({QString(), QStringLiteral("q")}));
        // The chord means "leave the terminal" only when one is on screen.
        // Anywhere else it does what it did before terminals existed.
        presentation.handleKey(Qt::Key_Q, Qt::NoModifier, QStringLiteral("q"));
        QCOMPARE(presentation.recall(), QStringLiteral("q"));
        QVERIFY(shiftEscape(presentation));
        QVERIFY(presentation.recall().isEmpty());
        QVERIFY(presentation.active());
        presentation.stop();
    }

    void theOverviewTakesTheKeyboardBack() {
        Presentation presentation;
        begin(presentation, deck({QStringLiteral("terminal"), QString()}));
        QVERIFY(presentation.terminalFocused());
        shiftEscape(presentation);

        presentation.handleKey(Qt::Key_O, Qt::NoModifier, QStringLiteral("o"));
        QVERIFY(presentation.overview());
        QVERIFY(!presentation.terminalVisible());
        presentation.handleKey(Qt::Key_O, Qt::NoModifier, QStringLiteral("o"));
        QVERIFY(presentation.terminalVisible());

        presentation.stop();
    }

    // --- The shell behind a talk ---------------------------------------------

    void oneShellLastsTheWholeTalk() {
        ShellSandbox sandbox;
        Presentation presentation;
        presentation.setTerminalDirectory(sandbox.directory.path());
        QSignalSpy scripts(&presentation, &Presentation::runInView);
        const QString audience = QStringLiteral("audience");

        begin(presentation, deck({QString(), QStringLiteral("terminal"), QString()}));
        QVERIFY(!presentation.terminalSession());

        // A page cannot conjure a shell: not off a terminal slide.
        presentation.terminalAttached(audience);
        presentation.terminalResized(audience, 90, 30);
        QVERIFY(!presentation.terminalSession());

        presentation.gotoSlide(1);
        presentation.terminalResized(audience, 90, 30);
        QVERIFY(presentation.terminalSession());
        QTRY_VERIFY_WITH_TIMEOUT(presentation.terminalSession()->running(), 5000);
        QCOMPARE(presentation.terminalSession()->cols(), 90);
        QCOMPARE(presentation.terminalSession()->rows(), 30);

        presentation.terminalInput(audience, "MARK=kept; echo first-$((2+3))\r");
        QTRY_VERIFY_WITH_TIMEOUT(written(scripts, audience).contains("first-5"), 5000);

        // Away and back: the same shell, with what was set in it still set.
        shiftEscape(presentation);
        presentation.gotoSlide(2);
        QVERIFY(!presentation.terminalVisible());
        // Not on screen, so nothing typed reaches it.
        presentation.terminalInput(audience, "echo leaked-$((1+1))\r");
        presentation.gotoSlide(1);
        QVERIFY(presentation.terminalFocused());
        presentation.terminalInput(audience, "echo again-$MARK\r");
        QTRY_VERIFY_WITH_TIMEOUT(written(scripts, audience).contains("again-kept"), 5000);
        QVERIFY(!written(scripts, audience).contains("leaked-2"));

        // The presenter window's page joins late and is shown the same screen.
        const QString presenter = QStringLiteral("presenter");
        presentation.viewReady(presenter);
        presentation.terminalAttached(presenter);
        QVERIFY(written(scripts, presenter).contains("first-5"));
        bool sized = false;
        for (const QList<QVariant> &emission : scripts) {
            sized = sized || (emission.at(0).toString() == presenter
                              && emission.at(1).toString().contains(
                                     QStringLiteral("terminalSetSize(90, 30)")));
        }
        QVERIFY(sized);

        // Only the page that owns the grid may change it.
        presentation.terminalResized(presenter, 20, 5);
        QCOMPARE(presentation.terminalSession()->cols(), 90);

        QPointer<TerminalSession> session = presentation.terminalSession();
        presentation.stop();
        QVERIFY(session.isNull());
        QVERIFY(!presentation.terminalSession());
    }

    void onlyThePageWithTheKeyboardMayType() {
        ShellSandbox sandbox;
        Presentation presentation;
        presentation.setTerminalDirectory(sandbox.directory.path());
        QSignalSpy scripts(&presentation, &Presentation::runInView);
        const QString audience = QStringLiteral("audience");
        const QString presenter = QStringLiteral("presenter");

        begin(presentation, deck({QStringLiteral("terminal")}));
        presentation.viewReady(presenter);
        presentation.terminalAttached(audience);
        presentation.terminalAttached(presenter);
        presentation.terminalResized(audience, 80, 24);
        QTRY_VERIFY_WITH_TIMEOUT(presentation.terminalSession()
                                     && presentation.terminalSession()->running(), 5000);

        // Both pages run an emulator and both would answer the shell's
        // queries. The presenter's page took focus, so only it is believed.
        presentation.terminalFocusedIn(presenter);
        presentation.terminalInput(audience, "echo wrong-$((1+1))\r");
        presentation.terminalInput(presenter, "echo right-$((2+2))\r");
        QTRY_VERIFY_WITH_TIMEOUT(written(scripts, audience).contains("right-4"), 5000);
        QVERIFY(!written(scripts, audience).contains("wrong-2"));

        // Released, the shell hears nobody.
        shiftEscape(presentation);
        presentation.terminalInput(presenter, "echo late-$((3+3))\r");
        presentation.handleKey(Qt::Key_Return, Qt::NoModifier, QStringLiteral("\r"));
        presentation.terminalInput(presenter, "echo done-$((4+4))\r");
        QTRY_VERIFY_WITH_TIMEOUT(written(scripts, audience).contains("done-8"), 5000);
        QVERIFY(!written(scripts, audience).contains("late-6"));

        presentation.stop();
    }

    // --- Which screen (macOS) ----------------------------------------------

    void aLaptopPanelTakesThePresenterView() {
        // A Mac at a desk: the external monitor has the menu bar, so it is
        // "primary", and the audience still belongs on it.
        const MonitorAssignment desk = assignOutputs(
            {PresentationOutput{QStringLiteral("External"), true, false},
             PresentationOutput{QStringLiteral("Built-in"), false, true}});
        QCOMPARE(desk.presenter, 1);
        QCOMPARE(desk.audience, 0);

        // Desk monitor and a projector: the projector is the one that is not
        // the main display.
        const MonitorAssignment three = assignOutputs(
            {PresentationOutput{QStringLiteral("Desk"), true, false},
             PresentationOutput{QStringLiteral("Built-in"), false, true},
             PresentationOutput{QStringLiteral("Projector"), false, false}});
        QCOMPARE(three.presenter, 1);
        QCOMPARE(three.audience, 2);

        // Lid closed, or a platform that cannot say: `primary` decides, exactly
        // as it always has.
        const MonitorAssignment unknown = assignOutputs(
            {PresentationOutput{QStringLiteral("HDMI-A-1"), false, false},
             PresentationOutput{QStringLiteral("eDP-1"), true, false}});
        QCOMPARE(unknown.presenter, 1);
        QCOMPARE(unknown.audience, 0);

        // One screen is one screen, built in or not.
        const MonitorAssignment alone =
            assignOutputs({PresentationOutput{QStringLiteral("Built-in"), true, true}});
        QVERIFY(alone.sharedOutput());
    }

    void aMissingFontFallsBackToAnInstalledOne() {
        const QStringList preferred{QStringLiteral("iA Writer Mono S"),
                                    QStringLiteral("Menlo")};
        QCOMPARE(Platform::firstInstalledFamily(preferred, {QStringLiteral("Menlo")},
                                                QStringLiteral("Courier")),
                 QStringLiteral("Menlo"));
        QCOMPARE(Platform::firstInstalledFamily(
                     preferred, {QStringLiteral("ia writer mono s"), QStringLiteral("Menlo")},
                     QStringLiteral("Courier")),
                 QStringLiteral("iA Writer Mono S"));
        QCOMPARE(Platform::firstInstalledFamily(preferred, {}, QStringLiteral("Courier")),
                 QStringLiteral("Courier"));
    }
};

OMAPRESENT_TEST_SUITE(TerminalTest)
#include "tst_terminal.moc"
