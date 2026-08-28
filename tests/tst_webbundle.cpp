#include <QtTest>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

#include "testrunner.h"
#include "webbundle.h"

// Suite for src/webbundle.h. Owned by the webbundle agent.
//
// Every case builds a real bundle into a QTemporaryDir. The renderer is a
// fixture rather than src/renderer/ so these tests do not go red when the
// renderer agent lands a change; one case at the end builds against the real
// src/renderer/ to prove the two fit together.

namespace {

void writeText(const QString &path, const QString &text)
{
    QDir().mkpath(QFileInfo(path).path());
    QFile file(path);
    QVERIFY2(file.open(QIODevice::WriteOnly | QIODevice::Truncate),
             qPrintable(path + QStringLiteral(": ") + file.errorString()));
    file.write(text.toUtf8());
}

QString readText(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QString::fromUtf8(file.readAll());
}

// Every file under `root`, relative to it, sorted — what files() must match.
QStringList filesOnDisk(const QString &root)
{
    QStringList found;
    const QString prefix = QDir::cleanPath(root) + QStringLiteral("/");
    QDirIterator it(root, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
    while (it.hasNext())
        found += it.next().mid(prefix.size());
    found.sort();
    return found;
}

// A renderer that looks like the real one: an entry module that imports a
// helper module, a stylesheet, and a vendored library nothing imports.
void writeRendererFixture(const QString &dir)
{
    writeText(dir + QStringLiteral("/render.html"),
              QStringLiteral("<!doctype html><div id=\"deck\"></div>\n"));
    writeText(dir + QStringLiteral("/deck.css"),
              QStringLiteral("body { background: var(--op-background); }\n"));
    writeText(dir + QStringLiteral("/deckparse.js"),
              QStringLiteral("export const MARKER = \"deckparse\";\n"
                             "export function splitBlocks(markdown) {\n"
                             "    return markdown.split(\"\\n\\n\");\n"
                             "}\n"));
    writeText(dir + QStringLiteral("/render.js"),
              QStringLiteral("import { splitBlocks, MARKER } from './deckparse.js';\n"
                             "window.omapresent = {\n"
                             "    render(deck) { window.rendered = splitBlocks(MARKER); },\n"
                             "    update() {}, goto() {}, next() {}, previous() {},\n"
                             "    onState: null,\n"
                             "};\n"));
    writeText(dir + QStringLiteral("/orphan.js"),
              QStringLiteral("export const UNUSED = 1;\n"));
    writeText(dir + QStringLiteral("/package.json"), QStringLiteral("{ \"type\": \"module\" }\n"));
    writeText(dir + QStringLiteral("/renderer.qrc"), QStringLiteral("<RCC></RCC>\n"));
    writeText(dir + QStringLiteral("/vendor/markdown-it.js"),
              QStringLiteral("window.markdownit = function () { return {}; };\n"));
    writeText(dir + QStringLiteral("/vendor/LICENSES.md"),
              QStringLiteral("markdown-it — MIT\n"));
}

QJsonObject smallPalette()
{
    QJsonArray ansi;
    for (int i = 0; i < 16; ++i)
        ansi.append(QStringLiteral("#0%1%1%1%1%1").arg(i % 10));

    QJsonObject palette{
        {QStringLiteral("mode"), QStringLiteral("dark")},
        {QStringLiteral("background"), QStringLiteral("#282828")},
        {QStringLiteral("foreground"), QStringLiteral("#ebdbb2")},
        {QStringLiteral("accent"), QStringLiteral("#d79921")},
        {QStringLiteral("muted"), QStringLiteral("#928374")},
        {QStringLiteral("selection"), QStringLiteral("#504945")},
        {QStringLiteral("dark_background"), QStringLiteral("#1d2021")},
        {QStringLiteral("dark_foreground"), QStringLiteral("#a89984")},
        {QStringLiteral("bright_red"), QStringLiteral("#fb4934")},
        // Anything that is not a colour must not reach the stylesheet.
        {QStringLiteral("brown"), QStringLiteral("red; } body { display: none")},
        {QStringLiteral("ansi"), ansi},
    };
    return palette;
}

QJsonObject slide(int index, const QString &markdown)
{
    return QJsonObject{
        {QStringLiteral("index"), index},
        {QStringLiteral("markdown"), markdown},
        {QStringLiteral("recallKey"), QString()},
        {QStringLiteral("skip"), false},
        {QStringLiteral("sourceStartLine"), 0},
        {QStringLiteral("sourceEndLine"), 0},
    };
}

QString fileUrl(const QString &path)
{
    return QUrl::fromLocalFile(path).toString();
}

bool makeSymlink(const QString &target, const QString &linkPath)
{
#ifdef Q_OS_UNIX
    return ::symlink(QFile::encodeName(target).constData(),
                     QFile::encodeName(linkPath).constData()) == 0;
#else
    return QFile::link(target, linkPath) && QFileInfo(linkPath).isSymLink();
#endif
}

} // namespace

class WebBundleTest : public QObject {
    Q_OBJECT

private:
    // One sandbox per test: sources/ holds the "author's machine", renderer/
    // the renderer fixture, out/ the bundle.
    QTemporaryDir *m_sandbox = nullptr;

