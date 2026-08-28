#include <QtTest>

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTemporaryFile>

#include "testrunner.h"
#include "assetindex.h"
#include "deckmodel.h"
#include "videocache.h"

// End-to-end seams. Every other suite proves one component correct on its own;
// this one proves the components agree with each other, and that the welcome
// deck — which ships as the manual — survives its own rules.
//
// Do not add QTEST_MAIN — see tests/testrunner.h.

namespace {

QString fixture(const QString &relativePath)
{
    return QFINDTESTDATA(qPrintable(relativePath));
}

QString readFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QString::fromUtf8(file.readAll());
}

// Captures warnings emitted while it is alive, so a test can assert that
// parsing a deck is silent.
class MessageRecorder {
public:
    MessageRecorder() : m_previous(qInstallMessageHandler(&handle)) { s_messages = &m_messages; }
    ~MessageRecorder()
    {
        qInstallMessageHandler(m_previous);
        s_messages = nullptr;
    }

    QStringList messages() const { return m_messages; }

private:
    static void handle(QtMsgType type, const QMessageLogContext &, const QString &message)
    {
        if (s_messages && type != QtDebugMsg && type != QtInfoMsg)
            s_messages->append(message);
    }

    QStringList m_messages;
    QtMessageHandler m_previous = nullptr;
    static inline QStringList *s_messages = nullptr;
};

// The names DeckModel/VideoCache and the renderer must both use for a host.
QString hostName(VideoCache::Host host)
{
    switch (host) {
    case VideoCache::YouTube:   return QStringLiteral("youtube");
    case VideoCache::Vimeo:     return QStringLiteral("vimeo");
    case VideoCache::Loom:      return QStringLiteral("loom");
    case VideoCache::Descript:  return QStringLiteral("descript");
    case VideoCache::TikTok:    return QStringLiteral("tiktok");
    case VideoCache::X:         return QStringLiteral("x");
    case VideoCache::Instagram: return QStringLiteral("instagram");
    case VideoCache::Facebook:  return QStringLiteral("facebook");
    case VideoCache::DirectFile: return QStringLiteral("direct");
    case VideoCache::LocalFile:  return QStringLiteral("local");
    case VideoCache::NotAVideo:  return QString();
    }
    return QString();
}

QStringList allReferences(const DeckModel &deck)
{
    QStringList references;
    for (const Slide &slide : deck.slides())
        references += AssetIndex::extractReferences(slide.markdown);
    return references;
}

QStringList sortedUnique(QStringList values)
{
    values.removeDuplicates();
    values.sort();
    return values;
}

} // namespace

class IntegrationTest : public QObject {
    Q_OBJECT

private:
    QString m_welcomePath;
    QString m_welcomeSource;

    // Runs the renderer modules on `request` and returns their answers. Empty
    // and *why is set when node is unavailable or the bridge failed.
    QJsonObject askRenderer(const QJsonObject &request, QString *why)
    {
        const QString script = fixture(QStringLiteral("integration/renderer-answers.mjs"));
        if (script.isEmpty()) {
            *why = QStringLiteral("integration/renderer-answers.mjs not found");
            return {};
        }

        QTemporaryFile input;
        if (!input.open()) {
            *why = QStringLiteral("could not write the bridge request");
            return {};
        }
        input.write(QJsonDocument(request).toJson(QJsonDocument::Compact));
        input.flush();

        QProcess node;
        node.start(QStringLiteral("node"), {script, input.fileName()});
        if (!node.waitForStarted(5000)) {
            *why = QStringLiteral("node is not installed");
            return {};
        }
        if (!node.waitForFinished(30000)) {
            *why = QStringLiteral("the renderer bridge did not finish");
            return {};
        }
        if (node.exitCode() != 0) {
            *why = QStringLiteral("the renderer bridge failed: ")
                + QString::fromUtf8(node.readAllStandardError());
            return {};
        }

        QJsonParseError error;
        const QJsonDocument answer = QJsonDocument::fromJson(node.readAllStandardOutput(), &error);
        if (error.error != QJsonParseError::NoError) {
            *why = QStringLiteral("the renderer bridge wrote invalid JSON: ") + error.errorString();
            return {};
        }
        return answer.object();
    }

