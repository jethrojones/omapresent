#include <QtTest>

#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>

#include "testrunner.h"
#include "deckmodel.h"

// Suite for src/deckmodel.h — spec §4.1 separators, §4.3 comments,
// §4.4 frontmatter, §4.9 recall tags. Do not add QTEST_MAIN — see
// tests/testrunner.h.

namespace {

QVector<Slide> slidesOf(const QString &source)
{
    DeckModel deck;
    deck.setSource(source);
    return deck.slides();
}

// The fixture the line-number and integration cases share. Line numbers are
// in the comment column and are asserted directly, so keep them in sync.
const char *realisticDeck = R"MD(---
title: Quarterly Review
author: Jethro Jones
slide-numbers: true
publish:
  slug: q3-review
  access: link
---

# Where we are

Revenue is up. // not a comment: `//` is not the first thing on the line

--- {q}

// a hidden note
## The numbers

%% still drafting this %%
- one
- two

// --- {x}

## Draft slide

This never ships.

--- {r, skip}

## The survey

<!-- TODO: real link -->
https://example.com/survey

---

```cpp
// this comment is content
---
```

The end.
)MD";
// 0  ---                    8  (blank)          16 ## The numbers   24 ## Draft slide   32 <!-- TODO -->     40 ```
// 1  title:                 9  # Where we are   17 (blank)          25 (blank)          33 https://...       41 (blank)
// 2  author:               10  (blank)          18 %% ... %%        26 This never...    34 (blank)           42 The end.
// 3  slide-numbers:        11  Revenue is up.   19 - one            27 (blank)          35 ---               43 (blank)
// 4  publish:              12  (blank)          20 - two            28 --- {r, skip}    36 (blank)
// 5    slug:               13  --- {q}          21 (blank)          29 (blank)          37 ```cpp
// 6    access:             14  (blank)          22 // --- {x}       30 ## The survey    38 // this comment...
// 7  ---                   15  // a hidden...   23 (blank)          31 (blank)          39 ---

} // namespace

class DeckModelTest : public QObject {
    Q_OBJECT

private slots:
    // --- §4.1 separators --------------------------------------------------

    void isSeparatorLine_data()
    {
        QTest::addColumn<QString>("previous");
        QTest::addColumn<QString>("line");
        QTest::addColumn<QString>("next");
        QTest::addColumn<bool>("separator");

        QTest::newRow("plain") << "" << "---" << "" << true;
        QTest::newRow("file edges") << QString() << "---" << QString() << true;
        QTest::newRow("indented separator") << "" << "   ---  " << "  " << true;
        QTest::newRow("recall tag") << "" << "--- {q}" << "" << true;
        QTest::newRow("recall tag, no space") << "" << "---{q, skip}" << "" << true;
        QTest::newRow("setext heading") << "A heading" << "---" << "" << false;
        QTest::newRow("no blank after") << "" << "---" << "text" << false;
        QTest::newRow("no blank either side") << "a" << "---" << "b" << false;
        QTest::newRow("asterisks") << "" << "***" << "" << false;
        QTest::newRow("underscores") << "" << "___" << "" << false;
        QTest::newRow("spaced dashes") << "" << "- - -" << "" << false;
        QTest::newRow("four dashes") << "" << "----" << "" << false;
        QTest::newRow("two dashes") << "" << "--" << "" << false;
        QTest::newRow("trailing junk") << "" << "--- but wait" << "" << false;
        QTest::newRow("drop marker is not a separator") << "" << "// ---" << "" << false;
        QTest::newRow("blank line") << "" << "" << "" << false;
    }

    void isSeparatorLine()
    {
        QFETCH(QString, previous);
        QFETCH(QString, line);
        QFETCH(QString, next);
        QFETCH(bool, separator);

        QCOMPARE(DeckModel::isSeparatorLine(line, previous, next), separator);
    }