    QString sandbox(const QString &relative) const
    {
        return QDir(m_sandbox->path()).filePath(relative);
    }

    QString outputDir() const { return sandbox(QStringLiteral("out")); }

    // A deck with two same-named images from different directories, an
    // unresolved reference, a cached video with a poster, and a palette.
    QJsonObject sampleDeck() const
    {
        writeText(sandbox(QStringLiteral("sources/pictures/budget.png")),
                  QStringLiteral("first budget"));
        writeText(sandbox(QStringLiteral("sources/archive/budget.png")),
                  QStringLiteral("second budget"));
        writeText(sandbox(QStringLiteral("sources/deck/.omapresent-cache/talk.mp4")),
                  QStringLiteral("video bytes"));
        writeText(sandbox(QStringLiteral("sources/deck/.omapresent-cache/talk.jpg")),
                  QStringLiteral("poster bytes"));
        writeText(sandbox(QStringLiteral("sources/backdrop.png")),
                  QStringLiteral("desktop background"));

        return QJsonObject{
            {QStringLiteral("mode"), QStringLiteral("preview")},
            {QStringLiteral("frontmatter"),
             QJsonObject{
                 {QStringLiteral("title"), QStringLiteral("Quarterly Review")},
                 {QStringLiteral("author"), QStringLiteral("Jethro Jones")},
                 {QStringLiteral("date"), QStringLiteral("2026-09-01")},
                 {QStringLiteral("font"), QStringLiteral("IBM Plex Sans")},
                 {QStringLiteral("root"), QStringLiteral("..")},
                 {QStringLiteral("progress"), true},
             }},
            {QStringLiteral("slides"),
             QJsonArray{slide(0, QStringLiteral("# Hello\n\n![[budget.png]]\n")),
                        slide(1, QStringLiteral("## Numbers\n\nA note.\n"))}},
            {QStringLiteral("assets"),
             QJsonObject{
                 {QStringLiteral("budget.png"),
                  fileUrl(sandbox(QStringLiteral("sources/pictures/budget.png")))},
                 {QStringLiteral("archive/budget.png"),
                  fileUrl(sandbox(QStringLiteral("sources/archive/budget.png")))},
                 {QStringLiteral("missing.png"), QString()},
             }},
            {QStringLiteral("media"),
             QJsonObject{
                 {QStringLiteral("talk.mp4"),
                  QJsonObject{
                      {QStringLiteral("host"), QStringLiteral("localfile")},
                      {QStringLiteral("embedUrl"), QString()},
                      {QStringLiteral("cachedFile"),
                       fileUrl(sandbox(QStringLiteral(
                           "sources/deck/.omapresent-cache/talk.mp4")))},
                      {QStringLiteral("poster"),
                       fileUrl(sandbox(QStringLiteral(
                           "sources/deck/.omapresent-cache/talk.jpg")))},
                      {QStringLiteral("status"), QStringLiteral("cached")},
                  }},
             }},
            {QStringLiteral("palette"), smallPalette()},
            {QStringLiteral("backgroundImage"),
             fileUrl(sandbox(QStringLiteral("sources/backdrop.png")))},
            {QStringLiteral("textScale"), 1.0},
        };
    }

    void configure(WebBundle *bundle) const
    {
        bundle->setDeck(sampleDeck());
        bundle->setDeckDir(sandbox(QStringLiteral("sources/deck")));
        bundle->setRendererDir(sandbox(QStringLiteral("renderer")));
    }

    // The deck JSON the page carries, parsed back out of the <script> block.
    QJsonObject inlinedDeck(const QString &htmlPath) const
    {
        const QString html = readText(htmlPath);
        const QString open =
            QStringLiteral("<script type=\"application/json\" id=\"op-deck\">");
        const qsizetype start = html.indexOf(open);
        if (start < 0)
            return {};
        const qsizetype end = html.indexOf(QStringLiteral("</script>"), start);
        const QString json = html.mid(start + open.size(), end - start - open.size());
        return QJsonDocument::fromJson(json.toUtf8()).object();
    }

private slots:
    void init()
    {
        m_sandbox = new QTemporaryDir;
        QVERIFY(m_sandbox->isValid());
        writeRendererFixture(sandbox(QStringLiteral("renderer")));
    }

    void cleanup()
    {
        delete m_sandbox;
        m_sandbox = nullptr;
    }

    // --- what gets written ------------------------------------------------

