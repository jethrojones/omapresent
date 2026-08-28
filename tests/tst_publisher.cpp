#include <QtTest>

#include <QDir>
#include <QFile>
#include <QHash>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QSet>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QUrl>

#include <functional>

#include "testrunner.h"
#include "publisher.h"

namespace {

class ScopedConfigHome {
public:
    explicit ScopedConfigHome(const QString &path)
        : wasSet(qEnvironmentVariableIsSet("XDG_CONFIG_HOME")),
          previous(qgetenv("XDG_CONFIG_HOME")) {
        qputenv("XDG_CONFIG_HOME", path.toUtf8());
    }

    ~ScopedConfigHome() {
        if (wasSet)
            qputenv("XDG_CONFIG_HOME", previous);
        else
            qunsetenv("XDG_CONFIG_HOME");
    }

private:
    bool wasSet;
    QByteArray previous;
};

struct HttpRequest {
    QByteArray method;
    QString target;
    QString path;
    QMap<QByteArray, QByteArray> headers;
    QByteArray body;
};

struct HttpResponse {
    int status = 200;
    QByteArray contentType = QByteArrayLiteral("application/json");
    QByteArray body = QByteArrayLiteral("{}");
};

HttpResponse jsonResponse(const QJsonObject &body, int status = 200) {
    return {status, QByteArrayLiteral("application/json"),
            QJsonDocument(body).toJson(QJsonDocument::Compact)};
}

class LocalHttpServer {
public:
    using Handler = std::function<HttpResponse(const HttpRequest &)>;

    LocalHttpServer() {
        QObject::connect(&server, &QTcpServer::newConnection, &server, [this] {
            while (QTcpSocket *socket = server.nextPendingConnection()) {
                buffers.insert(socket, {});
                QObject::connect(socket, &QTcpSocket::readyRead, socket,
                                 [this, socket] { readRequest(socket); });
                QObject::connect(socket, &QTcpSocket::disconnected, socket,
                                 [this, socket] {
                    buffers.remove(socket);
                    socket->deleteLater();
                });
            }
        });
    }

    bool listen() {
        return server.listen(QHostAddress::LocalHost, 0);
    }

    QString baseUrl() const {
        return QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort());
    }

    QString url(const QString &path) const {
        return baseUrl() + path;
    }

    void check(bool condition, const QString &message) {
        if (!condition && error.isEmpty())
            error = message;
    }

    Handler handler;
    QList<HttpRequest> requests;
    QString error;

private:
    static QByteArray reasonPhrase(int status) {
        if (status == 200)
            return QByteArrayLiteral("OK");
        if (status == 403)
            return QByteArrayLiteral("Forbidden");
        if (status == 503)
            return QByteArrayLiteral("Service Unavailable");
        return QByteArrayLiteral("Error");
    }

    void readRequest(QTcpSocket *socket) {
        QByteArray &buffer = buffers[socket];
        buffer += socket->readAll();
        const qsizetype headerEnd = buffer.indexOf("\r\n\r\n");
        if (headerEnd < 0)
            return;

        const QList<QByteArray> rawLines = buffer.left(headerEnd).split('\n');
        if (rawLines.isEmpty()) {
            check(false, QStringLiteral("Mock received an empty request."));
            return;
        }
        const QList<QByteArray> requestLine = rawLines.constFirst().trimmed().split(' ');
        if (requestLine.size() < 2) {
            check(false, QStringLiteral("Mock received a malformed request line."));
            return;
        }

        QMap<QByteArray, QByteArray> headers;
        for (qsizetype i = 1; i < rawLines.size(); ++i) {
            const QByteArray line = rawLines.at(i).trimmed();
            const qsizetype colon = line.indexOf(':');
            if (colon > 0)
                headers.insert(line.left(colon).trimmed().toLower(),
                               line.mid(colon + 1).trimmed());
        }
        bool lengthOk = false;
        const qsizetype contentLength = headers.value(
            QByteArrayLiteral("content-length"), QByteArrayLiteral("0"))
                                            .toLongLong(&lengthOk);
        if (!lengthOk || contentLength < 0) {
            check(false, QStringLiteral("Mock received an invalid Content-Length."));
            return;
        }
        const qsizetype requestSize = headerEnd + 4 + contentLength;
        if (buffer.size() < requestSize)
            return;

        const QUrl target(QString::fromUtf8(requestLine.at(1)));
        HttpRequest request{requestLine.at(0), QString::fromUtf8(requestLine.at(1)),
                            target.path(), headers,
                            buffer.mid(headerEnd + 4, contentLength)};
        requests.append(request);
        buffer.remove(0, requestSize);

        const HttpResponse response = handler
            ? handler(request)
            : jsonResponse(QJsonObject{{QStringLiteral("message"),
                                        QStringLiteral("No mock route")}}, 503);
        QByteArray wire = QByteArrayLiteral("HTTP/1.1 ")
            + QByteArray::number(response.status) + ' '
            + reasonPhrase(response.status) + QByteArrayLiteral("\r\n")
            + QByteArrayLiteral("Content-Type: ") + response.contentType
            + QByteArrayLiteral("\r\nContent-Length: ")
            + QByteArray::number(response.body.size())
            + QByteArrayLiteral("\r\nConnection: close\r\n\r\n")
            + response.body;
        socket->write(wire);
        socket->disconnectFromHost();
    }

