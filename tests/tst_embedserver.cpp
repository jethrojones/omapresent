#include <QtTest>

#include <QElapsedTimer>
#include <QHostAddress>
#include <QSignalSpy>
#include <QTcpSocket>

#include "embedserver.h"
#include "testrunner.h"

// Suite for src/embedserver.h. Owned by the webbundle agent.
//
// This server exists so a hosted video player has an origin it will accept. It
// is the only listening socket in the application, so most of what is below is
// about what it refuses.

namespace {

// A whole HTTP exchange against a running server, or an empty string when the
// server never answered.
//
// Deliberately not QTcpSocket::waitFor*: the server runs in this same thread,
// and those block on one socket without ever letting its newConnection through.
// Everything here has to go through the shared event loop.
QByteArray request(quint16 port, const QByteArray &raw, int timeoutMs = 3000)
{
    QTcpSocket socket;
    QElapsedTimer clock;
    clock.start();

    socket.connectToHost(QHostAddress::LocalHost, port);
    while (socket.state() == QAbstractSocket::ConnectingState
           || socket.state() == QAbstractSocket::HostLookupState) {
        if (clock.elapsed() > timeoutMs)
            return {};
        QTest::qWait(5);
    }
    if (socket.state() != QAbstractSocket::ConnectedState)
        return {};

    socket.write(raw);
    socket.flush();

    // The server answers with Connection: close, so the disconnect is the end
    // of the response and not a guess about how long to wait.
    QByteArray response;
    while (clock.elapsed() <= timeoutMs) {
        QTest::qWait(5);
        response += socket.readAll();
        if (socket.state() == QAbstractSocket::UnconnectedState)
            break;
    }
    response += socket.readAll();
    return response;
}

QByteArray get(quint16 port, const QByteArray &target, const QByteArray &host)
{
    return request(port, "GET " + target + " HTTP/1.1\r\nHost: " + host + "\r\n\r\n");
}

int statusOf(const QByteArray &response)
{
    const QList<QByteArray> parts = response.left(response.indexOf('\r')).split(' ');
    return parts.size() > 1 ? parts.at(1).toInt() : 0;
}

}  // namespace

class EmbedServerTest : public QObject {
    Q_OBJECT

private:
    QByteArray authority(const EmbedServer &server) const
    {
        return "127.0.0.1:" + QByteArray::number(server.port());
    }

    QByteArray embedPath(const EmbedServer &server) const
    {
        return "/" + server.token().toLatin1() + "/embed.html";
    }

private slots:
    // --- what it does -----------------------------------------------------

    void nothingListensUntilTheBaseIsAskedFor()
    {
        // SEC-002: opening a deck must reach nothing. The renderer only asks
        // for the base when the reader clicks Play, so construction alone must
        // not open a socket.
        EmbedServer server;
        QVERIFY(!server.isListening());
        QCOMPARE(server.port(), quint16(0));

        QVERIFY(!server.baseUrl().isEmpty());
        QVERIFY(server.isListening());
    }

    void bindsToLoopbackOnAnEphemeralPort()
    {
        EmbedServer server;
        QVERIFY(!server.baseUrl().isEmpty());

        QVERIFY(server.port() != 0);
        // Ephemeral, not a number anyone can guess or collide with.
        EmbedServer second;
        QVERIFY(!second.baseUrl().isEmpty());
        QVERIFY(second.port() != server.port());

        // Reachable on loopback...
        QCOMPARE(statusOf(get(server.port(), embedPath(server), authority(server))), 200);

        // ...and bound to loopback and nothing else, so no other interface on
        // this machine — and nothing off it — has an address to reach.
        QCOMPARE(server.address(), QHostAddress(QHostAddress::LocalHost));
        QVERIFY(server.address().isLoopback());
    }