    void splitsOnSeparators()
    {
        const QVector<Slide> slides = slidesOf(QStringLiteral(
            "# One\n\n---\n\n# Two\n\n---\n\n# Three\n"));
        QCOMPARE(slides.size(), 3);
        QCOMPARE(slides.at(0).markdown, QStringLiteral("# One\n"));
        QCOMPARE(slides.at(1).markdown, QStringLiteral("# Two\n"));
        QCOMPARE(slides.at(2).markdown, QStringLiteral("# Three\n"));
    }

    void headingsDoNotStartSlides()
    {
        const QVector<Slide> slides = slidesOf(QStringLiteral(
            "# One\n\n## Two\n\n### Three\n"));
        QCOMPARE(slides.size(), 1);
    }

    void setextHeadingIsNotASeparator()
    {
        // No blank line before the dashes: this is `A heading` as an h2.
        const QVector<Slide> slides = slidesOf(QStringLiteral(
            "A heading\n---\n\nBody text.\n"));
        QCOMPARE(slides.size(), 1);
        QCOMPARE(slides.first().markdown, QStringLiteral("A heading\n---\n\nBody text.\n"));
    }

    void separatorNeedsABlankLineAfterIt()
    {
        const QVector<Slide> slides = slidesOf(QStringLiteral("# One\n\n---\n# Two\n"));
        QCOMPARE(slides.size(), 1);
    }

    void thematicBreaksAreNotSeparators()
    {
        for (const QString &rule : {QStringLiteral("***"), QStringLiteral("___"),
                                    QStringLiteral("- - -"), QStringLiteral("----")}) {
            const QVector<Slide> slides =
                slidesOf(QStringLiteral("# One\n\n%1\n\n# Two\n").arg(rule));
            QCOMPARE(slides.size(), 1);
        }
    }

    void separatorInsideFenceIsContent_data()
    {
        QTest::addColumn<QString>("fence");

        QTest::newRow("backticks") << "```";
        QTest::newRow("backticks with info") << "```markdown";
        QTest::newRow("long backticks") << "`````";
        QTest::newRow("tildes") << "~~~";
        QTest::newRow("tildes with info") << "~~~ md";
        QTest::newRow("indented fence") << "   ```";
    }

    void separatorInsideFenceIsContent()
    {
        QFETCH(QString, fence);
        const QString closer = fence.trimmed().startsWith(QLatin1Char('~'))
            ? QStringLiteral("~~~~~") : QStringLiteral("`````");

        const QVector<Slide> slides = slidesOf(
            QStringLiteral("# One\n\n%1\n\n---\n\n%2\n\nAfter.\n").arg(fence, closer));
        QCOMPARE(slides.size(), 1);
        QVERIFY(slides.first().markdown.contains(QStringLiteral("\n---\n")));
    }

    void fenceClosedByAShorterRunStaysOpen()
    {
        // ``` cannot close ````` — the `---` is still inside the code block.
        const QVector<Slide> slides = slidesOf(QStringLiteral(
            "`````\n```\n\n---\n\n`````\n\nAfter.\n"));
        QCOMPARE(slides.size(), 1);
    }

    void noSeparatorsIsOneSlide()
    {
        const QVector<Slide> slides = slidesOf(QStringLiteral(
            "# Long\n\nA paragraph.\n\nAnother paragraph.\n"));
        QCOMPARE(slides.size(), 1);
        QCOMPARE(slides.first().sourceStartLine, 0);
        QCOMPARE(slides.first().sourceEndLine, 4);
    }

    void emptySlidesAreDropped()
    {
        // A trailing separator, and two in a row, leave nothing to show.
        const QVector<Slide> slides = slidesOf(QStringLiteral(
            "# One\n\n---\n\n---\n\n# Two\n\n---\n\n"));
        QCOMPARE(slides.size(), 2);
        QCOMPARE(slides.at(1).markdown, QStringLiteral("# Two\n"));
    }

    void emptyDocumentHasNoSlides()
    {
        QCOMPARE(slidesOf(QString()).size(), 0);
        QCOMPARE(slidesOf(QStringLiteral("\n\n   \n")).size(), 0);
    }

