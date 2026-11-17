#include <QtTest>

#include <QElapsedTimer>
#include <QGuiApplication>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQuickWindow>
#include <QTcpSocket>
#include <QTemporaryDir>

#include "presentation.h"
#include "remoteserver.h"
#include "settings.h"
#include "testrunner.h"

// Suite for src/remoteserver.h: the phone remote.
//
// It is a listening socket that can move the talk, so most of what is below is
// about when it listens, where, and what it refuses.

namespace {

// One HTTP exchange through the shared event loop (see tst_embedserver.cpp for
// why this cannot use QTcpSocket::waitFor*).
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

QByteArray send(quint16 port, const QByteArray &method, const QByteArray &target)
{
    return request(port, method + ' ' + target + " HTTP/1.1\r\nHost: remote\r\n\r\n");
}

QByteArray body(const QByteArray &response)
{
    const qsizetype split = response.indexOf("\r\n\r\n");
    return split < 0 ? QByteArray() : response.mid(split + 4);
}

QJsonObject json(const QByteArray &response)
{
    return QJsonDocument::fromJson(body(response)).object();
}

QJsonObject deck(const QStringList &recallKeys)
{
    QJsonArray slides;
    for (int i = 0; i < recallKeys.size(); ++i) {
        slides.append(QJsonObject{{QStringLiteral("index"), i},
                                  {QStringLiteral("markdown"), QStringLiteral("# Slide %1").arg(i + 1)},
                                  {QStringLiteral("recallKey"), recallKeys.at(i)},
                                  {QStringLiteral("skip"), false}});
    }
    return QJsonObject{{QStringLiteral("mode"), QStringLiteral("present")},
                       {QStringLiteral("slides"), slides}};
}

RemoteServer::Request parse(const QByteArray &line, const QString &token)
{
    return RemoteServer::parseRequest(line + "\r\nHost: x", token);
}

const QString kToken = QStringLiteral("0123456789abcdef0123456789abcdef");

}  // namespace

class RemoteServerTest : public QObject {
    Q_OBJECT

private:
    // A running talk with windows that are plain QQuickWindows, as
    // tst_presentation.cpp does, so nothing needs WebEngine.
    static void startTalk(Presentation &presentation, const QStringList &recallKeys)
    {
        presentation.setEnvironmentPreferences(false, false);
        presentation.setDeck(deck(recallKeys));
        presentation.setWindowFactoryForTesting([](const QString &) {
            return new QQuickWindow;
        });
        presentation.start(0);
    }

private slots:
    // --- Which address ------------------------------------------------------

    void tailnetAddressesAreRecognised()
    {
        QVERIFY(RemoteServer::isTailnetAddress(QHostAddress(QStringLiteral("100.64.0.1"))));
        QVERIFY(RemoteServer::isTailnetAddress(QHostAddress(QStringLiteral("100.104.184.128"))));
        QVERIFY(RemoteServer::isTailnetAddress(QHostAddress(QStringLiteral("100.127.255.254"))));
        QVERIFY(RemoteServer::isTailnetAddress(QHostAddress(QStringLiteral("fd7a:115c:a1e0::1"))));
        // An IPv4-mapped tailnet peer is still a tailnet peer.
        QVERIFY(RemoteServer::isTailnetAddress(QHostAddress(QStringLiteral("::ffff:100.79.225.54"))));
    }

    void everythingElseIsNotTheTailnet()
    {
        for (const char *text : {"127.0.0.1", "192.168.0.20", "10.0.0.105", "100.63.255.255",
                                 "100.128.0.0", "8.8.8.8", "0.0.0.0", "::1", "fe80::1",
                                 "fd7a:115c:a1e1::1"}) {
            QVERIFY2(!RemoteServer::isTailnetAddress(QHostAddress(QString::fromLatin1(text))), text);
        }
        QVERIFY(!RemoteServer::isTailnetAddress(QHostAddress()));
    }

    void picksTheIpv4TailnetAddressAndNothingElse()
    {
        const QList<QHostAddress> addresses{
            QHostAddress(QStringLiteral("127.0.0.1")),
            QHostAddress(QStringLiteral("192.168.0.50")),
            QHostAddress(QStringLiteral("fd7a:115c:a1e0::9")),
            QHostAddress(QStringLiteral("100.104.184.128"))};
        QCOMPARE(RemoteServer::tailnetAddress(addresses),
                 QHostAddress(QStringLiteral("100.104.184.128")));
        QVERIFY(RemoteServer::tailnetAddress({QHostAddress(QStringLiteral("192.168.0.50")),
                                              QHostAddress(QStringLiteral("127.0.0.1"))})
                    .isNull());
        QVERIFY(RemoteServer::tailnetAddress({}).isNull());
    }

