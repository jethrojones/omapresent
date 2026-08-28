#include <QtTest>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>

#include "testrunner.h"
#include "deckmodel.h"
#include "presentation.h"
#include "renderhost.h"

// The editor-to-everything-else seam, spec §4.10: "live two-way: editing
// updates the preview and any running presentation immediately, holding slide
// + scroll position where possible."
//
// An edit is a full re-parse: DeckModel::setSource -> toJson -> the deck the
// preview and both presentation windows are pushed. Everything here goes
// through that path and through DeckNavigator, which is the pure, windowless
// authority for where the talk is, so none of it needs a compositor.
//
// Do not add QTEST_MAIN — see tests/testrunner.h.

namespace {

QString slideBody(int n)
{
    return QStringLiteral("# Slide %1\n\nProse for slide %1.\n").arg(n);
}

QStringList slideBodies(int count)
{
    QStringList bodies;
    for (int i = 0; i < count; ++i)
        bodies.append(slideBody(i));
    return bodies;
}

QString joinSlides(const QStringList &bodies)
{
    return bodies.join(QStringLiteral("\n---\n\n"));
}

// One edit: re-parse and push the new deck the way the app does.
void applyEdit(DeckModel *deck, DeckNavigator *navigator, const QString &source)
{
    deck->setSource(source);
    navigator->setDeck(deck->toJson());
}

QString markdownAt(const DeckModel &deck, int flowIndex)
{
    const QJsonArray slides = deck.toJson().value(QStringLiteral("slides")).toArray();
    for (const QJsonValue &value : slides) {
        const QJsonObject slide = value.toObject();
        if (slide.value(QStringLiteral("index")).toInt() == flowIndex)
            return slide.value(QStringLiteral("markdown")).toString();
    }
    return {};
}

int flowIndexOfMarkdown(const DeckModel &deck, const QString &markdown)
{
    const QJsonArray slides = deck.toJson().value(QStringLiteral("slides")).toArray();
    for (const QJsonValue &value : slides) {
        const QJsonObject slide = value.toObject();
        if (slide.value(QStringLiteral("markdown")).toString() == markdown)
            return slide.value(QStringLiteral("index")).toInt();
    }
    return -1;
}

} // namespace

class LiveSyncTest : public QObject {
    Q_OBJECT

private slots:

    // --- Holding the slide across an edit ---------------------------------

    void editingTheCurrentSlideKeepsTheIndex()
    {
        QStringList bodies = slideBodies(10);
        DeckModel deck;
        DeckNavigator navigator;
        applyEdit(&deck, &navigator, joinSlides(bodies));

        navigator.gotoSlide(7);
        QCOMPARE(navigator.slideIndex(), 7);

        // Type into slide 7.
        bodies[7] = QStringLiteral("# Slide 7\n\nProse for slide 7, now with more words.\n");
        applyEdit(&deck, &navigator, joinSlides(bodies));

        QCOMPARE(navigator.slideCount(), 10);
        QCOMPARE(navigator.slideIndex(), 7);
        QVERIFY(markdownAt(deck, 7).contains(QStringLiteral("more words")));
    }

    void editingAnotherSlideKeepsTheCurrentOne()
    {
        QStringList bodies = slideBodies(10);
        DeckModel deck;
        DeckNavigator navigator;
        applyEdit(&deck, &navigator, joinSlides(bodies));
        navigator.gotoSlide(7);

        // An edit somewhere else entirely must not move the presenter.
        bodies[2] = QStringLiteral("# Slide 2\n\nRewritten from scratch.\n");
        applyEdit(&deck, &navigator, joinSlides(bodies));

        QCOMPARE(navigator.slideIndex(), 7);
        QCOMPARE(markdownAt(deck, 7), slideBody(7));
    }