    void carriageReturnsAreNormalised()
    {
        const QVector<Slide> slides = slidesOf(QStringLiteral(
            "# One\r\n\r\n---\r\n\r\n# Two\r\n"));
        QCOMPARE(slides.size(), 2);
        QCOMPARE(slides.at(1).markdown, QStringLiteral("# Two\n"));
        QCOMPARE(slides.at(1).sourceStartLine, 4);
    }

    // --- §4.4 frontmatter -------------------------------------------------

    void frontmatterOnlyAtTheTopOfTheFile()
    {
        DeckModel deck;
        deck.setSource(QStringLiteral("# One\n\n---\ntitle: Nope\n---\n\n# Two\n"));
        QVERIFY(deck.frontmatterRaw().isEmpty());
        QVERIFY(deck.frontmatter().isEmpty());
    }

    void frontmatterCloseIsNotASeparator()
    {
        DeckModel deck;
        deck.setSource(QStringLiteral("---\ntitle: Q3\n---\n\n# One\n\n---\n\n# Two\n"));
        QCOMPARE(deck.frontmatterRaw(), QStringLiteral("title: Q3"));
        QCOMPARE(deck.slideCount(), 2);
        QCOMPARE(deck.slides().first().markdown, QStringLiteral("# One\n"));
        QCOMPARE(deck.slides().first().sourceStartLine, 4);
    }

    void firstSlideStartsImmediatelyAfterFrontmatter()
    {
        DeckModel deck;
        deck.setSource(QStringLiteral("---\ntitle: Q3\n---\n# One\n"));
        QCOMPARE(deck.slideCount(), 1);
        QCOMPARE(deck.slides().first().markdown, QStringLiteral("# One\n"));
        QCOMPARE(deck.slides().first().sourceStartLine, 3);
    }

    void unterminatedFrontmatterIsContent()
    {
        DeckModel deck;
        deck.setSource(QStringLiteral("---\ntitle: Q3\n\n# One\n"));
        QVERIFY(deck.frontmatterRaw().isEmpty());
        QCOMPARE(deck.slideCount(), 1);
    }

    void frontmatterOnlyDocumentHasNoSlides()
    {
        DeckModel deck;
        deck.setSource(QStringLiteral("---\ntitle: Q3\n---\n"));
        QCOMPARE(deck.frontmatter().value(QStringLiteral("title")).toString(),
                 QStringLiteral("Q3"));
        QCOMPARE(deck.slideCount(), 0);
    }

    void parseFrontmatterScalars_data()
    {
        QTest::addColumn<QString>("yaml");
        QTest::addColumn<QString>("key");
        QTest::addColumn<QVariant>("value");

        QTest::newRow("bare") << "title: Quarterly Review" << "title"
                              << QVariant(QStringLiteral("Quarterly Review"));
        QTest::newRow("double quoted") << "font: \"IBM Plex Sans\"" << "font"
                                       << QVariant(QStringLiteral("IBM Plex Sans"));
        QTest::newRow("single quoted") << "aspect: '16:9'" << "aspect"
                                       << QVariant(QStringLiteral("16:9"));
        QTest::newRow("colon in bare value") << "footer: a: b" << "footer"
                                             << QVariant(QStringLiteral("a: b"));
        QTest::newRow("true") << "progress: true" << "progress" << QVariant(true);
        QTest::newRow("false") << "slide-numbers: false" << "slide-numbers" << QVariant(false);
        QTest::newRow("trailing comment") << "theme: gruvbox   # override" << "theme"
                                          << QVariant(QStringLiteral("gruvbox"));
        QTest::newRow("hash inside quotes") << "header: \"Budget # 3\"  # note" << "header"
                                            << QVariant(QStringLiteral("Budget # 3"));
        QTest::newRow("tokens survive") << "footer: \"{title} - {slide}/{count}\"" << "footer"
                                        << QVariant(QStringLiteral("{title} - {slide}/{count}"));
        QTest::newRow("tilde path") << "root: ~/Documents/aibrain" << "root"
                                    << QVariant(QStringLiteral("~/Documents/aibrain"));
        QTest::newRow("unknown key kept") << "mystery: 42" << "mystery"
                                          << QVariant(QStringLiteral("42"));
    }