    void servesTheShimAndNothingElse()
    {
        EmbedServer server;
        QVERIFY(!server.baseUrl().isEmpty());

        const QByteArray response = get(server.port(), embedPath(server), authority(server));
        QCOMPARE(statusOf(response), 200);
        QVERIFY(response.contains("text/html"));
        QVERIFY(response.contains("Omapresent embed"));
        QVERIFY(response.contains("no-store"));
        QVERIFY(response.contains("nosniff"));

        // The base is the shim's own directory, so the renderer resolves
        // "embed.html" against it and gets exactly this.
        QVERIFY(server.baseUrl().endsWith(server.token() + QStringLiteral("/")));
        QVERIFY(server.baseUrl().startsWith(QStringLiteral("http://127.0.0.1:")));
    }

    void aQueryStringReachesTheShim()
    {
        EmbedServer server;
        QVERIFY(!server.baseUrl().isEmpty());
        // The video id travels as a query; it must not affect routing.
        QCOMPARE(statusOf(get(server.port(), embedPath(server) + "?v=aqz-KE-bpKQ",
                              authority(server))),
                 200);
    }

    // --- what it refuses --------------------------------------------------

    void refusesEveryOtherResource()
    {
        EmbedServer server;
        QVERIFY(!server.baseUrl().isEmpty());
        const QByteArray token = server.token().toLatin1();

        // The renderer, the deck stylesheet and the vendored libraries are all
        // compiled into the same resource tree. None of them is served.
        const QList<QByteArray> targets{
            "/" + token + "/render.js",
            "/" + token + "/deck.css",
            "/" + token + "/render.html",
            "/" + token + "/vendor/katex.mjs",
            "/" + token + "/",
            "/" + token,
            "/embed.html",
            "/",
        };
        for (const QByteArray &target : targets) {
            QCOMPARE(statusOf(get(server.port(), target, authority(server))), 404);
        }
    }

    void refusesAWrongToken()
    {
        EmbedServer server;
        QVERIFY(!server.baseUrl().isEmpty());

        QCOMPARE(statusOf(get(server.port(), "/deadbeef/embed.html", authority(server))), 404);
        QCOMPARE(statusOf(get(server.port(), "//embed.html", authority(server))), 404);
        // A token that is a prefix of the real one is still the wrong token.
        const QByteArray shortened = server.token().toLatin1().left(8);
        QCOMPARE(statusOf(get(server.port(), "/" + shortened + "/embed.html",
                              authority(server))),
                 404);
    }

    void refusesAnotherHost()
    {
        EmbedServer server;
        QVERIFY(!server.baseUrl().isEmpty());

        // A page that resolved some name to 127.0.0.1 to reach us — the classic
        // rebinding move — arrives with its own Host header.
        QCOMPARE(statusOf(get(server.port(), embedPath(server), "evil.example.com")), 404);
        QCOMPARE(statusOf(get(server.port(), embedPath(server), "localhost:1")), 404);
        QCOMPARE(statusOf(request(server.port(),
                                  "GET " + embedPath(server) + " HTTP/1.1\r\n\r\n")),
                 404);
    }

    void acceptsAMixedCaseHostAndRefusesADuplicate()
    {
        EmbedServer server;
        QVERIFY(!server.baseUrl().isEmpty());
        const QByteArray path = embedPath(server);
        const QByteArray host = authority(server);

        QCOMPARE(statusOf(request(server.port(),
                                  "GET " + path + " HTTP/1.1\r\nHOST: " + host + "\r\n\r\n")),
                 200);
        QCOMPARE(statusOf(request(server.port(),
                                  "GET " + path + " HTTP/1.1\r\nHost: " + host
                                      + "\r\nHost: evil.example.com\r\n\r\n")),
                 404);
        // Whitespace before the colon makes it something other than a Host.
        QCOMPARE(statusOf(request(server.port(),
                                  "GET " + path + " HTTP/1.1\r\nHost : " + host + "\r\n\r\n")),
                 404);
    }

    void refusesEveryMethodButGet()
    {
        EmbedServer server;
        QVERIFY(!server.baseUrl().isEmpty());
        const QByteArray host = authority(server);

        for (const QByteArray &method : {QByteArray("POST"), QByteArray("PUT"),
                                         QByteArray("DELETE"), QByteArray("OPTIONS"),
                                         QByteArray("HEAD"), QByteArray("TRACE")}) {
            const QByteArray response = request(
                server.port(), method + " " + embedPath(server) + " HTTP/1.1\r\nHost: " + host
                    + "\r\nContent-Length: 0\r\n\r\n");
            QCOMPARE(statusOf(response), 404);
        }
    }

