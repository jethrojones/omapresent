#include <QGuiApplication>
#include <QJsonArray>
#include <QSet>
#include <QtTest>

#include "testrunner.h"
#include "omarchytheme.h"
#include "presentation.h"

// Suite for src/presentation.h. Windows, DBus and the web engine are not
// testable from here, so what is covered is the logic underneath them: where
// the talk is, how the keys move it, and which output each window belongs on.
class PresentationTest : public QObject {
    Q_OBJECT

private:
    // A deck in the shape of docs/renderer-contract.md §1. One entry per slide:
    // its recall binding, empty when unbound. Slides listed in `skipped` are
    // `--- {q, skip}` — poppable, but out of the linear flow.
    static QJsonObject deck(const QStringList &recallKeys, const QList<int> &skipped = {}) {
        QJsonArray slides;
        int flowIndex = 0;
        for (int i = 0; i < recallKeys.size(); ++i) {
            const bool skip = skipped.contains(i);
            slides.append(QJsonObject{
                {QStringLiteral("index"), skip ? -1 : flowIndex++},
                {QStringLiteral("markdown"), QStringLiteral("# Slide %1").arg(i + 1)},
                {QStringLiteral("recallKey"), recallKeys.at(i)},
                {QStringLiteral("skip"), skip}});
        }
        return QJsonObject{{QStringLiteral("mode"), QStringLiteral("present")},
                           {QStringLiteral("slides"), slides}};
    }

    static QJsonObject plainDeck(int slideCount) {
        return deck(QStringList(slideCount, QString()));
    }

    static PresentationOutput output(const QString &name, bool primary = false) {
        return PresentationOutput{name, primary};
    }

private slots:
    // --- Monitors (spec §5.1) ---------------------------------------------

    void noOutputsAssignsNothing() {
        const MonitorAssignment assignment = assignOutputs({});
        QVERIFY(assignment.isEmpty());
        QVERIFY(!assignment.sharedOutput());
        QCOMPARE(assignment.audience, -1);
        QCOMPARE(assignment.presenter, -1);
    }

    void oneOutputIsShared() {
        const MonitorAssignment assignment = assignOutputs({output("eDP-1", true)});
        QVERIFY(!assignment.isEmpty());
        QVERIFY(assignment.sharedOutput());
        QCOMPARE(assignment.audience, 0);
        QCOMPARE(assignment.presenter, 0);
    }

    void audienceTakesTheNonPrimaryOutput() {
        const MonitorAssignment assignment =
            assignOutputs({output("eDP-1", true), output("HDMI-A-1")});
        QCOMPARE(assignment.audience, 1);
        QCOMPARE(assignment.presenter, 0);
        QVERIFY(!assignment.sharedOutput());
    }

    void audienceTakesTheNonPrimaryOutputWhicheverWayRound() {
        const MonitorAssignment assignment =
            assignOutputs({output("HDMI-A-1"), output("eDP-1", true)});
        QCOMPARE(assignment.audience, 0);
        QCOMPARE(assignment.presenter, 1);
    }

    void aThirdOutputIsLeftAlone() {
        const MonitorAssignment assignment = assignOutputs(
            {output("HDMI-A-1"), output("eDP-1", true), output("DP-2")});
        QCOMPARE(assignment.audience, 0);
        QCOMPARE(assignment.presenter, 1);
    }

    void withNoPrimaryTheFirstOutputHoldsThePresenter() {
        const MonitorAssignment assignment =
            assignOutputs({output("HDMI-A-1"), output("HDMI-A-2")});
        QCOMPARE(assignment.presenter, 0);
        QCOMPARE(assignment.audience, 1);
    }

    // --- Navigation (spec §5.2) -------------------------------------------

    void nextStepsFragmentsBeforeSlides() {
        DeckNavigator navigator;
        navigator.setDeck(plainDeck(3));
        navigator.noteFragmentCount(0, 3);

        QVERIFY(navigator.next());
        QCOMPARE(navigator.slideIndex(), 0);
        QCOMPARE(navigator.fragment(), 1);
        QVERIFY(navigator.next());
        QCOMPARE(navigator.fragment(), 2);

        QVERIFY(navigator.next());
        QCOMPARE(navigator.slideIndex(), 1);
        QCOMPARE(navigator.fragment(), 0);
    }

