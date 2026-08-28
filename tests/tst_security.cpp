#include <QtTest>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUrl>

#include "assetindex.h"
#include "deckmodel.h"
#include "publisher.h"
#include "testrunner.h"
#include "videocache.h"
#include "webbundle.h"

namespace {

bool writeBytes(const QString &path, const QByteArray &contents)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        return false;
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
        && file.write(contents) == contents.size();
}

QByteArray readBytes(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

bool makeSymlink(const QString &target, const QString &link)
{
    if (!QDir().mkpath(QFileInfo(link).absolutePath()))
        return false;
    return QFile::link(target, link) && QFileInfo(link).isSymLink();
}

QJsonObject minimalDeck(const QJsonObject &assets = {})
{
    return QJsonObject{
        {QStringLiteral("frontmatter"), QJsonObject{}},
        {QStringLiteral("slides"),
         QJsonArray{QJsonObject{{QStringLiteral("index"), 0},
                                {QStringLiteral("markdown"), QStringLiteral("# Test\n")},
                                {QStringLiteral("recallKey"), QString()},
                                {QStringLiteral("skip"), false},
                                {QStringLiteral("sourceStartLine"), 0},
                                {QStringLiteral("sourceEndLine"), 0}}}},
        {QStringLiteral("assets"), assets},
        {QStringLiteral("media"), QJsonObject{}},
        {QStringLiteral("palette"), QJsonObject{}},
        {QStringLiteral("backgroundImage"), QString()},
        {QStringLiteral("textScale"), 1.0},
    };
}

bool writeRendererFixture(const QString &directory)
{
    return writeBytes(
        QDir(directory).filePath(QStringLiteral("render.js")),
        QByteArrayLiteral(
            "window.omapresent = { render() {}, update() {}, next() {}, "
            "previous() {}, goto() {} };\n"));
}

class ScopedEnvironment {
public:
    ScopedEnvironment(const char *name, const QByteArray &value)
        : m_name(name), m_wasSet(qEnvironmentVariableIsSet(name)),
          m_previous(qgetenv(name))
    {
        qputenv(m_name.constData(), value);
    }

    ~ScopedEnvironment()
    {
        if (m_wasSet)
            qputenv(m_name.constData(), m_previous);
        else
            qunsetenv(m_name.constData());
    }

private:
    QByteArray m_name;
    bool m_wasSet = false;
    QByteArray m_previous;
};

} // namespace

class SecurityTest : public QObject {
    Q_OBJECT

private slots:
    void commandProviderKeepsDeckTextOutOfTheShell()
    {
        QTemporaryDir sandbox;
        QVERIFY(sandbox.isValid());
        ScopedEnvironment configHome("XDG_CONFIG_HOME", sandbox.path().toUtf8());

        const QString marker = sandbox.filePath(QStringLiteral("injected"));
        const QString observed = sandbox.filePath(QStringLiteral("observed-slug"));
        const QString configDirectory = sandbox.filePath(QStringLiteral("omapresent"));
        QVERIFY(QDir().mkpath(configDirectory));

        const QString command = QStringLiteral(
            "printf \"%s\" \"$OMAPRESENT_SLUG\" > \"%1\"; "
            "echo https://example.test/deck")
                                    .arg(observed);
        const QString config = QStringLiteral(
            "default = \"test\"\n\n"
            "[providers.test]\n"
            "type = \"command\"\n"
            "publish = '%1'\n")
                                   .arg(command);
        QVERIFY(writeBytes(QDir(configDirectory).filePath(QStringLiteral("publish.toml")),
                           config.toUtf8()));

        const QString bundle = sandbox.filePath(QStringLiteral("bundle"));
        QVERIFY(writeBytes(QDir(bundle).filePath(QStringLiteral("index.html")),
                           QByteArrayLiteral("deck")));

        const QString hostile = QStringLiteral("deck; touch %1; #").arg(marker);
        Publisher publisher;
        QSignalSpy published(&publisher, &Publisher::published);
        QSignalSpy failed(&publisher, &Publisher::failed);
        publisher.publish(bundle, hostile, QStringLiteral("test"), QStringLiteral("link"));
        QVERIFY2(published.wait(5000), failed.isEmpty()
                     ? "The command provider timed out."
                     : qPrintable(failed.constFirst().constFirst().toString()));

        QVERIFY(!QFileInfo::exists(marker));
        QCOMPARE(QString::fromUtf8(readBytes(observed)), Publisher::slugify(hostile));
    }

