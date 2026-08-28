#include <QtTest>

#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTcpServer>
#include <QTcpSocket>
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

class ScopedEnvironment {
public:
    ScopedEnvironment(const QByteArray &name, const QByteArray &value)
        : m_name(name), m_old(qgetenv(name.constData())), m_had(qEnvironmentVariableIsSet(name.constData()))
    {
        qputenv(m_name.constData(), value);
    }

    ~ScopedEnvironment()
    {
        if (m_had)
            qputenv(m_name.constData(), m_old);
        else
            qunsetenv(m_name.constData());
    }

private:
    QByteArray m_name;
    QByteArray m_old;
    bool m_had = false;
};

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

    void longReadSetsThePageAroundTheArticle()
    {
        WebBundle bundle;
        configure(&bundle);
        QVERIFY2(bundle.build(outputDir()), qPrintable(bundle.lastError()));

        const QString css = readText(outputDir() + QStringLiteral("/assets/bundle.css"));

        // The article inside #deck belongs to the renderer's read branch in
        // deck.css. This stylesheet must not reach into it: two owners of the
        // same rules is how the long read ended up being set for a projector
        // in the first place, and the duplicate loses on specificity anyway.
        QVERIFY2(!css.contains(QStringLiteral("[data-op-view=\"read\"] #deck")),
                 "bundle.css is styling the renderer's article again");

        // What it does own is the page around the article.
        QVERIFY(css.contains(QStringLiteral(".op-masthead,")));
        QVERIFY(css.contains(QStringLiteral("[data-op-view=\"read\"] .op-chrome")));
        QVERIFY(css.contains(QStringLiteral("[data-op-view=\"read\"] .op-footer")));

        // deck.css sets every heading for a projector — centred, inside 24ch —
        // and the masthead sits outside #deck where the read branch never
        // reaches it, so it has to opt out by name or it is set as a slide.
        const qsizetype masthead = css.indexOf(QStringLiteral(".op-masthead h1 {"));
        QVERIFY(masthead >= 0);
        const QString mastheadRule = css.mid(masthead, css.indexOf(u'}', masthead) - masthead);
        QVERIFY(mastheadRule.contains(QStringLiteral("max-width: none;")));
        QVERIFY(mastheadRule.contains(QStringLiteral("text-align: left;")));

        // The chrome is drawn to the article's text box (38rem less its two
        // 1.25rem gutters), not to the outer column, so its rules line up with
        // the prose rather than overhanging it.
        QVERIFY(css.contains(QStringLiteral("width: min(35.5rem, 100% - 2.5rem);")));

        // Load order still matters for what little does overlap.
        const QString html = readText(outputDir() + QStringLiteral("/read/index.html"));
        const qsizetype rendererCss = html.indexOf(QStringLiteral("assets/deck.css"));
        const qsizetype bundleCss = html.indexOf(QStringLiteral("assets/bundle.css"));
        QVERIFY(rendererCss >= 0);
        QVERIFY(bundleCss > rendererCss);

        // §9.2's promotion of speaker-note prose to body text is the renderer's
        // doing, and it keys off this attribute — so what is testable here is
        // the flag, not the prose. The deck JSON is inlined in both pages, so
        // note text is present in both files either way; only the rendered DOM
        // differs, and that is checked in a browser, not from C++.
        QVERIFY(html.contains(QStringLiteral("<html lang=\"en\" data-op-view=\"read\">")));

        const QString deckHtml = readText(outputDir() + QStringLiteral("/index.html"));
        QVERIFY(deckHtml.contains(QStringLiteral("<html lang=\"en\" data-op-view=\"deck\">")));
        // The deck view keeps its notes in the subtitle track instead.
        QVERIFY(deckHtml.contains(QStringLiteral("id=\"op-notes\"")));
        QVERIFY(!html.contains(QStringLiteral("id=\"op-notes\"")));
    }

    void publishedSubtitleIsVisibleAndToggleableInBrowser()
    {
        const QString chromium = QStringLiteral("/usr/bin/chromium");
        if (!QFileInfo::exists(chromium))
            QSKIP("Chromium is not installed.");

        const QString renderer = QFINDTESTDATA("../src/renderer");
        if (renderer.isEmpty())
            QSKIP("src/renderer is not beside the test binary");

        QTcpServer imageServer;
        QVERIFY(imageServer.listen(QHostAddress::LocalHost, 0));

        QJsonObject deck = sampleDeck();
        QJsonObject frontmatter = deck.value(QStringLiteral("frontmatter")).toObject();
        frontmatter.remove(QStringLiteral("title"));
        deck.insert(QStringLiteral("frontmatter"), frontmatter);
        const QString remoteNoteImage =
            QStringLiteral("http://127.0.0.1:%1/note.png")
                .arg(imageServer.serverPort());
        QJsonObject assets = deck.value(QStringLiteral("assets")).toObject();
        assets.insert(remoteNoteImage, remoteNoteImage);
        deck.insert(QStringLiteral("assets"), assets);
        deck.insert(QStringLiteral("slides"), QJsonArray{
            slide(0, QStringLiteral("Visible speaker note with ![Remote note image](%1).\n\n"
                                    "```markdown\n# Example only\n```")
                         .arg(remoteNoteImage)),
            slide(1, QStringLiteral("# Browser fallback title")),
        });

        WebBundle bundle;
        bundle.setDeck(deck);
        bundle.setDeckDir(sandbox(QStringLiteral("sources/deck")));
        bundle.setRendererDir(renderer);
        QVERIFY2(bundle.build(outputDir()), qPrintable(bundle.lastError()));

        const QString htmlPath = outputDir() + QStringLiteral("/index.html");
        QString html = readText(htmlPath);
        const QString probe = QStringLiteral(R"HTML(
<script>
setTimeout(function () {
    var notes = document.getElementById("op-notes");
    var toggle = document.getElementById("op-notes-toggle");
    var rect = notes.getBoundingClientRect();
    var visible = !notes.hidden && getComputedStyle(notes).display !== "none" &&
        rect.height > 0 && rect.top >= 0 && rect.bottom <= innerHeight + 1;
    document.body.dataset.subtitleVisible = String(visible);
    document.body.dataset.subtitleHasText = String(
        notes.textContent.includes("Visible speaker note"));
    document.body.dataset.pageTitle = document.title;
    var imageLoader = notes.querySelector("[data-op-remote-image]");
    document.body.dataset.noteImageDeferred = String(
        !!imageLoader && !notes.querySelector("img[src^='http']"));
    imageLoader.click();
    var activatedImage = notes.querySelector("img[src^='http://127.0.0.1:']");
    document.body.dataset.noteImageActivated = String(!!activatedImage);
    activatedImage.addEventListener("load", function () {
        window.omapresent.scrollBy(0);
        setTimeout(function () {
            document.body.dataset.noteImagePersisted = String(
                !!notes.querySelector("img[src^='http://127.0.0.1:']") &&
                !notes.querySelector("[data-op-remote-image]"));
            toggle.focus();
            toggle.dispatchEvent(new KeyboardEvent("keydown", {
                key: "ArrowRight", bubbles: true, cancelable: true
            }));
            setTimeout(function () {
                document.body.dataset.chromeKeySlide =
                    document.querySelector(".op-slide").dataset.slideIndex;
            }, 50);
        }, 100);
    });
    toggle.click();
    document.body.dataset.subtitleHiddenAfterToggle = String(notes.hidden);
    toggle.click();
    rect = notes.getBoundingClientRect();
    document.body.dataset.subtitleVisibleAfterToggle = String(
        !notes.hidden && getComputedStyle(notes).display !== "none" &&
        rect.height > 0 && rect.bottom <= innerHeight + 1);
}, 700);
</script>
)HTML");
        html.replace(QStringLiteral("</body>"), probe + QStringLiteral("</body>"));
        writeText(htmlPath, html);

        const QString profile = sandbox(QStringLiteral("chromium-profile"));
        QDir().mkpath(profile);
        QProcess browser;
        browser.start(chromium, {
            QStringLiteral("--headless"), QStringLiteral("--no-sandbox"),
            QStringLiteral("--disable-gpu"),
            QStringLiteral("--allow-file-access-from-files"),
            QStringLiteral("--user-data-dir=") + profile,
            QStringLiteral("--virtual-time-budget=2200"),
            QStringLiteral("--dump-dom"), QUrl::fromLocalFile(htmlPath).toString(),
        });
        QVERIFY2(browser.waitForStarted(5000), qPrintable(browser.errorString()));
        int imageRequests = 0;
        const QByteArray pixel = QByteArray::fromBase64(
            "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNk+A8AAQUBAScY42YAAAAASUVORK5CYII=");
        QElapsedTimer deadline;
        deadline.start();
        while (browser.state() != QProcess::NotRunning && deadline.elapsed() < 15000) {
            if (!imageServer.waitForNewConnection(50)) {
                QCoreApplication::processEvents();
                continue;
            }
            while (imageServer.hasPendingConnections()) {
                QTcpSocket *socket = imageServer.nextPendingConnection();
                socket->waitForReadyRead(1000);
                const QByteArray request = socket->readAll();
                if (request.startsWith("GET /note.png ")) {
                    imageRequests += 1;
                    socket->write("HTTP/1.1 200 OK\r\nContent-Type: image/png\r\nContent-Length: "
                                  + QByteArray::number(pixel.size())
                                  + "\r\nConnection: close\r\n\r\n" + pixel);
                } else {
                    socket->write("HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
                }
                socket->waitForBytesWritten(1000);
                socket->disconnectFromHost();
                socket->deleteLater();
            }
            QCoreApplication::processEvents();
        }
        QVERIFY2(browser.state() == QProcess::NotRunning,
                 "Chromium did not finish within 15 seconds.");
        QCOMPARE(browser.exitStatus(), QProcess::NormalExit);
        QCOMPARE(browser.exitCode(), 0);
        QCOMPARE(imageRequests, 1);

        const QString rendered = QString::fromUtf8(browser.readAllStandardOutput());
        QVERIFY(rendered.contains(QStringLiteral("data-subtitle-visible=\"true\"")));
        QVERIFY(rendered.contains(QStringLiteral("data-subtitle-has-text=\"true\"")));
        QVERIFY(rendered.contains(QStringLiteral("data-page-title=\"Browser fallback title\"")));
        QVERIFY(rendered.contains(QStringLiteral("data-note-image-deferred=\"true\"")));
        QVERIFY(rendered.contains(QStringLiteral("data-note-image-activated=\"true\"")));
        QVERIFY(rendered.contains(QStringLiteral("data-note-image-persisted=\"true\"")));
        QVERIFY(rendered.contains(QStringLiteral("data-chrome-key-slide=\"1\"")));
        QVERIFY(rendered.contains(QStringLiteral("data-subtitle-hidden-after-toggle=\"true\"")));
        QVERIFY(rendered.contains(QStringLiteral("data-subtitle-visible-after-toggle=\"true\"")));
    }

    void publishedTitleFallsBackToFirstVisibleHeading()
    {
        QJsonObject deck = sampleDeck();
        QJsonObject frontmatter = deck.value(QStringLiteral("frontmatter")).toObject();
        frontmatter.remove(QStringLiteral("title"));
        frontmatter.insert(QStringLiteral("publish"), QJsonObject{
            {QStringLiteral("slug"), QStringLiteral("slug-must-not-win")}
        });
        deck.insert(QStringLiteral("frontmatter"), frontmatter);
        deck.insert(QStringLiteral("slides"), QJsonArray{
            QJsonObject{
                {QStringLiteral("index"), -1},
                {QStringLiteral("markdown"), QStringLiteral("# Hidden recall heading")},
                {QStringLiteral("recallKey"), QStringLiteral("q")},
                {QStringLiteral("skip"), true},
            },
            slide(0, QStringLiteral("```markdown\n```not a close\n# Example only\n```\n\n"
                                    "## First visible heading ###")),
            slide(1, QStringLiteral("# Later heading")),
        });

        WebBundle bundle;
        bundle.setDeck(deck);
        bundle.setDeckDir(sandbox(QStringLiteral("sources/deck")));
        bundle.setRendererDir(sandbox(QStringLiteral("renderer")));
        QVERIFY2(bundle.build(outputDir()), qPrintable(bundle.lastError()));

        const QString deckHtml = readText(outputDir() + QStringLiteral("/index.html"));
        const QString readHtml = readText(outputDir() + QStringLiteral("/read/index.html"));
        QVERIFY(deckHtml.contains(
            QStringLiteral("<title>First visible heading</title>")));
        QVERIFY(readHtml.contains(
            QStringLiteral("<h1>First visible heading</h1>")));

        deck.insert(QStringLiteral("slides"), QJsonArray{
            slide(0, QStringLiteral("Setext title\n==="))
        });
        WebBundle setextBundle;
        setextBundle.setDeck(deck);
        setextBundle.setDeckDir(sandbox(QStringLiteral("sources/deck")));
        setextBundle.setRendererDir(sandbox(QStringLiteral("renderer")));
        QVERIFY2(setextBundle.build(sandbox(QStringLiteral("out-setext"))),
                 qPrintable(setextBundle.lastError()));
        QVERIFY(readText(sandbox(QStringLiteral("out-setext/index.html")))
                    .contains(QStringLiteral("<title>Setext title</title>")));
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

    void copiesOnlyTheTrustedOmarchyWallpaperOutsideAssetRoots()
    {
        const QString stateRoot = sandbox(QStringLiteral("state"));
        ScopedEnvironment stateHome("XDG_STATE_HOME", stateRoot.toUtf8());
        const QString current = stateRoot + QStringLiteral("/omarchy/current");
        const QString wallpaper =
            current + QStringLiteral("/theme/backgrounds/wallpaper.png");
        const QString selected = current + QStringLiteral("/background");
        writeText(wallpaper, QStringLiteral("trusted wallpaper"));
        if (!makeSymlink(wallpaper, selected))
            QSKIP("This filesystem cannot create symlinks.");

        QJsonObject deck = sampleDeck();
        deck.insert(QStringLiteral("backgroundImage"), fileUrl(selected));
        WebBundle bundle;
        bundle.setDeck(deck);
        bundle.setDeckDir(sandbox(QStringLiteral("sources/deck")));
        bundle.setRendererDir(sandbox(QStringLiteral("renderer")));
        QVERIFY2(bundle.build(outputDir()), qPrintable(bundle.lastError()));

        const QString bundled =
            inlinedDeck(outputDir() + QStringLiteral("/index.html"))
                .value(QStringLiteral("backgroundImage")).toString();
        QVERIFY(bundled.startsWith(QStringLiteral("media/")));
        QCOMPARE(readText(QDir(outputDir()).filePath(bundled)),
                 QStringLiteral("trusted wallpaper"));

        const QString secret = sandbox(QStringLiteral("outside/secret.png"));
        writeText(secret, QStringLiteral("must not publish"));
        QVERIFY(QFile::remove(selected));
        QVERIFY(makeSymlink(secret, selected));

        WebBundle rejected;
        rejected.setDeck(deck);
        rejected.setDeckDir(sandbox(QStringLiteral("sources/deck")));
        rejected.setRendererDir(sandbox(QStringLiteral("renderer")));
        const QString rejectedOutput = sandbox(QStringLiteral("out-rejected-wallpaper"));
        QVERIFY2(rejected.build(rejectedOutput), qPrintable(rejected.lastError()));
        QCOMPARE(inlinedDeck(rejectedOutput + QStringLiteral("/index.html"))
                     .value(QStringLiteral("backgroundImage")).toString(),
                 QString());
        for (const QString &relative : rejected.files()) {
            if (relative.startsWith(QStringLiteral("media/")))
                QVERIFY(readText(QDir(rejectedOutput).filePath(relative))
                        != QStringLiteral("must not publish"));
        }

        QVERIFY(QDir(current).removeRecursively());
        const QString outsideCurrent = sandbox(QStringLiteral("outside/fake-current"));
        writeText(outsideCurrent + QStringLiteral("/background"),
                  QStringLiteral("parent link secret"));
        QVERIFY(makeSymlink(outsideCurrent, current));

        WebBundle parentRejected;
        parentRejected.setDeck(deck);
        parentRejected.setDeckDir(sandbox(QStringLiteral("sources/deck")));
        parentRejected.setRendererDir(sandbox(QStringLiteral("renderer")));
        const QString parentOutput = sandbox(QStringLiteral("out-parent-wallpaper"));
        QVERIFY2(parentRejected.build(parentOutput),
                 qPrintable(parentRejected.lastError()));
        QCOMPARE(inlinedDeck(parentOutput + QStringLiteral("/index.html"))
                     .value(QStringLiteral("backgroundImage")).toString(),
                 QString());
        for (const QString &relative : parentRejected.files()) {
            if (relative.startsWith(QStringLiteral("media/")))
                QVERIFY(readText(QDir(parentOutput).filePath(relative))
                        != QStringLiteral("parent link secret"));
        }
    }

    void streamsLargeMediaWithoutChangingBytes()
    {
        const QString sourcePath =
            sandbox(QStringLiteral("sources/deck/large-video.mp4"));
        QVERIFY(QDir().mkpath(QFileInfo(sourcePath).path()));

        constexpr qint64 chunk = 1024 * 1024;
        constexpr qint64 sourceSize = 5 * chunk + 37;
        const QByteArray first("BEGIN");
        const QByteArray boundary("CHUNK-BOUNDARY");
        const QByteArray last("END");
        QFile source(sourcePath);
        QVERIFY2(source.open(QIODevice::ReadWrite | QIODevice::Truncate),
                 qPrintable(source.errorString()));
        QVERIFY(source.resize(sourceSize));
        QCOMPARE(source.write(first), first.size());
        QVERIFY(source.seek(chunk - 4));
        QCOMPARE(source.write(boundary), boundary.size());
        QVERIFY(source.seek(sourceSize - last.size()));
        QCOMPARE(source.write(last), last.size());
        source.close();

        QJsonObject deck = sampleDeck();
        QJsonObject assets = deck.value(QStringLiteral("assets")).toObject();
        assets.insert(QStringLiteral("large-video.mp4"), fileUrl(sourcePath));
        deck.insert(QStringLiteral("assets"), assets);

        WebBundle bundle;
        bundle.setDeck(deck);
        bundle.setDeckDir(sandbox(QStringLiteral("sources/deck")));
        bundle.setRendererDir(sandbox(QStringLiteral("renderer")));
        QVERIFY2(bundle.build(outputDir()), qPrintable(bundle.lastError()));

        const QString relative =
            inlinedDeck(outputDir() + QStringLiteral("/index.html"))
                .value(QStringLiteral("assets")).toObject()
                .value(QStringLiteral("large-video.mp4")).toString();
        QVERIFY(relative.startsWith(QStringLiteral("media/")));

        QFile copied(QDir(outputDir()).filePath(relative));
        QVERIFY2(copied.open(QIODevice::ReadOnly), qPrintable(copied.errorString()));
        QCOMPARE(copied.size(), sourceSize);
        QCOMPARE(copied.read(first.size()), first);
        QVERIFY(copied.seek(chunk - 4));
        QCOMPARE(copied.read(boundary.size()), boundary);
        QVERIFY(copied.seek(sourceSize - last.size()));
        QCOMPARE(copied.read(last.size()), last);
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

    void unresolvedReferencesBecomePlaceholdersAndRemoteImagesStayDeferred()
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
        // An unresolved local reference stays unresolved. A remote image URL
        // remains inert deck data for the renderer's explicit load button.
        QCOMPARE(written.value(QStringLiteral("missing.png")).toString(), QString());
        QCOMPARE(written.value(QStringLiteral("remote.png")).toString(),
                 QStringLiteral("https://example.com/remote.png"));
        QCOMPARE(written.value(QStringLiteral("stale.png")).toString(), QString());

        const QString bundleJs = readText(outputDir() + QStringLiteral("/assets/bundle.js"));
        QVERIFY(bundleJs.contains(QStringLiteral("video, input, textarea, select")));
        QVERIFY(bundleJs.contains(QStringLiteral("button, a")));
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

    void rejectsSymlinkedOutputSubdirectory()
    {
        const QString outside = sandbox(QStringLiteral("outside"));
        const QString sentinel = outside + QStringLiteral("/render.js");
        const QString assetsLink = outputDir() + QStringLiteral("/assets");
        writeText(sentinel, QStringLiteral("must stay unchanged"));
        QVERIFY(QDir().mkpath(outputDir()));
        if (!makeSymlink(outside, assetsLink))
            QSKIP("This filesystem cannot create symlinks.");

        WebBundle bundle;
        configure(&bundle);
        QVERIFY(!bundle.build(outputDir()));
        QVERIFY(bundle.lastError().contains(QStringLiteral("writable")));
        QVERIFY(bundle.files().isEmpty());
        QCOMPARE(bundle.totalBytes(), 0);
        QCOMPARE(readText(sentinel), QStringLiteral("must stay unchanged"));
        QVERIFY(QFileInfo(assetsLink).isSymLink());
    }

    void rejectsSymlinkedOutputFile()
    {
        const QString outside = sandbox(QStringLiteral("outside/render.js"));
        const QString linkedFile = outputDir() + QStringLiteral("/assets/render.js");
        writeText(outside, QStringLiteral("must stay unchanged"));
        QVERIFY(QDir().mkpath(QFileInfo(linkedFile).path()));
        if (!makeSymlink(outside, linkedFile))
            QSKIP("This filesystem cannot create symlinks.");

        WebBundle bundle;
        configure(&bundle);
        QVERIFY(!bundle.build(outputDir()));
        QVERIFY(bundle.lastError().contains(QStringLiteral("Refused")));
        QVERIFY(bundle.files().isEmpty());
        QCOMPARE(bundle.totalBytes(), 0);
        QCOMPARE(readText(outside), QStringLiteral("must stay unchanged"));
        QVERIFY(QFileInfo(linkedFile).isSymLink());
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