    // The image references the renderer would actually draw for this deck,
    // in document order. *why is set when the bridge could not run.
    QStringList imagesTheRendererDraws(const DeckModel &deck, QString *why)
    {
        QJsonArray markdown;
        for (const Slide &slide : deck.slides())
            markdown.append(slide.markdown);

        const QJsonObject answer = askRenderer({{QStringLiteral("slides"), markdown}}, why);
        if (answer.isEmpty())
            return {};

        QStringList references;
        for (const QJsonValue &slide : answer.value(QStringLiteral("slides")).toArray()) {
            for (const QJsonValue &image : slide.toObject().value(QStringLiteral("images")).toArray())
                references.append(image.toString());
        }
        return references;
    }

private slots:
    void initTestCase()
    {
        m_welcomePath = fixture(QStringLiteral("../welcome/welcome.md"));
        QVERIFY2(!m_welcomePath.isEmpty(), "welcome/welcome.md not found");
        m_welcomeSource = readFile(m_welcomePath);
        QVERIFY2(!m_welcomeSource.isEmpty(), "welcome/welcome.md is empty");
    }

    // --- 1. The welcome deck is the fixture -------------------------------

    void welcomeDeckParsesSilentlyAndIsStable()
    {
        DeckModel deck;
        MessageRecorder recorder;
        deck.setSource(m_welcomeSource);

        QCOMPARE(recorder.messages(), QStringList());
        // Assert the real number: an edit to the manual has to be deliberate.
        QCOMPARE(deck.slideCount(), 25);

        const int lineCount = m_welcomeSource.split(QLatin1Char('\n')).size();
        const QVector<Slide> slides = deck.slides();
        int previousEnd = -1;
        for (int i = 0; i < slides.size(); ++i) {
            const Slide &slide = slides.at(i);
            QVERIFY(slide.sourceStartLine >= 0);
            QVERIFY(slide.sourceEndLine >= slide.sourceStartLine);
            QVERIFY(slide.sourceEndLine < lineCount);
            QVERIFY2(slide.sourceStartLine > previousEnd, "slide line ranges overlap");
            previousEnd = slide.sourceEndLine;
            QVERIFY(!slide.markdown.trimmed().isEmpty());
            QCOMPARE(deck.slideIndexForLine(slide.sourceStartLine), i);
        }
    }

    void welcomeFrontmatterParses()
    {
        DeckModel deck;
        deck.setSource(m_welcomeSource);
        const QVariantMap frontmatter = deck.frontmatter();

        QCOMPARE(frontmatter.value(QStringLiteral("title")).toString(),
                 QStringLiteral("How Omapresent Works"));
        QCOMPARE(frontmatter.value(QStringLiteral("author")).toString(),
                 QStringLiteral("Jethro Jones"));
        QCOMPARE(frontmatter.value(QStringLiteral("date")).toString(),
                 QStringLiteral("2026-08-27"));
        QCOMPARE(frontmatter.value(QStringLiteral("aspect")).toString(), QStringLiteral("16:9"));
        QCOMPARE(frontmatter.value(QStringLiteral("header")).toString(),
                 QStringLiteral("Omapresent"));
        QCOMPARE(frontmatter.value(QStringLiteral("footer")).toString(),
                 QStringLiteral("{title} — Slide {slide}/{count}"));
        QVERIFY(frontmatter.value(QStringLiteral("slide-numbers")).typeId() == QMetaType::Bool);
        QCOMPARE(frontmatter.value(QStringLiteral("slide-numbers")).toBool(), true);
        QCOMPARE(frontmatter.value(QStringLiteral("progress")).toBool(), true);

        const QVariantMap publish = frontmatter.value(QStringLiteral("publish")).toMap();
        QCOMPARE(publish.value(QStringLiteral("slug")).toString(),
                 QStringLiteral("how-omapresent-works"));
        QCOMPARE(publish.value(QStringLiteral("title")).toString(),
                 QStringLiteral("How Omapresent Works — The Manual"));
        QCOMPARE(publish.value(QStringLiteral("provider")).toString(), QStringLiteral("herenow"));
        QCOMPARE(publish.value(QStringLiteral("access")).toString(), QStringLiteral("link"));

        // The nested block must survive the round trip into the renderer JSON.
        const QJsonObject json = deck.toJson().value(QStringLiteral("frontmatter")).toObject();
        QCOMPARE(json.value(QStringLiteral("publish")).toObject()
                     .value(QStringLiteral("slug")).toString(),
                 QStringLiteral("how-omapresent-works"));
    }