    void parseFrontmatterScalars()
    {
        QFETCH(QString, yaml);
        QFETCH(QString, key);
        QFETCH(QVariant, value);

        const QVariantMap parsed = DeckModel::parseFrontmatter(yaml);
        QCOMPARE(parsed.value(key), value);
    }

    void parseFrontmatterKeepsBooleansAsBooleans()
    {
        const QVariantMap parsed = DeckModel::parseFrontmatter(
            QStringLiteral("progress: true\nslide-numbers: false"));
        QVERIFY(parsed.value(QStringLiteral("progress")).typeId() == QMetaType::Bool);
        QVERIFY(parsed.value(QStringLiteral("slide-numbers")).typeId() == QMetaType::Bool);
    }

    void parseFrontmatterNestsPublish()
    {
        const QVariantMap parsed = DeckModel::parseFrontmatter(QStringLiteral(
            "title: Q3\n"
            "publish:\n"
            "  slug: q3-review\n"
            "  provider: herenow\n"
            "  access: link\n"
            "date: 2026-09-01\n"));

        const QVariantMap publish = parsed.value(QStringLiteral("publish")).toMap();
        QCOMPARE(publish.size(), 3);
        QCOMPARE(publish.value(QStringLiteral("slug")).toString(), QStringLiteral("q3-review"));
        QCOMPARE(publish.value(QStringLiteral("access")).toString(), QStringLiteral("link"));
        // The un-indented key after the block goes back to the top level.
        QCOMPARE(parsed.value(QStringLiteral("date")).toString(), QStringLiteral("2026-09-01"));
    }

    void parseFrontmatterSurvivesMalformedYaml()
    {
        const QVariantMap parsed = DeckModel::parseFrontmatter(QStringLiteral(
            "title: Q3\n"
            "this line has no colon\n"
            ": no key\n"
            "# a comment\n"
            "\n"
            "font: \"unterminated\n"
            "author: Jethro\n"));

        QCOMPARE(parsed.value(QStringLiteral("title")).toString(), QStringLiteral("Q3"));
        QCOMPARE(parsed.value(QStringLiteral("author")).toString(), QStringLiteral("Jethro"));
        QCOMPARE(parsed.value(QStringLiteral("font")).toString(), QStringLiteral("\"unterminated"));
        QVERIFY(!parsed.contains(QStringLiteral("this line has no colon")));
        QCOMPARE(parsed.size(), 3);
    }

    void parseFrontmatterEmptyValueIsAString()
    {
        const QVariantMap parsed =
            DeckModel::parseFrontmatter(QStringLiteral("title:\nauthor: Jethro\n"));
        QCOMPARE(parsed.value(QStringLiteral("title")).toString(), QString());
        QCOMPARE(parsed.value(QStringLiteral("author")).toString(), QStringLiteral("Jethro"));
    }

    // --- §4.3 comments ----------------------------------------------------

    void stripsLineComments()
    {
        QCOMPARE(DeckModel::stripComments(QStringLiteral("a\n// b\nc\n")),
                 QStringLiteral("a\nc\n"));
        QCOMPARE(DeckModel::stripComments(QStringLiteral("    // indented\nkeep\n")),
                 QStringLiteral("keep\n"));
        // `//` has to be the first thing on the line.
        QCOMPARE(DeckModel::stripComments(QStringLiteral("see https://example.com\n")),
                 QStringLiteral("see https://example.com\n"));
    }

    void stripsPercentSpans()
    {
        QCOMPARE(DeckModel::stripComments(QStringLiteral("a %%hidden%% b\n")),
                 QStringLiteral("a  b\n"));
        QCOMPARE(DeckModel::stripComments(QStringLiteral("a\n%% one\ntwo %%\nb\n")),
                 QStringLiteral("a\nb\n"));
        QCOMPARE(DeckModel::stripComments(QStringLiteral("a\n%%hidden%%\nb\n")),
                 QStringLiteral("a\nb\n"));
    }

