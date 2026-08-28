#include <QtTest>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUrl>

#include "testrunner.h"
#include "videocache.h"

class VideoCacheTest : public QObject {
    Q_OBJECT

private slots:
    void hostFor_data();
    void hostFor();
    void isBareUrlLine();
    void extractUrls();
    void embedUrlFor_data();
    void embedUrlFor();
    void oEmbedUrlFor();
    void cacheKeyStable();
    void describeEmbed();
    void describeQr();
    void describeCachedFromHandWrittenIndex();
    void describeCachedIgnoresPathTraversal();
    void describeLocalFileWithoutIndex();
    void describeVertical();
    void describeNeverTouchesNetwork();
    void prefetchEmpty();
    void prefetchCopiesLocalFileAndIsNoopOnRetry();
    void prefetchSkipsAlreadyCachedRemote();
    void prefetchNotAVideoIsNotAFailure();
    void prefetchMissingLocalFileFails();
};

static bool writeBytes(const QString &path, const QByteArray &bytes)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    return f.write(bytes) == bytes.size() && f.flush();
}

void VideoCacheTest::hostFor_data()
{
    QTest::addColumn<QString>("url");
    QTest::addColumn<int>("expected");

    auto row = [](const char *name, const QString &url, VideoCache::Host host) {
        QTest::newRow(name) << url << int(host);
    };

    row("youtu.be", QStringLiteral("https://youtu.be/dQw4w9wgxcQ"), VideoCache::YouTube);
    row("youtu.be-http", QStringLiteral("http://youtu.be/dQw4w9wgxcQ"), VideoCache::YouTube);
    row("youtu.be-slash", QStringLiteral("https://youtu.be/dQw4w9wgxcQ/"), VideoCache::YouTube);
    row("youtu.be-noscheme", QStringLiteral("youtu.be/dQw4w9wgxcQ"), VideoCache::YouTube);
    row("watch", QStringLiteral("https://www.youtube.com/watch?v=dQw4w9wgxcQ"), VideoCache::YouTube);
    row("watch-no-www", QStringLiteral("https://youtube.com/watch?v=dQw4w9wgxcQ"), VideoCache::YouTube);
    row("watch-http", QStringLiteral("http://www.youtube.com/watch?v=dQw4w9wgxcQ"), VideoCache::YouTube);
    row("watch-params",
        QStringLiteral("https://www.youtube.com/watch?v=dQw4w9wgxcQ&t=42s&feature=share"),
        VideoCache::YouTube);
    row("watch-v-not-first",
        QStringLiteral("https://www.youtube.com/watch?feature=share&v=dQw4w9wgxcQ"),
        VideoCache::YouTube);
    row("shorts", QStringLiteral("https://www.youtube.com/shorts/dQw4w9wgxcQ"), VideoCache::YouTube);
    row("shorts-slash", QStringLiteral("https://youtube.com/shorts/dQw4w9wgxcQ/"), VideoCache::YouTube);
    row("embed", QStringLiteral("https://www.youtube.com/embed/dQw4w9wgxcQ"), VideoCache::YouTube);
    row("nocookie",
        QStringLiteral("https://www.youtube-nocookie.com/embed/dQw4w9wgxcQ"),
        VideoCache::YouTube);
    row("m-youtube", QStringLiteral("https://m.youtube.com/watch?v=dQw4w9wgxcQ"), VideoCache::YouTube);
    row("live", QStringLiteral("https://www.youtube.com/live/dQw4w9wgxcQ"), VideoCache::YouTube);
    row("www-no-scheme", QStringLiteral("www.youtube.com/watch?v=dQw4w9wgxcQ"), VideoCache::YouTube);

    row("vimeo", QStringLiteral("https://vimeo.com/123456789"), VideoCache::Vimeo);
    row("vimeo-slash", QStringLiteral("https://vimeo.com/123456789/"), VideoCache::Vimeo);
    row("vimeo-hash", QStringLiteral("https://vimeo.com/123456789/abcdef"), VideoCache::Vimeo);
    row("vimeo-player", QStringLiteral("https://player.vimeo.com/video/123456789"), VideoCache::Vimeo);
    row("vimeo-www", QStringLiteral("https://www.vimeo.com/123456789"), VideoCache::Vimeo);
    row("vimeo-http", QStringLiteral("http://vimeo.com/123456789"), VideoCache::Vimeo);
    row("vimeo-channel",
        QStringLiteral("https://vimeo.com/channels/staffpicks/123456789"),
        VideoCache::Vimeo);
    row("vimeo-group",
        QStringLiteral("https://vimeo.com/groups/name/videos/123456789"),
        VideoCache::Vimeo);

    row("loom-share", QStringLiteral("https://www.loom.com/share/abc123def456"), VideoCache::Loom);
    row("loom-slash", QStringLiteral("https://loom.com/share/abc123def456/"), VideoCache::Loom);
    row("loom-embed", QStringLiteral("https://www.loom.com/embed/abc123def456"), VideoCache::Loom);
    row("loom-http", QStringLiteral("http://loom.com/share/abc123def456"), VideoCache::Loom);

    row("descript-view", QStringLiteral("https://share.descript.com/view/abc123"), VideoCache::Descript);
    row("descript-slash", QStringLiteral("https://share.descript.com/view/abc123/"), VideoCache::Descript);
    row("descript-embed", QStringLiteral("https://share.descript.com/embed/abc123"), VideoCache::Descript);

    row("tiktok-video",
        QStringLiteral("https://www.tiktok.com/@user/video/1234567890123456789"),
        VideoCache::TikTok);
    row("tiktok-slash",
        QStringLiteral("https://tiktok.com/@user/video/1234567890123456789/"),
        VideoCache::TikTok);
    row("tiktok-vm", QStringLiteral("https://vm.tiktok.com/ZMabcdef/"), VideoCache::TikTok);
    row("tiktok-embed",
        QStringLiteral("https://www.tiktok.com/embed/v2/1234567890123456789"),
        VideoCache::TikTok);

    row("x-status", QStringLiteral("https://x.com/jack/status/20"), VideoCache::X);
    row("twitter-status", QStringLiteral("https://twitter.com/jack/status/20"), VideoCache::X);
    row("twitter-www", QStringLiteral("https://www.twitter.com/jack/status/20/"), VideoCache::X);
    row("twitter-mobile", QStringLiteral("https://mobile.twitter.com/jack/status/20"), VideoCache::X);
    row("x-i-status", QStringLiteral("https://x.com/i/status/20"), VideoCache::X);
    row("twitter-http-params", QStringLiteral("http://twitter.com/jack/status/20?s=20"), VideoCache::X);

    row("ig-p", QStringLiteral("https://www.instagram.com/p/SHORTCODE/"), VideoCache::Instagram);
    row("ig-p-noslash", QStringLiteral("https://instagram.com/p/SHORTCODE"), VideoCache::Instagram);
    row("ig-reel", QStringLiteral("https://www.instagram.com/reel/SHORTCODE/"), VideoCache::Instagram);
    row("ig-reels", QStringLiteral("https://www.instagram.com/reels/SHORTCODE/"), VideoCache::Instagram);
    row("ig-utm", QStringLiteral("http://instagram.com/p/SHORTCODE/?utm_source=ig"), VideoCache::Instagram);

    row("fb-watch", QStringLiteral("https://www.facebook.com/watch?v=123456"), VideoCache::Facebook);
    row("fb-videophp", QStringLiteral("https://facebook.com/video.php?v=123456"), VideoCache::Facebook);
    row("fb-videos", QStringLiteral("https://www.facebook.com/someone/videos/123456"), VideoCache::Facebook);
    row("fb-watch-short", QStringLiteral("https://fb.watch/abcXYZ/"), VideoCache::Facebook);
    row("fb-reel", QStringLiteral("https://www.facebook.com/reel/123456"), VideoCache::Facebook);
    row("fb-share", QStringLiteral("https://www.facebook.com/share/v/abc123"), VideoCache::Facebook);

    row("direct-mp4", QStringLiteral("https://cdn.example.com/talk.mp4"), VideoCache::DirectFile);
    row("direct-webm", QStringLiteral("http://example.com/a.webm"), VideoCache::DirectFile);
    row("direct-mov-query", QStringLiteral("https://example.com/path/video.MOV?token=1"),
        VideoCache::DirectFile);
    row("direct-spaces", QStringLiteral("https://example.com/my%20file.mp4"), VideoCache::DirectFile);

    row("local-mp4", QStringLiteral("file:///tmp/talk.mp4"), VideoCache::LocalFile);
    row("local-spaces", QStringLiteral("file:///tmp/my%20video.webm"), VideoCache::LocalFile);

    row("bare-word", QStringLiteral("hello"), VideoCache::NotAVideo);
    row("empty", QString(), VideoCache::NotAVideo);
    row("mailto", QStringLiteral("mailto:writer@example.com"), VideoCache::NotAVideo);
    row("unrecognised", QStringLiteral("https://example.com/page"), VideoCache::NotAVideo);
    row("youtube-in-path",
        QStringLiteral("https://evil.example/youtube.com/watch?v=dQw4w9wgxcQ"),
        VideoCache::NotAVideo);
    row("youtube-in-query",
        QStringLiteral("https://example.com/?url=https://youtube.com/watch?v=abc"),
        VideoCache::NotAVideo);
    row("youtube-suffix-domain",
        QStringLiteral("https://youtube.com.evil.com/watch?v=dQw4w9wgxcQ"),
        VideoCache::NotAVideo);
    row("notyoutube",
        QStringLiteral("https://notyoutube.com/watch?v=dQw4w9wgxcQ"),
        VideoCache::NotAVideo);
    row("youtube-home", QStringLiteral("https://youtube.com/"), VideoCache::NotAVideo);
    row("youtube-feed", QStringLiteral("https://www.youtube.com/feed"), VideoCache::NotAVideo);
    row("vimeo-watch-page", QStringLiteral("https://vimeo.com/watch"), VideoCache::NotAVideo);
    row("x-profile", QStringLiteral("https://x.com/jack"), VideoCache::NotAVideo);
    row("ig-profile", QStringLiteral("https://instagram.com/user"), VideoCache::NotAVideo);
    row("tiktok-profile", QStringLiteral("https://www.tiktok.com/@user"), VideoCache::NotAVideo);
    row("javascript", QStringLiteral("javascript:alert(1)"), VideoCache::NotAVideo);
    row("relative-mp4", QStringLiteral("clip.mp4"), VideoCache::LocalFile);
    row("relative-dot-webm", QStringLiteral("./media/movie.webm"), VideoCache::LocalFile);
    row("relative-spaces", QStringLiteral("./media/keynote clip.mov"), VideoCache::LocalFile);
}