    // --- What a request may ask for ------------------------------------------

    void parsesThePageTheStateAndTheActions()
    {
        QCOMPARE(parse("GET /" + kToken.toLatin1() + "/ HTTP/1.1", kToken).kind,
                 RemoteServer::Request::Page);
        QCOMPARE(parse("GET /" + kToken.toLatin1() + "/state HTTP/1.1", kToken).kind,
                 RemoteServer::Request::State);
        QCOMPARE(parse("GET /" + kToken.toLatin1() + "/state?x=1 HTTP/1.0", kToken).kind,
                 RemoteServer::Request::State);
        for (const char *action : {"next", "prev", "black", "white", "clear"}) {
            const auto parsed = parse("POST /" + kToken.toLatin1() + "/" + action + " HTTP/1.1", kToken);
            QCOMPARE(parsed.kind, RemoteServer::Request::Action);
            QCOMPARE(parsed.action, QString::fromLatin1(action));
        }
        const auto recall = parse("POST /" + kToken.toLatin1() + "/recall/Q HTTP/1.1", kToken);
        QCOMPARE(recall.kind, RemoteServer::Request::Action);
        QCOMPARE(recall.action, QStringLiteral("recall"));
        QCOMPARE(recall.key, QStringLiteral("q"));
    }

    void parsesTheSlideListAndTheJump()
    {
        const QByteArray t = kToken.toLatin1();
        QCOMPARE(parse("GET /" + t + "/slides HTTP/1.1", kToken).kind,
                 RemoteServer::Request::Slides);
        const auto jump = parse("POST /" + t + "/goto/27 HTTP/1.1", kToken);
        QCOMPARE(jump.kind, RemoteServer::Request::Action);
        QCOMPARE(jump.action, QStringLiteral("goto"));
        QCOMPARE(jump.number, 27);
        for (const char *bad : {"goto/0", "goto/-1", "goto/01", "goto/12345", "goto/1a",
                                "goto/", "goto", "goto/1/2", "goto/%31", "slides"}) {
            QVERIFY2(parse("POST /" + t + "/" + bad + " HTTP/1.1", kToken).kind
                         == RemoteServer::Request::Invalid, bad);
        }
        QCOMPARE(parse("GET /" + t + "/goto/2 HTTP/1.1", kToken).kind,
                 RemoteServer::Request::Invalid);
    }

    void namesASlideByItsHeading()
    {
        QCOMPARE(RemoteServer::headingOf(QStringLiteral("# Step 1\n## Have your AI build your vault\n\nNotes.")),
                 QStringLiteral("Step 1"));
        QCOMPARE(RemoteServer::headingOf(QStringLiteral("// a comment\n\n## **Bold** and `code`")),
                 QStringLiteral("Bold and code"));
        QCOMPARE(RemoteServer::headingOf(QStringLiteral("Just a note line.\n\nAnother.")),
                 QStringLiteral("Just a note line."));
        QCOMPARE(RemoteServer::headingOf(QStringLiteral("- first bullet\n- second")),
                 QStringLiteral("first bullet"));
        QCOMPARE(RemoteServer::headingOf(QStringLiteral("See [the page](https://example.com/x)")),
                 QStringLiteral("See the page"));
        QCOMPARE(RemoteServer::headingOf(QStringLiteral("![[photo.png]]")), QStringLiteral("(image)"));
        // A `#` inside a code fence is a comment in someone's shell, not a heading.
        QCOMPARE(RemoteServer::headingOf(QStringLiteral("```\n# not this\n```\n# This")),
                 QStringLiteral("This"));
        // "#hashtag" is not a heading; an empty "#" is skipped.
        QCOMPARE(RemoteServer::headingOf(QStringLiteral("#hashtag first\n#\n## Real")),
                 QStringLiteral("Real"));
        QCOMPARE(RemoteServer::headingOf(QString()), QString());
        const QString longOne = RemoteServer::headingOf(QStringLiteral("# ") + QString(300, u'x'));
        QCOMPARE(longOne.size(), 80);
    }