    void stripsHtmlComments()
    {
        QCOMPARE(DeckModel::stripComments(QStringLiteral("a <!--hidden--> b\n")),
                 QStringLiteral("a  b\n"));
        QCOMPARE(DeckModel::stripComments(QStringLiteral("a\n<!-- one\ntwo -->\nb\n")),
                 QStringLiteral("a\nb\n"));
        QCOMPARE(DeckModel::stripComments(QStringLiteral("keep <!-- x --> and <!-- y --> me\n")),
                 QStringLiteral("keep  and  me\n"));
    }

    void unterminatedSpanRunsToEndOfFile()
    {
        // Everything after the opener is comment, trailing newline included.
        QCOMPARE(DeckModel::stripComments(QStringLiteral("a\n<!-- oops\nb\nc\n")),
                 QStringLiteral("a"));
        QCOMPARE(slidesOf(QStringLiteral("# One\n\n%% oops\n\n---\n\n# Two\n")).size(), 1);
    }

    void blankLinesSurviveCommentStripping()
    {
        QCOMPARE(DeckModel::stripComments(QStringLiteral("a\n\nb\n")),
                 QStringLiteral("a\n\nb\n"));
    }

    void keepsCommentsInsideFences()
    {
        const QString code = QStringLiteral(
            "```cpp\n// not a comment here\nint x; /* nor this */\n```\n");
        QCOMPARE(DeckModel::stripComments(code), code);

        const QString html = QStringLiteral(
            "~~~html\n<!-- a real HTML comment, shown to the audience -->\n~~~\n");
        QCOMPARE(DeckModel::stripComments(html), html);

        const QString obsidian = QStringLiteral("```\n%% kept %%\n```\n");
        QCOMPARE(DeckModel::stripComments(obsidian), obsidian);
    }

    void commentsResumeAfterAFence()
    {
        QCOMPARE(DeckModel::stripComments(QStringLiteral("```\n// kept\n```\n// dropped\nend\n")),
                 QStringLiteral("```\n// kept\n```\nend\n"));
    }

    void dropMarkerRemovesTheFollowingSlide()
    {
        DeckModel deck;
        deck.setSource(QStringLiteral(
            "# One\n\n// ---\n\n# Draft\n\n---\n\n# Two\n"));
        QCOMPARE(deck.slideCount(), 2);
        QCOMPARE(deck.slides().at(0).markdown, QStringLiteral("# One\n"));
        QCOMPARE(deck.slides().at(1).markdown, QStringLiteral("# Two\n"));
    }

    void dropMarkerDoesNotShiftRecallBindings()
    {
        DeckModel deck;
        deck.setSource(QStringLiteral(
            "# One\n\n--- {a}\n\n# Two\n\n// --- {b}\n\n# Draft\n\n--- {c}\n\n# Three\n"));

        const QVector<Slide> slides = deck.slides();
        QCOMPARE(slides.size(), 3);
        QCOMPARE(slides.at(0).recallKey, QString());
        QCOMPARE(slides.at(1).recallKey, QStringLiteral("a"));
        QCOMPARE(slides.at(2).recallKey, QStringLiteral("c"));
        QCOMPARE(slides.at(2).markdown, QStringLiteral("# Three\n"));
    }

    void strayDropMarkerIsJustAComment()
    {
        // No blank line before it, so it is not a separator — and it must not
        // survive into the slide either.
        DeckModel deck;
        deck.setSource(QStringLiteral("# One\n// ---\n\nBody.\n"));
        QCOMPARE(deck.slideCount(), 1);
        QCOMPARE(deck.slides().first().markdown, QStringLiteral("# One\n\nBody.\n"));
    }

    // --- §4.9 recall tags -------------------------------------------------