void VideoCacheTest::hostFor()
{
    QFETCH(QString, url);
    QFETCH(int, expected);
    QCOMPARE(int(VideoCache::hostFor(url)), expected);
}

void VideoCacheTest::isBareUrlLine()
{
    QVERIFY(VideoCache::isBareUrlLine(QStringLiteral("https://youtu.be/abc")));
    QVERIFY(VideoCache::isBareUrlLine(QStringLiteral("  https://youtu.be/abc  ")));
    QVERIFY(VideoCache::isBareUrlLine(QStringLiteral("http://example.com/x")));
    QVERIFY(VideoCache::isBareUrlLine(QStringLiteral("mailto:a@b.com")));
    QVERIFY(VideoCache::isBareUrlLine(QStringLiteral("www.example.com/x")));
    QVERIFY(VideoCache::isBareUrlLine(QStringLiteral("youtu.be/dQw4w9wgxcQ")));
    QVERIFY(VideoCache::isBareUrlLine(QStringLiteral("file:///tmp/talk.mp4")));

    QVERIFY(!VideoCache::isBareUrlLine(QStringLiteral("See https://youtu.be/abc later")));
    QVERIFY(!VideoCache::isBareUrlLine(QStringLiteral("https://youtu.be/abc more")));
    QVERIFY(!VideoCache::isBareUrlLine(QStringLiteral("hello")));
    QVERIFY(!VideoCache::isBareUrlLine(QString()));
    QVERIFY(!VideoCache::isBareUrlLine(QStringLiteral("   ")));
    QVERIFY(!VideoCache::isBareUrlLine(QStringLiteral("clip.mp4")));
    QVERIFY(!VideoCache::isBareUrlLine(QStringLiteral("./media/movie.webm")));
    QVERIFY(!VideoCache::isBareUrlLine(QStringLiteral("`https://youtu.be/abc`")));
}

