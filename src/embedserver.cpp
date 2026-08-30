#include "embedserver.h"

#include <QFile>
#include <QHostAddress>
#include <QRandomGenerator>
#include <QTcpSocket>
#include <QTimer>

namespace {

// A request this server will answer fits in well under a kilobyte. Anything
// larger is either a mistake or someone testing how much we will buffer.
constexpr int kMaxRequestBytes = 8 * 1024;
constexpr int kIdleTimeoutMs = 5000;

// The one resource this server serves. Not a directory, not a prefix — this
// exact name, mapped to this exact compiled-in file.
const char kEmbedName[] = "embed.html";
const char kEmbedResource[] = ":/renderer/embed.html";

QString randomToken()
{
    // 128 bits, so a token cannot be guessed by a page that knows only the port.
    QByteArray raw(16, Qt::Uninitialized);
    QRandomGenerator::system()->generate(raw.begin(), raw.end());
    return QString::fromLatin1(raw.toHex());
}

bool looksLikeTraversal(const QString &text)
{
    return text.contains(QStringLiteral("..")) || text.contains(u'\\') ||
           text.contains(u'\0');
}

}  // namespace

EmbedServer::EmbedServer(QObject *parent) : QObject(parent), m_token(randomToken())
{
    connect(&m_server, &QTcpServer::newConnection, this, &EmbedServer::onNewConnection);
}

EmbedServer::~EmbedServer() = default;

EmbedServer *EmbedServer::instance()
{
    static EmbedServer server;
    return &server;
}

QString EmbedServer::baseUrl()
{
    if (!m_server.isListening()) {
        // Loopback only, and a port the operating system picks: nothing outside
        // this machine can reach it, and nothing can predict where it is.
        if (!m_server.listen(QHostAddress::LocalHost, 0))
            return {};
    }
    return QStringLiteral("http://127.0.0.1:%1/%2/").arg(m_server.serverPort()).arg(m_token);
}

QString EmbedServer::decodedPathSegment(const QString &segment, bool *ok)
{
    QString decoded;
    decoded.reserve(segment.size());
    for (qsizetype i = 0; i < segment.size(); ++i) {
        const QChar character = segment.at(i);
        if (character != u'%') {
            decoded += character;
            continue;
        }
        if (i + 2 >= segment.size()) {
            if (ok)
                *ok = false;
            return {};
        }
        bool parsed = false;
        const int value = segment.mid(i + 1, 2).toInt(&parsed, 16);
        // A decoded byte may not become a separator or a terminator: that is
        // how "%2e%2e%2f" and "%00" try to grow a path segment into two.
        if (!parsed || value <= 0 || value == '/' || value == '\\' || value == '%') {
            if (ok)
                *ok = false;
            return {};
        }
        decoded += QChar(value);
        i += 2;
    }
    if (ok)
        *ok = true;
    return decoded;
}