    void listsSlidesAndJumpsOverHttp()
    {
        if (QGuiApplication::screens().isEmpty())
            QSKIP("No screen is available for a presentation window.");
        QTemporaryDir sandbox;
        Presentation presentation;
        RemoteServer remote(&presentation);
        remote.setTokenPath(sandbox.filePath(QStringLiteral("remote-token")));
        remote.setAddressForTesting(QHostAddress::LocalHost, 0);
        remote.setEnabled(true);

        // Four slides; the third is `--- {q, skip}`: poppable, out of the flow.
        QJsonArray slides;
        const QStringList keys{QString(), QString(), QStringLiteral("q"), QString()};
        int flow = 0;
        for (int i = 0; i < keys.size(); ++i) {
            const bool skip = i == 2;
            slides.append(QJsonObject{{QStringLiteral("index"), skip ? -1 : flow++},
                                      {QStringLiteral("markdown"), QStringLiteral("# Slide %1").arg(i + 1)},
                                      {QStringLiteral("recallKey"), keys.at(i)},
                                      {QStringLiteral("skip"), skip}});
        }
        presentation.setEnvironmentPreferences(false, false);
        presentation.setDeck(QJsonObject{{QStringLiteral("mode"), QStringLiteral("present")},
                                         {QStringLiteral("slides"), slides}});
        presentation.setWindowFactoryForTesting([](const QString &) { return new QQuickWindow; });
        presentation.start(0);
        const quint16 port = remote.port();
        const QByteArray base = '/' + remote.token().toLatin1() + '/';

        const QJsonObject list = json(send(port, "GET", base + "slides"));
        const QJsonArray flowSlides = list.value(QStringLiteral("slides")).toArray();
        QCOMPARE(flowSlides.size(), 3);
        QCOMPARE(flowSlides.at(2).toObject().value(QStringLiteral("number")).toInt(), 3);
        // Numbered as the audience counts them: the skipped slide is not "3".
        QCOMPARE(flowSlides.at(2).toObject().value(QStringLiteral("heading")).toString(),
                 QStringLiteral("Slide 4"));
        const QJsonArray recalls = list.value(QStringLiteral("recalls")).toArray();
        QCOMPARE(recalls.size(), 1);
        QCOMPARE(recalls.at(0).toObject().value(QStringLiteral("key")).toString(), QStringLiteral("q"));
        QCOMPARE(recalls.at(0).toObject().value(QStringLiteral("heading")).toString(),
                 QStringLiteral("Slide 3"));

        QJsonObject state = json(send(port, "POST", base + "goto/3"));
        QCOMPARE(state.value(QStringLiteral("slide")).toInt(), 3);
        QCOMPARE(presentation.slideIndex(), 2);
        send(port, "POST", base + "goto/1");
        QCOMPARE(presentation.slideIndex(), 0);
        // Past the end is refused, not clamped: the list never offers it.
        send(port, "POST", base + "goto/9");
        QCOMPARE(presentation.slideIndex(), 0);
        // A jump takes a recall overlay down with it.
        send(port, "POST", base + "recall/q");
        QCOMPARE(presentation.recall(), QStringLiteral("q"));
        send(port, "POST", base + "goto/2");
        QCOMPARE(presentation.recall(), QString());
        QCOMPARE(presentation.slideIndex(), 1);

        presentation.stop();
        // With no talk running the list is empty rather than stale.
        QCOMPARE(remote.slides().value(QStringLiteral("slides")).toArray().size(), 0);
    }

    void aGetNeverMovesTheTalk()
    {
        // A link preview, a prefetch or a reload must not advance a slide.
        for (const char *action : {"next", "prev", "black", "white", "clear", "recall/q"}) {
            QCOMPARE(parse("GET /" + kToken.toLatin1() + "/" + action + " HTTP/1.1", kToken).kind,
                     RemoteServer::Request::Invalid);
        }
    }