    void addingASlideAfterTheCurrentOneKeepsTheCurrentOne()
    {
        QStringList bodies = slideBodies(10);
        DeckModel deck;
        DeckNavigator navigator;
        applyEdit(&deck, &navigator, joinSlides(bodies));
        navigator.gotoSlide(7);

        bodies.insert(9, QStringLiteral("# A new slide near the end\n"));
        applyEdit(&deck, &navigator, joinSlides(bodies));

        QCOMPARE(navigator.slideCount(), 11);
        QCOMPARE(navigator.slideIndex(), 7);
        QCOMPARE(markdownAt(deck, 7), slideBody(7));
    }

    void skippedSlidesDoNotShiftTheFlowIndex()
    {
        QStringList bodies = slideBodies(10);
        DeckModel deck;
        DeckNavigator navigator;
        applyEdit(&deck, &navigator, joinSlides(bodies));
        navigator.gotoSlide(7);

        // A recall slide tagged `{q, skip}` is outside the linear flow, so
        // adding one before the current slide must not move anything.
        QString source = joinSlides(bodies);
        const int marker = source.indexOf(slideBody(3));
        source.insert(marker, QStringLiteral("# An aside\n\n--- {q, skip}\n\n"));
        applyEdit(&deck, &navigator, source);

        QCOMPARE(navigator.slideCount(), 10);
        QCOMPARE(navigator.slideIndex(), 7);
        QCOMPARE(markdownAt(deck, 7), slideBody(7));
        QCOMPARE(navigator.recallKeys(), QStringList({QStringLiteral("q")}));
    }

    // --- Inserting and deleting before the current slide -------------------

    void insertingASlideBeforeTheCurrentOneFollowsTheUnchangedContent()
    {
        QStringList bodies = slideBodies(10);
        DeckModel deck;
        DeckNavigator navigator;
        applyEdit(&deck, &navigator, joinSlides(bodies));
        navigator.gotoSlide(7);
        const QString onScreen = markdownAt(deck, 7);
        QCOMPARE(onScreen, slideBody(7));

        bodies.insert(2, QStringLiteral("# An inserted slide\n"));
        applyEdit(&deck, &navigator, joinSlides(bodies));

        QCOMPARE(navigator.slideCount(), 11);
        QCOMPARE(navigator.slideIndex(), 8);
        // The presenter follows the unchanged content to its new flow index.
        QCOMPARE(markdownAt(deck, navigator.slideIndex()), onScreen);

        QCOMPARE(flowIndexOfMarkdown(deck, onScreen), 8);
    }

    void deletingASlideBeforeTheCurrentOneFollowsTheUnchangedContent()
    {
        QStringList bodies = slideBodies(10);
        DeckModel deck;
        DeckNavigator navigator;
        applyEdit(&deck, &navigator, joinSlides(bodies));
        navigator.gotoSlide(7);
        const QString onScreen = markdownAt(deck, 7);

        bodies.removeAt(2);
        applyEdit(&deck, &navigator, joinSlides(bodies));

        QCOMPARE(navigator.slideCount(), 9);
        QCOMPARE(navigator.slideIndex(), 6);
        // The presenter follows the unchanged content to its new flow index.
        QCOMPARE(markdownAt(deck, navigator.slideIndex()), onScreen);
        QCOMPARE(flowIndexOfMarkdown(deck, onScreen), 6);
    }

    // --- Deleting the slide the presenter is on ---------------------------

    void deletingTheCurrentSlideLandsOnTheOneThatFollowedIt()
    {
        // DECISION: the index is kept, so the presenter lands on the slide that
        // came *after* the deleted one — exactly where pressing Right would
        // have taken them. Moving backwards instead would rewind the talk, and
        // blanking the screen would be worse than either. This is also what
        // every list UI does when you delete the selected row.
        QStringList bodies = slideBodies(10);
        DeckModel deck;
        DeckNavigator navigator;
        applyEdit(&deck, &navigator, joinSlides(bodies));
        navigator.gotoSlide(7);

        bodies.removeAt(7);
        applyEdit(&deck, &navigator, joinSlides(bodies));

        QCOMPARE(navigator.slideCount(), 9);
        QCOMPARE(navigator.slideIndex(), 7);
        QCOMPARE(markdownAt(deck, 7), slideBody(8));
    }

