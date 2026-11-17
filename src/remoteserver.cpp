#include "remoteserver.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkInterface>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTcpSocket>
#include <QTextStream>
#include <QTimer>

#include "presentation.h"

namespace {

constexpr int kMaxRequestBytes = 8 * 1024;
constexpr int kIdleTimeoutMs = 5000;
const char kPageResource[] = ":/remote/remote.html";

QString randomToken()
{
    QByteArray raw(16, Qt::Uninitialized);
    QRandomGenerator::system()->generate(raw.begin(), raw.end());
    return QString::fromLatin1(raw.toHex());
}

bool looksLikeToken(const QString &text)
{
    static const QRegularExpression pattern(QStringLiteral("^[0-9a-f]{32}$"));
    return pattern.match(text).hasMatch();
}

QString defaultTokenPath()
{
    const QString config = QStandardPaths::writableLocation(QStandardPaths::HomeLocation)
        + QStringLiteral("/.config/omapresent");
    return config + QStringLiteral("/remote-token");
}

}  // namespace

RemoteServer::RemoteServer(Presentation *presentation, QObject *parent)
    : QObject(parent), m_presentation(presentation), m_tokenPath(defaultTokenPath())
{
    connect(&m_server, &QTcpServer::newConnection, this, &RemoteServer::onNewConnection);
    if (m_presentation)
        connect(m_presentation, &Presentation::activeChanged, this, &RemoteServer::update);
}

RemoteServer::~RemoteServer() = default;

void RemoteServer::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    update();
}

void RemoteServer::setTokenPath(const QString &path)
{
    m_tokenPath = path;
    m_token.clear();
}

void RemoteServer::setAddressForTesting(const QHostAddress &address, quint16 port)
{
    m_testAddress = address;
    m_testPort = port;
    m_testing = true;
}

QString RemoteServer::url() const
{
    if (!m_server.isListening())
        return {};
    return QStringLiteral("http://%1:%2/%3/")
        .arg(m_server.serverAddress().toString())
        .arg(m_server.serverPort())
        .arg(m_token);
}

bool RemoteServer::isTailnetAddress(const QHostAddress &address)
{
    if (address.isNull())
        return false;
    bool isV4 = false;
    const quint32 v4 = address.toIPv4Address(&isV4);
    if (isV4) {
        // 100.64.0.0/10
        return (v4 & 0xFFC00000u) == 0x64400000u;
    }
    if (address.protocol() == QAbstractSocket::IPv6Protocol) {
        // fd7a:115c:a1e0::/48
        const Q_IPV6ADDR raw = address.toIPv6Address();
        return raw[0] == 0xfd && raw[1] == 0x7a && raw[2] == 0x11 && raw[3] == 0x5c
            && raw[4] == 0xa1 && raw[5] == 0xe0;
    }
    return false;
}

QHostAddress RemoteServer::tailnetAddress(const QList<QHostAddress> &addresses)
{
    for (const QHostAddress &address : addresses) {
        if (address.protocol() == QAbstractSocket::IPv4Protocol && isTailnetAddress(address))
            return address;
    }
    return {};
}

// The token survives between talks so a bookmark on the phone keeps working.
// It is created on first use and readable by this user only.
void RemoteServer::ensureToken()
{
    if (!m_token.isEmpty())
        return;

    QFile existing(m_tokenPath);
    if (existing.open(QIODevice::ReadOnly)) {
        const QString stored = QString::fromLatin1(existing.readAll()).trimmed();
        if (looksLikeToken(stored)) {
            m_token = stored;
            return;
        }
    }

    m_token = randomToken();
    QDir().mkpath(QFileInfo(m_tokenPath).absolutePath());
    QFile file(m_tokenPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
        file.write(m_token.toLatin1() + '\n');
    }
    // If the file could not be written the token still works for this run.
}