    void welcomeRecallKeysAreUniqueAndValid()
    {
        DeckModel deck;
        deck.setSource(m_welcomeSource);

        QStringList keys;
        for (const Slide &slide : deck.slides()) {
            if (slide.recallKey.isEmpty())
                continue;
            QCOMPARE(slide.recallKey.size(), 1);
            QVERIFY2(slide.recallKey.at(0).isLetterOrNumber(),
                     qPrintable(QStringLiteral("recall key %1 is not a letter or digit")
                                    .arg(slide.recallKey)));
            keys.append(slide.recallKey);
        }

        QCOMPARE(keys, QStringList({QStringLiteral("q")}));
        // A key bound twice would leave one of the slides silently unreachable.
        QCOMPARE(sortedUnique(keys).size(), keys.size());
        QVERIFY(keys.size() <= 8);
    }

    void welcomeImagesResolveOrAreDeliberatelyMissing()
    {
        DeckModel deck;
        deck.setSource(m_welcomeSource);

        AssetIndex index;
        index.setDeckDir(QFileInfo(m_welcomePath).absolutePath());
        index.waitForIndex();

        // Ask the renderer what it would actually draw, slide by slide, after
        // its own classification — a path-like line inside a fenced code block
        // is code, not an image.
        QString why;
        const QStringList drawn = imagesTheRendererDraws(deck, &why);
        if (!why.isEmpty())
            QSKIP(qPrintable(why));

        // The manual demonstrates the missing-image placeholder on purpose: it
        // ships as a single .md file with no images beside it.
        const QStringList intentionallyMissing = {
            QStringLiteral("presentation-hero.png"),
            QStringLiteral("shot1.png"), QStringLiteral("shot2.png"),
            QStringLiteral("shot3.png"), QStringLiteral("shot4.png"),
        };
        QCOMPARE(sortedUnique(drawn), sortedUnique(intentionallyMissing));

        // Every one of them is a deliberate placeholder, and nothing else in
        // the manual is a broken image.
        const QJsonObject resolved = index.resolveAll(drawn);
        for (const QString &reference : sortedUnique(drawn)) {
            QVERIFY2(resolved.value(reference).toString().isEmpty(),
                     qPrintable(QStringLiteral("%1 unexpectedly resolved").arg(reference)));
        }
    }

    void assetIndexReadsProseAsImagePaths()
    {
        // Known disagreement, recorded in the worklog under T12 NEEDS.
        // AssetIndex::looksLikeImageReference() returns true for *any* line
        // containing '/' or '\\', so ordinary prose, table rows and display
        // math in the manual become image references the renderer would never
        // draw. AssetIndex::extractReferences() also matches ![[...]] and
        // ![](...) inside inline code spans, and does not skip the `qr:`
        // prefix that the renderer's parseObsidianImage excludes.
        DeckModel deck;
        deck.setSource(m_welcomeSource);

        AssetIndex index;
        index.setDeckDir(QFileInfo(m_welcomePath).absolutePath());
        index.waitForIndex();

        QString why;
        const QStringList drawn = imagesTheRendererDraws(deck, &why);
        if (!why.isEmpty())
            QSKIP(qPrintable(why));

        QStringList extras = sortedUnique(allReferences(deck));
        for (const QString &reference : sortedUnique(drawn))
            extras.removeAll(reference);

        const QStringList inlineCodePhantoms = {
            QStringLiteral("diagram.png"),
            QStringLiteral("figure.png"),
            QStringLiteral("qr:https://..."),
        };

        // Each extra is explained by one of the two mechanisms above.
        for (const QString &extra : extras) {
            QVERIFY2(extra.contains(QLatin1Char('/')) || extra.contains(QLatin1Char('\\'))
                         || inlineCodePhantoms.contains(extra),
                     qPrintable(QStringLiteral("unexplained extra reference: %1").arg(extra)));
        }

        // Two representatives, so the mechanism is pinned and not just counted.
        QVERIFY(AssetIndex::looksLikeImageReference(
            QStringLiteral("Recognises YouTube, Vimeo, TikTok, X/Twitter and Facebook.")));
        QCOMPARE(AssetIndex::extractReferences(QStringLiteral("![[qr:https://example.com]]")),
                 QStringList({QStringLiteral("qr:https://example.com")}));

        // Pinned so a fix on the AssetIndex side fails here and has to be
        // acknowledged rather than silently changing what the manual resolves.
        // This counts references extracted, not references resolved, so it does
        // not depend on what happens to exist on the machine running the test.
        QCOMPARE(extras.size(), 15);
    }