    void buildsBothViewsAndCrossLinksThem()
    {
        WebBundle bundle;
        configure(&bundle);
        QVERIFY2(bundle.build(outputDir()), qPrintable(bundle.lastError()));
        QVERIFY(bundle.lastError().isEmpty());

        const QString deckHtml = readText(outputDir() + QStringLiteral("/index.html"));
        const QString readHtml =
            readText(outputDir() + QStringLiteral("/read/index.html"));
        QVERIFY(!deckHtml.isEmpty());
        QVERIFY(!readHtml.isEmpty());

        QVERIFY(deckHtml.contains(QStringLiteral("href=\"read/index.html\"")));
        QVERIFY(readHtml.contains(QStringLiteral("href=\"../index.html\"")));

        // The deck view carries the notes subtitles and their toggle (spec §9.1).
        QVERIFY(deckHtml.contains(QStringLiteral("id=\"op-notes\"")));
        QVERIFY(deckHtml.contains(QStringLiteral("id=\"op-notes-toggle\"")));
        QVERIFY(readText(outputDir() + QStringLiteral("/assets/bundle.js"))
                    .contains(QStringLiteral("notesHtml")));

        // The long read is a different view of the same deck, not the deck.
        QCOMPARE(inlinedDeck(outputDir() + QStringLiteral("/index.html"))
                     .value(QStringLiteral("view")).toString(),
                 QStringLiteral("deck"));
        QCOMPARE(inlinedDeck(outputDir() + QStringLiteral("/read/index.html"))
                     .value(QStringLiteral("view")).toString(),
                 QStringLiteral("read"));
        QCOMPARE(inlinedDeck(outputDir() + QStringLiteral("/index.html"))
                     .value(QStringLiteral("mode")).toString(),
                 QStringLiteral("web"));
    }

    void longReadResetsProjectorTypographyIntoArticleFlow()
    {
        WebBundle bundle;
        configure(&bundle);
        QVERIFY2(bundle.build(outputDir()), qPrintable(bundle.lastError()));

        const QString css = readText(outputDir() + QStringLiteral("/assets/bundle.css"));
        QVERIFY(css.contains(QStringLiteral(
            "[data-op-view=\"read\"] #deck .op-stack {\n"
            "    display: block;")));
        QVERIFY(css.contains(QStringLiteral(
            "[data-op-view=\"read\"] #deck h1 { font-size: 2.75rem; }")));
        QVERIFY(css.contains(QStringLiteral(
            "[data-op-view=\"read\"] #deck p,\n"
            "[data-op-view=\"read\"] #deck li,")));
        QVERIFY(css.contains(QStringLiteral(
            "[data-op-view=\"read\"] #deck .op-notes.is-flow-note {")));
        QVERIFY(css.contains(QStringLiteral("background: transparent;")));
        QVERIFY(css.contains(QStringLiteral(
            "[data-op-view=\"read\"] #deck .op-slide-header,")));
        QVERIFY(css.contains(QStringLiteral(
            "[data-op-view=\"read\"] #deck .op-media.is-vertical .op-player {")));
        QVERIFY(css.contains(QStringLiteral("aspect-ratio: 9 / 16;")));

        // The final bundle stylesheet must win over the renderer's projector
        // rules without changing the deck view.
        const QString html = readText(outputDir() + QStringLiteral("/read/index.html"));
        const qsizetype rendererCss = html.indexOf(QStringLiteral("assets/deck.css"));
        const qsizetype bundleCss = html.indexOf(QStringLiteral("assets/bundle.css"));
        QVERIFY(rendererCss >= 0);
        QVERIFY(bundleCss > rendererCss);
    }

    void reportsExactlyWhatItWrote()
    {
        WebBundle bundle;
        configure(&bundle);
        QVERIFY2(bundle.build(outputDir()), qPrintable(bundle.lastError()));

        QStringList reported = bundle.files();
        reported.sort();
        QCOMPARE(reported, filesOnDisk(outputDir()));
        QVERIFY(reported.contains(QStringLiteral("index.html")));
        QVERIFY(reported.contains(QStringLiteral("read/index.html")));
        QVERIFY(reported.contains(QStringLiteral("assets/render.js")));
        QVERIFY(reported.contains(QStringLiteral("assets/deck.css")));
        QVERIFY(reported.contains(QStringLiteral("assets/theme.css")));
        QVERIFY(reported.contains(QStringLiteral("assets/vendor/LICENSES.md")));

        qint64 onDisk = 0;
        for (const QString &relative : reported)
            onDisk += QFileInfo(QDir(outputDir()).filePath(relative)).size();
        QCOMPARE(bundle.totalBytes(), onDisk);
    }

    void progressCountsUpToTheFileCount()
    {
        WebBundle bundle;
        configure(&bundle);

        int lastDone = 0;
        int lastTotal = 0;
        connect(&bundle, &WebBundle::progress,
                [&](int done, int total, const QString &) {
                    QVERIFY(done <= total);
                    QVERIFY(done >= lastDone);
                    lastDone = done;
                    lastTotal = total;
                });

        QVERIFY2(bundle.build(outputDir()), qPrintable(bundle.lastError()));
        QCOMPARE(lastDone, lastTotal);
        QCOMPARE(lastTotal, bundle.files().size());
    }

    // --- media ------------------------------------------------------------