QString EmbedServer::resourceForRequest(const QByteArray &requestHead, const QString &token,
                                       const QString &authority)
{
    // Split on LF and tolerate the CR, rather than requiring CRLF: what must be
    // strict is what we accept as a request, not how forgiving the split is.
    QList<QByteArray> lines;
    for (const QByteArray &line : requestHead.split('\n'))
        lines += line.endsWith('\r') ? line.left(line.size() - 1) : line;
    if (lines.isEmpty())
        return {};

    const QList<QByteArray> parts = lines.constFirst().split(' ');
    if (parts.size() != 3)
        return {};
    // GET only. HEAD would be harmless but there is no reason to answer it.
    if (parts.at(0) != QByteArrayLiteral("GET"))
        return {};
    // Exactly the two versions this server speaks. "HTTP/1." with anything
    // after it is not a version, it is a guess.
    if (parts.at(2) != QByteArrayLiteral("HTTP/1.1") && parts.at(2) != QByteArrayLiteral("HTTP/1.0"))
        return {};

    // Field names are case-insensitive, and exactly one Host is allowed: two of
    // them is how a request smuggles a second opinion about where it is going.
    QByteArray host;
    int hostCount = 0;
    for (qsizetype i = 1; i < lines.size(); ++i) {
        const QByteArray line = lines.at(i);
        const qsizetype colon = line.indexOf(':');
        if (colon <= 0)
            continue;
        // No trimming: a field name is the bytes before the colon, and RFC 7230
        // allows no space between them. "Host :" and " Host:" are not Host
        // headers, and treating them as one is how a request gets a second
        // opinion about where it is going past a proxy that read it strictly.
        if (line.left(colon).toLower() != QByteArrayLiteral("host"))
            continue;
        ++hostCount;
        host = line.mid(colon + 1).trimmed();
    }
    if (hostCount != 1)
        return {};

    // A browser that reached us by any name other than the loopback address is
    // a browser someone re-pointed at us. Refuse before doing anything else.
    if (QString::fromLatin1(host).compare(authority, Qt::CaseInsensitive) != 0)
        return {};

    QString target = QString::fromLatin1(parts.at(1));
    const qsizetype query = target.indexOf(u'?');
    if (query >= 0)
        target.truncate(query);
    if (!target.startsWith(u'/'))
        return {};
    // Reject traversal as written, before any decoding, so that a form which
    // only appears after decoding cannot hide behind one that does not.
    if (looksLikeTraversal(target))
        return {};

    const QStringList segments = target.mid(1).split(u'/');
    if (segments.size() != 2)
        return {};

    bool tokenOk = false;
    const QString requestToken = decodedPathSegment(segments.at(0), &tokenOk);
    if (!tokenOk || requestToken.isEmpty() || token.isEmpty())
        return {};
    // Length-independent comparison is not the concern here — the token is not
    // a secret an attacker can probe repeatedly, because a wrong one is a 404
    // and there is nothing else on this server to reach.
    if (requestToken != token)
        return {};

    bool nameOk = false;
    const QString name = decodedPathSegment(segments.at(1), &nameOk);
    if (!nameOk || looksLikeTraversal(name))
        return {};
    if (name != QLatin1String(kEmbedName))
        return {};

    return QString::fromLatin1(kEmbedResource);
}

void EmbedServer::onNewConnection()
{
    while (QTcpSocket *socket = m_server.nextPendingConnection()) {
        // Belt and braces: the listen address already excludes anything but
        // loopback, and this refuses a connection that somehow arrives anyway.
        if (!socket->peerAddress().isLoopback()) {
            socket->abort();
            socket->deleteLater();
            continue;
        }
        connect(socket, &QTcpSocket::readyRead, this, [this, socket]() { onReadyRead(socket); });
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        // A connection that never finishes its request does not get to sit here.
        QTimer::singleShot(kIdleTimeoutMs, socket, [socket]() {
            if (socket->state() != QAbstractSocket::UnconnectedState)
                socket->disconnectFromHost();
        });
    }
}

void EmbedServer::onReadyRead(QTcpSocket *socket)
{
    QByteArray buffered = socket->property("request").toByteArray();
    buffered += socket->readAll();
    if (buffered.size() > kMaxRequestBytes) {
        respond(socket, 431, QByteArrayLiteral("Request too large\n"));
        return;
    }
    socket->setProperty("request", buffered);

    const int headerEnd = buffered.indexOf(QByteArrayLiteral("\r\n\r\n"));
    if (headerEnd < 0)
        return;   // still arriving

    const QString authority = QStringLiteral("127.0.0.1:%1").arg(m_server.serverPort());
    const QString resource =
        resourceForRequest(buffered.left(headerEnd), m_token, authority);
    if (resource.isEmpty()) {
        respond(socket, 404, QByteArrayLiteral("Not found\n"));
        return;
    }

    QFile file(resource);
    if (!file.open(QIODevice::ReadOnly)) {
        respond(socket, 404, QByteArrayLiteral("Not found\n"));
        return;
    }
    respond(socket, 200, file.readAll(), QByteArrayLiteral("text/html; charset=utf-8"));
}

void EmbedServer::respond(QTcpSocket *socket, int status, const QByteArray &body,
                          const QByteArray &contentType)
{
    const QByteArray reason = status == 200   ? QByteArrayLiteral("OK")
                            : status == 431   ? QByteArrayLiteral("Request Header Fields Too Large")
                                              : QByteArrayLiteral("Not Found");
    QByteArray response = "HTTP/1.1 " + QByteArray::number(status) + ' ' + reason + "\r\n";
    response += "Content-Type: " + contentType + "\r\n";
    response += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
    // This page exists to be framed by the renderer and to frame the host's
    // player. It is not for anyone else to cache, sniff or link into.
    response += "Cache-Control: no-store\r\n";
    response += "X-Content-Type-Options: nosniff\r\n";
    response += "Referrer-Policy: strict-origin-when-cross-origin\r\n";
    response += "Connection: close\r\n\r\n";
    response += body;

    socket->write(response);
    socket->flush();
    socket->disconnectFromHost();
}