    void refusesAnythingWithoutTheToken()
    {
        const QByteArray t = kToken.toLatin1();
        const QList<QByteArray> bad{
            "GET / HTTP/1.1",
            "GET /state HTTP/1.1",
            "POST /next HTTP/1.1",
            "GET /wrong/ HTTP/1.1",
            "GET /" + t.left(31) + "/ HTTP/1.1",
            "GET /" + t + "x/ HTTP/1.1",
            "GET /" + t + " HTTP/1.1",                 // no trailing slash: one segment
            "GET /" + t + "/nothing HTTP/1.1",
            "GET /" + t + "/../state HTTP/1.1",
            "GET /" + t + "/%73tate HTTP/1.1",
            "GET /" + t + "/state/extra HTTP/1.1",
            "POST /" + t + "/ HTTP/1.1",
            "POST /" + t + "/state HTTP/1.1",
            "POST /" + t + "/recall/ HTTP/1.1",
            "POST /" + t + "/recall/qq HTTP/1.1",
            "POST /" + t + "/recall/- HTTP/1.1",
            "POST /" + t + "/recall/q/x HTTP/1.1",
            "POST /" + t + "/stop HTTP/1.1",
            "PUT /" + t + "/next HTTP/1.1",
            "DELETE /" + t + "/ HTTP/1.1",
            "GET /" + t + "/ HTTP/2",
            "GET /" + t + "/",
            "",
            "GET " + t + "/ HTTP/1.1"};
        for (const QByteArray &line : bad)
            QVERIFY2(parse(line, kToken).kind == RemoteServer::Request::Invalid, line.constData());
        // And an empty token matches nothing, not everything.
        QCOMPARE(parse("GET // HTTP/1.1", QString()).kind, RemoteServer::Request::Invalid);
    }

    // --- When it listens -----------------------------------------------------

    void offByDefault()
    {
        QCOMPARE(Settings::defaults()
                     .value(QStringLiteral("presentation")).toObject()
                     .value(QStringLiteral("remote")).toBool(true),
                 false);
    }

    void doesNotListenUntilEnabledAndPresenting()
    {
        if (QGuiApplication::screens().isEmpty())
            QSKIP("No screen is available for a presentation window.");
        QTemporaryDir sandbox;
        Presentation presentation;
        RemoteServer remote(&presentation);
        remote.setTokenPath(sandbox.filePath(QStringLiteral("remote-token")));
        remote.setAddressForTesting(QHostAddress::LocalHost, 0);

        QVERIFY(!remote.isListening());
        remote.setEnabled(true);
        QVERIFY2(!remote.isListening(), "enabled, but no talk is running");
        QVERIFY(remote.url().isEmpty());

        QVERIFY(presentation.remoteUrl().isEmpty());
        startTalk(presentation, {QString(), QString()});
        QVERIFY(remote.isListening());
        QVERIFY(remote.address().isLoopback());   // the test address, never Any
        QVERIFY(remote.url().endsWith(u'/' + remote.token() + u'/'));

        // The presenter window draws its QR code from these two.
        QCOMPARE(presentation.remoteUrl(), remote.url());
        QVERIFY2(!presentation.remoteInUse(), "no phone has asked for anything yet");
        // A wrong token is not a phone: the code stays up.
        send(remote.port(), "GET", "/wrong/state");
        QVERIFY(!presentation.remoteInUse());
        send(remote.port(), "GET", '/' + remote.token().toLatin1() + "/state");
        QVERIFY2(presentation.remoteInUse(), "a phone has the address now");

        presentation.stop();
        QVERIFY2(!remote.isListening(), "the talk ended, so the socket must close");
        QVERIFY(presentation.remoteUrl().isEmpty());
        QVERIFY(!presentation.remoteInUse());
    }

    void staysClosedWhenTheSettingIsOff()
    {
        if (QGuiApplication::screens().isEmpty())
            QSKIP("No screen is available for a presentation window.");
        QTemporaryDir sandbox;
        Presentation presentation;
        RemoteServer remote(&presentation);
        remote.setTokenPath(sandbox.filePath(QStringLiteral("remote-token")));
        remote.setAddressForTesting(QHostAddress::LocalHost, 0);

        startTalk(presentation, {QString(), QString()});
        QVERIFY(!remote.isListening());

        remote.setEnabled(true);
        QVERIFY(remote.isListening());
        remote.setEnabled(false);
        QVERIFY(!remote.isListening());
        presentation.stop();
    }

    void neverListensWithoutATailnetAddressToBindTo()
    {
        if (QGuiApplication::screens().isEmpty())
            QSKIP("No screen is available for a presentation window.");
        QTemporaryDir sandbox;
        Presentation presentation;
        RemoteServer remote(&presentation);
        remote.setTokenPath(sandbox.filePath(QStringLiteral("remote-token")));
        // The test seam with a null address stands in for "no Tailscale".
        remote.setAddressForTesting(QHostAddress(), 0);
        remote.setEnabled(true);
        startTalk(presentation, {QString()});
        QVERIFY(!remote.isListening());
        presentation.stop();
    }