    void indexedAssetSymlinkCannotLeaveTheAssetRoot()
    {
        QTemporaryDir sandbox;
        QVERIFY(sandbox.isValid());
        const QDir root(sandbox.path());
        const QString deckDir = root.filePath(QStringLiteral("deck"));
        const QString assetRoot = root.filePath(QStringLiteral("assets"));
        const QString renderer = root.filePath(QStringLiteral("renderer"));
        const QString output = root.filePath(QStringLiteral("bundle"));
        const QString secret = root.filePath(QStringLiteral("outside/id_rsa"));
        const QString alias = QDir(assetRoot).filePath(QStringLiteral("cover.png"));

        QVERIFY(QDir().mkpath(deckDir));
        QVERIFY(writeBytes(secret, QByteArrayLiteral("PRIVATE-KEY-MATERIAL")));
        if (!makeSymlink(secret, alias))
            QSKIP("This filesystem cannot create the symlink needed by SEC-001.");
        QVERIFY(writeRendererFixture(renderer));

        AssetIndex index;
        index.setDeckDir(deckDir);
        index.setRoot(assetRoot);
        index.waitForIndex();
        const QString resolved = index.resolve(QStringLiteral("cover.png"));

        QJsonObject assets;
        if (!resolved.isEmpty())
            assets.insert(QStringLiteral("cover.png"), QUrl::fromLocalFile(resolved).toString());
        WebBundle bundle;
        bundle.setDeck(minimalDeck(assets));
        bundle.setDeckDir(deckDir);
        bundle.setRendererDir(renderer);
        QVERIFY2(bundle.build(output), qPrintable(bundle.lastError()));

        bool leaked = false;
        for (const QString &relative : bundle.files()) {
            if (relative.startsWith(QStringLiteral("media/"))
                && readBytes(QDir(output).filePath(relative))
                    == QByteArrayLiteral("PRIVATE-KEY-MATERIAL")) {
                leaked = true;
            }
        }

        QEXPECT_FAIL("", "SEC-001: AssetIndex and WebBundle follow an indexed symlink outside the asset root.", Continue);
        QVERIFY2(!leaked, "The publish bundle copied the target of an asset symlink.");
    }

    void bundleOutputSymlinkCannotLeaveTheOutputRoot()
    {
        QTemporaryDir sandbox;
        QVERIFY(sandbox.isValid());
        const QDir root(sandbox.path());
        const QString output = root.filePath(QStringLiteral("bundle"));
        const QString outside = root.filePath(QStringLiteral("outside"));
        const QString renderer = root.filePath(QStringLiteral("renderer"));
        const QString sentinel = QDir(outside).filePath(QStringLiteral("render.js"));

        QVERIFY(QDir().mkpath(output));
        QVERIFY(QDir().mkpath(outside));
        QVERIFY(writeRendererFixture(renderer));
        QVERIFY(writeBytes(sentinel, QByteArrayLiteral("KEEP-ME")));
        if (!makeSymlink(outside, QDir(output).filePath(QStringLiteral("assets"))))
            QSKIP("This filesystem cannot create the symlink needed by SEC-005.");

        WebBundle bundle;
        bundle.setDeck(minimalDeck());
        bundle.setDeckDir(root.filePath(QStringLiteral("deck")));
        bundle.setRendererDir(renderer);
        bundle.build(output);

        const bool escaped = readBytes(sentinel) != QByteArrayLiteral("KEEP-ME");
        QEXPECT_FAIL("", "SEC-005: WebBundle follows a pre-existing directory symlink below its output root.", Continue);
        QVERIFY2(!escaped, "The bundle build overwrote a file outside its output root.");
    }

    void videoCacheSymlinkCannotLeaveTheDeckDirectory()
    {
        QTemporaryDir sandbox;
        QVERIFY(sandbox.isValid());
        const QDir root(sandbox.path());
        const QString deckDir = root.filePath(QStringLiteral("deck"));
        const QString outside = root.filePath(QStringLiteral("outside"));
        QVERIFY(QDir().mkpath(deckDir));
        QVERIFY(QDir().mkpath(outside));
        QVERIFY(writeBytes(QDir(deckDir).filePath(QStringLiteral("clip.mp4")),
                           QByteArrayLiteral("video")));
        if (!makeSymlink(outside,
                         QDir(deckDir).filePath(QStringLiteral(".omapresent-cache")))) {
            QSKIP("This filesystem cannot create the symlink needed by SEC-003.");
        }

        VideoCache cache;
        cache.setDeckDir(deckDir);
        QSignalSpy finished(&cache, &VideoCache::prefetchFinished);
        cache.prefetch(QStringList{QStringLiteral("clip.mp4")});
        QCOMPARE(finished.size(), 1);

        const bool escaped = QFileInfo::exists(
            QDir(outside).filePath(QStringLiteral("index.json")));
        QEXPECT_FAIL("", "SEC-003: VideoCache follows a .omapresent-cache directory symlink.", Continue);
        QVERIFY2(!escaped, "Preparing offline wrote cache state outside the deck directory.");
    }

    void hostileParserInputsComplete()
    {
        const QString longLine(8 * 1024 * 1024, QLatin1Char('x'));
        DeckModel longDeck;
        longDeck.setSource(QStringLiteral("---\n") + longLine);
        QCOMPARE(longDeck.slideCount(), 1);
        QVERIFY(AssetIndex::extractReferences(longLine).isEmpty());
        QVERIFY(VideoCache::extractUrls(longLine).isEmpty());

        QString manySlides;
        manySlides.reserve(160000);
        constexpr int slideCount = 10000;
        for (int i = 0; i < slideCount; ++i) {
            if (i)
                manySlides += QStringLiteral("\n---\n\n");
            manySlides += QStringLiteral("# Slide\n");
        }
        DeckModel largeDeck;
        largeDeck.setSource(manySlides);
        QCOMPARE(largeDeck.slideCount(), slideCount);

        DeckModel unclosedFence;
        unclosedFence.setSource(QStringLiteral("```text\n") + longLine);
        QCOMPARE(unclosedFence.slideCount(), 1);
    }
};

OMAPRESENT_TEST_SUITE(SecurityTest)
#include "tst_security.moc"

// Created by Codex GPT-5.6 Sol on 2026-08-27 21:29 PT on ombee.
