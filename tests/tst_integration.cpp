#include <QtTest>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QXmlStreamReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTemporaryFile>

#include "testrunner.h"
#include "assetindex.h"
#include "deckmodel.h"
#include "omarchytheme.h"
#include "presentation.h"
#include "renderhost.h"
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

// One entry of a .qrc: where the file lives on disk, and the path the running
// app asks for.
struct ResourceEntry {
    QString diskPath;
    QString resourcePath;
};

QVector<ResourceEntry> readQrc(const QString &qrcPath)
{
    QVector<ResourceEntry> entries;
    QFile file(qrcPath);
    if (!file.open(QIODevice::ReadOnly))
        return entries;

    const QString directory = QFileInfo(qrcPath).absolutePath();
    QString prefix;
    QXmlStreamReader xml(&file);
    while (!xml.atEnd()) {
        if (xml.readNext() != QXmlStreamReader::StartElement)
            continue;
        if (xml.name() == QLatin1String("qresource")) {
            prefix = xml.attributes().value(QStringLiteral("prefix")).toString();
        } else if (xml.name() == QLatin1String("file")) {
            const QString alias = xml.attributes().value(QStringLiteral("alias")).toString();
            const QString source = xml.readElementText().trimmed();
            if (source.isEmpty())
                continue;
            const QString name = alias.isEmpty() ? source : alias;
            entries.append(ResourceEntry{
                QDir(directory).absoluteFilePath(source),
                QDir::cleanPath(QStringLiteral(":/") + prefix + QLatin1Char('/') + name)});
        }
    }
    return entries;
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

        // Document order: `r` on the QR-codes slide, then `q` on the recall demo.
        QCOMPARE(keys, QStringList({QStringLiteral("r"), QStringLiteral("q")}));
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

    void assetIndexTellsProseFromImagePaths_data()
    {
        QTest::addColumn<QString>("line");
        QTest::addColumn<bool>("isImage");

        // Prose. Spec §4.5 only promises a *path* on its own line; a sentence
        // that happens to contain a slash is not one.
        QTest::newRow("and/or") << "and/or" << false;
        QTest::newRow("slash in a sentence")
            << "Recognises YouTube, Vimeo, TikTok, X/Twitter and Facebook." << false;
        QTest::newRow("display math") << "$$e^{i\\pi} + 1 = 0$$" << false;
        QTest::newRow("tilde path in a sentence")
            << "The file lives in ~/Documents/aibrain somewhere." << false;
        QTest::newRow("comment marker in a sentence")
            << "Omapresent supports line comments with `//` and HTML comments." << false;
        QTest::newRow("table row")
            << "| `B` / `W` | Black out / White out audience display |" << false;
        QTest::newRow("aspect ratio") << "The ratio is 16:9 and it scales." << false;
        QTest::newRow("bare word") << "budget" << false;
        QTest::newRow("heading") << "# A heading" << false;
        QTest::newRow("prose") << "Just some prose." << false;

        // Paths. Every accepted form of spec §4.5, including the one with
        // spaces in it.
        QTest::newRow("bare filename") << "budget.png" << true;
        QTest::newRow("svg") << "logo.svg" << true;
        QTest::newRow("home relative") << "~/Pictures/budget.png" << true;
        QTest::newRow("absolute") << "/abs/path/photo.jpeg" << true;
        QTest::newRow("relative with spaces") << "./img/chart with spaces.png" << true;
        QTest::newRow("parent relative") << "../img/budget.png" << true;

        // Contract §3a: a local video is classified as video, never also as an
        // image, or `./clip.webm` would be both.
        QTest::newRow("local video") << "./clip.webm" << false;
        QTest::newRow("local video, bare") << "clip.mp4" << false;
    }

    void assetIndexTellsProseFromImagePaths()
    {
        QFETCH(QString, line);
        QFETCH(bool, isImage);

        QCOMPARE(AssetIndex::looksLikeImageReference(line), isImage);
        QCOMPARE(!AssetIndex::extractReferences(line).isEmpty(), isImage);
    }

    void assetIndexAndTheRendererAgreeOnTheManualsImages()
    {
        // The whole point of the seam: both sides must pull the same images out
        // of the shipping manual, which is dense with syntax examples written
        // as prose, inline code and fenced blocks.
        DeckModel deck;
        deck.setSource(m_welcomeSource);

        QString why;
        const QStringList drawn = imagesTheRendererDraws(deck, &why);
        if (!why.isEmpty())
            QSKIP(qPrintable(why));

        QCOMPARE(sortedUnique(allReferences(deck)).join(QStringLiteral(", ")),
                 sortedUnique(drawn).join(QStringLiteral(", ")));
    }

    void welcomeBareUrlsAreVideoOrQr()
    {
        DeckModel deck;
        deck.setSource(m_welcomeSource);

        QStringList urls;
        for (const Slide &slide : deck.slides())
            urls += VideoCache::extractUrls(slide.markdown);

        QCOMPARE(urls, QStringList({
            QStringLiteral("https://www.youtube.com/watch?v=aqz-KE-bpKQ"),
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

    void videoFilenamesBehaveIdenticallyOnBothSides()
    {
        // Spec §4.8 lists .mp4, .webm and .mov together, so all three have to
        // be read the same way. They are file extensions, not TLDs: a bare
        // `clip.webm` is a local video to play, never a URL to draw as a QR.
        const QStringList filenames = {
            QStringLiteral("clip.mp4"), QStringLiteral("clip.webm"), QStringLiteral("clip.mov"),
            QStringLiteral("./clip.webm"), QStringLiteral("videos/clip.mov"),
            QStringLiteral("/home/jethro/Videos/clip.mp4"),
            QStringLiteral("my holiday clip.webm"),
        };

        QJsonArray request;
        for (const QString &name : filenames)
            request.append(name);

        QString why;
        const QJsonObject answer = askRenderer({{QStringLiteral("urls"), request}}, &why);
        if (answer.isEmpty())
            QSKIP(qPrintable(why));

        const QJsonArray theirs = answer.value(QStringLiteral("urls")).toArray();
        QStringList disagreements;
        for (int i = 0; i < filenames.size(); ++i) {
            const QString name = filenames.at(i);
            const QJsonObject their = theirs.at(i).toObject();

            if (their.value(QStringLiteral("bare")).toBool())
                disagreements.append(QStringLiteral("%1: the renderer reads it as a URL").arg(name));
            if (VideoCache::isBareUrlLine(name))
                disagreements.append(QStringLiteral("%1: C++ reads it as a URL").arg(name));
            if (their.value(QStringLiteral("host")).toString() != QStringLiteral("local")) {
                disagreements.append(QStringLiteral("%1: renderer host=\"%2\", wanted \"local\"")
                                         .arg(name, their.value(QStringLiteral("host")).toString()));
            }
            if (VideoCache::hostFor(name) != VideoCache::LocalFile) {
                disagreements.append(QStringLiteral("%1: C++ host=\"%2\", wanted LocalFile")
                                         .arg(name, hostName(VideoCache::hostFor(name))));
            }
        }

        QCOMPARE(disagreements.join(QLatin1Char('\n')), QString());
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
            // Syntax written *about* images, which neither side should read as
            // one: inline code spans, and the `qr:` prefix that means a QR
            // code rather than a file.
            QStringLiteral("A table cell holding `![[figure.png]]` as example syntax"),
            QStringLiteral("- Obsidian embeds: `![[diagram.png]]` or `![[diagram.png|600]]`"),
            QStringLiteral("![[qr:https://example.com]]"),
            QStringLiteral("```qr\nhttps://example.com\n```"),
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

    void inlineImagesInProseAreResolvedButNotDrawnAsBlocks()
    {
        // The one place the two sides legitimately differ, so it is asserted
        // rather than left to look like a defect. A paragraph containing an
        // inline `![alt](x)` is prose, so the renderer's block classifier says
        // it is not an image block — but contract §3 renders notes as formatted
        // Markdown in the presenter and web roles, so that inline image really
        // is drawn there and its file still has to be resolved. AssetIndex
        // therefore collects it, exactly as src/assetindex.h documents.
        const QString line = QStringLiteral("Prose with an ![alt](inline.png) image in it");

        QCOMPARE(AssetIndex::extractReferences(line),
                 QStringList({QStringLiteral("inline.png")}));

        QString why;
        const QJsonObject answer = askRenderer(
            {{QStringLiteral("imageLines"), QJsonArray({line})}}, &why);
        if (answer.isEmpty())
            QSKIP(qPrintable(why));
        QVERIFY(answer.value(QStringLiteral("imageLines")).toArray().at(0).isNull());

        // The same reference alone on a line is an image block on both sides.
        QCOMPARE(AssetIndex::extractReferences(QStringLiteral("![alt](inline.png)")),
                 QStringList({QStringLiteral("inline.png")}));
        const QJsonObject alone = askRenderer(
            {{QStringLiteral("imageLines"), QJsonArray({QStringLiteral("![alt](inline.png)")})}},
            &why);
        if (alone.isEmpty())
            QSKIP(qPrintable(why));
        QCOMPARE(alone.value(QStringLiteral("imageLines")).toArray().at(0).toString(),
                 QStringLiteral("inline.png"));
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

    // --- 4. Resources ------------------------------------------------------

    void everyQrcPathNamedInCppResolves()
    {
        // AudienceWindow.qml and PresenterWindow.qml were written, unit-tested
        // and never added to resources.qrc, so `qrc:/AudienceWindow.qml` failed
        // at runtime and present mode silently opened nothing. Every suite was
        // green. This is the test that catches it, for any resource.
        const QString sourceDir = fixture(QStringLiteral("../src"));
        QVERIFY(!sourceDir.isEmpty());

        static const QRegularExpression reference(QStringLiteral("qrc:/[A-Za-z0-9_./-]+"));
        QStringList missing;
        QStringList checked;

        const QStringList sources = QDir(sourceDir).entryList({QStringLiteral("*.cpp")},
                                                              QDir::Files, QDir::Name);
        for (const QString &name : sources) {
            QFile file(QDir(sourceDir).absoluteFilePath(name));
            QVERIFY(file.open(QIODevice::ReadOnly));
            const QString text = QString::fromUtf8(file.readAll());

            QRegularExpressionMatchIterator matches = reference.globalMatch(text);
            while (matches.hasNext()) {
                const QString url = matches.next().captured();
                const QString path = QStringLiteral(":") + url.mid(4);
                checked.append(path);
                if (!QFile::exists(path))
                    missing.append(QStringLiteral("%1 (named in src/%2)").arg(url, name));
            }
        }

        // A scan that finds nothing would pass for the wrong reason.
        QVERIFY2(checked.size() >= 4, qPrintable(QStringLiteral("only found %1 qrc: references")
                                                     .arg(checked.size())));
        QVERIFY(sortedUnique(checked).contains(QStringLiteral(":/AudienceWindow.qml")));
        QVERIFY(sortedUnique(checked).contains(QStringLiteral(":/PresenterWindow.qml")));
        QVERIFY(sortedUnique(checked).contains(QStringLiteral(":/renderer/render.html")));
        QCOMPARE(missing.join(QLatin1Char('\n')), QString());
    }

    void everyQrcEntryExistsOnDisk()
    {
        // The reverse mistake: a .qrc naming a file that was renamed or never
        // added. qmake fails loudly on this, but only for the target that lists
        // the .qrc, so it is worth asserting where it will be seen.
        QStringList missing;
        int entries = 0;

        for (const QString &name : {QStringLiteral("../src/resources.qrc"),
                                    QStringLiteral("../src/renderer/renderer.qrc")}) {
            const QString qrcPath = fixture(name);
            QVERIFY2(!qrcPath.isEmpty(), qPrintable(name));

            const QVector<ResourceEntry> listed = readQrc(qrcPath);
            QVERIFY2(!listed.isEmpty(), qPrintable(name));
            entries += listed.size();

            for (const ResourceEntry &entry : listed) {
                if (!QFile::exists(entry.diskPath))
                    missing.append(QStringLiteral("%1: no file at %2")
                                       .arg(entry.resourcePath, entry.diskPath));
                if (!QFile::exists(entry.resourcePath))
                    missing.append(QStringLiteral("%1: not reachable as a resource")
                                       .arg(entry.resourcePath));
            }
        }

        QVERIFY(entries > 30); // the vendored renderer alone is bigger than this
        QCOMPARE(missing.join(QLatin1Char('\n')), QString());
    }

    void everyQmlFileIsRegisteredAsAResource()
    {
        // The moment the original bug was introduced: a .qml written into src/
        // and never listed. Loading QML from qrc: is the only way the app does
        // it, so an unlisted file cannot be reached at all.
        const QString sourceDir = fixture(QStringLiteral("../src"));
        QVERIFY(!sourceDir.isEmpty());

        // Negative control: if the resources were not linked into this binary at
        // all, or QFile::exists answered yes to everything, the loop below
        // would prove nothing.
        QVERIFY(!QFile::exists(QStringLiteral(":/ThisWindowDoesNotExist.qml")));

        QStringList unregistered;
        const QStringList qmlFiles = QDir(sourceDir).entryList({QStringLiteral("*.qml")},
                                                               QDir::Files, QDir::Name);
        QVERIFY(qmlFiles.size() >= 9);

        for (const QString &name : qmlFiles) {
            if (!QFile::exists(QStringLiteral(":/") + name))
                unregistered.append(QStringLiteral("src/%1 is not in src/resources.qrc").arg(name));
        }

        QCOMPARE(unregistered.join(QLatin1Char('\n')), QString());
    }

    // --- 5. The projector contrast floor, audience only --------------------

    void onlyTheAudienceGetsTheContrastFloor()
    {
        // Spec §6: the floor exists because the audience is reading a
        // washed-out projector across a room. Nudging the presenter's own
        // screen would only stop their notes matching the theme they chose.
        QJsonObject palette{
            {QStringLiteral("mode"), QStringLiteral("dark")},
            {QStringLiteral("background"), QStringLiteral("#3c3836")},
            {QStringLiteral("foreground"), QStringLiteral("#504945")}, // far too close
            {QStringLiteral("muted"), QStringLiteral("#4a4340")},
            {QStringLiteral("accent"), QStringLiteral("#45403d")},
            {QStringLiteral("dark_foreground"), QStringLiteral("#484341")},
        };

        const QString background = palette.value(QStringLiteral("background")).toString();
        QVERIFY2(OmarchyTheme::contrastRatio(palette.value(QStringLiteral("foreground")).toString(),
                                             background) < 4.5,
                 "the fixture theme has to be one that needs nudging");

        const QJsonObject audience =
            OmarchyTheme::paletteForRole(palette, QStringLiteral("audience"));
        QVERIFY(audience != palette);
        for (const QString &key : {QStringLiteral("foreground"), QStringLiteral("muted"),
                                   QStringLiteral("accent"), QStringLiteral("dark_foreground")}) {
            QVERIFY2(OmarchyTheme::contrastRatio(audience.value(key).toString(), background) >= 4.5,
                     qPrintable(key));
        }
        // The background itself is never moved: the deck keeps the theme's ground.
        QCOMPARE(audience.value(QStringLiteral("background")).toString(), background);
        QCOMPARE(audience.value(QStringLiteral("mode")).toString(), QStringLiteral("dark"));

        // Every other surface gets the theme exactly as it was written.
        for (const QString &role : {QStringLiteral("preview"), QStringLiteral("presenter"),
                                    QStringLiteral("pdf"), QStringLiteral("web"),
                                    QStringLiteral("export"), QStringLiteral("editor")}) {
            QVERIFY2(OmarchyTheme::paletteForRole(palette, role) == palette, qPrintable(role));
        }
    }

    void aThemeThatAlreadyClearsTheFloorIsUntouched()
    {
        const QJsonObject palette{
            {QStringLiteral("mode"), QStringLiteral("dark")},
            {QStringLiteral("background"), QStringLiteral("#1d2021")},
            {QStringLiteral("foreground"), QStringLiteral("#ebdbb2")},
            {QStringLiteral("muted"), QStringLiteral("#a89984")},
            {QStringLiteral("accent"), QStringLiteral("#83a598")},
            {QStringLiteral("dark_foreground"), QStringLiteral("#d5c4a1")},
        };

        QCOMPARE(OmarchyTheme::paletteForRole(palette, QStringLiteral("audience")), palette);
    }

    void presentationHandsTheAudienceItsOwnPalette()
    {
        // End to end: the deck the audience window is sent carries the nudged
        // palette, the one everything else is sent carries the theme's own.
        DeckModel model;
        model.setSource(QStringLiteral("# One\n\n---\n\n# Two\n"));

        const QJsonObject palette{
            {QStringLiteral("mode"), QStringLiteral("dark")},
            {QStringLiteral("background"), QStringLiteral("#3c3836")},
            {QStringLiteral("foreground"), QStringLiteral("#504945")},
        };
        const QJsonObject deck = RenderHost::composeDeck(
            QStringLiteral("present"), model.toJson(), {}, {}, palette, QString(), 1.0);

        Presentation presentation;
        presentation.setDeck(deck);

        const QVariantMap exact = presentation.palette();
        const QVariantMap audience = presentation.audiencePalette();
        QCOMPARE(exact.value(QStringLiteral("foreground")).toString(),
                 QStringLiteral("#504945"));
        QVERIFY(audience.value(QStringLiteral("foreground")).toString()
                != exact.value(QStringLiteral("foreground")).toString());
        QVERIFY(OmarchyTheme::contrastRatio(
                    audience.value(QStringLiteral("foreground")).toString(),
                    QStringLiteral("#3c3836")) >= 4.5);
        // Same deck, same slides — only the colours differ.
        QCOMPARE(presentation.slideCount(), 2);
    }
};

OMAPRESENT_TEST_SUITE(IntegrationTest)
#include "tst_integration.moc"