    // --- The token -----------------------------------------------------------

    void theTokenIsKeptPrivateAndReused()
    {
        if (QGuiApplication::screens().isEmpty())
            QSKIP("No screen is available for a presentation window.");
        QTemporaryDir sandbox;
        const QString path = sandbox.filePath(QStringLiteral("nested/remote-token"));
        QString first;
        {
            Presentation presentation;
            RemoteServer remote(&presentation);
            remote.setTokenPath(path);
            remote.setAddressForTesting(QHostAddress::LocalHost, 0);
            remote.setEnabled(true);
            startTalk(presentation, {QString()});
            first = remote.token();
            presentation.stop();
        }
        QCOMPARE(first.size(), 32);
        QVERIFY(QFile::exists(path));
        const QFileDevice::Permissions permissions = QFile::permissions(path);
        QVERIFY(!(permissions & (QFileDevice::ReadGroup | QFileDevice::ReadOther
                                 | QFileDevice::WriteGroup | QFileDevice::WriteOther)));
        {
            Presentation presentation;
            RemoteServer remote(&presentation);
            remote.setTokenPath(path);
            remote.setAddressForTesting(QHostAddress::LocalHost, 0);
            remote.setEnabled(true);
            startTalk(presentation, {QString()});
            QCOMPARE(remote.token(), first);
            presentation.stop();
        }
    }