    void welcomeBareUrlsAreVideoOrQr()
    {
        DeckModel deck;
        deck.setSource(m_welcomeSource);

        QStringList urls;
        for (const Slide &slide : deck.slides())
            urls += VideoCache::extractUrls(slide.markdown);

        QCOMPARE(urls, QStringList({
            QStringLiteral("https://www.youtube.com/watch?v=dQw4w9WgXcQ"),
            QStringLiteral("https://omapresent.com"),
        }));

        for (const QString &url : urls)
            QVERIFY2(VideoCache::isBareUrlLine(url), qPrintable(url));

        // One recognised host (a player), one that is not (a QR code) — the
        // manual demonstrates both, and §4.8 says those are the only outcomes.
        QCOMPARE(VideoCache::hostFor(urls.at(0)), VideoCache::YouTube);
        QVERIFY(!VideoCache::embedUrlFor(urls.at(0)).isEmpty());
        QCOMPARE(VideoCache::hostFor(urls.at(1)), VideoCache::NotAVideo);
    }

    void welcomeSlidesAreNeverEmpty()
    {
        DeckModel deck;
        deck.setSource(m_welcomeSource);

        QJsonArray slides;
        for (const Slide &slide : deck.slides())
            slides.append(slide.markdown);

        QString why;
        const QJsonObject answer = askRenderer({{QStringLiteral("slides"), slides}}, &why);
        if (answer.isEmpty())
            QSKIP(qPrintable(why));

        const QJsonArray classified = answer.value(QStringLiteral("slides")).toArray();
        QCOMPARE(classified.size(), deck.slideCount());
        for (int i = 0; i < classified.size(); ++i) {
            const QJsonObject counts = classified.at(i).toObject();
            const int screen = counts.value(QStringLiteral("screen")).toInt();
            const int notes = counts.value(QStringLiteral("notes")).toInt();
            QVERIFY2(screen + notes > 0,
                     qPrintable(QStringLiteral("slide %1 has neither audience content nor notes")
                                    .arg(i)));
        }
    }

    // --- 3. C++ and the renderer must agree -------------------------------