    void parseSeparatorTag_data()
    {
        QTest::addColumn<QString>("line");
        QTest::addColumn<QString>("key");
        QTest::addColumn<bool>("skip");

        QTest::newRow("plain") << "---" << QString() << false;
        QTest::newRow("letter") << "--- {q}" << "q" << false;
        QTest::newRow("digit") << "--- {1}" << "1" << false;
        QTest::newRow("no space") << "---{q}" << "q" << false;
        QTest::newRow("skip") << "--- {q, skip}" << "q" << true;
        QTest::newRow("whitespace is insignificant") << "---   {  q ,  skip  }" << "q" << true;
        QTest::newRow("skip first") << "--- {skip, q}" << "q" << true;
        QTest::newRow("uppercase key kept") << "--- {Q}" << "Q" << false;
        QTest::newRow("multi-character key ignored") << "--- {qq}" << QString() << false;
        QTest::newRow("unknown word ignored") << "--- {q, wat}" << "q" << false;
        QTest::newRow("empty braces") << "--- {}" << QString() << false;
        QTest::newRow("trailing indent") << "  --- {z}  " << "z" << false;
    }

    void parseSeparatorTag()
    {
        QFETCH(QString, line);
        QFETCH(QString, key);
        QFETCH(bool, skip);

        QString parsedKey = QStringLiteral("dirty");
        bool parsedSkip = true;
        DeckModel::parseSeparatorTag(line, &parsedKey, &parsedSkip);
        QCOMPARE(parsedKey, key);
        QCOMPARE(parsedSkip, skip);
    }

    void recallTagBindsTheFollowingSlide()
    {
        const QVector<Slide> slides = slidesOf(QStringLiteral(
            "# One\n\n--- {q}\n\n# Question\n\n---\n\n# Two\n"));
        QCOMPARE(slides.size(), 3);
        QCOMPARE(slides.at(0).recallKey, QString());
        QCOMPARE(slides.at(1).recallKey, QStringLiteral("q"));
        QCOMPARE(slides.at(1).markdown, QStringLiteral("# Question\n"));
        QCOMPARE(slides.at(2).recallKey, QString());
    }

    void skippedSlidesLeaveTheLinearFlow()
    {
        DeckModel deck;
        deck.setSource(QStringLiteral(
            "# One\n\n--- {q, skip}\n\n# Aside\n\n---\n\n# Two\n"));

        const QVector<Slide> slides = deck.slides();
        QCOMPARE(slides.size(), 3);
        QVERIFY(!slides.at(0).skipInFlow);
        QVERIFY(slides.at(1).skipInFlow);
        QVERIFY(!slides.at(2).skipInFlow);

        const QJsonArray json = deck.toJson().value(QStringLiteral("slides")).toArray();
        QCOMPARE(json.at(0).toObject().value(QStringLiteral("index")).toInt(), 0);
        QCOMPARE(json.at(1).toObject().value(QStringLiteral("index")).toInt(), -1);
        QCOMPARE(json.at(2).toObject().value(QStringLiteral("index")).toInt(), 1);
    }

    void ninthRecallBindingIsIgnored()
    {
        QString source = QStringLiteral("# Intro\n");
        for (const QChar key : QStringLiteral("abcdefgh"))
            source += QStringLiteral("\n--- {%1}\n\n# Slide %1\n").arg(key);
        source += QStringLiteral("\n--- {i, skip}\n\n# Ninth\n");

        QTest::ignoreMessage(QtWarningMsg,
                             QRegularExpression(QStringLiteral("ignoring recall tag \\{i\\}")));

        DeckModel deck;
        deck.setSource(source);

        const QVector<Slide> slides = deck.slides();
        QCOMPARE(slides.size(), 10);
        QCOMPARE(slides.at(8).recallKey, QStringLiteral("h"));
        // The binding goes, and so does its `skip`: an unreachable slide is
        // worse than an unbound one.
        QCOMPARE(slides.at(9).recallKey, QString());
        QVERIFY(!slides.at(9).skipInFlow);
    }