    void copiesImagesAndRewritesThemRelatively()
    {
        WebBundle bundle;
        configure(&bundle);
        QVERIFY2(bundle.build(outputDir()), qPrintable(bundle.lastError()));

        const QJsonObject deckAssets =
            inlinedDeck(outputDir() + QStringLiteral("/index.html"))
                .value(QStringLiteral("assets")).toObject();
        const QString budget = deckAssets.value(QStringLiteral("budget.png")).toString();
        QVERIFY(budget.startsWith(QStringLiteral("media/")));
        QVERIFY(QFileInfo::exists(QDir(outputDir()).filePath(budget)));
        QCOMPARE(readText(QDir(outputDir()).filePath(budget)),
                 QStringLiteral("first budget"));

        // The long read sits one directory down, so its copy of the deck says so.
        const QJsonObject readAssets =
            inlinedDeck(outputDir() + QStringLiteral("/read/index.html"))
                .value(QStringLiteral("assets")).toObject();
        QCOMPARE(readAssets.value(QStringLiteral("budget.png")).toString(),
                 QStringLiteral("../") + budget);
        QVERIFY(QFileInfo::exists(
            QDir(outputDir() + QStringLiteral("/read")).filePath(
                readAssets.value(QStringLiteral("budget.png")).toString())));

        // A cached video and its poster travel with the deck.
        const QJsonObject video =
            inlinedDeck(outputDir() + QStringLiteral("/index.html"))
                .value(QStringLiteral("media")).toObject()
                .value(QStringLiteral("talk.mp4")).toObject();
        QVERIFY(video.value(QStringLiteral("cachedFile")).toString()
                    .startsWith(QStringLiteral("media/")));
        QVERIFY(video.value(QStringLiteral("poster")).toString()
                    .startsWith(QStringLiteral("media/")));
        QVERIFY(QFileInfo::exists(QDir(outputDir()).filePath(
            video.value(QStringLiteral("cachedFile")).toString())));

        // The background image is the missing-asset placeholder; it travels too.
        const QString background =
            inlinedDeck(outputDir() + QStringLiteral("/index.html"))
                .value(QStringLiteral("backgroundImage")).toString();
        QVERIFY(background.startsWith(QStringLiteral("media/")));
        QVERIFY(QFileInfo::exists(QDir(outputDir()).filePath(background)));
    }

    void sameNamedImagesFromDifferentDirectoriesBothSurvive()
    {
        WebBundle bundle;
        configure(&bundle);
        QVERIFY2(bundle.build(outputDir()), qPrintable(bundle.lastError()));

        const QJsonObject assets =
            inlinedDeck(outputDir() + QStringLiteral("/index.html"))
                .value(QStringLiteral("assets")).toObject();
        const QString first = assets.value(QStringLiteral("budget.png")).toString();
        const QString second = assets.value(QStringLiteral("archive/budget.png")).toString();

        QVERIFY(!first.isEmpty());
        QVERIFY(!second.isEmpty());
        QVERIFY(first != second);
        QCOMPARE(readText(QDir(outputDir()).filePath(first)),
                 QStringLiteral("first budget"));
        QCOMPARE(readText(QDir(outputDir()).filePath(second)),
                 QStringLiteral("second budget"));
    }

    void copiesMediaSymlinkWhoseTargetStaysInsideAssetRoot()
    {
        const QString target = sandbox(QStringLiteral("sources/pictures/actual.png"));
        const QString alias = sandbox(QStringLiteral("sources/deck/cover.png"));
        writeText(target, QStringLiteral("inside asset"));
        QVERIFY(QDir().mkpath(QFileInfo(alias).path()));
        if (!makeSymlink(target, alias))
            QSKIP("This filesystem cannot create symlinks.");

        QJsonObject deck = sampleDeck();
        QJsonObject assets = deck.value(QStringLiteral("assets")).toObject();
        assets.insert(QStringLiteral("cover.png"), fileUrl(alias));
        deck.insert(QStringLiteral("assets"), assets);

        WebBundle bundle;
        bundle.setDeck(deck);
        bundle.setDeckDir(sandbox(QStringLiteral("sources/deck")));
        bundle.setRendererDir(sandbox(QStringLiteral("renderer")));
        QVERIFY2(bundle.build(outputDir()), qPrintable(bundle.lastError()));

        const QString bundled =
            inlinedDeck(outputDir() + QStringLiteral("/index.html"))
                .value(QStringLiteral("assets")).toObject()
                .value(QStringLiteral("cover.png")).toString();
        QVERIFY(bundled.startsWith(QStringLiteral("media/")));
        QCOMPARE(readText(QDir(outputDir()).filePath(bundled)),
                 QStringLiteral("inside asset"));
    }