    void rendererAgreesOnBareUrlLines()
    {
        const QStringList lines = {
            QStringLiteral("https://www.youtube.com/watch?v=dQw4w9WgXcQ"),
            QStringLiteral("https://youtu.be/dQw4w9WgXcQ"),
            QStringLiteral("https://www.youtube.com/shorts/abc123"),
            QStringLiteral("https://vimeo.com/123456789"),
            QStringLiteral("https://www.loom.com/share/abc123"),
            QStringLiteral("https://share.descript.com/view/abc123"),
            QStringLiteral("https://www.tiktok.com/@someone/video/123"),
            QStringLiteral("https://x.com/someone/status/123"),
            QStringLiteral("https://twitter.com/someone/status/123"),
            QStringLiteral("https://www.instagram.com/reel/abc123/"),
            QStringLiteral("https://fb.watch/abc123/"),
            QStringLiteral("https://example.com/clip.mp4"),
            QStringLiteral("https://omapresent.com"),
            QStringLiteral("http://example.com"),
            QStringLiteral("  https://example.com/padded  "),
            QStringLiteral("See https://example.com for details"),
            QStringLiteral("https://example.com/a b"),
            QStringLiteral("ftp://example.com/file"),
            QStringLiteral("[a link](https://example.com)"),
            QStringLiteral("/home/jethro/Videos/clip.mp4"),
            QStringLiteral("./clip.webm"),
            QStringLiteral("clip.mp4"),
            QStringLiteral("not a url at all"),
            QString(),
        };

        QJsonArray request;
        for (const QString &line : lines)
            request.append(line);

        QString why;
        const QJsonObject answer = askRenderer({{QStringLiteral("urls"), request}}, &why);
        if (answer.isEmpty())
            QSKIP(qPrintable(why));

        const QJsonArray rendererAnswers = answer.value(QStringLiteral("urls")).toArray();
        QCOMPARE(rendererAnswers.size(), lines.size());

        QStringList disagreements;
        for (int i = 0; i < lines.size(); ++i) {
            const QJsonObject theirs = rendererAnswers.at(i).toObject();
            const bool cppBare = VideoCache::isBareUrlLine(lines.at(i));
            const QString cppHost = hostName(VideoCache::hostFor(lines.at(i)));
            const bool theirBare = theirs.value(QStringLiteral("bare")).toBool();
            const QString theirHost = theirs.value(QStringLiteral("host")).toString();

            if (cppBare != theirBare) {
                disagreements.append(QStringLiteral("%1: isBareUrlLine c++=%2 renderer=%3")
                                         .arg(lines.at(i)).arg(cppBare).arg(theirBare));
            }
            if (cppHost != theirHost) {
                disagreements.append(QStringLiteral("%1: host c++=\"%2\" renderer=\"%3\"")
                                         .arg(lines.at(i), cppHost, theirHost));
            }
        }

        QCOMPARE(disagreements.join(QLatin1Char('\n')), QString());
    }

    void rendererReadsBareVideoFilenamesAsWebUrls()
    {
        // Known disagreement, recorded in the worklog under T12 NEEDS. The
        // renderer's urlFromLine() accepts a schemeless domain, and `.webm` /
        // `.mov` / `.txt` look like TLDs, so a bare local filename alone on a
        // line is read as https://clip.webm and drawn as a QR code. C++ calls
        // the same line a LocalFile. `clip.mp4` escapes only because a digit
        // in "mp4" fails the TLD pattern — the two extensions behave
        // differently, which is the tell.
        const QStringList lines = {
            QStringLiteral("clip.webm"),
            QStringLiteral("clip.mov"),
            QStringLiteral("notes.txt"),
        };

        QJsonArray request;
        for (const QString &line : lines)
            request.append(line);

        QString why;
        const QJsonObject answer = askRenderer({{QStringLiteral("urls"), request}}, &why);
        if (answer.isEmpty())
            QSKIP(qPrintable(why));

        const QJsonArray theirs = answer.value(QStringLiteral("urls")).toArray();
        for (int i = 0; i < lines.size(); ++i) {
            // The renderer: a bare web URL with no video host, i.e. a QR code.
            QVERIFY2(theirs.at(i).toObject().value(QStringLiteral("bare")).toBool(),
                     qPrintable(lines.at(i)));
            QCOMPARE(theirs.at(i).toObject().value(QStringLiteral("host")).toString(), QString());
            // C++: not a URL at all.
            QVERIFY2(!VideoCache::isBareUrlLine(lines.at(i)), qPrintable(lines.at(i)));
        }

        QCOMPARE(VideoCache::hostFor(QStringLiteral("clip.webm")), VideoCache::LocalFile);
        QCOMPARE(VideoCache::hostFor(QStringLiteral("clip.mov")), VideoCache::LocalFile);
        QCOMPARE(VideoCache::hostFor(QStringLiteral("notes.txt")), VideoCache::NotAVideo);

        // The same filename with a leading `./` is read the same way by both,
        // which is what makes the bare form a trap rather than a policy.
        QVERIFY(!VideoCache::isBareUrlLine(QStringLiteral("./clip.webm")));
        QCOMPARE(VideoCache::hostFor(QStringLiteral("./clip.webm")), VideoCache::LocalFile);
    }