    void nextStopsAtTheEndOfTheDeck() {
        DeckNavigator navigator;
        navigator.setDeck(plainDeck(2));
        QVERIFY(navigator.next());
        QCOMPARE(navigator.slideIndex(), 1);
        QVERIFY(!navigator.next());
        QCOMPARE(navigator.slideIndex(), 1);
        QCOMPARE(navigator.fragment(), 0);
    }

    void previousStopsAtTheStartOfTheDeck() {
        DeckNavigator navigator;
        navigator.setDeck(plainDeck(2));
        QVERIFY(!navigator.previous());
        QCOMPARE(navigator.slideIndex(), 0);
    }

    void previousUndoesTheLastStepForward() {
        DeckNavigator navigator;
        navigator.setDeck(plainDeck(3));
        navigator.noteFragmentCount(0, 3);
        navigator.next();
        navigator.next();
        navigator.next();  // onto slide 1
        QCOMPARE(navigator.slideIndex(), 1);

        QVERIFY(navigator.previous());
        QCOMPARE(navigator.slideIndex(), 0);
        QCOMPARE(navigator.fragment(), 2);
    }

    void steppingBackOntoAnUnseenSlideWaitsForItsFragmentCount() {
        DeckNavigator navigator;
        navigator.setDeck(plainDeck(4));
        navigator.gotoSlide(2);

        QVERIFY(navigator.previous());
        QCOMPARE(navigator.slideIndex(), 1);
        // Nothing has told us how many fragments slide 1 has yet.
        QCOMPARE(navigator.fragment(), 0);

        navigator.noteFragmentCount(1, 4);
        QCOMPARE(navigator.fragment(), 3);
    }

    void aReportedFragmentCountNeverStrandsUsPastTheEnd() {
        DeckNavigator navigator;
        navigator.setDeck(plainDeck(2));
        navigator.noteFragmentCount(0, 4);
        navigator.next();
        navigator.next();
        QCOMPARE(navigator.fragment(), 2);

        // The slide was edited down to two fragments while we sat on it.
        navigator.noteFragmentCount(0, 2);
        QCOMPARE(navigator.fragment(), 1);
    }

    void gotoSlideClampsAndResetsFragments() {
        DeckNavigator navigator;
        navigator.setDeck(plainDeck(3));
        navigator.noteFragmentCount(0, 3);
        navigator.next();

        QVERIFY(navigator.gotoSlide(99));
        QCOMPARE(navigator.slideIndex(), 2);
        QCOMPARE(navigator.fragment(), 0);
        QVERIFY(navigator.gotoSlide(-5));
        QCOMPARE(navigator.slideIndex(), 0);
        QCOMPARE(navigator.fragment(), 0);
        QVERIFY(!navigator.gotoSlide(0));  // already there
    }

    void skippedSlidesLeaveTheFlowButKeepTheirBinding() {
        DeckNavigator navigator;
        navigator.setDeck(deck({QString(), QStringLiteral("q"), QString()}, {1}));

        QCOMPARE(navigator.slideCount(), 2);
        QCOMPARE(navigator.recallKeys(), QStringList{QStringLiteral("q")});
        QVERIFY(navigator.isRecallKey(QStringLiteral("q")));
    }

    void aShorterDeckClampsThePosition() {
        DeckNavigator navigator;
        navigator.setDeck(plainDeck(5));
        navigator.gotoSlide(4);

        navigator.setDeck(plainDeck(2));
        QCOMPARE(navigator.slideCount(), 2);
        QCOMPARE(navigator.slideIndex(), 1);
    }

    // --- Jump to a slide number (spec §5.2) -------------------------------

    void digitsThenEnterJumpToTheSlideNumber() {
        DeckNavigator navigator;
        navigator.setDeck(plainDeck(20));

        QVERIFY(navigator.appendJumpDigit(QStringLiteral("1")));
        QVERIFY(navigator.appendJumpDigit(QStringLiteral("2")));
        QCOMPARE(navigator.jumpBuffer(), QStringLiteral("12"));
        QVERIFY(navigator.jumpPending());

        QVERIFY(navigator.commitJump());
        QCOMPARE(navigator.slideIndex(), 11);
        QVERIFY(!navigator.jumpPending());
    }

    void aJumpPastTheEndLandsOnTheLastSlide() {
        DeckNavigator navigator;
        navigator.setDeck(plainDeck(3));

        navigator.appendJumpDigit(QStringLiteral("9"));
        navigator.appendJumpDigit(QStringLiteral("9"));
        QVERIFY(navigator.commitJump());
        QCOMPARE(navigator.slideIndex(), 2);
    }