    // --- line numbers -----------------------------------------------------

    void sourceLinesSurviveCommentStripping()
    {
        const QVector<Slide> slides = slidesOf(QStringLiteral(
            "// a dropped line\n"
            "// another\n"
            "# Heading\n"
            "\n"
            "%% a dropped span %%\n"
            "Body.\n"));

        QCOMPARE(slides.size(), 1);
        QCOMPARE(slides.first().sourceStartLine, 2);
        QCOMPARE(slides.first().sourceEndLine, 5);
        QCOMPARE(slides.first().markdown, QStringLiteral("# Heading\n\nBody.\n"));
    }

    void slideIndexForLine()
    {
        DeckModel deck;
        deck.setSource(QString::fromUtf8(realisticDeck));

        QCOMPARE(deck.slideIndexForLine(-1), -1);   // out of range
        QCOMPARE(deck.slideIndexForLine(3), -1);    // inside the frontmatter
        QCOMPARE(deck.slideIndexForLine(7), -1);    // the frontmatter close
        QCOMPARE(deck.slideIndexForLine(8), 0);     // blank line above slide 0
        QCOMPARE(deck.slideIndexForLine(11), 0);
        QCOMPARE(deck.slideIndexForLine(13), 1);    // the separator itself
        QCOMPARE(deck.slideIndexForLine(16), 1);
        QCOMPARE(deck.slideIndexForLine(21), 1);
        QCOMPARE(deck.slideIndexForLine(22), -1);   // the dropped slide
        QCOMPARE(deck.slideIndexForLine(26), -1);
        QCOMPARE(deck.slideIndexForLine(28), 2);
        QCOMPARE(deck.slideIndexForLine(33), 2);
        QCOMPARE(deck.slideIndexForLine(39), 3);    // `---` inside the fence
        QCOMPARE(deck.slideIndexForLine(42), 3);
        QCOMPARE(deck.slideIndexForLine(9999), -1);
    }

    // --- toJson -----------------------------------------------------------

    void toJsonMatchesTheRendererContract()
    {
        DeckModel deck;
        deck.setSource(QStringLiteral(
            "---\ntitle: Q3\nprogress: true\npublish:\n  slug: q3\n---\n\n"
            "# One\n\n--- {q, skip}\n\n# Aside\n"));

        const QJsonObject json = deck.toJson();
        QCOMPARE(json.keys(), QStringList({QStringLiteral("frontmatter"), QStringLiteral("slides")}));

        const QJsonObject frontmatter = json.value(QStringLiteral("frontmatter")).toObject();
        QCOMPARE(frontmatter.value(QStringLiteral("title")).toString(), QStringLiteral("Q3"));
        QCOMPARE(frontmatter.value(QStringLiteral("progress")).toBool(), true);
        QCOMPARE(frontmatter.value(QStringLiteral("publish")).toObject()
                     .value(QStringLiteral("slug")).toString(), QStringLiteral("q3"));

        const QJsonArray slides = json.value(QStringLiteral("slides")).toArray();
        QCOMPARE(slides.size(), 2);

        const QJsonObject aside = slides.at(1).toObject();
        QCOMPARE(aside.keys(), QStringList({QStringLiteral("index"), QStringLiteral("markdown"),
                                            QStringLiteral("recallKey"), QStringLiteral("skip"),
                                            QStringLiteral("sourceEndLine"),
                                            QStringLiteral("sourceStartLine")}));
        QCOMPARE(aside.value(QStringLiteral("index")).toInt(), -1);
        QCOMPARE(aside.value(QStringLiteral("markdown")).toString(), QStringLiteral("# Aside\n"));
        QCOMPARE(aside.value(QStringLiteral("recallKey")).toString(), QStringLiteral("q"));
        QCOMPARE(aside.value(QStringLiteral("skip")).toBool(), true);
        QCOMPARE(aside.value(QStringLiteral("sourceStartLine")).toInt(), 11);
        QCOMPARE(aside.value(QStringLiteral("sourceEndLine")).toInt(), 11);
    }