    void refusesTraversalInEveryEncoding()
    {
        EmbedServer server;
        QVERIFY(!server.baseUrl().isEmpty());
        const QByteArray token = server.token().toLatin1();
        const QByteArray host = authority(server);

        const QList<QByteArray> attempts{
            "/" + token + "/../../../../etc/passwd",
            "/" + token + "/%2e%2e%2f%2e%2e%2fetc%2fpasswd",
            "/" + token + "/..%252f..%252fetc%252fpasswd",
            "/" + token + "/%2e%2e/embed.html",
            "/" + token + "/..\\..\\etc\\passwd",
            "/" + token + "/embed.html%00.png",
            "/../" + token + "/embed.html",
            "/" + token + "/subdir/embed.html",
        };
        for (const QByteArray &attempt : attempts) {
            QCOMPARE(statusOf(get(server.port(), attempt, host)), 404);
        }
    }

    void refusesAnOversizedRequest()
    {
        EmbedServer server;
        QVERIFY(!server.baseUrl().isEmpty());

        QByteArray raw = "GET " + embedPath(server) + " HTTP/1.1\r\nHost: " + authority(server)
            + "\r\n";
        for (int i = 0; i < 400; ++i)
            raw += "X-Filler-" + QByteArray::number(i) + ": " + QByteArray(64, 'a') + "\r\n";
        raw += "\r\n";

        QCOMPARE(statusOf(request(server.port(), raw)), 431);
    }

    // --- the pure routing helper -----------------------------------------

    void resourceForRequestAcceptsOnlyTheShim()
    {
        const QString token = QStringLiteral("0123456789abcdef");
        const QString authority = QStringLiteral("127.0.0.1:4321");
        const QString shim = QStringLiteral(":/renderer/embed.html");
        const auto head = [](const QByteArray &line, const QByteArray &headers) {
            return line + "\r\n" + headers;
        };
        const QByteArray hostHeader = "Host: 127.0.0.1:4321\r\n";
        const QByteArray good = "GET /0123456789abcdef/embed.html HTTP/1.1";

        QCOMPARE(EmbedServer::resourceForRequest(head(good, hostHeader), token, authority), shim);
        QCOMPARE(EmbedServer::resourceForRequest(
                     head("GET /0123456789abcdef/embed.html?v=abc HTTP/1.0", hostHeader),
                     token, authority),
                 shim);

        // Field names are case-insensitive; the value still has to match.
        QCOMPARE(EmbedServer::resourceForRequest(head(good, "HOST: 127.0.0.1:4321\r\n"),
                                                 token, authority),
                 shim);
        QCOMPARE(EmbedServer::resourceForRequest(head(good, "hOsT:   127.0.0.1:4321  \r\n"),
                                                 token, authority),
                 shim);
        // And other fields around it change nothing.
        QCOMPARE(EmbedServer::resourceForRequest(
                     head(good, "Accept: */*\r\nHost: 127.0.0.1:4321\r\nX-Thing: 1\r\n"),
                     token, authority),
                 shim);

        // A field name is the bytes before the colon, and HTTP allows no space
        // between them. "Host :" is not a Host header, and reading it as one is
        // how a request gets a second opinion past a stricter proxy in front.
        for (const QByteArray &malformed : {QByteArray("Host : 127.0.0.1:4321\r\n"),
                                            QByteArray("Host\t: 127.0.0.1:4321\r\n"),
                                            QByteArray(" Host: 127.0.0.1:4321\r\n"),
                                            QByteArray("\tHost: 127.0.0.1:4321\r\n")}) {
            QVERIFY2(EmbedServer::resourceForRequest(head(good, malformed), token, authority)
                         .isEmpty(),
                     malformed.constData());
        }
        // A well-formed one alongside a malformed one is still exactly one Host.
        QCOMPARE(EmbedServer::resourceForRequest(
                     head(good, "Host : evil.example.com\r\nHost: 127.0.0.1:4321\r\n"),
                     token, authority),
                 shim);

        // Two Host headers is a request with two opinions about where it is
        // going, and this server does not pick one.
        QVERIFY(EmbedServer::resourceForRequest(
                    head(good, "Host: 127.0.0.1:4321\r\nHost: evil.example.com\r\n"),
                    token, authority)
                    .isEmpty());
        QVERIFY(EmbedServer::resourceForRequest(
                    head(good, "Host: evil.example.com\r\nHost: 127.0.0.1:4321\r\n"),
                    token, authority)
                    .isEmpty());
        // None at all is no better.
        QVERIFY(EmbedServer::resourceForRequest(head(good, QByteArray()), token, authority)
                    .isEmpty());

        // Every refusal, without needing a socket to prove it.
        const QList<QByteArray> refused{
            "POST /0123456789abcdef/embed.html HTTP/1.1",
            "GET /0123456789abcdef/embed.html",
            "GET /0123456789abcdef/embed.html HTTP/1.1 extra",
            "GET /0123456789abcdef/embed.html HTTP/1.2",
            "GET /0123456789abcdef/embed.html HTTP/1.",
            "GET /0123456789abcdef/embed.html HTTP/2",
            "GET 0123456789abcdef/embed.html HTTP/1.1",
            "GET /0123456789abcdef/render.js HTTP/1.1",
            "GET /wrongtoken/embed.html HTTP/1.1",
            "GET /0123456789abcdef/../embed.html HTTP/1.1",
            "GET /0123456789abcdef/%2e%2e%2fembed.html HTTP/1.1",
            "GET /0123456789abcdef/a/embed.html HTTP/1.1",
            "GET /embed.html HTTP/1.1",
        };
        for (const QByteArray &line : refused) {
            QVERIFY2(EmbedServer::resourceForRequest(head(line, hostHeader), token, authority)
                         .isEmpty(),
                     line.constData());
        }

        // A wrong Host is refused even for the one good request line.
        QVERIFY(EmbedServer::resourceForRequest(head(good, "Host: evil.example.com\r\n"),
                                                token, authority)
                    .isEmpty());
        // An empty token never matches, so a server that failed to make one
        // cannot be reached by asking for nothing.
        QVERIFY(EmbedServer::resourceForRequest(head("GET //embed.html HTTP/1.1", hostHeader),
                                                QString(), authority)
                    .isEmpty());
    }