void VideoCacheTest::extractUrls()
{
    const QString slide = QStringLiteral(
        "# Title\n"
        "\n"
        "https://youtu.be/abc\n"
        "\n"
        "This note mentions https://example.com/in-prose and should not count.\n"
        "\n"
        "```python\n"
        "print(\"https://youtu.be/fenced\")\n"
        "https://youtu.be/still-fenced\n"
        "```\n"
        "\n"
        "https://vimeo.com/123\n"
        "\n"
        "~~~\n"
        "https://www.loom.com/share/nope\n"
        "~~~\n"
        "\n"
        "  https://x.com/jack/status/20  \n"
        "\n"
        "// a comment-looking line is still not a URL\n");

    const QStringList got = VideoCache::extractUrls(slide);
    const QStringList expected = {
        QStringLiteral("https://youtu.be/abc"),
        QStringLiteral("https://vimeo.com/123"),
        QStringLiteral("https://x.com/jack/status/20"),
    };
    QCOMPARE(got, expected);
    QCOMPARE(VideoCache::extractUrls(QString()), QStringList());
}

void VideoCacheTest::embedUrlFor_data()
{
    QTest::addColumn<QString>("url");
    QTest::addColumn<QString>("embed");

    QTest::newRow("youtube-watch")
        << QStringLiteral("https://www.youtube.com/watch?v=dQw4w9wgxcQ&t=42s")
        << QStringLiteral("https://www.youtube.com/embed/dQw4w9wgxcQ");
    QTest::newRow("youtu.be")
        << QStringLiteral("https://youtu.be/dQw4w9wgxcQ/")
        << QStringLiteral("https://www.youtube.com/embed/dQw4w9wgxcQ");
    QTest::newRow("shorts")
        << QStringLiteral("https://www.youtube.com/shorts/dQw4w9wgxcQ")
        << QStringLiteral("https://www.youtube.com/embed/dQw4w9wgxcQ");
    QTest::newRow("vimeo")
        << QStringLiteral("https://vimeo.com/123456789")
        << QStringLiteral("https://player.vimeo.com/video/123456789");
    QTest::newRow("vimeo-hash")
        << QStringLiteral("https://vimeo.com/123456789/abcdef")
        << QStringLiteral("https://player.vimeo.com/video/123456789?h=abcdef");
    QTest::newRow("vimeo-player")
        << QStringLiteral("https://player.vimeo.com/video/123456789")
        << QStringLiteral("https://player.vimeo.com/video/123456789");
    QTest::newRow("loom")
        << QStringLiteral("https://www.loom.com/share/abc123def456")
        << QStringLiteral("https://www.loom.com/embed/abc123def456");
    QTest::newRow("descript")
        << QStringLiteral("https://share.descript.com/view/abc123")
        << QStringLiteral("https://share.descript.com/embed/abc123");
    QTest::newRow("tiktok")
        << QStringLiteral("https://www.tiktok.com/@user/video/1234567890123456789")
        << QStringLiteral("https://www.tiktok.com/embed/v2/1234567890123456789");
    QTest::newRow("x")
        << QStringLiteral("https://x.com/jack/status/20")
        << QStringLiteral("https://platform.twitter.com/embed/Tweet.html?id=20");
    QTest::newRow("ig-p")
        << QStringLiteral("https://www.instagram.com/p/SHORTCODE/")
        << QStringLiteral("https://www.instagram.com/p/SHORTCODE/embed/");
    QTest::newRow("ig-reel")
        << QStringLiteral("https://www.instagram.com/reel/SHORTCODE/")
        << QStringLiteral("https://www.instagram.com/reel/SHORTCODE/embed/");
    QTest::newRow("direct")
        << QStringLiteral("https://cdn.example.com/talk.mp4")
        << QStringLiteral("https://cdn.example.com/talk.mp4");
    QTest::newRow("unrecognised")
        << QStringLiteral("https://example.com/page")
        << QString();
    QTest::newRow("mailto")
        << QStringLiteral("mailto:a@b.com")
        << QString();
}