    void rejectsMediaSymlinkWhoseTargetLeavesAssetRoot()
    {
        const QString secret = sandbox(QStringLiteral("outside/id_rsa"));
        const QString alias = sandbox(QStringLiteral("sources/deck/cover.png"));
        writeText(secret, QStringLiteral("PRIVATE-KEY-MATERIAL"));
        QVERIFY(QDir().mkpath(QFileInfo(alias).path()));
        if (!makeSymlink(secret, alias))
            QSKIP("This filesystem cannot create symlinks.");

        QJsonObject deck = sampleDeck();
        QJsonObject assets = deck.value(QStringLiteral("assets")).toObject();
        assets.insert(QStringLiteral("cover.png"), fileUrl(alias));
        deck.insert(QStringLiteral("assets"), assets);

        WebBundle bundle;
        bundle.setDeck(deck);
        bundle.setDeckDir(sandbox(QStringLiteral("sources/deck")));
        bundle.setRendererDir(sandbox(QStringLiteral("renderer")));
        QVERIFY2(bundle.build(outputDir()), qPrintable(bundle.lastError()));

        QCOMPARE(inlinedDeck(outputDir() + QStringLiteral("/index.html"))
                     .value(QStringLiteral("assets")).toObject()
                     .value(QStringLiteral("cover.png")).toString(),
                 QString());
        for (const QString &relative : bundle.files()) {
            if (relative.startsWith(QStringLiteral("media/")))
                QVERIFY(readText(QDir(outputDir()).filePath(relative))
                        != QStringLiteral("PRIVATE-KEY-MATERIAL"));
        }
    }

    void unresolvedAndRemoteReferencesBecomePlaceholders()
    {
        QJsonObject deck = sampleDeck();
        QJsonObject assets = deck.value(QStringLiteral("assets")).toObject();
        assets.insert(QStringLiteral("remote.png"),
                      QStringLiteral("https://example.com/remote.png"));
        assets.insert(QStringLiteral("stale.png"),
                      fileUrl(sandbox(QStringLiteral("sources/deleted.png"))));
        deck.insert(QStringLiteral("assets"), assets);

        WebBundle bundle;
        bundle.setDeck(deck);
        bundle.setDeckDir(sandbox(QStringLiteral("sources/deck")));
        bundle.setRendererDir(sandbox(QStringLiteral("renderer")));
        QVERIFY2(bundle.build(outputDir()), qPrintable(bundle.lastError()));

        const QJsonObject written =
            inlinedDeck(outputDir() + QStringLiteral("/index.html"))
                .value(QStringLiteral("assets")).toObject();
        // An unresolved reference stays unresolved; a remote image we cannot
        // vendor becomes one, rather than a link off the bundle.
        QCOMPARE(written.value(QStringLiteral("missing.png")).toString(), QString());
        QCOMPARE(written.value(QStringLiteral("remote.png")).toString(), QString());
        QCOMPARE(written.value(QStringLiteral("stale.png")).toString(), QString());
    }

    void keepsHostedVideoEmbedUrls()
    {
        // A hosted video that was never cached is an embed: its URL is the only
        // way to play it, so it stays. Everything else is local.
        QJsonObject deck = sampleDeck();
        QJsonObject media = deck.value(QStringLiteral("media")).toObject();
        media.insert(QStringLiteral("https://youtu.be/abc"),
                     QJsonObject{
                         {QStringLiteral("host"), QStringLiteral("youtube")},
                         {QStringLiteral("embedUrl"),
                          QStringLiteral("https://www.youtube.com/embed/abc")},
                         {QStringLiteral("cachedFile"), QString()},
                         {QStringLiteral("poster"), QString()},
                         {QStringLiteral("status"), QStringLiteral("embed")},
                     });
        deck.insert(QStringLiteral("media"), media);

        WebBundle bundle;
        bundle.setDeck(deck);
        bundle.setDeckDir(sandbox(QStringLiteral("sources/deck")));
        bundle.setRendererDir(sandbox(QStringLiteral("renderer")));
        QVERIFY2(bundle.build(outputDir()), qPrintable(bundle.lastError()));

        QCOMPARE(inlinedDeck(outputDir() + QStringLiteral("/index.html"))
                     .value(QStringLiteral("media")).toObject()
                     .value(QStringLiteral("https://youtu.be/abc")).toObject()
                     .value(QStringLiteral("embedUrl")).toString(),
                 QStringLiteral("https://www.youtube.com/embed/abc"));
    }

    // --- self-contained ---------------------------------------------------

    void nothingPointsOutsideTheBundle()
    {
        WebBundle bundle;
        configure(&bundle);
        QVERIFY2(bundle.build(outputDir()), qPrintable(bundle.lastError()));

        const QStringList written = bundle.files();
        QVERIFY(!written.isEmpty());
        for (const QString &relative : written) {
            if (relative.startsWith(QStringLiteral("media/")))
                continue;   // copied bytes, not references
            const QString text = readText(QDir(outputDir()).filePath(relative));
            const QString where = relative + QStringLiteral(": ");
            QVERIFY2(!text.contains(QStringLiteral("qrc:")), qPrintable(where + text.left(0)));
            QVERIFY2(!text.contains(QStringLiteral("file://")), qPrintable(where));
            // The author's machine: the deck was built from a path under here.
            QVERIFY2(!text.contains(m_sandbox->path()), qPrintable(where));
            QVERIFY2(!text.contains(QStringLiteral("http://")), qPrintable(where));
            QVERIFY2(!text.contains(QStringLiteral("https://")), qPrintable(where));
        }
    }