void RemoteServer::update()
{
    const bool wanted = m_enabled && m_presentation && m_presentation->active();
    if (!wanted) {
        if (m_server.isListening()) {
            m_server.close();
            emit urlChanged();
        }
        if (m_presentation) {
            m_presentation->setRemoteUrl(QString());
            m_presentation->setRemoteInUse(false);
        }
        return;
    }
    if (m_server.isListening())
        return;

    const QHostAddress address = m_testing
        ? m_testAddress
        : tailnetAddress(QNetworkInterface::allAddresses());
    if (address.isNull()) {
        QTextStream(stderr) << "omapresent: phone remote is on, but this computer has no "
                               "Tailscale address. Not listening.\n";
        return;
    }

    ensureToken();
    const quint16 port = m_testing ? m_testPort : kPreferredPort;
    if (!m_server.listen(address, port) && !m_server.listen(address, 0)) {
        QTextStream(stderr) << "omapresent: phone remote could not listen on "
                            << address.toString() << ": " << m_server.errorString() << '\n';
        return;
    }
    QTextStream(stderr) << "omapresent: phone remote at " << url() << '\n';
    // The presenter window shows this as a QR code until a phone has used it.
    m_presentation->setRemoteInUse(false);
    m_presentation->setRemoteUrl(url());
    emit urlChanged();
}

RemoteServer::Request RemoteServer::parseRequest(const QByteArray &requestHead,
                                                 const QString &token)
{
    Request request;
    if (token.isEmpty())
        return request;

    const qsizetype lineEnd = requestHead.indexOf('\n');
    QByteArray first = lineEnd < 0 ? requestHead : requestHead.left(lineEnd);
    if (first.endsWith('\r'))
        first.chop(1);

    const QList<QByteArray> parts = first.split(' ');
    if (parts.size() != 3)
        return request;
    const QByteArray method = parts.at(0);
    if (method != QByteArrayLiteral("GET") && method != QByteArrayLiteral("POST"))
        return request;
    if (parts.at(2) != QByteArrayLiteral("HTTP/1.1") && parts.at(2) != QByteArrayLiteral("HTTP/1.0"))
        return request;

    QString target = QString::fromLatin1(parts.at(1));
    const qsizetype query = target.indexOf(u'?');
    if (query >= 0)
        target.truncate(query);
    // Nothing here is ever percent-encoded, a traversal or a backslash. A path
    // that contains one is not a path this server has.
    if (!target.startsWith(u'/') || target.contains(u'%') || target.contains(QStringLiteral(".."))
        || target.contains(u'\\'))
        return request;

    const QStringList segments = target.mid(1).split(u'/');
    if (segments.size() < 2 || segments.size() > 3)
        return request;
    if (segments.at(0) != token)
        return request;

    const QString name = segments.at(1);
    if (segments.size() == 2) {
        if (method == QByteArrayLiteral("GET")) {
            if (name.isEmpty())
                request.kind = Request::Page;
            else if (name == QStringLiteral("state"))
                request.kind = Request::State;
            else if (name == QStringLiteral("slides"))
                request.kind = Request::Slides;
            return request;
        }
        static const QStringList actions{QStringLiteral("next"), QStringLiteral("prev"),
                                         QStringLiteral("black"), QStringLiteral("white"),
                                         QStringLiteral("clear")};
        if (actions.contains(name)) {
            request.kind = Request::Action;
            request.action = name;
        }
        return request;
    }

    // Three segments: only POST /<token>/recall/<key> and /<token>/goto/<n>.
    if (method != QByteArrayLiteral("POST"))
        return request;
    if (name == QStringLiteral("goto")) {
        static const QRegularExpression digits(QStringLiteral("^[1-9][0-9]{0,3}$"));
        if (!digits.match(segments.at(2)).hasMatch())
            return request;
        request.kind = Request::Action;
        request.action = QStringLiteral("goto");
        request.number = segments.at(2).toInt();
        return request;
    }
    if (name != QStringLiteral("recall"))
        return request;
    const QString key = segments.at(2);
    if (key.size() != 1 || !(key.at(0).isLetterOrNumber()) || key.at(0).unicode() > 0x7f)
        return request;
    request.kind = Request::Action;
    request.action = QStringLiteral("recall");
    request.key = key.toLower();
    return request;
}

QJsonObject RemoteServer::state() const
{
    QJsonObject state;
    const bool active = m_presentation && m_presentation->active();
    state.insert(QStringLiteral("active"), active);
    if (!active)
        return state;

    state.insert(QStringLiteral("title"), m_presentation->deckTitle());
    state.insert(QStringLiteral("slide"), m_presentation->slideIndex() + 1);
    state.insert(QStringLiteral("count"), m_presentation->slideCount());
    state.insert(QStringLiteral("heading"), m_presentation->heading());
    state.insert(QStringLiteral("notesHtml"), m_presentation->notesHtml());
    state.insert(QStringLiteral("elapsed"), m_presentation->elapsedSeconds());
    state.insert(QStringLiteral("blank"), m_presentation->blank());
    state.insert(QStringLiteral("recall"), m_presentation->recall());
    state.insert(QStringLiteral("recallKeys"),
                 QJsonArray::fromStringList(m_presentation->recallKeys()));
    state.insert(QStringLiteral("terminal"), m_presentation->terminalVisible());
    return state;
}