void VideoCacheTest::embedUrlFor()
{
    QFETCH(QString, url);
    QFETCH(QString, embed);
    QCOMPARE(VideoCache::embedUrlFor(url), embed);
}

void VideoCacheTest::oEmbedUrlFor()
{
    const QString yt = VideoCache::oEmbedUrlFor(
        QStringLiteral("https://www.youtube.com/watch?v=dQw4w9wgxcQ"));
    QVERIFY(yt.startsWith(QStringLiteral("https://www.youtube.com/oembed?")));
    QVERIFY(yt.contains(QStringLiteral("url=")));
    QVERIFY(yt.contains(QStringLiteral("dQw4w9wgxcQ")));

    const QString vimeo = VideoCache::oEmbedUrlFor(QStringLiteral("https://vimeo.com/123456789"));
    QVERIFY(vimeo.startsWith(QStringLiteral("https://vimeo.com/api/oembed.json?url=")));

    const QString loom = VideoCache::oEmbedUrlFor(
        QStringLiteral("https://www.loom.com/share/abc123def456"));
    QVERIFY(loom.startsWith(QStringLiteral("https://www.loom.com/v1/oembed?url=")));

    const QString tiktok = VideoCache::oEmbedUrlFor(
        QStringLiteral("https://www.tiktok.com/@user/video/1234567890123456789"));
    QVERIFY(tiktok.startsWith(QStringLiteral("https://www.tiktok.com/oembed?url=")));

    const QString tweet = VideoCache::oEmbedUrlFor(QStringLiteral("https://x.com/jack/status/20"));
    QVERIFY(tweet.startsWith(QStringLiteral("https://publish.twitter.com/oembed?url=")));

    QVERIFY(VideoCache::oEmbedUrlFor(QStringLiteral("https://cdn.example.com/talk.mp4")).isEmpty());
    QVERIFY(VideoCache::oEmbedUrlFor(QStringLiteral("https://example.com/page")).isEmpty());
    QVERIFY(VideoCache::oEmbedUrlFor(QStringLiteral("hello")).isEmpty());
}

