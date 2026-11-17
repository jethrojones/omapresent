#pragma once

// RemoteServer — drive the talk from a phone.
//
// While a presentation is running, and only when `presentation.remote` is on,
// this serves one small page that shows where the talk is (slide number,
// heading, speaker notes, elapsed time) and has buttons for next, previous,
// black, white and the recall keys.
//
// It is the second listening socket in the app (EmbedServer is the first), so
// the same care applies:
//
//   * It does not listen unless the setting is on AND a talk is running.
//   * It binds to this machine's Tailscale address and nothing else. With no
//     Tailscale address it does not listen at all — never 0.0.0.0, never the
//     LAN.
//   * It refuses a peer that is not itself a Tailscale address.
//   * Every path carries a token. The token is kept on disk (mode 0600) so the
//     page can be bookmarked on the phone and still work next week.
//   * It serves one compiled-in page and a JSON state. It never reads a path
//     from the request, so there is no file for a traversal to reach.
//   * Anything that changes the talk is a POST, so a link preview or a
//     prefetching browser cannot advance a slide.
//
// It can do exactly what the buttons do. It cannot open a file, type into the
// live terminal, or leave present mode.

#include <QByteArray>
#include <QHostAddress>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>
#include <QTcpServer>

class Presentation;
class QTcpSocket;

class RemoteServer : public QObject {
    Q_OBJECT
    // "http://100.x.y.z:<port>/<token>/" while listening, otherwise empty.
    Q_PROPERTY(QString url READ url NOTIFY urlChanged)

public:
    // The port the remote asks for first. If it is taken the operating system
    // picks one, and url() says which.
    static constexpr quint16 kPreferredPort = 4747;

    explicit RemoteServer(Presentation *presentation, QObject *parent = nullptr);
    ~RemoteServer() override;

    // The `presentation.remote` setting. Turning it off stops listening at once.
    void setEnabled(bool enabled);
    bool enabled() const { return m_enabled; }

    bool isListening() const { return m_server.isListening(); }
    quint16 port() const { return m_server.serverPort(); }
    QHostAddress address() const { return m_server.serverAddress(); }
    QString token() const { return m_token; }
    QString url() const;

    // Where the token is kept. Defaults to ~/.config/omapresent/remote-token.
    // Set before the first listen; overridable for tests.
    void setTokenPath(const QString &path);
    // Tests have no Tailscale interface. This lets them bind to loopback and
    // accept a loopback peer. Never called by the app.
    void setAddressForTesting(const QHostAddress &address, quint16 port);

    // --- Pure helpers, directly unit-tested -------------------------------

    // Tailscale hands every device an address in 100.64.0.0/10 (and one in
    // fd7a:115c:a1e0::/48). Anything else is not the tailnet.
    static bool isTailnetAddress(const QHostAddress &address);
    // The first IPv4 tailnet address in the list, or a null address.
    static QHostAddress tailnetAddress(const QList<QHostAddress> &addresses);

    struct Request {
        enum Kind { Invalid, Page, State, Slides, Action };
        Kind kind = Invalid;
        // For Action: "next", "prev", "black", "white", "clear", "recall" or "goto".
        QString action;
        // For "recall": the key.
        QString key;
        // For "goto": the slide number as the audience counts it, from 1.
        int number = 0;
    };
    // What a request's header block asks for. Only these resolve:
    //   GET  /<token>/                 the page
    //   GET  /<token>/state            the JSON state
    //   POST /<token>/next|prev|black|white|clear
    //   POST /<token>/recall/<one letter or digit>
    //   GET  /<token>/slides           every slide's number and heading
    //   POST /<token>/goto/<slide number, 1 to 9999>
    static Request parseRequest(const QByteArray &requestHead, const QString &token);

    // The state the page polls for.
    QJsonObject state() const;
    // The slide list the page's "Slides" sheet shows: {"slides": [{"number",
    // "heading"}], "recalls": [{"key", "heading"}]}. Numbers count only the
    // slides in the arrow-key flow, so they match the counter on screen.
    QJsonObject slides() const;
    // What to call a slide in that list: its first heading, else its first
    // line of text, with the Markdown marks taken off. Never longer than 80
    // characters.
    static QString headingOf(const QString &markdown);
    // Applies one parsed action to the presentation. False when it is not an
    // action, or no talk is running.
    bool perform(const Request &request);

signals:
    void urlChanged();

private:
    void update();
    void ensureToken();
    void onNewConnection();
    void onReadyRead(QTcpSocket *socket);
    void respond(QTcpSocket *socket, int status, const QByteArray &body,
                 const QByteArray &contentType = QByteArrayLiteral("text/plain; charset=utf-8"));

    Presentation *m_presentation = nullptr;
    QTcpServer m_server;
    QString m_token;
    QString m_tokenPath;
    bool m_enabled = false;
    QHostAddress m_testAddress;
    quint16 m_testPort = 0;
    bool m_testing = false;
};