QString RemoteServer::headingOf(const QString &markdown)
{
    QString fallback;
    QString heading;
    bool inFence = false;
    const QStringList lines = markdown.split(u'\n');
    for (const QString &raw : lines) {
        const QString line = raw.trimmed();
        if (line.startsWith(QStringLiteral("```")) || line.startsWith(QStringLiteral("~~~"))) {
            inFence = !inFence;
            continue;
        }
        if (inFence || line.isEmpty() || line.startsWith(QStringLiteral("//"))
            || line.startsWith(QStringLiteral("<!--")) || line.startsWith(QStringLiteral("%%")))
            continue;
        if (line.startsWith(u'#')) {
            qsizetype i = 0;
            while (i < line.size() && line.at(i) == u'#')
                ++i;
            if (i <= 6 && (i == line.size() || line.at(i).isSpace())) {
                heading = line.mid(i).trimmed();
                if (!heading.isEmpty())
                    break;
                continue;
            }
        }
        if (fallback.isEmpty())
            fallback = line;
    }
    QString text = heading.isEmpty() ? fallback : heading;
    // Take the common inline marks off; this is a label, not a rendering.
    static const QRegularExpression image(QStringLiteral("!\\[\\[[^\\]]*\\]\\]|!\\[[^\\]]*\\]\\([^)]*\\)"));
    static const QRegularExpression link(QStringLiteral("\\[([^\\]]*)\\]\\([^)]*\\)"));
    static const QRegularExpression marks(QStringLiteral("[*_`=~]+"));
    static const QRegularExpression lead(QStringLiteral("^(?:[-+>]\\s+|\\d+[.)]\\s+)"));
    text.replace(image, QStringLiteral("(image)"));
    text.replace(link, QStringLiteral("\\1"));
    text.remove(marks);
    text.remove(lead);
    text = text.simplified();
    if (text.size() > 80)
        text = text.left(79).trimmed() + QChar(0x2026);
    return text;
}

QJsonObject RemoteServer::slides() const
{
    QJsonArray flow;
    QJsonArray recalls;
    if (m_presentation && m_presentation->active()) {
        const QJsonArray all = m_presentation->deckForRole(QStringLiteral("presenter"))
                                   .value(QStringLiteral("slides")).toArray();
        int number = 0;
        for (const QJsonValue &value : all) {
            const QJsonObject slide = value.toObject();
            const QString heading = headingOf(slide.value(QStringLiteral("markdown")).toString());
            const QString key = slide.value(QStringLiteral("recallKey")).toString();
            const bool terminal = slide.value(QStringLiteral("terminal")).toBool();
            const QString label = heading.isEmpty() && terminal ? QStringLiteral("Terminal")
                                                                : heading;
            if (!key.isEmpty())
                recalls.append(QJsonObject{{QStringLiteral("key"), key},
                                           {QStringLiteral("heading"), label}});
            if (!slide.value(QStringLiteral("skip")).toBool()) {
                ++number;
                flow.append(QJsonObject{{QStringLiteral("number"), number},
                                        {QStringLiteral("heading"), label}});
            }
        }
    }
    return QJsonObject{{QStringLiteral("slides"), flow}, {QStringLiteral("recalls"), recalls}};
}

bool RemoteServer::perform(const Request &request)
{
    if (request.kind != Request::Action || !m_presentation || !m_presentation->active())
        return false;

    Presentation *talk = m_presentation;
    if (request.action == QStringLiteral("next")) {
        talk->next();
    } else if (request.action == QStringLiteral("prev")) {
        talk->previous();
    } else if (request.action == QStringLiteral("black")) {
        talk->setBlank(talk->blank() == QStringLiteral("black") ? QString()
                                                                 : QStringLiteral("black"));
    } else if (request.action == QStringLiteral("white")) {
        talk->setBlank(talk->blank() == QStringLiteral("white") ? QString()
                                                                 : QStringLiteral("white"));
    } else if (request.action == QStringLiteral("clear")) {
        talk->setBlank(QString());
        talk->hideRecall();
    } else if (request.action == QStringLiteral("goto")) {
        if (request.number < 1 || request.number > talk->slideCount())
            return false;
        // A jump is a jump in the talk itself, so anything popped over it goes.
        if (!talk->recall().isEmpty())
            talk->hideRecall();
        talk->gotoSlide(request.number - 1);
    } else if (request.action == QStringLiteral("recall")) {
        if (!talk->recallKeys().contains(request.key))
            return false;
        // Same toggle the keyboard has: the key that put it up takes it down.
        if (talk->recall() == request.key)
            talk->hideRecall();
        else {
            if (!talk->recall().isEmpty())
                talk->hideRecall();
            talk->showRecall(request.key);
        }
    } else {
        return false;
    }
    return true;
}

