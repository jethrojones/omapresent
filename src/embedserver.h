#pragma once

// EmbedServer — spec §4.8, finding SEC-002. The smallest HTTP origin that will
// satisfy a hosted video player.
//
// Owner: the webbundle agent.
//
// YouTube's player refuses to configure when the page embedding it has an
// opaque origin: it wants a Referer, and neither `qrc:` nor `file:` sends one.
// That is "Error 153: Video player configuration error", and no embed URL
// parameter avoids it — measured across enablejsapi, autoplay and
// youtube-nocookie, all of which work unchanged from an http origin.
//
// So one frame, and only that frame, is served over loopback HTTP. The deck
// page stays on `qrc:` — moving it would break every `file:///` image and
// cached video in the app, because an http page may not load them.
//
// This server exists to hand out one compiled-in HTML file. It reads through
// QFile(":/renderer/…") and never touches a filesystem path, so there is no
// user file for a traversal to reach even if the path parsing were wrong. It
// binds to loopback only, on an ephemeral port, behind a per-session token, and
// it does not listen at all until something asks for baseUrl().

#include <QByteArray>
#include <QHostAddress>
#include <QObject>
#include <QString>
#include <QTcpServer>

class QTcpSocket;

class EmbedServer : public QObject {
    Q_OBJECT

public:
    explicit EmbedServer(QObject *parent = nullptr);
    ~EmbedServer() override;

    // The one instance the app shares. Not started by construction.
    static EmbedServer *instance();

    // "http://127.0.0.1:<port>/<token>/", starting the server on first call.
    // Empty when the socket cannot be bound, which is not fatal: the renderer
    // falls back to a QR code and an open-in-browser link.
    QString baseUrl();

    // True once the socket is listening. Nothing about opening a deck should
    // make this true — that is SEC-002's guarantee, and a test asserts it.
    bool isListening() const { return m_server.isListening(); }
    quint16 port() const { return m_server.serverPort(); }
    // The interface the socket is bound to. Always loopback; asserted in tests.
    QHostAddress address() const { return m_server.serverAddress(); }
    QString token() const { return m_token; }

    // --- Pure helpers, directly unit-tested -------------------------------
    // The bundled resource a request's headers ask for, or an empty string when
    // the request is not one this server will answer. `requestHead` is the
    // whole header block, `token` the session token, and `authority` the
    // "127.0.0.1:<port>" that Host must carry.
    //
    // Only "GET /<token>/embed.html[?query] HTTP/1.0|1.1", with exactly one
    // Host header naming that authority, resolves — and only to
    // ":/renderer/embed.html". Field names are matched case-insensitively, as
    // HTTP requires. Traversal is rejected before decoding and again after, so
    // "%2e%2e%2f" and "..%252f" fail along with the plain form.
    static QString resourceForRequest(const QByteArray &requestHead,
                                      const QString &token,
                                      const QString &authority);
    // Percent-decoding that refuses to produce a separator or a NUL, so a
    // decoded byte can never introduce a new path segment.
    static QString decodedPathSegment(const QString &segment, bool *ok);

private:
    void onNewConnection();
    void onReadyRead(QTcpSocket *socket);
    void respond(QTcpSocket *socket, int status, const QByteArray &body,
                 const QByteArray &contentType = QByteArrayLiteral("text/plain; charset=utf-8"));

    QTcpServer m_server;
    QString m_token;
};