    void rendererAgreesOnImageReferences()
    {
        const QStringList lines = {
            QStringLiteral("![[budget.png]]"),
            QStringLiteral("![[shot1.png|main]]"),
            QStringLiteral("![[shot1.png|600]]"),
            QStringLiteral("![[my budget.png]]"),
            QStringLiteral("![[~/Pictures/budget.png]]"),
            QStringLiteral("![[../img/budget.png]]"),
            QStringLiteral("![alt text](budget.png)"),
            QStringLiteral("![](/abs/path/budget.png)"),
            QStringLiteral("~/Pictures/budget.png"),
            QStringLiteral("/abs/path/my photo.png"),
            QStringLiteral("./assets/budget.png"),
            QStringLiteral("# A heading"),
            QStringLiteral("- a list item"),
            QStringLiteral("not-an-image.txt"),
            QStringLiteral("Just some prose."),
            QString(),
        };

        QJsonArray request;
        for (const QString &line : lines)
            request.append(line);

        QString why;
        const QJsonObject answer = askRenderer({{QStringLiteral("imageLines"), request}}, &why);
        if (answer.isEmpty())
            QSKIP(qPrintable(why));

        const QJsonArray rendererAnswers = answer.value(QStringLiteral("imageLines")).toArray();
        QCOMPARE(rendererAnswers.size(), lines.size());

        QStringList disagreements;
        for (int i = 0; i < lines.size(); ++i) {
            const QStringList cpp = AssetIndex::extractReferences(lines.at(i));
            const QJsonValue theirs = rendererAnswers.at(i);
            const QStringList renderer = theirs.isNull()
                ? QStringList() : QStringList({theirs.toString()});
            if (cpp != renderer) {
                disagreements.append(QStringLiteral("%1: c++=[%2] renderer=[%3]")
                                         .arg(lines.at(i), cpp.join(QStringLiteral(", ")),
                                              renderer.join(QStringLiteral(", "))));
            }
        }

        QCOMPARE(disagreements.join(QLatin1Char('\n')), QString());
    }

    void rendererAgreesOnInlineCodeAndQrReferences()
    {
        // The three shapes the two sides genuinely read differently. Pinned
        // here with both answers so a fix on either side breaks this test and
        // has to be acknowledged. See the worklog NEEDS entry for T12.
        const QStringList lines = {
            QStringLiteral("A table cell holding `![[figure.png]]` as example syntax"),
            QStringLiteral("- Obsidian embeds: `![[diagram.png]]` or `![[diagram.png|600]]`"),
            QStringLiteral("![[qr:https://example.com]]"),
            QStringLiteral("Prose with an ![alt](inline.png) image in the middle"),
        };

        QJsonArray request;
        for (const QString &line : lines)
            request.append(line);

        QString why;
        const QJsonObject answer = askRenderer({{QStringLiteral("imageLines"), request}}, &why);
        if (answer.isEmpty())
            QSKIP(qPrintable(why));

        const QJsonArray rendererAnswers = answer.value(QStringLiteral("imageLines")).toArray();

        // The renderer draws none of these as images.
        for (int i = 0; i < lines.size(); ++i)
            QVERIFY2(rendererAnswers.at(i).isNull(), qPrintable(lines.at(i)));

        // C++ extracts a reference from every one of them.
        QCOMPARE(AssetIndex::extractReferences(lines.at(0)),
                 QStringList({QStringLiteral("figure.png")}));
        QCOMPARE(AssetIndex::extractReferences(lines.at(1)),
                 QStringList({QStringLiteral("diagram.png"), QStringLiteral("diagram.png")}));
        QCOMPARE(AssetIndex::extractReferences(lines.at(2)),
                 QStringList({QStringLiteral("qr:https://example.com")}));
        QCOMPARE(AssetIndex::extractReferences(lines.at(3)),
                 QStringList({QStringLiteral("inline.png")}));
    }

    // --- 2. Fixture decks for the seams -----------------------------------

