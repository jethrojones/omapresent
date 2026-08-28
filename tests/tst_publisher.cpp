#include <QtTest>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "testrunner.h"
#include "publisher.h"

class PublisherTest : public QObject {
    Q_OBJECT

private slots:
    void slugifiesText() {
        QCOMPARE(Publisher::slugify(QStringLiteral("Already-a-slug")),
                 QStringLiteral("already-a-slug"));
        QCOMPARE(Publisher::slugify(QStringLiteral("  An API... & a Deck!! ")),
                 QStringLiteral("an-api-a-deck"));
        QCOMPARE(Publisher::slugify(QStringLiteral("Crème brûlée — déjà vu")),
                 QStringLiteral("creme-brulee-deja-vu"));
        QCOMPARE(Publisher::slugify(QStringLiteral("東京")), QStringLiteral("deck"));
        QCOMPARE(Publisher::slugify(QStringLiteral("---")), QStringLiteral("deck"));
        QCOMPARE(Publisher::slugify(Publisher::slugify(QStringLiteral("A  Deck"))),
                 QStringLiteral("a-deck"));
    }

    void parsesPublishConfig() {
        const QString config = QStringLiteral(
            "default = \"herenow\"\n"
            "\n"
            "[providers.herenow]\n"
            "type = \"herenow\"\n"
            "api_key = \"hn_xxx\" # blank/omitted means anonymous\n"
            "domain = \"omapresent.com\"\n"
            "mount_prefix = \"/presentations\"\n"
            "\n"
            "[providers.mybox]\n"
            "type = \"command\"\n"
            "publish = \"rsync -a $OMAPRESENT_BUNDLE/ somewhere && echo "
            "https://example.com/$OMAPRESENT_SLUG\"\n"
            "\n"
            "[providers.s3]\n"
            "type = \"s3\"\n"
            "endpoint = \"https://s3.us-west-002.backblazeb2.com\"\n"
            "bucket = \"my-decks\"\n"
            "prefix = \"presentations/\"\n"
            "base_url = \"https://decks.example.com\"\n"
            "access_key_id = \"test-id\"\n"
            "secret_access_key = \"test-secret\"\n");

        const QJsonObject root = Publisher::parseToml(config);
        QCOMPARE(root.value(QStringLiteral("default")).toString(),
                 QStringLiteral("herenow"));
        const QJsonObject providers = root.value(QStringLiteral("providers")).toObject();
        QCOMPARE(providers.size(), 3);
        const QJsonObject herenow = providers.value(QStringLiteral("herenow")).toObject();
        QCOMPARE(herenow.value(QStringLiteral("type")).toString(),
                 QStringLiteral("herenow"));
        QCOMPARE(herenow.value(QStringLiteral("api_key")).toString(),
                 QStringLiteral("hn_xxx"));
        QCOMPARE(herenow.value(QStringLiteral("mount_prefix")).toString(),
                 QStringLiteral("/presentations"));
        const QJsonObject command = providers.value(QStringLiteral("mybox")).toObject();
        QVERIFY(command.value(QStringLiteral("publish")).toString().contains(
            QStringLiteral("$OMAPRESENT_BUNDLE")));
        QCOMPARE(providers.value(QStringLiteral("s3")).toObject()
                     .value(QStringLiteral("bucket")).toString(),
                 QStringLiteral("my-decks"));
        QCOMPARE(providers.value(QStringLiteral("s3")).toObject()
                     .value(QStringLiteral("access_key_id")).toString(),
                 QStringLiteral("test-id"));
    }

    void patchesExistingKeyWithoutReformatting() {
        const QString before = QStringLiteral(
            "# Publish config\r\n"
            "default  =  \"herenow\"\r\n"
            "\r\n"
            "[providers.herenow] # keep this\r\n"
            "type = \"herenow\"\r\n"
            "api_key   =   \"old # value\"   # account key\r\n"
            "unknown = \"keep me\"\r\n"
            "\r\n"
            "[providers.other]\r\n"
            "type = \"future\"\r\n");
        QString expected = before;
        expected.replace(QStringLiteral("\"old # value\""),
                         QStringLiteral("\"new\\\"key\""));

        QCOMPARE(Publisher::patchToml(before,
                                      QStringLiteral("providers.herenow.api_key"),
                                      QStringLiteral("new\"key")),
                 expected);
    }

    void addsKeyToExistingTable() {
        const QString before = QStringLiteral(
            "# top\n"
            "[providers.herenow]\n"
            "type = \"herenow\"\n"
            "\n"
            "[providers.other]\n"
            "type = \"command\"\n");
        const QString after = Publisher::patchToml(
            before, QStringLiteral("providers.herenow.domain"),
            QStringLiteral("slides.example.com"));

        QVERIFY(after.startsWith(before.left(before.indexOf(
            QStringLiteral("[providers.other]")))));
        QVERIFY(after.contains(QStringLiteral("domain = \"slides.example.com\"\n")));
        QVERIFY(after.endsWith(QStringLiteral(
            "[providers.other]\n"
            "type = \"command\"\n")));
    }