    void aDamagedTokenFileIsReplaced()
    {
        if (QGuiApplication::screens().isEmpty())
            QSKIP("No screen is available for a presentation window.");
        QTemporaryDir sandbox;
        const QString path = sandbox.filePath(QStringLiteral("remote-token"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("short\n");
        file.close();

        Presentation presentation;
        RemoteServer remote(&presentation);
        remote.setTokenPath(path);
        remote.setAddressForTesting(QHostAddress::LocalHost, 0);
        remote.setEnabled(true);
        startTalk(presentation, {QString()});
        QCOMPARE(remote.token().size(), 32);
        QVERIFY(remote.token() != QStringLiteral("short"));
        presentation.stop();
    }

    // --- Over the wire -------------------------------------------------------

    void drivesTheTalkOverHttp()
    {
        if (QGuiApplication::screens().isEmpty())
            QSKIP("No screen is available for a presentation window.");
        QTemporaryDir sandbox;
        Presentation presentation;
        RemoteServer remote(&presentation);
        remote.setTokenPath(sandbox.filePath(QStringLiteral("remote-token")));
        remote.setAddressForTesting(QHostAddress::LocalHost, 0);
        remote.setEnabled(true);
        startTalk(presentation, {QString(), QStringLiteral("q"), QString()});
        QVERIFY(remote.isListening());
        const quint16 port = remote.port();
        const QByteArray base = '/' + remote.token().toLatin1() + '/';

        // The page.
        const QByteArray page = send(port, "GET", base);
        QVERIFY(page.startsWith("HTTP/1.1 200 OK"));
        QVERIFY(page.contains("Content-Type: text/html"));
        QVERIFY(page.contains("Content-Security-Policy:"));
        QVERIFY(body(page).contains("Omapresent remote"));

        // The state.
        QJsonObject state = json(send(port, "GET", base + "state"));
        QCOMPARE(state.value(QStringLiteral("active")).toBool(), true);
        QCOMPARE(state.value(QStringLiteral("slide")).toInt(), 1);
        QCOMPARE(state.value(QStringLiteral("count")).toInt(), 3);
        QCOMPARE(state.value(QStringLiteral("recallKeys")).toArray(),
                 QJsonArray{QStringLiteral("q")});

        // Next, next, back.
        state = json(send(port, "POST", base + "next"));
        QCOMPARE(state.value(QStringLiteral("slide")).toInt(), 2);
        QCOMPARE(presentation.slideIndex(), 1);
        send(port, "POST", base + "next");
        QCOMPARE(presentation.slideIndex(), 2);
        // Past the end stays on the last slide.
        send(port, "POST", base + "next");
        QCOMPARE(presentation.slideIndex(), 2);
        state = json(send(port, "POST", base + "prev"));
        QCOMPARE(state.value(QStringLiteral("slide")).toInt(), 2);

        // Black toggles, white replaces it, clear removes it.
        state = json(send(port, "POST", base + "black"));
        QCOMPARE(state.value(QStringLiteral("blank")).toString(), QStringLiteral("black"));
        state = json(send(port, "POST", base + "white"));
        QCOMPARE(presentation.blank(), QStringLiteral("white"));
        state = json(send(port, "POST", base + "white"));
        QCOMPARE(presentation.blank(), QString());
        send(port, "POST", base + "black");
        send(port, "POST", base + "clear");
        QCOMPARE(presentation.blank(), QString());

        // A recall key goes up and comes down, and puts the talk back.
        state = json(send(port, "POST", base + "recall/q"));
        QCOMPARE(state.value(QStringLiteral("recall")).toString(), QStringLiteral("q"));
        state = json(send(port, "POST", base + "recall/q"));
        QCOMPARE(state.value(QStringLiteral("recall")).toString(), QString());
        QCOMPARE(presentation.slideIndex(), 1);
        // A key the deck does not bind does nothing.
        send(port, "POST", base + "recall/z");
        QCOMPARE(presentation.recall(), QString());

        // A GET on an action changes nothing and is not found.
        const int before = presentation.slideIndex();
        QVERIFY(send(port, "GET", base + "next").startsWith("HTTP/1.1 404"));
        QCOMPARE(presentation.slideIndex(), before);

        presentation.stop();
    }

    void refusesTheWrongTokenOverHttp()
    {
        if (QGuiApplication::screens().isEmpty())
            QSKIP("No screen is available for a presentation window.");
        QTemporaryDir sandbox;
        Presentation presentation;
        RemoteServer remote(&presentation);
        remote.setTokenPath(sandbox.filePath(QStringLiteral("remote-token")));
        remote.setAddressForTesting(QHostAddress::LocalHost, 0);
        remote.setEnabled(true);
        startTalk(presentation, {QString(), QString()});
        const quint16 port = remote.port();

        QVERIFY(send(port, "GET", "/").startsWith("HTTP/1.1 404"));
        QVERIFY(send(port, "GET", "/state").startsWith("HTTP/1.1 404"));
        QVERIFY(send(port, "POST", "/next").startsWith("HTTP/1.1 404"));
        QVERIFY(send(port, "POST", "/ffffffffffffffffffffffffffffffff/next")
                    .startsWith("HTTP/1.1 404"));
        QCOMPARE(presentation.slideIndex(), 0);

        // And a request that never ends is cut off, not buffered forever.
        const QByteArray huge = "GET /" + QByteArray(16 * 1024, 'a') + " HTTP/1.1\r\n";
        QVERIFY(request(port, huge).startsWith("HTTP/1.1 431"));
        QCOMPARE(presentation.slideIndex(), 0);

        presentation.stop();
    }

    void thePresenterWindowShowsTheCodeUntilAPhoneConnects()
    {
        // PresenterWindow.qml pulls in QtWebEngine and cannot be instantiated
        // here (see tst_presentation.cpp), so this reads its source for the
        // bindings the QR code depends on.
        QFile file(QFINDTESTDATA("../src/PresenterWindow.qml"));
        QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString qml = QString::fromUtf8(file.readAll());
        QVERIFY(qml.contains(QStringLiteral("import \"qrc:/renderer/vendor/qrcode.mjs\" as QrCode")));
        QVERIFY(QFile::exists(QStringLiteral(":/renderer/vendor/qrcode.mjs")));
        QVERIFY(qml.contains(QStringLiteral("presentation.remoteUrl !== \"\"")));
        QVERIFY(qml.contains(QStringLiteral("!presentation.remoteInUse || remoteCodeWanted")));
        QVERIFY(qml.contains(QStringLiteral("objectName: \"remoteCode\"")));
        // Dark on white, unsmoothed, whatever the deck's theme is.
        QVERIFY(qml.contains(QStringLiteral("color: \"white\"")));
        QVERIFY(qml.contains(QStringLiteral("smooth: false")));
    }

    void actionsDoNothingWhenNoTalkIsRunning()
    {
        Presentation presentation;
        RemoteServer remote(&presentation);
        RemoteServer::Request next;
        next.kind = RemoteServer::Request::Action;
        next.action = QStringLiteral("next");
        QVERIFY(!remote.perform(next));
        QCOMPARE(remote.state().value(QStringLiteral("active")).toBool(true), false);
        QCOMPARE(remote.state().size(), 1);
    }
};

OMAPRESENT_TEST_SUITE(RemoteServerTest)
#include "tst_remoteserver.moc"