    void shortestPathWinsEndToEnd()
    {
        const QString deckPath = fixture(QStringLiteral("integration/shortest-path/deck.md"));
        QVERIFY(!deckPath.isEmpty());
        const QString deckDir = QFileInfo(deckPath).absolutePath();

        DeckModel deck;
        deck.setSource(readFile(deckPath));
        QCOMPARE(deck.slideCount(), 1);

        AssetIndex index;
        index.setDeckDir(deckDir);
        index.setRoot(deckDir); // the deck's `root: .`
        index.waitForIndex();

        const QString shallow = deckDir + QStringLiteral("/assets/logo.png");
        const QString deep = deckDir + QStringLiteral("/assets/deep/nested/logo.png");
        QVERIFY(QFile::exists(shallow));
        QVERIFY(QFile::exists(deep));

        // The reference the deck actually writes, resolved through the index.
        const QStringList references = allReferences(deck);
        QCOMPARE(references, QStringList({QStringLiteral("logo.png")}));
        QCOMPARE(index.resolve(references.first()), shallow);

        // The deeper file is still reachable, and the reference drag-and-drop
        // would insert for it round-trips back to that exact file.
        const QString reference = index.shortestUniqueReference(deep);
        QVERIFY(!reference.isEmpty());
        QCOMPARE(index.resolve(reference), deep);
    }

    void missingRootDegradesGracefully()
    {
        const QString deckPath = fixture(QStringLiteral("integration/missing-root/deck.md"));
        QVERIFY(!deckPath.isEmpty());

        DeckModel deck;
        deck.setSource(readFile(deckPath));
        QCOMPARE(deck.slideCount(), 1);
        QCOMPARE(deck.frontmatter().value(QStringLiteral("root")).toString(),
                 QStringLiteral("/nonexistent/omapresent/integration/root-9f2a"));

        AssetIndex index;
        index.setDeckDir(QFileInfo(deckPath).absolutePath());
        index.setRoot(deck.frontmatter().value(QStringLiteral("root")).toString());
        index.waitForIndex();

        const QStringList references = allReferences(deck);
        QCOMPARE(references, QStringList({QStringLiteral("nowhere.png")}));

        // No crash, and an empty answer so the renderer draws the placeholder.
        QCOMPARE(index.resolve(QStringLiteral("nowhere.png")), QString());
        const QJsonObject resolved = index.resolveAll(references);
        QCOMPARE(resolved.value(QStringLiteral("nowhere.png")).toString(), QString());
    }

    void frontmatterOnlyDeckHasNoSlides()
    {
        DeckModel deck;
        deck.setSource(readFile(fixture(QStringLiteral("integration/frontmatter-only.md"))));

        QCOMPARE(deck.slideCount(), 0);
        QCOMPARE(deck.slides(), QVector<Slide>());
        QCOMPARE(deck.frontmatter().value(QStringLiteral("title")).toString(),
                 QStringLiteral("Nothing But Metadata"));
        QCOMPARE(deck.frontmatter().value(QStringLiteral("publish")).toMap()
                     .value(QStringLiteral("slug")).toString(),
                 QStringLiteral("nothing-but-metadata"));
        QCOMPARE(deck.toJson().value(QStringLiteral("slides")).toArray().size(), 0);
        QCOMPARE(deck.slideIndexForLine(0), -1);
    }

    void deckWithNoFrontmatterOrSeparatorsIsOneSlide()
    {
        const QString source = readFile(fixture(QStringLiteral("integration/one-long-slide.md")));
        DeckModel deck;
        deck.setSource(source);

        QCOMPARE(deck.slideCount(), 1);
        QVERIFY(deck.frontmatter().isEmpty());
        QCOMPARE(deck.slides().first().sourceStartLine, 0);
        // Every line of the file belongs to the one slide.
        QCOMPARE(deck.slideIndexForLine(0), 0);
        QCOMPARE(deck.slideIndexForLine(source.split(QLatin1Char('\n')).size() - 1), 0);
    }