    void mediaFileNamesAreStableAndCollisionFree()
    {
        const QString first =
            WebBundle::mediaFileName(QStringLiteral("/home/jethro/Pictures/Q3 budget.png"));
        const QString second =
            WebBundle::mediaFileName(QStringLiteral("/home/jethro/Archive/Q3 budget.png"));

        QVERIFY(first.startsWith(QStringLiteral("q3-budget-")));
        QVERIFY(first.endsWith(QStringLiteral(".png")));
        QVERIFY(first != second);
        // Same path, same name, every time — republishing must not churn.
        QCOMPARE(WebBundle::mediaFileName(QStringLiteral("/home/jethro/Pictures/Q3 budget.png")),
                 first);
        QCOMPARE(WebBundle::mediaFileName(QStringLiteral("/home/jethro/Pictures/./Q3 budget.png")),
                 first);
        // Nothing that needs escaping in a URL survives into the name.
        for (const QChar character : first)
            QVERIFY(character.isLetterOrNumber() || character == u'-' || character == u'.');
        QVERIFY(WebBundle::mediaFileName(QStringLiteral("/tmp/东京")).startsWith(
            QStringLiteral("asset-")));
    }

    void rebuildingProducesTheSameFileNames()
    {
        WebBundle first;
        configure(&first);
        QVERIFY2(first.build(sandbox(QStringLiteral("out-a"))), qPrintable(first.lastError()));

        WebBundle second;
        configure(&second);
        QVERIFY2(second.build(sandbox(QStringLiteral("out-b"))), qPrintable(second.lastError()));

        QCOMPARE(second.files(), first.files());
        QCOMPARE(second.totalBytes(), first.totalBytes());
    }

    void rebuildingIntoTheSameDirectoryOverwritesCleanly()
    {
        WebBundle bundle;
        configure(&bundle);
        QVERIFY2(bundle.build(outputDir()), qPrintable(bundle.lastError()));
        const QStringList firstRun = bundle.files();

        QVERIFY2(bundle.build(outputDir()), qPrintable(bundle.lastError()));
        QCOMPARE(bundle.files(), firstRun);
        QCOMPARE(filesOnDisk(outputDir()), firstRun);
    }

    // --- theme ------------------------------------------------------------

    void bakesThePaletteIntoCustomProperties()
    {
        WebBundle bundle;
        configure(&bundle);
        QVERIFY2(bundle.build(outputDir()), qPrintable(bundle.lastError()));

        const QString css = readText(outputDir() + QStringLiteral("/assets/theme.css"));
        QVERIFY(css.contains(QStringLiteral("--op-background: #282828;")));
        QVERIFY(css.contains(QStringLiteral("--op-foreground: #ebdbb2;")));
        QVERIFY(css.contains(QStringLiteral("--op-dark-background: #1d2021;")));
        QVERIFY(css.contains(QStringLiteral("--op-bright-red: #fb4934;")));
        QVERIFY(css.contains(QStringLiteral("--op-ansi-0:")));
        QVERIFY(css.contains(QStringLiteral("--op-ansi-15:")));
        QVERIFY(css.contains(QStringLiteral("color-scheme: dark;")));
        QVERIFY(css.contains(QStringLiteral("--op-font-body: \"IBM Plex Sans\",")));
        QVERIFY(css.contains(QStringLiteral("--op-text-scale: 1;")));

        // The palette has one mode, so the bundle ships one theme: the reader's
        // desktop does not get a say (spec §9).
        QVERIFY(!css.contains(QStringLiteral("prefers-color-scheme")));
        // A value that is not a colour never reaches the stylesheet.
        QVERIFY(!css.contains(QStringLiteral("display: none")));
    }

    // --- the renderer -----------------------------------------------------

