#include <QtTest>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUrl>

#include "assetindex.h"
#include "backend.h"
#include "deckmodel.h"
#include "publisher.h"
#include "settings.h"
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

struct CliResult {
    bool finished = false;
    int exitCode = -1;
    QByteArray standardOutput;
    QByteArray standardError;
};

QString currentOmapresentExecutable()
{
    const QString backendSource = QFINDTESTDATA("../src/backend.cpp");
    if (backendSource.isEmpty())
        return {};
    const QDir repository(QFileInfo(backendSource).absoluteDir().absoluteFilePath(QStringLiteral("..")));
    const QString executable = repository.filePath(QStringLiteral("build/omapresent"));
    const QFileInfo executableInfo(executable);
    if (!executableInfo.isExecutable())
        return {};

    const QDateTime builtAt = executableInfo.lastModified();
    QDirIterator sourceFiles(repository.filePath(QStringLiteral("src")), QDir::Files,
                             QDirIterator::Subdirectories);
    while (sourceFiles.hasNext()) {
        sourceFiles.next();
        if (sourceFiles.fileInfo().lastModified() > builtAt)
            return {};
    }
    if (QFileInfo(repository.filePath(QStringLiteral("omapresent.pro"))).lastModified()
        > builtAt) {
        return {};
    }
    return executableInfo.absoluteFilePath();
}

CliResult runCli(const QString &executable, const QStringList &arguments,
                 const QString &configHome, const QString &stateHome,
                 const QByteArray &input = {})
{
    QProcess process;
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
    environment.insert(QStringLiteral("XDG_CONFIG_HOME"), configHome);
    environment.insert(QStringLiteral("XDG_STATE_HOME"), stateHome);
    environment.insert(QStringLiteral("XDG_DATA_HOME"),
                       QDir(stateHome).filePath(QStringLiteral("data")));
    process.setProcessEnvironment(environment);
    process.start(executable, arguments);
    if (!process.waitForStarted(5000))
        return {false, -1, {}, process.errorString().toUtf8()};
    if (!input.isEmpty())
        process.write(input);
    process.closeWriteChannel();

    CliResult result;
    result.finished = process.waitForFinished(15000);
    if (!result.finished) {
        process.kill();
        process.waitForFinished(5000);
    }
    result.exitCode = process.exitCode();
    result.standardOutput = process.readAllStandardOutput();
    result.standardError = process.readAllStandardError();
    return result;
}