void RemoteServer::onNewConnection()
{
    while (QTcpSocket *socket = m_server.nextPendingConnection()) {
        // The listen address already limits this to the tailnet. This refuses a
        // connection that somehow arrives from anywhere else.
        const QHostAddress peer = socket->peerAddress();
        const bool allowed = m_testing ? peer.isLoopback() : isTailnetAddress(peer);
        if (!allowed) {
            socket->abort();
            socket->deleteLater();
            continue;
        }
        connect(socket, &QTcpSocket::readyRead, this, [this, socket]() { onReadyRead(socket); });
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        QTimer::singleShot(kIdleTimeoutMs, socket, [socket]() {
            if (socket->state() != QAbstractSocket::UnconnectedState)
                socket->disconnectFromHost();
        });
    }
}

void RemoteServer::onReadyRead(QTcpSocket *socket)
{
    if (socket->property("answered").toBool())
        return;
    QByteArray buffered = socket->property("request").toByteArray();
    buffered += socket->readAll();
    if (buffered.size() > kMaxRequestBytes) {
        respond(socket, 431, QByteArrayLiteral("Request too large\n"));
        return;
    }
    socket->setProperty("request", buffered);

    const qsizetype headerEnd = buffered.indexOf(QByteArrayLiteral("\r\n\r\n"));
    if (headerEnd < 0)
        return;   // still arriving

    const Request request = parseRequest(buffered.left(headerEnd), m_token);
    // Any request with the right token means the phone has the address.
    if (request.kind != Request::Invalid && m_presentation)
        m_presentation->setRemoteInUse(true);
    switch (request.kind) {
    case Request::Page: {
        QFile file(QString::fromLatin1(kPageResource));
        if (!file.open(QIODevice::ReadOnly)) {
            respond(socket, 404, QByteArrayLiteral("Not found\n"));
            return;
        }
        respond(socket, 200, file.readAll(), QByteArrayLiteral("text/html; charset=utf-8"));
        return;
    }
    case Request::Action:
        // An action that could not be applied (no talk running, an unbound
        // recall key) still answers with the state, so the page corrects itself.
        perform(request);
        Q_FALLTHROUGH();
    case Request::State:
        respond(socket, 200, QJsonDocument(state()).toJson(QJsonDocument::Compact),
                QByteArrayLiteral("application/json; charset=utf-8"));
        return;
    case Request::Slides:
        respond(socket, 200, QJsonDocument(slides()).toJson(QJsonDocument::Compact),
                QByteArrayLiteral("application/json; charset=utf-8"));
        return;
    case Request::Invalid:
        break;
    }
    respond(socket, 404, QByteArrayLiteral("Not found\n"));
}

void RemoteServer::respond(QTcpSocket *socket, int status, const QByteArray &body,
                           const QByteArray &contentType)
{
    socket->setProperty("answered", true);
    const QByteArray reason = status == 200   ? QByteArrayLiteral("OK")
                            : status == 431   ? QByteArrayLiteral("Request Header Fields Too Large")
                                              : QByteArrayLiteral("Not Found");
    QByteArray response = "HTTP/1.1 " + QByteArray::number(status) + ' ' + reason + "\r\n";
    response += "Content-Type: " + contentType + "\r\n";
    response += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
    response += "Cache-Control: no-store\r\n";
    response += "X-Content-Type-Options: nosniff\r\n";
    response += "Referrer-Policy: no-referrer\r\n";
    response += "Content-Security-Policy: default-src 'none'; script-src 'unsafe-inline'; "
                "style-src 'unsafe-inline'; connect-src 'self'; base-uri 'none'; "
                "form-action 'none'; frame-ancestors 'none'\r\n";
    response += "Connection: close\r\n\r\n";
    response += body;

    socket->write(response);
    socket->flush();
    socket->disconnectFromHost();
}