    QTcpServer server;
    QHash<QTcpSocket *, QByteArray> buffers;
};

bool writeBytes(const QString &path, const QByteArray &contents) {
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        return false;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    return file.write(contents) == contents.size();
}

QJsonObject requestJson(const HttpRequest &request) {
    return QJsonDocument::fromJson(request.body).object();
}

QSet<QString> requestedFilePaths(const HttpRequest &request) {
    QSet<QString> paths;
    const QJsonArray files = requestJson(request).value(
        QStringLiteral("files")).toArray();
    for (const QJsonValue &file : files)
        paths.insert(file.toObject().value(QStringLiteral("path")).toString());
    return paths;
}

} // namespace

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

    void hereNowDoesNothingBeforeExplicitPublish() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        ScopedConfigHome configHome(temporary.path());
        LocalHttpServer server;
        QVERIFY(server.listen());

        const QString configPath = temporary.filePath(
            QStringLiteral("omapresent/publish.toml"));
        const QString config = QStringLiteral(
            "default = \"herenow\"\n\n"
            "[providers.herenow]\n"
            "type = \"herenow\"\n"
            "api_base = \"%1\"\n").arg(server.baseUrl());
        QVERIFY(writeBytes(configPath, config.toUtf8()));

        Publisher publisher;
        publisher.providers();
        publisher.reloadConfig();
        QTest::qWait(100);
        QCOMPARE(server.requests.size(), 0);
    }

    void hereNowDomainSetupIsExplicitAndReturnsDnsContract() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        ScopedConfigHome configHome(temporary.path());
        LocalHttpServer server;
        QVERIFY(server.listen());

        const QString config = QStringLiteral(
            "default = \"herenow\"\n\n"
            "[providers.herenow]\n"
            "type = \"herenow\"\n"
            "api_base = \"%1\"\n"
            "api_key = \"domain-test-key\"\n").arg(server.baseUrl());
        QVERIFY(writeBytes(temporary.filePath(QStringLiteral("omapresent/publish.toml")),
                           config.toUtf8()));

        int domainRequests = 0;
        const QJsonObject domainResponse{
            {QStringLiteral("domain"), QStringLiteral("decks.example.test")},
            {QStringLiteral("status"), QStringLiteral("pending")},
            {QStringLiteral("dns_instructions"), QJsonArray{
                QJsonObject{{QStringLiteral("type"), QStringLiteral("CNAME")},
                            {QStringLiteral("host"), QStringLiteral("decks")},
                            {QStringLiteral("value"), QStringLiteral("domains.here.now")}},
                QJsonObject{{QStringLiteral("type"), QStringLiteral("TXT")},
                            {QStringLiteral("host"), QStringLiteral("_here-now.decks")},
                            {QStringLiteral("value"), QStringLiteral("verify-123")}}}}};
        server.handler = [&](const HttpRequest &request) -> HttpResponse {
            server.check(request.headers.value(QByteArrayLiteral("authorization"))
                             == QByteArrayLiteral("Bearer domain-test-key"),
                         QStringLiteral("Domain setup omitted the provider token."));
            if (request.method == QByteArrayLiteral("GET")
                && request.path
                    == QStringLiteral("/api/v1/domains/decks.example.test")) {
                return jsonResponse(domainResponse);
            }
            server.check(request.method == QByteArrayLiteral("POST")
                             && request.path == QStringLiteral("/api/v1/domains"),
                         QStringLiteral("Domain setup used the wrong route."));
            server.check(requestJson(request).value(QStringLiteral("domain")).toString()
                             == QStringLiteral("decks.example.test"),
                         QStringLiteral("Domain setup sent the wrong domain."));
            ++domainRequests;
            if (domainRequests == 2) {
                return jsonResponse(QJsonObject{
                    {QStringLiteral("message"), QStringLiteral("domain already exists")}},
                                    409);
            }
            if (domainRequests == 3) {
                return jsonResponse(QJsonObject{
                    {QStringLiteral("message"),
                     QStringLiteral("domain belongs to another account")}}, 403);
            }
            return jsonResponse(domainResponse);
        };

        Publisher publisher;
        QSignalSpy finished(&publisher, &Publisher::domainSetupFinished);
        QSignalSpy setupFailures(&publisher, &Publisher::domainSetupFailed);
        QSignalSpy publishFailures(&publisher, &Publisher::failed);

        // Reading config and constructing the provider stay offline.
        publisher.reloadConfig();
        QTest::qWait(50);
        QCOMPARE(server.requests.size(), 0);

        QVERIFY(publisher.setupDomain(QStringLiteral("decks.example.test"),
                                      QStringLiteral("herenow")));
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 5000);
        QCOMPARE(setupFailures.size(), 0);
        QCOMPARE(publishFailures.size(), 0);
        QCOMPARE(finished.constFirst().at(0).toString(),
                 QStringLiteral("decks.example.test"));
        QCOMPARE(finished.constFirst().at(1).toString(),
                 QStringLiteral("pending"));
        const QJsonArray records = finished.constFirst().at(2).value<QJsonArray>();
        QCOMPARE(records.size(), 2);
        QCOMPARE(records.at(0).toObject().value(QStringLiteral("value")).toString(),
                 QStringLiteral("domains.here.now"));
        QVERIFY2(server.error.isEmpty(), qPrintable(server.error));

        // Re-adding a domain is useful after a restart. A conflict is followed
        // by the provider's status route so the records are still returned.
        QVERIFY(publisher.setupDomain(QStringLiteral("decks.example.test"),
                                      QStringLiteral("herenow")));
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 2, 5000);
        QCOMPARE(finished.at(1).at(2).value<QJsonArray>().size(), 2);

        QVERIFY(publisher.setupDomain(QStringLiteral("decks.example.test"),
                                      QStringLiteral("herenow")));
        QTRY_COMPARE_WITH_TIMEOUT(setupFailures.size(), 1, 5000);
        QVERIFY(setupFailures.constFirst().constFirst().toString().contains(
            QStringLiteral("domain belongs to another account")));
        QVERIFY(setupFailures.constFirst().constFirst().toString().contains(
            QStringLiteral("HTTP 403")));
        QCOMPARE(domainRequests, 3);
        QCOMPARE(server.requests.size(), 4);
    }

    void hereNowAnonymousPublishUploadsRefreshesAndFinalizesIdempotently() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        ScopedConfigHome configHome(temporary.path());
        LocalHttpServer server;
        QVERIFY(server.listen());

        const QString configPath = temporary.filePath(
            QStringLiteral("omapresent/publish.toml"));
        const QString config = QStringLiteral(
            "default = \"herenow\"\n\n"
            "[providers.herenow]\n"
            "type = \"herenow\"\n"
            "api_base = \"%1\"\n").arg(server.baseUrl());
        QVERIFY(writeBytes(configPath, config.toUtf8()));

        const QString bundle = temporary.filePath(QStringLiteral("bundle"));
        QVERIFY(writeBytes(QDir(bundle).filePath(QStringLiteral("index.html")),
                           QByteArrayLiteral("<h1>Deck</h1>")));
        QVERIFY(writeBytes(QDir(bundle).filePath(QStringLiteral("assets/app.js")),
                           QByteArrayLiteral("console.log('deck');")));

        int createCount = 0;
        int refreshCount = 0;
        int finalizeCount = 0;
        int finalizeEffects = 0;
        int expiredAttempts = 0;
        int signedUploadRequests = 0;
        QSet<QString> finalizedVersions;
        QMap<QString, QList<QByteArray>> successfulUploads;
        bool sentTwentyFourHourLink = false;

        server.handler = [&](const HttpRequest &request) -> HttpResponse {
            if (request.path.startsWith(QStringLiteral("/api/"))) {
                server.check(!request.headers.contains(QByteArrayLiteral("authorization")),
                             QStringLiteral("Anonymous API request carried Authorization."));
            }
            if (request.method == QByteArrayLiteral("POST")
                && request.path == QStringLiteral("/api/v1/publish")) {
                ++createCount;
                server.check(requestedFilePaths(request)
                                 == QSet<QString>{QStringLiteral("index.html"),
                                                  QStringLiteral("assets/app.js")},
                             QStringLiteral("Create request did not list both bundle files."));
                const QString suffix = QString::number(createCount);
                QJsonArray uploads{
                    QJsonObject{{QStringLiteral("path"), QStringLiteral("index.html")},
                                {QStringLiteral("url"),
                                 createCount == 1
                                     ? server.url(QStringLiteral("/upload/index-expired"))
                                           + QStringLiteral("?signature=expired")
                                     : server.url(QStringLiteral("/upload/index-") + suffix)
                                           + QStringLiteral("?signature=index-") + suffix}},
                    QJsonObject{{QStringLiteral("path"), QStringLiteral("assets/app.js")},
                                {QStringLiteral("url"),
                                 server.url(QStringLiteral("/upload/app-") + suffix)
                                     + QStringLiteral("?signature=app-") + suffix}}};
                sentTwentyFourHourLink = true;
                return jsonResponse(QJsonObject{
                    {QStringLiteral("slug"), QStringLiteral("anonymous-deck")},
                    {QStringLiteral("siteUrl"),
                     QStringLiteral("https://anonymous-deck.here.now")},
                    {QStringLiteral("versionId"), QStringLiteral("version-fixed")},
                    {QStringLiteral("claimToken"), QStringLiteral("claim-test-token")},
                    {QStringLiteral("claimUrl"),
                     QStringLiteral("https://here.now/claim/claim-test-token")},
                    {QStringLiteral("expiresAt"),
                     QStringLiteral("2026-08-28T21:12:00Z")},
                    {QStringLiteral("presignedUploads"), uploads}});
            }
            if (request.method == QByteArrayLiteral("PUT")
                && request.path == QStringLiteral("/upload/index-expired")) {
                server.check(!request.headers.contains(QByteArrayLiteral("authorization")),
                             QStringLiteral("Presigned PUT carried Authorization."));
                server.check(request.target.contains(QStringLiteral("?signature=expired")),
                             QStringLiteral("Expired PUT lost its signature query."));
                ++signedUploadRequests;
                ++expiredAttempts;
                return jsonResponse(QJsonObject{
                    {QStringLiteral("message"), QStringLiteral("presigned URL expired")}},
                                    403);
            }
            if (request.method == QByteArrayLiteral("POST")
                && request.path
                    == QStringLiteral("/api/v1/publish/anonymous-deck/uploads/refresh")) {
                ++refreshCount;
                server.check(requestJson(request).value(QStringLiteral("claimToken"))
                                 .toString() == QStringLiteral("claim-test-token"),
                             QStringLiteral("Refresh omitted the anonymous claim token."));
                return jsonResponse(QJsonObject{
                    {QStringLiteral("versionId"), QStringLiteral("version-fixed")},
                    {QStringLiteral("presignedUploads"), QJsonArray{
                        QJsonObject{{QStringLiteral("path"), QStringLiteral("index.html")},
                                    {QStringLiteral("url"),
                                     server.url(QStringLiteral("/upload/index-retry"))
                                         + QStringLiteral("?signature=index-retry")}},
                        QJsonObject{{QStringLiteral("path"),
                                     QStringLiteral("assets/app.js")},
                                    {QStringLiteral("url"),
                                     server.url(QStringLiteral("/upload/app-retry"))
                                         + QStringLiteral("?signature=app-retry")}}}}});
            }
            if (request.method == QByteArrayLiteral("PUT")
                && request.path.startsWith(QStringLiteral("/upload/"))) {
                server.check(!request.headers.contains(QByteArrayLiteral("authorization")),
                             QStringLiteral("Presigned PUT carried Authorization."));
                server.check(request.target.contains(QStringLiteral("?signature=")),
                             QStringLiteral("Presigned PUT lost its signature query."));
                ++signedUploadRequests;
                const QString file = request.path.contains(QStringLiteral("index"))
                    ? QStringLiteral("index.html") : QStringLiteral("assets/app.js");
                successfulUploads[file].append(request.body);
                return jsonResponse({});
            }
            if (request.method == QByteArrayLiteral("POST")
                && request.path
                    == QStringLiteral("/api/v1/publish/anonymous-deck/finalize")) {
                ++finalizeCount;
                const QString version = requestJson(request)
                                            .value(QStringLiteral("versionId")).toString();
                server.check(version == QStringLiteral("version-fixed"),
                             QStringLiteral("Finalize received the wrong versionId."));
                if (!finalizedVersions.contains(version)) {
                    finalizedVersions.insert(version);
                    ++finalizeEffects;
                }
                return jsonResponse(QJsonObject{
                    {QStringLiteral("siteUrl"),
                     QStringLiteral("https://anonymous-deck.here.now")},
                    {QStringLiteral("versionId"), version}});
            }
            server.check(false, QStringLiteral("Unexpected mock route: %1 %2")
                                    .arg(QString::fromLatin1(request.method), request.path));
            return jsonResponse(QJsonObject{
                {QStringLiteral("message"), QStringLiteral("unexpected mock route")}}, 503);
        };

        Publisher publisher;
        QSignalSpy published(&publisher, &Publisher::published);
        QSignalSpy failures(&publisher, &Publisher::failed);
        QSignalSpy claims(&publisher, &Publisher::claimAvailable);

        publisher.publish(bundle, QStringLiteral("Anonymous Deck"),
                          QStringLiteral("herenow"), QStringLiteral("link"));
        QTRY_COMPARE_WITH_TIMEOUT(published.size(), 1, 5000);
        publisher.publish(bundle, QStringLiteral("Anonymous Deck"),
                          QStringLiteral("herenow"), QStringLiteral("link"));
        QTRY_COMPARE_WITH_TIMEOUT(published.size(), 2, 5000);

        QCOMPARE(failures.size(), 0);
        QVERIFY2(server.error.isEmpty(), qPrintable(server.error));
        QCOMPARE(createCount, 2);
        QCOMPARE(refreshCount, 1);
        QCOMPARE(expiredAttempts, 1);
        QVERIFY(signedUploadRequests >= 5);
        QCOMPARE(finalizeCount, 2);
        QCOMPARE(finalizeEffects, 1);
        QVERIFY(sentTwentyFourHourLink);
        QVERIFY(claims.size() >= 1);
        QCOMPARE(claims.constFirst().at(1).toString(),
                 QStringLiteral("claim-test-token"));
        QCOMPARE(published.constFirst().at(0).toString(),
                 QStringLiteral("https://anonymous-deck.here.now"));
        QVERIFY(successfulUploads.value(QStringLiteral("index.html"))
                    .contains(QByteArrayLiteral("<h1>Deck</h1>")));
        QVERIFY(successfulUploads.value(QStringLiteral("assets/app.js"))
                    .contains(QByteArrayLiteral("console.log('deck');")));
    }

    void hereNowAuthenticatedAccess_data() {
        QTest::addColumn<QString>("access");
        QTest::addColumn<QString>("endpoint");
        QTest::addColumn<QString>("apiMode");

        QTest::newRow("link") << QStringLiteral("link")
                               << QStringLiteral("access")
                               << QStringLiteral("anyone_with_link");
        QTest::newRow("public") << QStringLiteral("public")
                                 << QStringLiteral("access")
                                 << QStringLiteral("anyone_with_link");
        QTest::newRow("password") << QStringLiteral("password")
                                   << QStringLiteral("metadata")
                                   << QStringLiteral("password");
        QTest::newRow("restricted") << QStringLiteral("restricted")
                                     << QStringLiteral("access")
                                     << QStringLiteral("restricted");
    }

    void hereNowAuthenticatedAccess() {
        QFETCH(QString, access);
        QFETCH(QString, endpoint);
        QFETCH(QString, apiMode);

        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        ScopedConfigHome configHome(temporary.path());
        LocalHttpServer server;
        QVERIFY(server.listen());

        const QString configPath = temporary.filePath(
            QStringLiteral("omapresent/publish.toml"));
        const QString config = QStringLiteral(
            "default = \"herenow\"\n\n"
            "[providers.herenow]\n"
            "type = \"herenow\"\n"
            "api_base = \"%1\"\n"
            "api_key = \"test-api-key\"\n"
            "password = \"correct horse\"\n"
            "allowed_emails = [\"reader@example.com\"]\n"
            "allowed_domains = [\"example.org\"]\n").arg(server.baseUrl());
        QVERIFY(writeBytes(configPath, config.toUtf8()));

        const QString bundle = temporary.filePath(QStringLiteral("bundle"));
        QVERIFY(writeBytes(QDir(bundle).filePath(QStringLiteral("index.html")),
                           QByteArrayLiteral("AUTH INDEX")));
        QVERIFY(writeBytes(QDir(bundle).filePath(QStringLiteral("slides/data.json")),
                           QByteArrayLiteral("{\"slides\":2}")));

        const bool privateMode = access == QStringLiteral("password")
            || access == QStringLiteral("restricted");
        QStringList sequence;
        QString accessPath;
        QJsonObject accessBody;
        QMap<QString, QByteArray> uploadedBodies;
        QByteArray bootstrapBody;

        auto uploadList = [&](const QString &phase) {
            return QJsonArray{
                QJsonObject{{QStringLiteral("path"), QStringLiteral("index.html")},
                            {QStringLiteral("url"),
                             server.url(QStringLiteral("/upload/") + phase
                                        + QStringLiteral("/index"))
                                 + QStringLiteral("?signature=") + phase
                                 + QStringLiteral("-index")}},
                QJsonObject{{QStringLiteral("path"), QStringLiteral("slides/data.json")},
                            {QStringLiteral("url"),
                             server.url(QStringLiteral("/upload/") + phase
                                        + QStringLiteral("/data"))
                                 + QStringLiteral("?signature=") + phase
                                 + QStringLiteral("-data")}}};
        };

        server.handler = [&](const HttpRequest &request) -> HttpResponse {
            if (request.path.startsWith(QStringLiteral("/api/"))) {
                server.check(request.headers.value(QByteArrayLiteral("authorization"))
                                 == QByteArrayLiteral("Bearer test-api-key"),
                             QStringLiteral("Authenticated API request lacked Bearer auth."));
            } else if (request.path.startsWith(QStringLiteral("/upload/"))) {
                server.check(!request.headers.contains(QByteArrayLiteral("authorization")),
                             QStringLiteral("Presigned upload carried Bearer auth."));
            }

            if (request.method == QByteArrayLiteral("POST")
                && request.path == QStringLiteral("/api/v1/publish")) {
                sequence.append(QStringLiteral("create"));
                const bool bootstrap = requestedFilePaths(request)
                    == QSet<QString>{QStringLiteral("index.html")};
                server.check(bootstrap == privateMode,
                             QStringLiteral("Private bootstrap shape was wrong."));
                return jsonResponse(QJsonObject{
                    {QStringLiteral("slug"), QStringLiteral("auth-deck")},
                    {QStringLiteral("siteUrl"), QStringLiteral("https://auth-deck.here.now")},
                    {QStringLiteral("versionId"),
                     bootstrap ? QStringLiteral("version-bootstrap")
                               : QStringLiteral("version-content")},
                    {QStringLiteral("presignedUploads"),
                     bootstrap
                         ? QJsonArray{QJsonObject{
                               {QStringLiteral("path"), QStringLiteral("index.html")},
                               {QStringLiteral("url"),
                                server.url(QStringLiteral("/upload/bootstrap/index"))
                                    + QStringLiteral("?signature=bootstrap")}}}
                         : uploadList(QStringLiteral("create"))}});
            }
            if (request.method == QByteArrayLiteral("PUT")
                && request.path == QStringLiteral("/api/v1/publish/auth-deck")) {
                sequence.append(QStringLiteral("update"));
                server.check(requestedFilePaths(request)
                                 == QSet<QString>{QStringLiteral("index.html"),
                                                  QStringLiteral("slides/data.json")},
                             QStringLiteral("Authenticated update omitted bundle files."));
                return jsonResponse(QJsonObject{
                    {QStringLiteral("slug"), QStringLiteral("auth-deck")},
                    {QStringLiteral("siteUrl"), QStringLiteral("https://auth-deck.here.now")},
                    {QStringLiteral("versionId"), QStringLiteral("version-content")},
                    {QStringLiteral("presignedUploads"),
                     uploadList(QStringLiteral("update"))}});
            }
            if (request.method == QByteArrayLiteral("PUT")
                && request.path.startsWith(QStringLiteral("/upload/"))) {
                server.check(request.target.contains(QStringLiteral("?signature=")),
                             QStringLiteral("Authenticated PUT lost its signature query."));
                const QString file = request.path.endsWith(QStringLiteral("/data"))
                    ? QStringLiteral("slides/data.json") : QStringLiteral("index.html");
                if (request.path.contains(QStringLiteral("bootstrap")))
                    bootstrapBody = request.body;
                else
                    uploadedBodies.insert(file, request.body);
                return jsonResponse({});
            }
            if (request.method == QByteArrayLiteral("POST")
                && request.path == QStringLiteral("/api/v1/publish/auth-deck/finalize")) {
                sequence.append(QStringLiteral("finalize"));
                const QString version = requestJson(request)
                                            .value(QStringLiteral("versionId")).toString();
                server.check(version == QStringLiteral("version-bootstrap")
                                 || version == QStringLiteral("version-content"),
                             QStringLiteral("Authenticated finalize had wrong versionId."));
                return jsonResponse(QJsonObject{
                    {QStringLiteral("siteUrl"), QStringLiteral("https://auth-deck.here.now")}});
            }
            if (request.method == QByteArrayLiteral("PATCH")
                && (request.path == QStringLiteral("/api/v1/publish/auth-deck/access")
                    || request.path
                        == QStringLiteral("/api/v1/publish/auth-deck/metadata"))) {
                sequence.append(QStringLiteral("access"));
                accessPath = request.path;
                accessBody = requestJson(request);
                return jsonResponse({});
            }
            server.check(false, QStringLiteral("Unexpected authenticated route: %1 %2")
                                    .arg(QString::fromLatin1(request.method), request.path));
            return jsonResponse(QJsonObject{
                {QStringLiteral("message"), QStringLiteral("unexpected mock route")}}, 503);
        };

        Publisher publisher;
        QSignalSpy published(&publisher, &Publisher::published);
        QSignalSpy failures(&publisher, &Publisher::failed);
        publisher.publish(bundle, QStringLiteral("Auth Deck"),
                          QStringLiteral("herenow"), access);
        QTRY_COMPARE_WITH_TIMEOUT(published.size(), 1, 5000);

        QCOMPARE(failures.size(), 0);
        QVERIFY2(server.error.isEmpty(), qPrintable(server.error));
        QCOMPARE(published.constFirst().at(0).toString(),
                 QStringLiteral("https://auth-deck.here.now"));
        QCOMPARE(uploadedBodies.value(QStringLiteral("index.html")),
                 QByteArrayLiteral("AUTH INDEX"));
        QCOMPARE(uploadedBodies.value(QStringLiteral("slides/data.json")),
                 QByteArrayLiteral("{\"slides\":2}"));
        QVERIFY(accessPath.endsWith(u'/' + endpoint));
        if (endpoint == QStringLiteral("metadata")) {
            QCOMPARE(accessBody.value(QStringLiteral("password")).toString(),
                     QStringLiteral("correct horse"));
            QCOMPARE(apiMode, QStringLiteral("password"));
        } else {
            QCOMPARE(accessBody.value(QStringLiteral("mode")).toString(), apiMode);
        }
        if (access == QStringLiteral("restricted")) {
            QCOMPARE(accessBody.value(QStringLiteral("allowedEmails")).toArray().size(), 1);
            QCOMPARE(accessBody.value(QStringLiteral("allowedDomains")).toArray().size(), 1);
        }
        if (privateMode) {
            QVERIFY(bootstrapBody.contains("Preparing presentation"));
            QVERIFY(!bootstrapBody.contains("AUTH INDEX"));
            QVERIFY(sequence.indexOf(QStringLiteral("access"))
                    < sequence.indexOf(QStringLiteral("update")));
            QCOMPARE(sequence.count(QStringLiteral("finalize")), 2);
        } else {
            QVERIFY(sequence.indexOf(QStringLiteral("access"))
                    > sequence.indexOf(QStringLiteral("finalize")));
            QCOMPARE(sequence.count(QStringLiteral("finalize")), 1);
        }
    }

    void hereNowMidUploadFailureIsActionable() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        ScopedConfigHome configHome(temporary.path());
        LocalHttpServer server;
        QVERIFY(server.listen());

        const QString config = QStringLiteral(
            "[providers.herenow]\n"
            "type = \"herenow\"\n"
            "api_base = \"%1\"\n").arg(server.baseUrl());
        QVERIFY(writeBytes(temporary.filePath(QStringLiteral("omapresent/publish.toml")),
                           config.toUtf8()));
        const QString bundle = temporary.filePath(QStringLiteral("bundle"));
        QVERIFY(writeBytes(QDir(bundle).filePath(QStringLiteral("index.html")),
                           QByteArrayLiteral("FAIL ME")));

        int finalizeCount = 0;
        server.handler = [&](const HttpRequest &request) -> HttpResponse {
            if (request.method == QByteArrayLiteral("POST")
                && request.path == QStringLiteral("/api/v1/publish")) {
                return jsonResponse(QJsonObject{
                    {QStringLiteral("slug"), QStringLiteral("failing-deck")},
                    {QStringLiteral("siteUrl"), QStringLiteral("https://failing-deck.here.now")},
                    {QStringLiteral("versionId"), QStringLiteral("version-fail")},
                    {QStringLiteral("claimToken"), QStringLiteral("claim-fail")},
                    {QStringLiteral("presignedUploads"), QJsonArray{QJsonObject{
                         {QStringLiteral("path"), QStringLiteral("index.html")},
                         {QStringLiteral("url"),
                          server.url(QStringLiteral("/upload/fail"))}}}}});
            }
            if (request.method == QByteArrayLiteral("PUT")
                && request.path == QStringLiteral("/upload/fail")) {
                return jsonResponse(QJsonObject{
                    {QStringLiteral("message"),
                     QStringLiteral("storage is temporarily unavailable; retry later")}}, 503);
            }
            if (request.path.endsWith(QStringLiteral("/finalize")))
                ++finalizeCount;
            return jsonResponse({});
        };

        Publisher publisher;
        QSignalSpy published(&publisher, &Publisher::published);
        QSignalSpy failures(&publisher, &Publisher::failed);
        publisher.publish(bundle, QStringLiteral("Failing Deck"),
                          QStringLiteral("herenow"), QStringLiteral("link"));
        QTRY_COMPARE_WITH_TIMEOUT(failures.size(), 1, 5000);

        QCOMPARE(published.size(), 0);
        QCOMPARE(finalizeCount, 0);
        const QString message = failures.constFirst().constFirst().toString();
        QVERIFY(message.contains(QStringLiteral("Upload of index.html")));
        QVERIFY(message.contains(QStringLiteral("storage is temporarily unavailable")));
        QVERIFY(message.contains(QStringLiteral("HTTP 503")));
    }

    void commandProviderPassesEnvironmentAndUsesLastStdoutLine() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        ScopedConfigHome configHome(temporary.path());

        const QString capturePath = temporary.filePath(QStringLiteral("capture.txt"));
        const QString scriptPath = temporary.filePath(QStringLiteral("publish-script"));
        const QByteArray script = QByteArrayLiteral("#!/bin/sh\n")
            + QByteArrayLiteral("printf 'bundle=%s\\nslug=%s\\nbody=%s\\n' ")
            + QByteArrayLiteral("\"$OMAPRESENT_BUNDLE\" \"$OMAPRESENT_SLUG\" ")
            + QByteArrayLiteral("\"$(cat \"$OMAPRESENT_BUNDLE/index.html\")\" > '")
            + capturePath.toUtf8() + QByteArrayLiteral("'\n")
            + QByteArrayLiteral("echo deployment-log\n")
            + QByteArrayLiteral("echo https://command.example.test/live/deck\n");
        QVERIFY(writeBytes(scriptPath, script));
        QVERIFY(QFile::setPermissions(scriptPath,
            QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));

        const QString config = QStringLiteral(
            "default = \"script\"\n\n"
            "[providers.script]\n"
            "type = \"command\"\n"
            "publish = \"%1\"\n").arg(scriptPath);
        QVERIFY(writeBytes(temporary.filePath(QStringLiteral("omapresent/publish.toml")),
                           config.toUtf8()));
        const QString bundle = temporary.filePath(QStringLiteral("bundle"));
        QVERIFY(writeBytes(QDir(bundle).filePath(QStringLiteral("index.html")),
                           QByteArrayLiteral("COMMAND BODY")));

        Publisher publisher;
        QSignalSpy published(&publisher, &Publisher::published);
        QSignalSpy failures(&publisher, &Publisher::failed);
        publisher.publish(bundle, QStringLiteral("Command Deck"),
                          QStringLiteral("script"), QStringLiteral("link"));
        QTRY_COMPARE_WITH_TIMEOUT(published.size(), 1, 5000);

        QCOMPARE(failures.size(), 0);
        QCOMPARE(published.constFirst().at(0).toString(),
                 QStringLiteral("https://command.example.test/live/deck"));
        QFile capture(capturePath);
        QVERIFY(capture.open(QIODevice::ReadOnly));
        const QByteArray captured = capture.readAll();
        QVERIFY(captured.contains("bundle="));
        QVERIFY(captured.contains("slug=command-deck\n"));
        QVERIFY(captured.contains("body=COMMAND BODY\n"));
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