    void decodingRefusesToInventPathSeparators()
    {
        bool ok = false;
        QCOMPARE(EmbedServer::decodedPathSegment(QStringLiteral("embed.html"), &ok),
                 QStringLiteral("embed.html"));
        QVERIFY(ok);

        // %2f, %5c and %00 are the three ways a decoded byte tries to become a
        // new path segment or end one early.
        for (const QString &hostile : {QStringLiteral("a%2fb"), QStringLiteral("a%5cb"),
                                       QStringLiteral("a%00b"), QStringLiteral("a%"),
                                       QStringLiteral("a%zz"), QStringLiteral("a%252f")}) {
            EmbedServer::decodedPathSegment(hostile, &ok);
            QVERIFY2(!ok, qPrintable(hostile));
        }
    }

    // --- the origin the whole thing exists for ----------------------------

    void theShimGivesThePlayerAnHttpOrigin()
    {
        EmbedServer server;
        const QString base = server.baseUrl();
        QVERIFY(!base.isEmpty());

        // Error 153 is what a hosted player answers when the page embedding it
        // has no origin to send. `qrc:` and `file:` have none; this does.
        const QUrl url(base);
        QCOMPARE(url.scheme(), QStringLiteral("http"));
        QCOMPARE(url.host(), QStringLiteral("127.0.0.1"));
        QVERIFY(url.port() > 0);

        // And the shim it serves builds the player from an id, so nothing a
        // deck author writes becomes part of a URL on this side.
        const QByteArray body = get(server.port(), embedPath(server), authority(server));
        QVERIFY(body.contains("A-Za-z0-9_-"));
        QVERIFY(body.contains("https://www.youtube.com"));
        QVERIFY(body.contains("onError"));
    }
};

OMAPRESENT_TEST_SUITE(EmbedServerTest)
#include "tst_embedserver.moc"