void VideoCacheTest::cacheKeyStable()
{
    const QString a = VideoCache::cacheKey(QStringLiteral("https://youtu.be/abc"));
    const QString b = VideoCache::cacheKey(QStringLiteral("  https://youtu.be/abc  "));
    const QString c = VideoCache::cacheKey(QStringLiteral("https://youtu.be/abd"));
    QCOMPARE(a.size(), 64);
    QCOMPARE(a, b);
    QVERIFY(a != c);
}

void VideoCacheTest::describeEmbed()
{
    VideoCache cache;
    const QJsonObject o = cache.describe(QStringLiteral("https://youtu.be/dQw4w9wgxcQ"));
    QCOMPARE(o.value(QStringLiteral("status")).toString(), QStringLiteral("embed"));
    QCOMPARE(o.value(QStringLiteral("host")).toString(), QStringLiteral("youtube"));
    QCOMPARE(o.value(QStringLiteral("embedUrl")).toString(),
             QStringLiteral("https://www.youtube.com/embed/dQw4w9wgxcQ"));
    QVERIFY(o.value(QStringLiteral("cachedFile")).toString().isEmpty());
    QCOMPARE(o.value(QStringLiteral("width")).toInt(), 0);
    QCOMPARE(o.value(QStringLiteral("height")).toInt(), 0);
    QCOMPARE(o.value(QStringLiteral("vertical")).toBool(), false);
}