    void deletingTheLastSlideWhileOnItClampsToTheNewLast()
    {
        QStringList bodies = slideBodies(10);
        DeckModel deck;
        DeckNavigator navigator;
        applyEdit(&deck, &navigator, joinSlides(bodies));
        navigator.gotoSlide(9);

        bodies.removeLast();
        applyEdit(&deck, &navigator, joinSlides(bodies));

        QCOMPARE(navigator.slideCount(), 9);
        QCOMPARE(navigator.slideIndex(), 8);
        QCOMPARE(markdownAt(deck, 8), slideBody(8));
    }

    void deletingEverySlideLeavesNoDanglingIndex()
    {
        DeckModel deck;
        DeckNavigator navigator;
        applyEdit(&deck, &navigator, joinSlides(slideBodies(10)));
        navigator.gotoSlide(7);

        // Select all, delete.
        applyEdit(&deck, &navigator, QString());

        QCOMPARE(deck.slideCount(), 0);
        QCOMPARE(navigator.slideCount(), 0);
        QCOMPARE(navigator.slideIndex(), 0);
        QCOMPARE(navigator.fragmentCount(), 0);
        // Nothing to navigate to, and asking must not crash or leave a
        // position pointing past the end.
        QCOMPARE(navigator.gotoSlide(5), false);
        QCOMPARE(navigator.next(), false);
        QCOMPARE(navigator.previous(), false);
        QCOMPARE(navigator.slideIndex(), 0);

        // Typing the deck back in recovers cleanly.
        applyEdit(&deck, &navigator, joinSlides(slideBodies(3)));
        QCOMPARE(navigator.slideCount(), 3);
        QCOMPARE(navigator.slideIndex(), 0);
    }

    void deletingDownToFewerSlidesThanTheCurrentIndexClamps()
    {
        DeckModel deck;
        DeckNavigator navigator;
        applyEdit(&deck, &navigator, joinSlides(slideBodies(10)));
        navigator.gotoSlide(9);

        applyEdit(&deck, &navigator, joinSlides(slideBodies(3)));

        QCOMPARE(navigator.slideCount(), 3);
        QCOMPARE(navigator.slideIndex(), 2);
        QVERIFY(navigator.slideIndex() < navigator.slideCount());
    }

    // --- Scroll position within a slide -----------------------------------

    void scrollPositionSurvivesAnEditInsideTheSlide()
    {
        QStringList bodies = slideBodies(10);
        bodies[3] = QStringLiteral("# Slide 3\n\n")
            + QStringLiteral("A long line of prose.\n").repeated(60);

        DeckModel deck;
        DeckNavigator navigator;
        applyEdit(&deck, &navigator, joinSlides(bodies));
        navigator.gotoSlide(3);
        navigator.setScrollFraction(0.42);
        QCOMPARE(navigator.scrollFraction(), 0.42);

        // Typing further down the same slide must not throw the presenter back
        // to the top of it.
        bodies[3] += QStringLiteral("\nOne more paragraph.\n");
        applyEdit(&deck, &navigator, joinSlides(bodies));

        QCOMPARE(navigator.slideIndex(), 3);
        QCOMPARE(navigator.scrollFraction(), 0.42);
    }

    void scrollPositionIsRememberedPerSlideAcrossEdits()
    {
        QStringList bodies = slideBodies(10);
        DeckModel deck;
        DeckNavigator navigator;
        applyEdit(&deck, &navigator, joinSlides(bodies));

        navigator.gotoSlide(2);
        navigator.setScrollFraction(0.25);
        navigator.gotoSlide(5);
        navigator.setScrollFraction(0.75);

        bodies[8] = QStringLiteral("# Slide 8\n\nEdited somewhere else.\n");
        applyEdit(&deck, &navigator, joinSlides(bodies));

        QCOMPARE(navigator.slideIndex(), 5);
        QCOMPARE(navigator.scrollFraction(), 0.75);
        navigator.gotoSlide(2);
        QCOMPARE(navigator.scrollFraction(), 0.25);
    }