    void aJumpBeforeTheStartLandsOnTheFirstSlide() {
        DeckNavigator navigator;
        navigator.setDeck(plainDeck(3));
        navigator.gotoSlide(2);

        navigator.appendJumpDigit(QStringLiteral("0"));
        QVERIFY(navigator.commitJump());
        QCOMPARE(navigator.slideIndex(), 0);
    }

    void onlyDigitsGoIntoTheJumpBuffer() {
        DeckNavigator navigator;
        navigator.setDeck(plainDeck(3));

        QVERIFY(!navigator.appendJumpDigit(QStringLiteral("x")));
        QVERIFY(!navigator.appendJumpDigit(QStringLiteral("12")));
        QVERIFY(!navigator.appendJumpDigit(QString()));
        QVERIFY(navigator.jumpBuffer().isEmpty());
        QVERIFY(!navigator.commitJump());
    }

    void aClearedJumpBufferCommitsNothing() {
        DeckNavigator navigator;
        navigator.setDeck(plainDeck(9));
        navigator.appendJumpDigit(QStringLiteral("7"));
        navigator.clearJump();

        QVERIFY(!navigator.commitJump());
        QCOMPARE(navigator.slideIndex(), 0);
    }

    void aBoundDigitIsTheBindingUntilSomethingIsTyped() {
        DeckNavigator navigator;
        navigator.setDeck(deck({QString(), QStringLiteral("1"), QString(), QString()}));

        // `1` is a recall key, so it never starts a jump...
        QVERIFY(!navigator.appendJumpDigit(QStringLiteral("1")));
        // ...but a leading zero gets you to slide 1 anyway.
        QVERIFY(navigator.appendJumpDigit(QStringLiteral("0")));
        QVERIFY(navigator.appendJumpDigit(QStringLiteral("1")));
        QVERIFY(navigator.commitJump());
        QCOMPARE(navigator.slideIndex(), 0);
    }

    // --- Recall overlays (spec §4.9) --------------------------------------

    void recallReturnsToExactlyWhereYouWere() {
        DeckNavigator navigator;
        navigator.setDeck(deck({QString(), QString(), QStringLiteral("q"), QString()}));
        navigator.gotoSlide(1);
        navigator.noteFragmentCount(1, 3);
        navigator.next();
        navigator.setScrollFraction(0.42);
        const DeckPosition before = navigator.position();

        QVERIFY(navigator.showRecall(QStringLiteral("q")));
        QCOMPARE(navigator.recall(), QStringLiteral("q"));

        QVERIFY(navigator.hideRecall());
        QVERIFY(navigator.recall().isEmpty());
        QCOMPARE(navigator.position().slideIndex, before.slideIndex);
        QCOMPARE(navigator.position().fragment, before.fragment);
        QCOMPARE(navigator.position().scrollFraction, before.scrollFraction);
    }

    void switchingOverlaysStillReturnsToTheSlideUnderneath() {
        DeckNavigator navigator;
        navigator.setDeck(deck({QString(), QStringLiteral("q"), QStringLiteral("x")}));
        navigator.gotoSlide(0);

        navigator.showRecall(QStringLiteral("q"));
        navigator.showRecall(QStringLiteral("x"));
        QCOMPARE(navigator.recall(), QStringLiteral("x"));

        navigator.hideRecall();
        QCOMPARE(navigator.slideIndex(), 0);
    }

    void unboundKeysShowNothing() {
        DeckNavigator navigator;
        navigator.setDeck(deck({QString(), QStringLiteral("q")}));

        QVERIFY(!navigator.showRecall(QStringLiteral("z")));
        QVERIFY(navigator.recall().isEmpty());
        QVERIFY(!navigator.hideRecall());
    }

    void anEditThatDropsTheBindingDropsTheOverlay() {
        DeckNavigator navigator;
        navigator.setDeck(deck({QString(), QStringLiteral("q")}));
        navigator.showRecall(QStringLiteral("q"));

        navigator.setDeck(plainDeck(2));
        QVERIFY(navigator.recall().isEmpty());
    }

    // --- Scroll memory (spec §4.7) ----------------------------------------

    void scrollIsRememberedPerSlide() {
        DeckNavigator navigator;
        navigator.setDeck(plainDeck(3));

        navigator.setScrollFraction(0.3);
        navigator.gotoSlide(1);
        QCOMPARE(navigator.scrollFraction(), 0.0);

        navigator.setScrollFraction(0.7);
        navigator.gotoSlide(0);
        QCOMPARE(navigator.scrollFraction(), 0.3);
        navigator.gotoSlide(1);
        QCOMPARE(navigator.scrollFraction(), 0.7);
    }