void VideoCacheTest::describeQr()
{
    VideoCache cache;
    const QJsonObject page = cache.describe(QStringLiteral("https://example.com/not-a-video"));
    QCOMPARE(page.value(QStringLiteral("status")).toString(), QStringLiteral("qr"));
    QVERIFY(page.value(QStringLiteral("embedUrl")).toString().isEmpty());
    QVERIFY(page.value(QStringLiteral("host")).toString().isEmpty());

    const QJsonObject word = cache.describe(QStringLiteral("hello"));
    QCOMPARE(word.value(QStringLiteral("status")).toString(), QStringLiteral("qr"));

    const QJsonObject mail = cache.describe(QStringLiteral("mailto:a@b.com"));
    QCOMPARE(mail.value(QStringLiteral("status")).toString(), QStringLiteral("qr"));
}

void VideoCacheTest::describeCachedFromHandWrittenIndex()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString cachePath = dir.filePath(QStringLiteral(".omapresent-cache"));
    QVERIFY(QDir().mkpath(cachePath));

    QVERIFY(writeBytes(QDir(cachePath).filePath(QStringLiteral("clip.mp4")), QByteArray("videobytes")));
    QVERIFY(writeBytes(QDir(cachePath).filePath(QStringLiteral("thumb.jpg")), QByteArray("jpeg")));

    const QString url = QStringLiteral("https://cdn.example.com/talk.mp4");
    QJsonObject index;
    QJsonObject entry;
    entry.insert(QStringLiteral("file"), QStringLiteral("clip.mp4"));
    entry.insert(QStringLiteral("poster"), QStringLiteral("thumb.jpg"));
    entry.insert(QStringLiteral("title"), QStringLiteral("Clip"));
    entry.insert(QStringLiteral("width"), 1920);
    entry.insert(QStringLiteral("height"), 1080);
    entry.insert(QStringLiteral("fetchedAt"), QStringLiteral("2026-08-27T00:00:00Z"));
    index.insert(url, entry);
    QVERIFY(writeBytes(QDir(cachePath).filePath(QStringLiteral("index.json")),
                       QJsonDocument(index).toJson()));

    VideoCache cache;
    cache.setDeckDir(dir.path());
    const QJsonObject o = cache.describe(url);
    QCOMPARE(o.value(QStringLiteral("status")).toString(), QStringLiteral("cached"));
    QCOMPARE(o.value(QStringLiteral("host")).toString(), QStringLiteral("direct"));
    QCOMPARE(o.value(QStringLiteral("title")).toString(), QStringLiteral("Clip"));
    QCOMPARE(o.value(QStringLiteral("width")).toInt(), 1920);
    QCOMPARE(o.value(QStringLiteral("height")).toInt(), 1080);
    QCOMPARE(o.value(QStringLiteral("vertical")).toBool(), false);
    QVERIFY(o.value(QStringLiteral("cachedFile")).toString().startsWith(QStringLiteral("file://")));
    QVERIFY(o.value(QStringLiteral("cachedFile")).toString().contains(QStringLiteral("clip.mp4")));
    QVERIFY(o.value(QStringLiteral("poster")).toString().contains(QStringLiteral("thumb.jpg")));
}

void VideoCacheTest::describeCachedIgnoresPathTraversal()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString cachePath = dir.filePath(QStringLiteral(".omapresent-cache"));
    QVERIFY(QDir().mkpath(cachePath));

    const QString outside = dir.filePath(QStringLiteral("secret.mp4"));
    QVERIFY(writeBytes(outside, QByteArray("secret")));

    const QString url = QStringLiteral("https://cdn.example.com/talk.mp4");
    QJsonObject index;
    QJsonObject entry;
    entry.insert(QStringLiteral("file"), QStringLiteral("../secret.mp4"));
    index.insert(url, entry);
    QVERIFY(writeBytes(QDir(cachePath).filePath(QStringLiteral("index.json")),
                       QJsonDocument(index).toJson()));

    VideoCache cache;
    cache.setDeckDir(dir.path());
    const QJsonObject o = cache.describe(url);
    QCOMPARE(o.value(QStringLiteral("status")).toString(), QStringLiteral("embed"));
    QVERIFY(o.value(QStringLiteral("cachedFile")).toString().isEmpty());
}