    void packagesTheRendererModuleGraph()
    {
        WebBundle bundle;
        configure(&bundle);
        QVERIFY2(bundle.build(outputDir()), qPrintable(bundle.lastError()));

        const QString js = readText(outputDir() + QStringLiteral("/assets/render.js"));
        // The modules travel as text and are loaded from blob: URLs, because a
        // browser will not fetch an ES module from a page opened off the disk.
        QVERIFY(js.contains(QStringLiteral("URL.createObjectURL")));
        QVERIFY(js.contains(QStringLiteral("window.omapresentReady")));
        QVERIFY(js.contains(QStringLiteral("function splitBlocks")));
        // A dependency is created before the module that imports it, and the
        // entry module is last so the loader starts it.
        QVERIFY(js.indexOf(QStringLiteral("\"deckparse.js\": ")) <
                js.indexOf(QStringLiteral("\"render.js\": ")));
        // Its specifier was rewritten to the marker the loader resolves.
        QVERIFY(js.contains(QStringLiteral("omapresent:module/deckparse.js")));
        QVERIFY(!js.contains(QStringLiteral("from './deckparse.js'")));

        // Nothing on either page is an ES module: the pages have to run from a
        // folder, and the loader is a plain script.
        QVERIFY(!readText(outputDir() + QStringLiteral("/index.html"))
                     .contains(QStringLiteral("type=\"module\"")));

        // A module inside the loader is not also copied beside it, and neither
        // is one nothing imports — there would be no way to load it.
        QVERIFY(!bundle.files().contains(QStringLiteral("assets/deckparse.js")));
        QVERIFY(!bundle.files().contains(QStringLiteral("assets/orphan.js")));
        // The renderer's own entry page is not the bundle's entry page, and how
        // the renderer is built is not a published deck's business.
        QVERIFY(!bundle.files().contains(QStringLiteral("assets/render.html")));
        QVERIFY(!bundle.files().contains(QStringLiteral("assets/package.json")));
        QVERIFY(!bundle.files().contains(QStringLiteral("assets/renderer.qrc")));
        // A vendored library nothing imports is loaded as a plain script.
        QVERIFY(bundle.files().contains(QStringLiteral("assets/vendor/markdown-it.js")));
        QVERIFY(readText(outputDir() + QStringLiteral("/index.html"))
                    .contains(QStringLiteral("<script src=\"assets/vendor/markdown-it.js\">")));
    }

    void findsEveryModuleSpecifierIncludingMinifiedOnes()
    {
        const QString source = QStringLiteral(
            "import { splitBlocks } from './deckparse.js';\n"
            "import {\n"
            "    fitDecision,\n"
            "} from \"./layout.js\";\n"
            "import markdownit from './vendor/markdown-it.mjs';\n"
            "import './side-effect.js';\n"
            "export { thing } from './media.js';\n"
            "import katex from 'katex';\n"
            "const lazy = () => import('./lazy.js');\n"
            "var x=1;export{x as default};\n");

        // Bare specifiers are left out: a bundle has no package resolution.
        QCOMPARE(WebBundle::moduleImports(source),
                 QStringList({QStringLiteral("./deckparse.js"), QStringLiteral("./layout.js"),
                              QStringLiteral("./vendor/markdown-it.mjs"),
                              QStringLiteral("./side-effect.js"), QStringLiteral("./media.js"),
                              QStringLiteral("./lazy.js")}));

        QVERIFY(WebBundle::moduleImports(QStringLiteral("var x = 1;\n")).isEmpty());
    }

    void rewritesOnlyTheSpecifiersItWasGiven()
    {
        const QString source = QStringLiteral(
            "import { a } from './deckparse.js';\n"
            "import b from \"./layout.js\";\n"
            "const message = 'imported from ./deckparse.js';\n");

        QHash<QString, QString> replacements;
        replacements.insert(QStringLiteral("./deckparse.js"),
                            QStringLiteral("omapresent:module/deckparse.js"));

        const QString rewritten = WebBundle::withModuleImports(source, replacements);
        QVERIFY(rewritten.contains(
            QStringLiteral("import { a } from 'omapresent:module/deckparse.js';")));
        // An unknown specifier is left exactly as the renderer wrote it.
        QVERIFY(rewritten.contains(QStringLiteral("import b from \"./layout.js\";")));
        // Quoting, spacing and everything around the specifier survive.
        QCOMPARE(rewritten.count(u'\n'), source.count(u'\n'));
        QCOMPARE(WebBundle::withModuleImports(source, {}), source);
    }

    void buildsAgainstTheRealRenderer()
    {
        const QString renderer = QFINDTESTDATA("../src/renderer");
        if (renderer.isEmpty())
            QSKIP("src/renderer is not beside the test binary");

        WebBundle bundle;
        bundle.setDeck(sampleDeck());
        bundle.setDeckDir(sandbox(QStringLiteral("sources/deck")));
        bundle.setRendererDir(renderer);
        QVERIFY2(bundle.build(outputDir()), qPrintable(bundle.lastError()));

        QVERIFY(bundle.files().contains(QStringLiteral("assets/render.js")));
        QVERIFY(bundle.files().contains(QStringLiteral("assets/deck.css")));
        QVERIFY(readText(outputDir() + QStringLiteral("/index.html"))
                    .contains(QStringLiteral("assets/deck.css")));
        const QString rendererScript =
            readText(outputDir() + QStringLiteral("/assets/render.js"));
        QVERIFY(rendererScript.contains(QStringLiteral("flowAllBlocks")));
        QVERIFY(rendererScript.contains(QStringLiteral("is-flow-note")));
    }

    // --- escaping ---------------------------------------------------------