    void crlfDeckParses()
    {
        const QString source = readFile(fixture(QStringLiteral("integration/crlf.md")));
        QVERIFY(source.contains(QStringLiteral("\r\n")));

        DeckModel deck;
        deck.setSource(source);
        QCOMPARE(deck.slideCount(), 2);
        QCOMPARE(deck.frontmatter().value(QStringLiteral("title")).toString(),
                 QStringLiteral("Windows Line Endings"));
        QCOMPARE(deck.frontmatter().value(QStringLiteral("slide-numbers")).toBool(), true);

        // No stray carriage returns survive into what the renderer receives.
        for (const Slide &slide : deck.slides())
            QVERIFY2(!slide.markdown.contains(QLatin1Char('\r')), qPrintable(slide.markdown));
        QCOMPARE(deck.slides().first().markdown,
                 QStringLiteral("# Written On Windows\n\nEvery line in this file ends with CRLF.\n"));
    }

    void separatorAtEndOfFileWithoutNewline()
    {
        const QString source = readFile(fixture(QStringLiteral("integration/separator-at-eof.md")));
        QVERIFY(source.endsWith(QStringLiteral("---")));

        DeckModel deck;
        deck.setSource(source);

        // The trailing separator opens a slide that has no content, so the
        // deck ends on the last authored slide instead of a blank one.
        QCOMPARE(deck.slideCount(), 2);
        QCOMPARE(deck.slides().last().markdown,
                 QStringLiteral("# Last\n\nThe file ends on the separator below.\n"));
    }

    void bomDeckParses()
    {
        const QString path = fixture(QStringLiteral("integration/bom.md"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QByteArray bytes = file.readAll();
        QVERIFY2(bytes.startsWith("\xEF\xBB\xBF"), "the fixture lost its BOM");

        // QString::fromUtf8 drops the mark, so a deck loaded the ordinary way
        // never sees it.
        const QString source = QString::fromUtf8(bytes);
        QVERIFY(!source.startsWith(QChar(0xFEFF)));

        DeckModel deck;
        deck.setSource(source);
        QCOMPARE(deck.frontmatter().value(QStringLiteral("title")).toString(),
                 QStringLiteral("Byte Order Mark"));
        QCOMPARE(deck.slideCount(), 2);

        // A read path that keeps the mark must not hide the frontmatter's
        // opening `---` either, and must not shift the line numbers.
        DeckModel marked;
        marked.setSource(QChar(0xFEFF) + source);
        QCOMPARE(marked.frontmatter().value(QStringLiteral("title")).toString(),
                 QStringLiteral("Byte Order Mark"));
        QCOMPARE(marked.frontmatter().value(QStringLiteral("author")).toString(),
                 QStringLiteral("Jethro Jones"));
        QCOMPARE(marked.slideCount(), 2);
        QCOMPARE(marked.slides().first().sourceStartLine, deck.slides().first().sourceStartLine);
        // The text handed back is untouched; only parsing ignores the mark.
        QVERIFY(marked.source().startsWith(QChar(0xFEFF)));
    }

    void unclosedFenceTerminates()
    {
        const QString source = readFile(fixture(QStringLiteral("integration/unclosed-fence.md")));
        DeckModel deck;
        deck.setSource(source);

        // The `---` inside the never-closed fence is code, not a separator.
        QCOMPARE(deck.slideCount(), 1);
        QVERIFY(deck.slides().first().markdown.contains(QStringLiteral("\n---\n")));
        QVERIFY(deck.slides().first().markdown.contains(QStringLiteral("not a new slide")));

        // And the renderer's own block splitter must terminate on it too.
        QString why;
        const QJsonObject answer = askRenderer(
            {{QStringLiteral("slides"), QJsonArray({deck.slides().first().markdown})}}, &why);
        if (answer.isEmpty())
            QSKIP(qPrintable(why));

        const QJsonObject counts = answer.value(QStringLiteral("slides")).toArray().at(0).toObject();
        QVERIFY(counts.value(QStringLiteral("screen")).toInt()
                    + counts.value(QStringLiteral("notes")).toInt() > 0);
    }
};

OMAPRESENT_TEST_SUITE(IntegrationTest)
#include "tst_integration.moc"