void VideoCacheTest::describeLocalFileWithoutIndex()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString src = dir.filePath(QStringLiteral("my video.mp4"));
    QVERIFY(writeBytes(src, QByteArray("videobytes")));
    const QString url = QUrl::fromLocalFile(src).toString(QUrl::FullyEncoded);

    VideoCache cache;
    cache.setDeckDir(dir.path());
    const QJsonObject o = cache.describe(url);
    QCOMPARE(o.value(QStringLiteral("host")).toString(), QStringLiteral("local"));
    QCOMPARE(o.value(QStringLiteral("status")).toString(), QStringLiteral("cached"));
    QVERIFY(o.value(QStringLiteral("cachedFile")).toString().contains(QStringLiteral("my")));

    const QJsonObject missing = cache.describe(QStringLiteral("file:///tmp/omapresent-no-such.mp4"));
    QCOMPARE(missing.value(QStringLiteral("host")).toString(), QStringLiteral("local"));
    QCOMPARE(missing.value(QStringLiteral("status")).toString(), QStringLiteral("qr"));

    const QString relative = QStringLiteral("nested clip.mp4");
    QVERIFY(writeBytes(dir.filePath(relative), QByteArray("videobytes")));
    const QJsonObject rel = cache.describe(relative);
    QCOMPARE(rel.value(QStringLiteral("host")).toString(), QStringLiteral("local"));
    QCOMPARE(rel.value(QStringLiteral("status")).toString(), QStringLiteral("cached"));
}

void VideoCacheTest::describeVertical()
{
    VideoCache cache;
    QVERIFY(cache.describe(QStringLiteral("https://www.youtube.com/shorts/dQw4w9wgxcQ"))
                .value(QStringLiteral("vertical"))
                .toBool());
    QVERIFY(cache.describe(QStringLiteral("https://www.tiktok.com/@user/video/1234567890123456789"))
                .value(QStringLiteral("vertical"))
                .toBool());
    QVERIFY(cache.describe(QStringLiteral("https://www.instagram.com/reel/SHORTCODE/"))
                .value(QStringLiteral("vertical"))
                .toBool());

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString cachePath = dir.filePath(QStringLiteral(".omapresent-cache"));
    QVERIFY(QDir().mkpath(cachePath));
    QVERIFY(writeBytes(QDir(cachePath).filePath(QStringLiteral("clip.mp4")), QByteArray("videobytes")));
    const QString url = QStringLiteral("https://cdn.example.com/portrait.mp4");
    QJsonObject index;
    QJsonObject entry;
    entry.insert(QStringLiteral("file"), QStringLiteral("clip.mp4"));
    entry.insert(QStringLiteral("width"), 1080);
    entry.insert(QStringLiteral("height"), 1920);
    index.insert(url, entry);
    QVERIFY(writeBytes(QDir(cachePath).filePath(QStringLiteral("index.json")),
                       QJsonDocument(index).toJson()));
    cache.setDeckDir(dir.path());
    QVERIFY(cache.describe(url).value(QStringLiteral("vertical")).toBool());
}

void VideoCacheTest::describeNeverTouchesNetwork()
{
    // describe() must answer from disk / URL shape only. A YouTube URL with no
    // cache still returns "embed" immediately — it must not wait on oEmbed.
    VideoCache cache;
    QSignalSpy progress(&cache, &VideoCache::prefetchProgress);
    QSignalSpy finished(&cache, &VideoCache::prefetchFinished);
    const QJsonObject o = cache.describe(QStringLiteral("https://youtu.be/dQw4w9wgxcQ"));
    QCOMPARE(o.value(QStringLiteral("status")).toString(), QStringLiteral("embed"));
    QCOMPARE(progress.count(), 0);
    QCOMPARE(finished.count(), 0);
}

void VideoCacheTest::prefetchEmpty()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    VideoCache cache;
    cache.setDeckDir(dir.path());
    QSignalSpy progress(&cache, &VideoCache::prefetchProgress);
    QSignalSpy finished(&cache, &VideoCache::prefetchFinished);
    cache.prefetch({});
    QCOMPARE(finished.count(), 1);
    QCOMPARE(finished.at(0).at(0).toStringList(), QStringList());
    QVERIFY(progress.count() >= 1);
}