    void addsMissingTable() {
        const QString before = QStringLiteral(
            "default = \"herenow\"\n"
            "# existing bytes stay unchanged\n");
        const QString after = Publisher::patchToml(
            before, QStringLiteral("providers.mybox.publish"),
            QStringLiteral("deploy $OMAPRESENT_BUNDLE"));

        QVERIFY(after.startsWith(before));
        QVERIFY(after.contains(QStringLiteral(
            "[providers.mybox]\n"
            "publish = \"deploy $OMAPRESENT_BUNDLE\"\n")));
    }

    void loadsProvidersFromTemporaryConfig() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const QByteArray oldConfigHome = qgetenv("XDG_CONFIG_HOME");
        QVERIFY(qputenv("XDG_CONFIG_HOME", temporary.path().toUtf8()));

        {
            Publisher missing;
            const QJsonObject providers = missing.providers();
            QCOMPARE(providers.size(), 1);
            QCOMPARE(providers.value(QStringLiteral("herenow")).toObject()
                         .value(QStringLiteral("type")).toString(),
                     QStringLiteral("herenow"));
            QCOMPARE(missing.defaultProvider(), QStringLiteral("herenow"));
        }

        const QString directory = temporary.filePath(QStringLiteral("omapresent"));
        QVERIFY(QDir().mkpath(directory));
        QFile file(QDir(directory).filePath(QStringLiteral("publish.toml")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        const QByteArray fixture(
            "default = \"mybox\"\n\n"
            "[providers.mybox]\n"
            "type = \"command\"\n"
            "publish = \"deploy\"\n");
        QCOMPARE(file.write(fixture), fixture.size());
        file.close();

        {
            Publisher configured;
            QCOMPARE(configured.defaultProvider(), QStringLiteral("mybox"));
            const QJsonObject providers = configured.providers();
            QCOMPARE(providers.size(), 2);
            QCOMPARE(providers.value(QStringLiteral("mybox")).toObject()
                         .value(QStringLiteral("publish")).toString(),
                     QStringLiteral("deploy"));
            QCOMPARE(providers.value(QStringLiteral("herenow")).toObject()
                         .value(QStringLiteral("type")).toString(),
                     QStringLiteral("herenow"));
            QVERIFY(configured.setProviderKey(QStringLiteral("mybox"),
                                              QStringLiteral("publish"),
                                              QStringLiteral("deploy --safe")));
        }

        QVERIFY(file.open(QIODevice::ReadOnly));
        const QByteArray patched = file.readAll();
        QVERIFY(patched.contains("publish = \"deploy --safe\""));
        if (oldConfigHome.isNull())
            qunsetenv("XDG_CONFIG_HOME");
        else
            qputenv("XDG_CONFIG_HOME", oldConfigHome);
    }

    void rejectsUnknownAccessBeforeUpload() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const QByteArray oldConfigHome = qgetenv("XDG_CONFIG_HOME");
        QVERIFY(qputenv("XDG_CONFIG_HOME", temporary.path().toUtf8()));

        Publisher publisher;
        QSignalSpy failures(&publisher, &Publisher::failed);
        publisher.publish(QStringLiteral("/not-used"), QStringLiteral("deck"),
                          QStringLiteral("herenow"), QStringLiteral("passwrod"));
        QCOMPARE(failures.size(), 1);
        QVERIFY(failures.constFirst().constFirst().toString().contains(
            QStringLiteral("is invalid")));

        failures.clear();
        publisher.republish(QStringLiteral("/not-used"), QStringLiteral("deck"),
                            QStringLiteral("herenow"), QStringLiteral(" Password "));
        QCOMPARE(failures.size(), 1);
        QVERIFY(failures.constFirst().constFirst().toString().contains(
            QStringLiteral("is invalid")));

        if (oldConfigHome.isNull())
            qunsetenv("XDG_CONFIG_HOME");
        else
            qputenv("XDG_CONFIG_HOME", oldConfigHome);
    }

    void drainsVerboseCommandOutput() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const QByteArray oldConfigHome = qgetenv("XDG_CONFIG_HOME");
        QVERIFY(qputenv("XDG_CONFIG_HOME", temporary.path().toUtf8()));