    void scrollIsClampedToTheSlide() {
        DeckNavigator navigator;
        navigator.setDeck(plainDeck(2));

        navigator.setScrollFraction(4.0);
        QCOMPARE(navigator.scrollFraction(), 1.0);
        navigator.setScrollFraction(-1.0);
        QCOMPARE(navigator.scrollFraction(), 0.0);
    }

    // --- Keys, through the object both windows talk to (spec §5.2) --------

    void arrowsAndHomeEndMoveTheDeck() {
        Presentation presentation;
        presentation.setDeck(plainDeck(4));

        QVERIFY(presentation.handleKey(Qt::Key_Right, Qt::NoModifier, QStringLiteral("")));
        QCOMPARE(presentation.slideIndex(), 1);
        QVERIFY(presentation.handleKey(Qt::Key_Left, Qt::NoModifier, QStringLiteral("")));
        QCOMPARE(presentation.slideIndex(), 0);
        QVERIFY(presentation.handleKey(Qt::Key_End, Qt::NoModifier, QStringLiteral("")));
        QCOMPARE(presentation.slideIndex(), 3);
        QVERIFY(presentation.handleKey(Qt::Key_Home, Qt::NoModifier, QStringLiteral("")));
        QCOMPARE(presentation.slideIndex(), 0);
        QCOMPARE(presentation.slideCount(), 4);
    }

    void digitsThenEnterJumpThroughTheKeyHandler() {
        Presentation presentation;
        presentation.setDeck(plainDeck(15));

        QVERIFY(presentation.handleKey(Qt::Key_1, Qt::NoModifier, QStringLiteral("1")));
        QVERIFY(presentation.handleKey(Qt::Key_4, Qt::NoModifier, QStringLiteral("4")));
        QCOMPARE(presentation.jumpBuffer(), QStringLiteral("14"));
        QVERIFY(presentation.handleKey(Qt::Key_Return, Qt::NoModifier, QStringLiteral("")));
        QCOMPARE(presentation.slideIndex(), 13);
        QVERIFY(presentation.jumpBuffer().isEmpty());
    }

    void escapeAbandonsAHalfTypedJump() {
        Presentation presentation;
        presentation.setDeck(plainDeck(15));
        QSignalSpy exits(&presentation, &Presentation::requestExit);

        presentation.handleKey(Qt::Key_1, Qt::NoModifier, QStringLiteral("1"));
        QVERIFY(presentation.handleKey(Qt::Key_Escape, Qt::NoModifier, QStringLiteral("")));
        QVERIFY(presentation.jumpBuffer().isEmpty());
        QCOMPARE(exits.count(), 0);
        QCOMPARE(presentation.slideIndex(), 0);
    }

    void blackAndWhiteToggleTheAudienceScreen() {
        Presentation presentation;
        presentation.setDeck(plainDeck(2));

        QVERIFY(presentation.handleKey(Qt::Key_B, Qt::NoModifier, QStringLiteral("b")));
        QCOMPARE(presentation.blank(), QStringLiteral("black"));
        QVERIFY(presentation.handleKey(Qt::Key_B, Qt::NoModifier, QStringLiteral("b")));
        QVERIFY(presentation.blank().isEmpty());

        presentation.handleKey(Qt::Key_W, Qt::NoModifier, QStringLiteral("w"));
        QCOMPARE(presentation.blank(), QStringLiteral("white"));
        presentation.handleKey(Qt::Key_B, Qt::NoModifier, QStringLiteral("b"));
        QCOMPARE(presentation.blank(), QStringLiteral("black"));
    }

    void escapeClearsWhatIsOpenBeforeItExits() {
        Presentation presentation;
        presentation.setDeck(plainDeck(2));
        QSignalSpy exits(&presentation, &Presentation::requestExit);

        presentation.setBlank(QStringLiteral("black"));
        presentation.handleKey(Qt::Key_Escape, Qt::NoModifier, QStringLiteral(""));
        QVERIFY(presentation.blank().isEmpty());
        QCOMPARE(exits.count(), 0);

        presentation.handleKey(Qt::Key_Escape, Qt::NoModifier, QStringLiteral(""));
        QCOMPARE(exits.count(), 1);
    }