    // --- Cursor to slide, and back ----------------------------------------

    void slideIndexForLineRoundTripsInsideEverySlide()
    {
        DeckModel deck;
        deck.setSource(QStringLiteral(
            "---\ntitle: Round Trip\n---\n\n"
            "# One\n\nProse.\n\n"
            "--- {q}\n\n"
            "# Two\n\n- a\n- b\n\n"
            "---\n\n"
            "# Three\n\nLast.\n"));

        const QVector<Slide> slides = deck.slides();
        QCOMPARE(slides.size(), 3);

        for (int i = 0; i < slides.size(); ++i) {
            const Slide &slide = slides.at(i);
            for (int line = slide.sourceStartLine; line <= slide.sourceEndLine; ++line) {
                QCOMPARE(deck.slideIndexForLine(line), i);
                // And back again: the slide it names really does own the line.
                const Slide &found = slides.at(deck.slideIndexForLine(line));
                QVERIFY(line >= found.sourceStartLine && line <= found.sourceEndLine);
            }
        }
    }

    void slideIndexForLineOnBlanksSeparatorsAndFrontmatter()
    {
        //  0 ---            5 (blank)      10 # Two
        //  1 title:         6 Prose.       11 (blank)
        //  2 ---            7 (blank)      12 Last.
        //  3 (blank)        8 --- {q}
        //  4 # One          9 (blank)
        DeckModel deck;
        deck.setSource(QStringLiteral(
            "---\ntitle: Round Trip\n---\n\n"
            "# One\n\nProse.\n\n"
            "--- {q}\n\n"
            "# Two\n\nLast.\n"));

        // Frontmatter belongs to no slide.
        QCOMPARE(deck.slideIndexForLine(0), -1);
        QCOMPARE(deck.slideIndexForLine(1), -1);
        QCOMPARE(deck.slideIndexForLine(2), -1);

        // The blank line above the first slide's content is still that slide's.
        QCOMPARE(deck.slideIndexForLine(3), 0);
        QCOMPARE(deck.slideIndexForLine(4), 0);
        // A blank line inside a slide.
        QCOMPARE(deck.slideIndexForLine(5), 0);
        // The blank line after the last content line, before the separator.
        QCOMPARE(deck.slideIndexForLine(7), 0);
        // The separator line itself belongs to the slide it introduces, so a
        // caret parked on `---` presents from the slide below it, which is the
        // one the author is about to write.
        QCOMPARE(deck.slideIndexForLine(8), 1);
        QCOMPARE(deck.slideIndexForLine(9), 1);
        QCOMPARE(deck.slideIndexForLine(10), 1);

        // Past the end of the file.
        QCOMPARE(deck.slideIndexForLine(999), -1);
        QCOMPARE(deck.slideIndexForLine(-1), -1);
    }

    void slideIndexForLineNeverGoesBackwards()
    {
        DeckModel deck;
        deck.setSource(QStringLiteral(
            "---\ntitle: Ordering\n---\n\n"
            "# One\n\nProse.\n\n"
            "---\n\n"
            "# Two\n\nProse.\n\n"
            "// ---\n\n"
            "# A draft nobody sees\n\n"
            "---\n\n"
            "# Three\n"));

        QCOMPARE(deck.slideCount(), 3);

        int previous = -1;
        int lines = deck.source().split(QLatin1Char('\n')).size();
        for (int line = 0; line < lines; ++line) {
            const int index = deck.slideIndexForLine(line);
            if (index < 0)
                continue; // frontmatter, or the dropped draft slide
            QVERIFY2(index >= previous,
                     qPrintable(QStringLiteral("line %1 maps back to slide %2 after %3")
                                    .arg(line).arg(index).arg(previous)));
            previous = index;
        }
        QCOMPARE(previous, 2);

        // The draft slide's lines belong to nothing, and its neighbours are
        // still numbered as if it were not there.
        const int draft = deck.source().split(QLatin1Char('\n')).indexOf(
            QStringLiteral("# A draft nobody sees"));
        QVERIFY(draft > 0);
        QCOMPARE(deck.slideIndexForLine(draft), -1);
    }