bool writeCommandProvider(const QString &configHome, const QString &command)
{
    const QString config = QStringLiteral(
        "default = \"test\"\n\n"
        "[providers.test]\n"
        "type = \"command\"\n"
        "publish = '%1'\n")
                               .arg(command);
    return writeBytes(QDir(configHome).filePath(QStringLiteral("omapresent/publish.toml")),
                      config.toUtf8());
}

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
        QVERIFY2(!escaped, "Preparing offline wrote cache state outside the deck directory.");
    }

    void cliFlagsRequireConfirmationUnlessExplicit()
    {
        const Backend::CommandLine confirmed = Backend::parseCommandLine(
            {QStringLiteral("publish"), QStringLiteral("deck.md")});
        QVERIFY(confirmed.error.isEmpty());
        QVERIFY(!confirmed.assumeYes);

        const Backend::CommandLine longFlag = Backend::parseCommandLine(
            {QStringLiteral("publish"), QStringLiteral("deck.md"),
             QStringLiteral("--yes")});
        QVERIFY(longFlag.assumeYes);
        const Backend::CommandLine shortFlag = Backend::parseCommandLine(
            {QStringLiteral("publish"), QStringLiteral("deck.md"),
             QStringLiteral("-y")});
        QVERIFY(shortFlag.assumeYes);
    }

    void cliReportsActionableFailuresAndPublishesWithoutFrontmatter()
    {
        QTemporaryDir sandbox;
        QVERIFY(sandbox.isValid());
        const QString executable = currentOmapresentExecutable();
        if (executable.isEmpty())
            QSKIP("Run ./bin/build before ./bin/test to check the CLI executable.");
        const QString configHome = sandbox.filePath(QStringLiteral("config"));
        const QString stateHome = sandbox.filePath(QStringLiteral("state"));
        const QString marker = sandbox.filePath(QStringLiteral("provider-ran"));
        QVERIFY(writeCommandProvider(
            configHome,
            QStringLiteral("touch %1; echo https://example.test/no-frontmatter")
                .arg(marker)));

        const QString missing = sandbox.filePath(QStringLiteral("missing.md"));
        CliResult result = runCli(executable,
                                  {QStringLiteral("publish"), missing,
                                   QStringLiteral("--yes")},
                                  configHome, stateHome);
        QVERIFY2(result.finished, qPrintable(QString::fromUtf8(result.standardError)));
        QCOMPARE(result.exitCode, 1);
        QVERIFY(result.standardError.contains("No such file:"));
        QVERIFY(result.standardError.contains(missing.toUtf8()));

        const QString deck = sandbox.filePath(QStringLiteral("plain-deck.md"));
        QVERIFY(writeBytes(deck, QByteArrayLiteral("# No frontmatter\n\nBody.\n")));
        result = runCli(executable,
                        {QStringLiteral("publish"), deck,
                         QStringLiteral("--provider"), QStringLiteral("missing-provider"),
                         QStringLiteral("--yes")},
                        configHome, stateHome);
        QVERIFY2(result.finished, qPrintable(QString::fromUtf8(result.standardError)));
        QCOMPARE(result.exitCode, 1);
        QVERIFY(result.standardError.contains("missing-provider"));
        QVERIFY(result.standardError.contains("is not configured"));

        result = runCli(executable,
                        {QStringLiteral("publish"), deck,
                         QStringLiteral("--provider"), QStringLiteral("test")},
                        configHome, stateHome, QByteArrayLiteral("n\n"));
        QVERIFY2(result.finished, qPrintable(QString::fromUtf8(result.standardError)));
        QCOMPARE(result.exitCode, 1);
        QVERIFY(result.standardError.contains("Nothing was uploaded."));
        QVERIFY(!QFileInfo::exists(marker));

        result = runCli(executable,
                        {QStringLiteral("publish"), deck,
                         QStringLiteral("--provider"), QStringLiteral("test"),
                         QStringLiteral("--yes")},
                        configHome, stateHome);
        QVERIFY2(result.finished, qPrintable(QString::fromUtf8(result.standardError)));
        QCOMPARE(result.exitCode, 0);
        QVERIFY(result.standardOutput.contains("https://example.test/no-frontmatter"));
        QVERIFY(QFileInfo::exists(marker));
    }

    void cliRejectsADirectoryBeforePublishing()
    {
        QTemporaryDir sandbox;
        QVERIFY(sandbox.isValid());
        const QString executable = currentOmapresentExecutable();
        if (executable.isEmpty())
            QSKIP("Run ./bin/build before ./bin/test to check the CLI executable.");
        const QString configHome = sandbox.filePath(QStringLiteral("config"));
        const QString stateHome = sandbox.filePath(QStringLiteral("state"));
        QVERIFY(writeCommandProvider(configHome,
                                     QStringLiteral("echo https://example.test/wrong-deck")));
        const QString inputDirectory = sandbox.filePath(QStringLiteral("not-a-deck"));
        QVERIFY(QDir().mkpath(inputDirectory));

        const CliResult result = runCli(
            executable,
            {QStringLiteral("publish"), inputDirectory,
             QStringLiteral("--provider"), QStringLiteral("test"),
             QStringLiteral("--yes")},
            configHome, stateHome);
        QVERIFY2(result.finished, qPrintable(QString::fromUtf8(result.standardError)));
        const bool vulnerable = result.exitCode == 0
            && result.standardOutput.contains("https://example.test/wrong-deck");
        const bool safelyRejected = result.exitCode != 0
            && result.standardError.contains("not-a-deck");
        QVERIFY2(vulnerable || safelyRejected,
                 qPrintable(QString::fromUtf8(result.standardOutput
                                              + result.standardError)));
        QEXPECT_FAIL("", "SEC-006: the CLI publishes an empty deck after a directory fails to open.", Continue);
        QVERIFY(safelyRejected);
    }

    void cliRejectsAnUnreadableFileBeforePublishing()
    {
        QTemporaryDir sandbox;
        QVERIFY(sandbox.isValid());
        const QString executable = currentOmapresentExecutable();
        if (executable.isEmpty())
            QSKIP("Run ./bin/build before ./bin/test to check the CLI executable.");
        const QString configHome = sandbox.filePath(QStringLiteral("config"));
        const QString stateHome = sandbox.filePath(QStringLiteral("state"));
        QVERIFY(writeCommandProvider(configHome,
                                     QStringLiteral("echo https://example.test/wrong-deck")));
        const QString deck = sandbox.filePath(QStringLiteral("unreadable.md"));
        QVERIFY(writeBytes(deck, QByteArrayLiteral("# Private\n")));
        QVERIFY(QFile::setPermissions(deck, QFileDevice::Permissions{}));
        QFile probe(deck);
        if (probe.open(QIODevice::ReadOnly)) {
            probe.close();
            QSKIP("This test user can read a mode-000 file.");
        }

        const CliResult result = runCli(
            executable,
            {QStringLiteral("publish"), deck,
             QStringLiteral("--provider"), QStringLiteral("test"),
             QStringLiteral("--yes")},
            configHome, stateHome);
        QVERIFY2(result.finished, qPrintable(QString::fromUtf8(result.standardError)));
        const bool vulnerable = result.exitCode == 0
            && result.standardOutput.contains("https://example.test/wrong-deck");
        const bool safelyRejected = result.exitCode != 0
            && result.standardError.contains("unreadable.md");
        QVERIFY2(vulnerable || safelyRejected,
                 qPrintable(QString::fromUtf8(result.standardOutput
                                              + result.standardError)));
        QEXPECT_FAIL("", "SEC-006: the CLI publishes an empty deck after a file fails to open.", Continue);
        QVERIFY(safelyRejected);
    }

    void firstRunRefusesASymlinkedSkillDirectory()
    {
        QTemporaryDir home;
        QTemporaryDir outside;
        QTemporaryDir skill;
        QVERIFY(home.isValid() && outside.isValid() && skill.isValid());
        const QString agentRoot = home.filePath(QStringLiteral(".claude"));
        const QString skillsLink = QDir(agentRoot).filePath(QStringLiteral("skills"));
        QVERIFY(QDir().mkpath(agentRoot));
        if (!makeSymlink(outside.path(), skillsLink))
            QSKIP("This filesystem cannot create the symlink needed by SEC-007.");

        const QStringList directories = Backend::agentSkillDirectories(home.path());
        Backend::installAgentSkill(skill.path(), QStringLiteral("omapresent"), directories);
        const bool escaped = QFileInfo(QDir(outside.path())
                                           .filePath(QStringLiteral("omapresent")))
                                 .isSymLink();
        QEXPECT_FAIL("", "SEC-007: first run follows a symlinked skills directory.", Continue);
        QVERIFY2(!escaped, "First run created the skill link outside the agent directory.");
    }

    void firstRunPreservesARealSkillDirectoryWithContent()
    {
        QTemporaryDir home;
        QTemporaryDir skill;
        QVERIFY(home.isValid() && skill.isValid());
        const QString existing = home.filePath(
            QStringLiteral(".claude/skills/omapresent"));
        const QString sentinel = QDir(existing).filePath(QStringLiteral("keep.txt"));
        QVERIFY(writeBytes(sentinel, QByteArrayLiteral("user content")));

        const QStringList directories = Backend::agentSkillDirectories(home.path());
        QCOMPARE(directories, QStringList{home.filePath(QStringLiteral(".claude/skills"))});
        QVERIFY(Backend::installAgentSkill(skill.path(), QStringLiteral("omapresent"),
                                           directories)
                    .isEmpty());
        QVERIFY(QFileInfo(existing).isDir());
        QVERIFY(!QFileInfo(existing).isSymLink());
        QCOMPARE(readBytes(sentinel), QByteArrayLiteral("user content"));
    }

    void settingsPatchPreservesHostileUnknownText()
    {
        QTemporaryDir sandbox;
        QVERIFY(sandbox.isValid());
        const QString path = sandbox.filePath(QStringLiteral("settings.toml"));
        const QString longKey = QStringLiteral("unknown_")
            + QString(256 * 1024, QLatin1Char('k'));
        const QString longAssignment = longKey + QStringLiteral(" = \"keep exactly\"");
        const QString original = QStringLiteral(
            "# hostile text must survive\n"
            "[editor]\n")
            + longAssignment
            + QStringLiteral(
                "\nbroken = \"unterminated\n"
                "unknown_before = 'keep before'\n"
                "[editor]\n"
                "unknown_after = 'keep after'\n"
                "[presentation]\n"
                "inhibit_idle = true\n");
        QVERIFY(writeBytes(path, original.toUtf8()));

        Settings settings;
        settings.setPath(path);
        const QString font = QStringLiteral("first line\nsecond line");
        QVERIFY(settings.setValue(QStringLiteral("editor.font"), font));
        const QString patched = QString::fromUtf8(readBytes(path));

        QVERIFY(patched.contains(QStringLiteral("# hostile text must survive")));
        QVERIFY(patched.contains(longAssignment));
        QVERIFY(patched.contains(QStringLiteral("broken = \"unterminated")));
        QVERIFY(patched.contains(QStringLiteral("unknown_before = 'keep before'")));
        QVERIFY(patched.contains(QStringLiteral("unknown_after = 'keep after'")));
        QCOMPARE(patched.count(QStringLiteral("[editor]")), 2);
        QVERIFY(patched.contains(QStringLiteral("font = \"first line\\nsecond line\"")));
        QCOMPARE(settings.stringValue(QStringLiteral("editor.font")), font);
    }

    void settingsPatchAppliesWithADuplicateKnownKey()
    {
        QTemporaryDir sandbox;
        QVERIFY(sandbox.isValid());
        const QString path = sandbox.filePath(QStringLiteral("settings.toml"));
        const QByteArray original = QByteArrayLiteral(
            "[editor]\n"
            "theme = \"first\"\n"
            "unknown_one = \"keep one\"\n"
            "[editor]\n"
            "theme = \"second\"\n"
            "unknown_two = \"keep two\"\n");
        QVERIFY(writeBytes(path, original));

        Settings settings;
        settings.setPath(path);
        QVERIFY(settings.setValue(QStringLiteral("editor.theme"),
                                  QStringLiteral("patched")));
        const QByteArray patched = readBytes(path);
        QVERIFY(patched.contains("unknown_one = \"keep one\""));
        QVERIFY(patched.contains("unknown_two = \"keep two\""));
        QEXPECT_FAIL("", "SEC-008: patchToml changes the shadowed key in the first duplicate table.", Continue);
        QCOMPARE(settings.stringValue(QStringLiteral("editor.theme")),
                 QStringLiteral("patched"));
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