    void aBoundKeyShowsAndHidesItsOverlay() {
        Presentation presentation;
        presentation.setDeck(deck({QString(), QStringLiteral("q")}));

        QVERIFY(presentation.handleKey(Qt::Key_Q, Qt::NoModifier, QStringLiteral("q")));
        QCOMPARE(presentation.recall(), QStringLiteral("q"));
        QVERIFY(presentation.handleKey(Qt::Key_Q, Qt::NoModifier, QStringLiteral("q")));
        QVERIFY(presentation.recall().isEmpty());

        // Space and Escape dismiss it too (spec §4.9).
        presentation.handleKey(Qt::Key_Q, Qt::NoModifier, QStringLiteral("q"));
        presentation.handleKey(Qt::Key_Space, Qt::NoModifier, QStringLiteral(" "));
        QVERIFY(presentation.recall().isEmpty());
        presentation.handleKey(Qt::Key_Q, Qt::NoModifier, QStringLiteral("q"));
        presentation.handleKey(Qt::Key_Escape, Qt::NoModifier, QStringLiteral(""));
        QVERIFY(presentation.recall().isEmpty());
    }

    void anOverlayLeavesThePositionAlone() {
        Presentation presentation;
        presentation.setDeck(deck({QString(), QString(), QStringLiteral("q")}));
        presentation.gotoSlide(1);

        presentation.handleKey(Qt::Key_Q, Qt::NoModifier, QStringLiteral("q"));
        presentation.handleKey(Qt::Key_Q, Qt::NoModifier, QStringLiteral("q"));
        QCOMPARE(presentation.slideIndex(), 1);
    }

    void aReservedKeyBeatsADeckBinding() {
        Presentation presentation;
        presentation.setDeck(deck({QString(), QStringLiteral("b")}));

        presentation.handleKey(Qt::Key_B, Qt::NoModifier, QStringLiteral("b"));
        QCOMPARE(presentation.blank(), QStringLiteral("black"));
        QVERIFY(presentation.recall().isEmpty());
    }

    void unhandledKeysAreLeftForTheWindow() {
        Presentation presentation;
        presentation.setDeck(plainDeck(2));

        QVERIFY(!presentation.handleKey(Qt::Key_Z, Qt::NoModifier, QStringLiteral("z")));
        QVERIFY(!presentation.handleKey(Qt::Key_S, Qt::ControlModifier, QStringLiteral("s")));
    }

    void controlQuestionOpensTheShortcutSheet() {
        Presentation presentation;
        presentation.setDeck(plainDeck(2));

        QVERIFY(presentation.handleKey(Qt::Key_Question,
                                       Qt::ControlModifier | Qt::ShiftModifier,
                                       QStringLiteral("?")));
        QVERIFY(presentation.shortcutsVisible());
        // Escape takes the sheet away without ending the talk.
        QSignalSpy exits(&presentation, &Presentation::requestExit);
        presentation.handleKey(Qt::Key_Escape, Qt::NoModifier, QStringLiteral(""));
        QVERIFY(!presentation.shortcutsVisible());
        QCOMPARE(exits.count(), 0);
    }

    void theOverviewGridPicksASlideWithTheArrows() {
        Presentation presentation;
        presentation.setDeck(plainDeck(4));

        QVERIFY(presentation.handleKey(Qt::Key_O, Qt::NoModifier, QStringLiteral("o")));
        QVERIFY(presentation.overview());
        presentation.handleKey(Qt::Key_Right, Qt::NoModifier, QStringLiteral(""));
        presentation.handleKey(Qt::Key_Down, Qt::NoModifier, QStringLiteral(""));
        QCOMPARE(presentation.slideIndex(), 2);

        presentation.handleKey(Qt::Key_Return, Qt::NoModifier, QStringLiteral(""));
        QVERIFY(!presentation.overview());
        QCOMPARE(presentation.slideIndex(), 2);
    }

    void notesOverlayIsForTheSingleScreenCase() {
        Presentation presentation;
        presentation.setDeck(plainDeck(2));

        // Nothing has been started, so no output is assigned and the overlay
        // stays out of the way rather than putting notes on the audience screen.
        QVERIFY(!presentation.singleOutput());
        presentation.handleKey(Qt::Key_N, Qt::NoModifier, QStringLiteral("n"));
        QVERIFY(!presentation.notesOverlay());
    }