    void resetsBetweenSources()
    {
        DeckModel deck;
        QSignalSpy spy(&deck, &DeckModel::deckChanged);

        deck.setSource(QStringLiteral("---\ntitle: Q3\n---\n\n# One\n\n---\n\n# Two\n"));
        QCOMPARE(deck.slideCount(), 2);
        QCOMPARE(spy.size(), 1);

        deck.setSource(QStringLiteral("# Only\n"));
        QCOMPARE(deck.slideCount(), 1);
        QVERIFY(deck.frontmatter().isEmpty());
        QVERIFY(deck.frontmatterRaw().isEmpty());
        QCOMPARE(spy.size(), 2);
    }

    // --- the whole thing together -----------------------------------------

    void realisticDocument()
    {
        DeckModel deck;
        deck.setSource(QString::fromUtf8(realisticDeck));

        const QVariantMap frontmatter = deck.frontmatter();
        QCOMPARE(frontmatter.value(QStringLiteral("title")).toString(),
                 QStringLiteral("Quarterly Review"));
        QCOMPARE(frontmatter.value(QStringLiteral("author")).toString(),
                 QStringLiteral("Jethro Jones"));
        QCOMPARE(frontmatter.value(QStringLiteral("slide-numbers")).toBool(), true);
        QCOMPARE(frontmatter.value(QStringLiteral("publish")).toMap()
                     .value(QStringLiteral("slug")).toString(), QStringLiteral("q3-review"));

        const QVector<Slide> slides = deck.slides();
        QCOMPARE(slides.size(), 4); // five separators, one of them a `// ---`

        QCOMPARE(slides.at(0).markdown, QStringLiteral(
            "# Where we are\n\nRevenue is up. // not a comment: `//` is not the first thing on the line\n"));
        QCOMPARE(slides.at(0).recallKey, QString());
        QCOMPARE(slides.at(0).sourceStartLine, 9);
        QCOMPARE(slides.at(0).sourceEndLine, 11);

        QCOMPARE(slides.at(1).markdown, QStringLiteral("## The numbers\n\n- one\n- two\n"));
        QCOMPARE(slides.at(1).recallKey, QStringLiteral("q"));
        QCOMPARE(slides.at(1).sourceStartLine, 16);
        QCOMPARE(slides.at(1).sourceEndLine, 20);

        QCOMPARE(slides.at(2).markdown,
                 QStringLiteral("## The survey\n\nhttps://example.com/survey\n"));
        QCOMPARE(slides.at(2).recallKey, QStringLiteral("r"));
        QVERIFY(slides.at(2).skipInFlow);
        QCOMPARE(slides.at(2).sourceStartLine, 30);
        QCOMPARE(slides.at(2).sourceEndLine, 33);

        QCOMPARE(slides.at(3).markdown, QStringLiteral(
            "```cpp\n// this comment is content\n---\n```\n\nThe end.\n"));
        QCOMPARE(slides.at(3).sourceStartLine, 37);
        QCOMPARE(slides.at(3).sourceEndLine, 42);

        // The draft slide is gone, and its `{x}` tag went with it.
        for (const Slide &slide : slides) {
            QVERIFY(!slide.markdown.contains(QStringLiteral("Draft slide")));
            QVERIFY(slide.recallKey != QStringLiteral("x"));
        }

        const QJsonArray json = deck.toJson().value(QStringLiteral("slides")).toArray();
        QCOMPARE(json.at(0).toObject().value(QStringLiteral("index")).toInt(), 0);
        QCOMPARE(json.at(1).toObject().value(QStringLiteral("index")).toInt(), 1);
        QCOMPARE(json.at(2).toObject().value(QStringLiteral("index")).toInt(), -1);
        QCOMPARE(json.at(3).toObject().value(QStringLiteral("index")).toInt(), 2);
    }
};

OMAPRESENT_TEST_SUITE(DeckModelTest)
#include "tst_deckmodel.moc"