    void inlinedDeckJsonCannotCloseItsOwnScriptTag()
    {
        const QString hostile =
            QStringLiteral("# Title</script><script>alert(1)</script>\n\n& more\n");
        QJsonObject deck = sampleDeck();
        deck.insert(QStringLiteral("slides"), QJsonArray{slide(0, hostile)});
        QJsonObject frontmatter = deck.value(QStringLiteral("frontmatter")).toObject();
        frontmatter.insert(QStringLiteral("title"), QStringLiteral("A & B <them>"));
        deck.insert(QStringLiteral("frontmatter"), frontmatter);

        WebBundle bundle;
        bundle.setDeck(deck);
        bundle.setDeckDir(sandbox(QStringLiteral("sources/deck")));
        bundle.setRendererDir(sandbox(QStringLiteral("renderer")));
        QVERIFY2(bundle.build(outputDir()), qPrintable(bundle.lastError()));

        const QString html = readText(outputDir() + QStringLiteral("/index.html"));
        QVERIFY(!html.contains(QStringLiteral("<script>alert(1)")));
        QVERIFY(html.contains(QStringLiteral("<title>A &amp; B &lt;them&gt;</title>")));
        // And the deck still parses back out, byte for byte.
        QCOMPARE(inlinedDeck(outputDir() + QStringLiteral("/index.html"))
                     .value(QStringLiteral("slides")).toArray().at(0).toObject()
                     .value(QStringLiteral("markdown")).toString(),
                 hostile);
    }

    // --- failures ---------------------------------------------------------

    void failsWithoutADeck()
    {
        WebBundle bundle;
        bundle.setRendererDir(sandbox(QStringLiteral("renderer")));
        QVERIFY(!bundle.build(outputDir()));
        QVERIFY(bundle.lastError().contains(QStringLiteral("deck")));
        QVERIFY(bundle.files().isEmpty());
        QVERIFY(!QFileInfo::exists(outputDir()));
    }

    void failsWhenTheRendererIsMissing()
    {
        WebBundle bundle;
        configure(&bundle);
        bundle.setRendererDir(sandbox(QStringLiteral("no-renderer-here")));
        QVERIFY(!bundle.build(outputDir()));
        QVERIFY(bundle.lastError().contains(QStringLiteral("render.js")));
        QVERIFY(bundle.files().isEmpty());
        // Nothing half-written: the directory it made is gone again.
        QVERIFY(!QFileInfo::exists(outputDir()));
    }

    void failsCleanlyWhenTheOutputDirectoryCannotBeCreated()
    {
        const QString locked = sandbox(QStringLiteral("locked"));
        QVERIFY(QDir().mkpath(locked));
        if (!QFile::setPermissions(locked, QFile::ReadOwner | QFile::ExeOwner))
            QSKIP("cannot drop write permission on this filesystem");
        if (QFile(locked + QStringLiteral("/probe")).open(QIODevice::WriteOnly))
            QSKIP("running with write access everywhere (root?)");

        WebBundle bundle;
        configure(&bundle);
        QVERIFY(!bundle.build(locked + QStringLiteral("/bundle")));
        QVERIFY(bundle.lastError().contains(QStringLiteral("bundle")));
        QVERIFY(bundle.lastError().contains(QStringLiteral("writable")));
        QVERIFY(bundle.files().isEmpty());
        QCOMPARE(bundle.totalBytes(), 0);
        QVERIFY(!QFileInfo::exists(locked + QStringLiteral("/bundle")));

        QFile::setPermissions(locked, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    }

    void leavesNoPartialBundleWhenAWriteFails()
    {
        const QString target = outputDir();
        QVERIFY(QDir().mkpath(target));
        if (!QFile::setPermissions(target, QFile::ReadOwner | QFile::ExeOwner))
            QSKIP("cannot drop write permission on this filesystem");
        if (QFile(target + QStringLiteral("/probe")).open(QIODevice::WriteOnly))
            QSKIP("running with write access everywhere (root?)");

        WebBundle bundle;
        configure(&bundle);
        QVERIFY(!bundle.build(target));
        QVERIFY(!bundle.lastError().isEmpty());
        QVERIFY(bundle.files().isEmpty());
        QCOMPARE(bundle.totalBytes(), 0);

        QFile::setPermissions(target, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
        // The directory the caller already had is still theirs, and still empty.
        QVERIFY(QFileInfo::exists(target));
        QVERIFY(filesOnDisk(target).isEmpty());
    }

    void aFailedRebuildRemovesOnlyWhatItWrote()
    {
        WebBundle bundle;
        configure(&bundle);
        QVERIFY2(bundle.build(outputDir()), qPrintable(bundle.lastError()));
        QVERIFY(QFileInfo::exists(outputDir() + QStringLiteral("/index.html")));

        // A second build that cannot find the renderer must not take the
        // published bundle down with it — it never got as far as writing.
        WebBundle broken;
        broken.setDeck(sampleDeck());
        broken.setRendererDir(sandbox(QStringLiteral("nowhere")));
        QVERIFY(!broken.build(outputDir()));
        QVERIFY(QFileInfo::exists(outputDir() + QStringLiteral("/index.html")));
    }
};

OMAPRESENT_TEST_SUITE(WebBundleTest)
#include "tst_webbundle.moc"