    void theShortcutSheetListsEveryKeyOnce() {
        Presentation presentation;
        const QStringList rows = presentation.shortcutReference();

        QVERIFY(rows.size() >= 10);
        QSet<QString> keys;
        for (const QString &row : rows) {
            const QStringList parts = row.split(QLatin1Char('\t'));
            QCOMPARE(parts.size(), 2);
            QVERIFY(!parts.at(0).trimmed().isEmpty());
            QVERIFY(!parts.at(1).trimmed().isEmpty());
            keys.insert(parts.at(0));
        }
        QCOMPARE(keys.size(), rows.size());
    }

    // --- The projector legibility floor (spec §6) -------------------------

    void onlyTheAudienceGetsTheContrastFloor() {
        // Mid grey on near-black: legible a foot from a laptop, not legible on
        // a projector across a room.
        const QJsonObject exact{{QStringLiteral("background"), QStringLiteral("#101010")},
                                {QStringLiteral("foreground"), QStringLiteral("#3a3a3a")},
                                {QStringLiteral("muted"), QStringLiteral("#2b2b2b")},
                                {QStringLiteral("accent"), QStringLiteral("#243056")}};
        QJsonObject deckJson = plainDeck(2);
        deckJson.insert(QStringLiteral("palette"), exact);

        Presentation presentation;
        presentation.setDeck(deckJson);

        // The presenter, the preview and anything else keep the theme exactly
        // as it was chosen.
        QCOMPARE(presentation.deckForRole(QStringLiteral("presenter"))
                     .value(QStringLiteral("palette")).toObject(), exact);
        QCOMPARE(presentation.deckForRole(QStringLiteral("preview"))
                     .value(QStringLiteral("palette")).toObject(), exact);
        QCOMPARE(QJsonObject::fromVariantMap(presentation.palette()), exact);

        // The audience does not.
        const QJsonObject audience = presentation.deckForRole(QStringLiteral("audience"))
                                         .value(QStringLiteral("palette")).toObject();
        QVERIFY(audience != exact);
        QCOMPARE(audience, OmarchyTheme::paletteForRole(exact, QStringLiteral("audience")));
        // The background is what everything is measured against, so it stays.
        QCOMPARE(audience.value(QStringLiteral("background")).toString(),
                 QStringLiteral("#101010"));
        QVERIFY(OmarchyTheme::contrastRatio(
                    audience.value(QStringLiteral("foreground")).toString(),
                    QStringLiteral("#101010")) >= 4.5);
        // ...and the window chrome reads the same floored palette.
        QCOMPARE(QJsonObject::fromVariantMap(presentation.audiencePalette()), audience);

        // Only the palette differs; the slides both windows draw are identical.
        QCOMPARE(presentation.deckForRole(QStringLiteral("audience"))
                     .value(QStringLiteral("slides")).toArray(),
                 presentation.deckForRole(QStringLiteral("presenter"))
                     .value(QStringLiteral("slides")).toArray());
    }

    void aPaletteThatAlreadyClearsTheFloorIsLeftAlone() {
        const QJsonObject exact{{QStringLiteral("background"), QStringLiteral("#101010")},
                                {QStringLiteral("foreground"), QStringLiteral("#ffffff")},
                                {QStringLiteral("muted"), QStringLiteral("#cccccc")},
                                {QStringLiteral("accent"), QStringLiteral("#eeeeee")}};
        QJsonObject deckJson = plainDeck(2);
        deckJson.insert(QStringLiteral("palette"), exact);

        Presentation presentation;
        presentation.setDeck(deckJson);
        QCOMPARE(presentation.deckForRole(QStringLiteral("audience"))
                     .value(QStringLiteral("palette")).toObject(), exact);
    }

    void aDeckWithNoPaletteStillRenders() {
        // A deck opened before the theme has loaded (renderer contract §1).
        Presentation presentation;
        presentation.setDeck(plainDeck(2));

        QVERIFY(presentation.audiencePalette().isEmpty());
        QCOMPARE(presentation.deckForRole(QStringLiteral("audience"))
                     .value(QStringLiteral("slides")).toArray().size(), 2);
    }

    void theTimerResets() {
        Presentation presentation;
        QCOMPARE(presentation.elapsedSeconds(), 0);
        presentation.resetTimer();
        QCOMPARE(presentation.elapsedSeconds(), 0);
    }

    void everyScreenIsAnOutput() {
        Presentation presentation;
        QCOMPARE(presentation.outputs().size(), QGuiApplication::screens().size());
    }
};

OMAPRESENT_TEST_SUITE(PresentationTest)
#include "tst_presentation.moc"