    // --- An edit that changes what a separator means ----------------------

    void removingTheBlankLineAboveASeparatorMergesTwoSlides()
    {
        DeckModel deck;
        DeckNavigator navigator;
        const QString split = QStringLiteral(
            "# One\n\nProse one.\n\n---\n\n# Two\n\nProse two.\n\n---\n\n# Three\n");
        applyEdit(&deck, &navigator, split);
        QCOMPARE(navigator.slideCount(), 3);
        navigator.gotoSlide(2);
        const QString onScreen = markdownAt(deck, navigator.slideIndex());
        QCOMPARE(onScreen, QStringLiteral("# Three\n"));

        // Delete the blank line above the first separator: `---` is now a
        // Setext underline for "Prose one.", so slides one and two merge.
        const QString merged = QStringLiteral(
            "# One\n\nProse one.\n---\n\n# Two\n\nProse two.\n\n---\n\n# Three\n");
        applyEdit(&deck, &navigator, merged);

        QCOMPARE(deck.slideCount(), 2);
        QVERIFY(deck.slides().first().markdown.contains(QStringLiteral("Prose one.")));
        QVERIFY(deck.slides().first().markdown.contains(QStringLiteral("# Two")));

        // The presenter follows the unchanged last slide to its new flow index.
        QCOMPARE(navigator.slideCount(), 2);
        QCOMPARE(navigator.slideIndex(), 1);
        QVERIFY(navigator.slideIndex() < navigator.slideCount());
        QCOMPARE(markdownAt(deck, navigator.slideIndex()), onScreen);

        // Typing the blank line back splits them again.
        applyEdit(&deck, &navigator, split);
        QCOMPARE(navigator.slideCount(), 3);
        QCOMPARE(navigator.slideIndex(), 2);
        QCOMPARE(markdownAt(deck, navigator.slideIndex()), onScreen);
    }

    void halfTypedSeparatorsDoNotSplitTheDeck()
    {
        // The keystrokes on the way to a separator: each intermediate state has
        // to parse to something sane, because the preview re-renders on all of
        // them.
        DeckModel deck;
        DeckNavigator navigator;
        const QStringList keystrokes = {
            QStringLiteral("# One\n\nProse.\n\n"),
            QStringLiteral("# One\n\nProse.\n\n-"),
            QStringLiteral("# One\n\nProse.\n\n--"),
            QStringLiteral("# One\n\nProse.\n\n---"),
            QStringLiteral("# One\n\nProse.\n\n---\n"),
            QStringLiteral("# One\n\nProse.\n\n---\n\n"),
            QStringLiteral("# One\n\nProse.\n\n---\n\n#"),
            QStringLiteral("# One\n\nProse.\n\n---\n\n# Two\n"),
        };

        for (const QString &source : keystrokes) {
            applyEdit(&deck, &navigator, source);
            QVERIFY2(deck.slideCount() >= 1, qPrintable(source));
            QVERIFY2(navigator.slideIndex() >= 0
                         && navigator.slideIndex() < qMax(1, navigator.slideCount()),
                     qPrintable(source));
        }

        QCOMPARE(deck.slideCount(), 2);
        QCOMPARE(navigator.slideCount(), 2);
    }

    // --- What actually gets pushed downstream ------------------------------