        const QString configDirectory = temporary.filePath(QStringLiteral("omapresent"));
        QVERIFY(QDir().mkpath(configDirectory));
        QFile config(QDir(configDirectory).filePath(QStringLiteral("publish.toml")));
        QVERIFY(config.open(QIODevice::WriteOnly));
        const QByteArray configText(
            "default = \"verbose\"\n\n"
            "[providers.verbose]\n"
            "type = \"command\"\n"
            "publish = \"yes noisy | head -n 20000; echo https://example.test/deck\"\n");
        QCOMPARE(config.write(configText), configText.size());
        config.close();

        const QString bundleDirectory = temporary.filePath(QStringLiteral("bundle"));
        QVERIFY(QDir().mkpath(bundleDirectory));
        QFile index(QDir(bundleDirectory).filePath(QStringLiteral("index.html")));
        QVERIFY(index.open(QIODevice::WriteOnly));
        QCOMPARE(index.write("deck"), 4);
        index.close();

        Publisher publisher;
        QSignalSpy published(&publisher, &Publisher::published);
        QSignalSpy failures(&publisher, &Publisher::failed);
        publisher.publish(bundleDirectory, QStringLiteral("Deck"),
                          QStringLiteral("verbose"), QStringLiteral("link"));
        QVERIFY(published.wait(5000));
        QCOMPARE(failures.size(), 0);
        QCOMPARE(published.constFirst().at(0).toString(),
                 QStringLiteral("https://example.test/deck"));

        if (oldConfigHome.isNull())
            qunsetenv("XDG_CONFIG_HOME");
        else
            qputenv("XDG_CONFIG_HOME", oldConfigHome);
    }

    void rejectsPrivateAccessForProvidersThatCannotEnforceIt() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const QByteArray oldConfigHome = qgetenv("XDG_CONFIG_HOME");
        QVERIFY(qputenv("XDG_CONFIG_HOME", temporary.path().toUtf8()));

        const QString configDirectory = temporary.filePath(QStringLiteral("omapresent"));
        QVERIFY(QDir().mkpath(configDirectory));
        QFile config(QDir(configDirectory).filePath(QStringLiteral("publish.toml")));
        QVERIFY(config.open(QIODevice::WriteOnly));
        const QByteArray configText(
            "[providers.storage]\n"
            "type = \"s3\"\n"
            "endpoint = \"https://storage.example.test\"\n"
            "bucket = \"decks\"\n"
            "base_url = \"https://decks.example.test\"\n"
            "access_key_id = \"id\"\n"
            "secret_access_key = \"secret\"\n\n"
            "[providers.script]\n"
            "type = \"command\"\n"
            "publish = \"echo https://example.test/deck\"\n");
        QCOMPARE(config.write(configText), configText.size());
        config.close();

        Publisher publisher;
        QSignalSpy failures(&publisher, &Publisher::failed);
        publisher.publish(QStringLiteral("/not-used"), QStringLiteral("deck"),
                          QStringLiteral("storage"), QStringLiteral("password"));
        QCOMPARE(failures.size(), 1);
        QVERIFY(failures.constFirst().constFirst().toString().contains(
            QStringLiteral("cannot enforce")));

        failures.clear();
        publisher.publish(QStringLiteral("/not-used"), QStringLiteral("deck"),
                          QStringLiteral("script"), QStringLiteral("restricted"));
        QCOMPARE(failures.size(), 1);
        QVERIFY(failures.constFirst().constFirst().toString().contains(
            QStringLiteral("cannot enforce")));

        if (oldConfigHome.isNull())
            qunsetenv("XDG_CONFIG_HOME");
        else
            qputenv("XDG_CONFIG_HOME", oldConfigHome);
    }

    void failsClosedWhenExistingConfigIsUnreadable() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const QByteArray oldConfigHome = qgetenv("XDG_CONFIG_HOME");
        QVERIFY(qputenv("XDG_CONFIG_HOME", temporary.path().toUtf8()));

        const QString configDirectory = temporary.filePath(QStringLiteral("omapresent"));
        QVERIFY(QDir().mkpath(configDirectory));
        QVERIFY(QDir().mkpath(QDir(configDirectory).filePath(
            QStringLiteral("publish.toml"))));

        Publisher publisher;
        QSignalSpy failures(&publisher, &Publisher::failed);
        publisher.publish(QStringLiteral("/not-used"), QStringLiteral("deck"),
                          QString(), QStringLiteral("link"));
        QCOMPARE(failures.size(), 1);
        QVERIFY(failures.constFirst().constFirst().toString().contains(
            QStringLiteral("Cannot read the existing publish config")));

        if (oldConfigHome.isNull())
            qunsetenv("XDG_CONFIG_HOME");
        else
            qputenv("XDG_CONFIG_HOME", oldConfigHome);
    }
};

OMAPRESENT_TEST_SUITE(PublisherTest)
#include "tst_publisher.moc"