void VideoCacheTest::prefetchCopiesLocalFileAndIsNoopOnRetry()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString src = dir.filePath(QStringLiteral("source clip.mp4"));
    QVERIFY(writeBytes(src, QByteArray("videobytes")));
    const QString url = QUrl::fromLocalFile(src).toString(QUrl::FullyEncoded);

    VideoCache cache;
    cache.setDeckDir(dir.path());
    QSignalSpy finished(&cache, &VideoCache::prefetchFinished);
    QSignalSpy changed(&cache, &VideoCache::cacheChanged);

    cache.prefetch(QStringList{url});
    QCOMPARE(finished.count(), 1);
    QCOMPARE(finished.at(0).at(0).toStringList(), QStringList());
    QVERIFY(changed.count() >= 1);

    const QJsonObject first = cache.describe(url);
    QCOMPARE(first.value(QStringLiteral("status")).toString(), QStringLiteral("cached"));
    const QString cached = first.value(QStringLiteral("cachedFile")).toString();
    QVERIFY(cached.contains(QStringLiteral(".omapresent-cache")));
    QVERIFY(QFileInfo::exists(QUrl(cached).toLocalFile()));

    cache.prefetch(QStringList{url});
    QCOMPARE(finished.count(), 2);
    QCOMPARE(finished.at(1).at(0).toStringList(), QStringList());
    QCOMPARE(cache.describe(url).value(QStringLiteral("cachedFile")).toString(), cached);
}

void VideoCacheTest::prefetchSkipsAlreadyCachedRemote()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString cachePath = dir.filePath(QStringLiteral(".omapresent-cache"));
    QVERIFY(QDir().mkpath(cachePath));
    QVERIFY(writeBytes(QDir(cachePath).filePath(QStringLiteral("clip.mp4")), QByteArray("videobytes")));

    const QString url = QStringLiteral("https://youtu.be/dQw4w9wgxcQ");
    QJsonObject index;
    QJsonObject entry;
    entry.insert(QStringLiteral("file"), QStringLiteral("clip.mp4"));
    entry.insert(QStringLiteral("title"), QStringLiteral("Already there"));
    entry.insert(QStringLiteral("fetchedAt"), QStringLiteral("2026-08-27T00:00:00Z"));
    index.insert(url, entry);
    QVERIFY(writeBytes(QDir(cachePath).filePath(QStringLiteral("index.json")),
                       QJsonDocument(index).toJson()));

    VideoCache cache;
    cache.setDeckDir(dir.path());
    QSignalSpy finished(&cache, &VideoCache::prefetchFinished);
    cache.prefetch(QStringList{url});
    QCOMPARE(finished.count(), 1);
    QCOMPARE(finished.at(0).at(0).toStringList(), QStringList());
    QCOMPARE(cache.describe(url).value(QStringLiteral("status")).toString(),
             QStringLiteral("cached"));
}

void VideoCacheTest::prefetchNotAVideoIsNotAFailure()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    VideoCache cache;
    cache.setDeckDir(dir.path());
    QSignalSpy finished(&cache, &VideoCache::prefetchFinished);
    cache.prefetch(QStringList{QStringLiteral("https://example.com/page"),
                               QStringLiteral("mailto:a@b.com")});
    QCOMPARE(finished.count(), 1);
    QCOMPARE(finished.at(0).at(0).toStringList(), QStringList());
}

void VideoCacheTest::prefetchMissingLocalFileFails()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    VideoCache cache;
    cache.setDeckDir(dir.path());
    QSignalSpy finished(&cache, &VideoCache::prefetchFinished);
    const QString url = QStringLiteral("file:///tmp/omapresent-no-such-video.mp4");
    cache.prefetch(QStringList{url});
    QCOMPARE(finished.count(), 1);
    QCOMPARE(finished.at(0).at(0).toStringList(), QStringList{url});
}

OMAPRESENT_TEST_SUITE(VideoCacheTest)
#include "tst_videocache.moc"