    void aLiveEditPushesUpdateSoThePageKeepsItsPosition()
    {
        DeckModel deck;
        deck.setSource(joinSlides(slideBodies(3)));

        // Contract §2: update() is the live-edit entry point and "explicitly
        // preserves position"; render() is the full replace.
        const QString script = RenderHost::callScript(QStringLiteral("update"), deck.toJson());
        QVERIFY(script.startsWith(QStringLiteral("window.omapresent && window.omapresent.update(")));
        QVERIFY(script.endsWith(QStringLiteral(");")));

        // What it carries is the deck, unaltered.
        const int open = script.indexOf(QLatin1Char('('));
        const QString payload = script.mid(open + 1, script.size() - open - 3);
        const QJsonObject sent = QJsonDocument::fromJson(payload.toUtf8()).object();
        QCOMPARE(sent.value(QStringLiteral("slides")).toArray().size(), 3);
        QCOMPARE(sent, deck.toJson());
    }

    void composedDeckKeepsTheSlidesAcrossAnEdit()
    {
        DeckModel deck;
        deck.setSource(joinSlides(slideBodies(4)));

        const QJsonObject composed = RenderHost::composeDeck(
            QStringLiteral("preview"), deck.toJson(), {}, {}, {}, QString(), 1.0);
        QCOMPARE(composed.value(QStringLiteral("mode")).toString(), QStringLiteral("preview"));
        QCOMPARE(composed.value(QStringLiteral("slides")).toArray().size(), 4);

        deck.setSource(joinSlides(slideBodies(5)));
        const QJsonObject after = RenderHost::composeDeck(
            QStringLiteral("preview"), deck.toJson(), {}, {}, {}, QString(), 1.0);
        QCOMPARE(after.value(QStringLiteral("slides")).toArray().size(), 5);
    }

    void renderHostAdoptsThePositionTheRendererReports()
    {
        RenderHost host;
        QSignalSpy spy(&host, &RenderHost::stateChanged);

        host.state(QStringLiteral(
            R"({"slideIndex":7,"slideCount":10,"fragment":1,"fragmentCount":4,)"
            R"("scrollFraction":0.42,"scrollable":true,"recall":"","blank":"",)"
            R"("overview":false,"heading":"Where we are"})"));

        QCOMPARE(spy.size(), 1);
        QCOMPARE(host.slideIndex(), 7);
        QCOMPARE(host.slideCount(), 10);
        QCOMPARE(host.scrollFraction(), 0.42);
        QCOMPARE(host.lastState().value(QStringLiteral("heading")).toString(),
                 QStringLiteral("Where we are"));

        // A scroll inside the same slide reports again without moving it.
        host.state(QStringLiteral(
            R"({"slideIndex":7,"slideCount":10,"scrollFraction":0.63})"));
        QCOMPARE(spy.size(), 2);
        QCOMPARE(host.slideIndex(), 7);
        QCOMPARE(host.scrollFraction(), 0.63);

        // Malformed input is ignored rather than resetting the position to 0 —
        // a dropped state event must not jump the audience to the first slide.
        host.state(QStringLiteral("not json"));
        QCOMPARE(spy.size(), 2);
        QCOMPARE(host.slideIndex(), 7);
        QCOMPARE(host.scrollFraction(), 0.63);
    }

    void everyEditEmitsExactlyOneDeckChange()
    {
        DeckModel deck;
        QSignalSpy spy(&deck, &DeckModel::deckChanged);

        deck.setSource(joinSlides(slideBodies(3)));
        QCOMPARE(spy.size(), 1);

        // Even an edit that changes nothing structural still notifies, because
        // the preview has to re-render the text that changed.
        deck.setSource(joinSlides(slideBodies(3)) + QStringLiteral("\nmore\n"));
        QCOMPARE(spy.size(), 2);
    }
};

OMAPRESENT_TEST_SUITE(LiveSyncTest)
#include "tst_livesync.moc"
