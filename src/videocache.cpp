#include "videocache.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUrl>
#include <QUrlQuery>

namespace {

const QStringList kVideoExtensions = {
    QStringLiteral("mp4"), QStringLiteral("webm"), QStringLiteral("mov")
};

bool hasWhitespace(const QString &s)
{
    for (const QChar c : s) {
        if (c.isSpace())
            return true;
    }
    return false;
}

bool hasScheme(const QString &s)
{
    static const QRegularExpression re(QStringLiteral("^[A-Za-z][A-Za-z0-9+.-]*:"));
    return re.match(s).hasMatch();
}

QString stripWww(QString host)
{
    host = host.toLower();
    if (host.startsWith(QStringLiteral("www.")))
        host = host.mid(4);
    const int colon = host.indexOf(QLatin1Char(':'));
    if (colon >= 0)
        host = host.left(colon);
    return host;
}

bool hostIs(const QString &host, const QString &domain)
{
    return host == domain || host.endsWith(QLatin1Char('.') + domain);
}

QString hostOf(const QUrl &url)
{
    return stripWww(url.host());
}

bool isKnownMediaHost(const QString &host)
{
    const QString h = stripWww(host);
    return hostIs(h, QStringLiteral("youtube.com"))
        || h == QStringLiteral("youtu.be")
        || hostIs(h, QStringLiteral("youtube-nocookie.com"))
        || hostIs(h, QStringLiteral("vimeo.com"))
        || hostIs(h, QStringLiteral("loom.com"))
        || hostIs(h, QStringLiteral("descript.com"))
        || hostIs(h, QStringLiteral("tiktok.com"))
        || h == QStringLiteral("x.com") || hostIs(h, QStringLiteral("x.com"))
        || hostIs(h, QStringLiteral("twitter.com"))
        || hostIs(h, QStringLiteral("instagram.com"))
        || hostIs(h, QStringLiteral("facebook.com"))
        || h == QStringLiteral("fb.com") || hostIs(h, QStringLiteral("fb.com"))
        || h == QStringLiteral("fb.watch");
}

QString hostPart(const QString &raw)
{
    QString t = raw.trimmed();
    if (t.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive))
        t = t.mid(8);
    else if (t.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive))
        t = t.mid(7);
    int end = t.size();
    const int slash = t.indexOf(QLatin1Char('/'));
    const int query = t.indexOf(QLatin1Char('?'));
    const int hash = t.indexOf(QLatin1Char('#'));
    if (slash >= 0)
        end = qMin(end, slash);
    if (query >= 0)
        end = qMin(end, query);
    if (hash >= 0)
        end = qMin(end, hash);
    return stripWww(t.left(end));
}

bool looksLikeUrl(const QString &trimmed)
{
    if (trimmed.isEmpty() || hasWhitespace(trimmed))
        return false;
    if (trimmed.startsWith(QStringLiteral("www."), Qt::CaseInsensitive))
        return true;
    if (hasScheme(trimmed)) {
        const QUrl url(trimmed);
        if (!url.isValid() || url.scheme().isEmpty())
            return false;
        const QString scheme = url.scheme().toLower();
        if (scheme == QStringLiteral("http") || scheme == QStringLiteral("https")
            || scheme == QStringLiteral("ftp")) {
            return !url.host().isEmpty();
        }
        if (scheme == QStringLiteral("file") || scheme == QStringLiteral("mailto"))
            return true;
        return false;
    }
    return isKnownMediaHost(hostPart(trimmed));
}

QUrl parseUrl(const QString &raw)
{
    QString s = raw.trimmed();
    if (s.isEmpty())
        return {};
    if (s.startsWith(QStringLiteral("www."), Qt::CaseInsensitive)
        || (!hasScheme(s) && isKnownMediaHost(hostPart(s)))) {
        s.prepend(QStringLiteral("https://"));
    }
    return QUrl(s);
}

QStringList pathSegments(const QUrl &url)
{
    QString path = url.path();
    while (path.endsWith(QLatin1Char('/')))
        path.chop(1);
    return path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
}

bool isVideoPath(const QString &path)
{
    QString p = path;
    while (p.endsWith(QLatin1Char('/')))
        p.chop(1);
    const int dot = p.lastIndexOf(QLatin1Char('.'));
    if (dot < 0 || dot == p.size() - 1)
        return false;
    const QString ext = p.mid(dot + 1).toLower();
    return kVideoExtensions.contains(ext);
}

QString videoExtension(const QString &path)
{
    QString p = path;
    while (p.endsWith(QLatin1Char('/')))
        p.chop(1);
    const int dot = p.lastIndexOf(QLatin1Char('.'));
    if (dot < 0)
        return QStringLiteral("mp4");
    const QString ext = p.mid(dot + 1).toLower();
    if (kVideoExtensions.contains(ext))
        return ext;
    return QStringLiteral("mp4");
}

bool isDigits(const QString &s)
{
    if (s.isEmpty())
        return false;
    for (const QChar c : s) {
        if (!c.isDigit())
            return false;
    }
    return true;
}

bool isMediaId(const QString &s)
{
    if (s.isEmpty())
        return false;
    for (const QChar c : s) {
        if (!c.isLetterOrNumber() && c != QLatin1Char('_') && c != QLatin1Char('-'))
            return false;
    }
    return true;
}

QString youtubeId(const QUrl &url)
{
    const QString host = hostOf(url);
    const bool youtube = host == QStringLiteral("youtu.be")
        || hostIs(host, QStringLiteral("youtube.com"))
        || hostIs(host, QStringLiteral("youtube-nocookie.com"));
    if (!youtube)
        return {};

    const QStringList segs = pathSegments(url);
    if (host == QStringLiteral("youtu.be")) {
        if (segs.isEmpty() || !isMediaId(segs.first()))
            return {};
        return segs.first();
    }

    QUrlQuery query(url);
    const QString v = query.queryItemValue(QStringLiteral("v"));
    if (!v.isEmpty() && isMediaId(v))
        return v;

    if (segs.size() >= 2) {
        const QString kind = segs.first().toLower();
        if ((kind == QStringLiteral("embed") || kind == QStringLiteral("shorts")
             || kind == QStringLiteral("live") || kind == QStringLiteral("v")
             || kind == QStringLiteral("e"))
            && isMediaId(segs.at(1))) {
            return segs.at(1);
        }
    }
    return {};
}

QString vimeoId(const QUrl &url, QString *hash = nullptr)
{
    if (hash)
        hash->clear();
    const QString host = hostOf(url);
    if (!hostIs(host, QStringLiteral("vimeo.com")))
        return {};

    const QStringList segs = pathSegments(url);
    int idIndex = -1;
    for (int i = 0; i < segs.size(); ++i) {
        if (isDigits(segs.at(i)))
            idIndex = i;
    }
    if (idIndex < 0)
        return {};

    if (hash && idIndex + 1 < segs.size()) {
        const QString next = segs.at(idIndex + 1);
        static const QStringList reserved = {
            QStringLiteral("channels"), QStringLiteral("groups"),
            QStringLiteral("videos"), QStringLiteral("ondemand"),
            QStringLiteral("album"), QStringLiteral("showcase"),
            QStringLiteral("staffpicks")
        };
        if (isMediaId(next) && !reserved.contains(next.toLower()))
            *hash = next;
    }
    return segs.at(idIndex);
}

QString loomId(const QUrl &url)
{
    if (!hostIs(hostOf(url), QStringLiteral("loom.com")))
        return {};
    const QStringList segs = pathSegments(url);
    if (segs.size() < 2)
        return {};
    const QString kind = segs.first().toLower();
    if (kind != QStringLiteral("share") && kind != QStringLiteral("embed"))
        return {};
    if (!isMediaId(segs.at(1)))
        return {};
    return segs.at(1);
}

QString descriptId(const QUrl &url)
{
    if (!hostIs(hostOf(url), QStringLiteral("descript.com")))
        return {};
    const QStringList segs = pathSegments(url);
    if (segs.size() < 2)
        return {};
    const QString kind = segs.first().toLower();
    if (kind != QStringLiteral("view") && kind != QStringLiteral("embed"))
        return {};
    if (!isMediaId(segs.at(1)))
        return {};
    return segs.at(1);
}

QString tiktokId(const QUrl &url)
{
    const QString host = hostOf(url);
    if (!hostIs(host, QStringLiteral("tiktok.com")))
        return {};
    const QStringList segs = pathSegments(url);
    if (segs.isEmpty())
        return {};

    if (host == QStringLiteral("vm.tiktok.com") || host == QStringLiteral("vt.tiktok.com")) {
        return isMediaId(segs.first()) ? segs.first() : QString();
    }

    for (int i = 0; i + 1 < segs.size(); ++i) {
        const QString kind = segs.at(i).toLower();
        if (kind == QStringLiteral("video") || kind == QStringLiteral("v")
            || kind == QStringLiteral("t") || kind == QStringLiteral("embed")) {
            QString id = segs.at(i + 1);
            if (id.toLower() == QStringLiteral("v2") && i + 2 < segs.size())
                id = segs.at(i + 2);
            if (isMediaId(id))
                return id;
        }
    }
    return {};
}

QString xStatusId(const QUrl &url)
{
    const QString host = hostOf(url);
    if (host != QStringLiteral("x.com") && !hostIs(host, QStringLiteral("x.com"))
        && !hostIs(host, QStringLiteral("twitter.com"))) {
        return {};
    }
    const QStringList segs = pathSegments(url);
    for (int i = 0; i + 1 < segs.size(); ++i) {
        if (segs.at(i).toLower() == QStringLiteral("status") && isDigits(segs.at(i + 1)))
            return segs.at(i + 1);
    }
    return {};
}

QString instagramCode(const QUrl &url, QString *kind = nullptr)
{
    if (kind)
        kind->clear();
    if (!hostIs(hostOf(url), QStringLiteral("instagram.com")))
        return {};
    const QStringList segs = pathSegments(url);
    if (segs.size() < 2)
        return {};
    const QString k = segs.first().toLower();
    if (k != QStringLiteral("p") && k != QStringLiteral("reel")
        && k != QStringLiteral("reels") && k != QStringLiteral("tv")) {
        return {};
    }
    if (!isMediaId(segs.at(1)))
        return {};
    if (kind)
        *kind = (k == QStringLiteral("reels")) ? QStringLiteral("reel") : k;
    return segs.at(1);
}

bool isFacebookVideo(const QUrl &url)
{
    const QString host = hostOf(url);
    if (host == QStringLiteral("fb.watch"))
        return !pathSegments(url).isEmpty();
    if (!hostIs(host, QStringLiteral("facebook.com")) && host != QStringLiteral("fb.com")
        && !hostIs(host, QStringLiteral("fb.com"))) {
        return false;
    }
    QUrlQuery query(url);
    if (!query.queryItemValue(QStringLiteral("v")).isEmpty())
        return true;
    const QStringList segs = pathSegments(url);
    for (int i = 0; i < segs.size(); ++i) {
        const QString s = segs.at(i).toLower();
        if (s == QStringLiteral("reel") || s == QStringLiteral("reels")
            || s == QStringLiteral("videos")) {
            if (i + 1 < segs.size() && isMediaId(segs.at(i + 1)))
                return true;
        }
        if (s == QStringLiteral("share") && i + 2 < segs.size()) {
            const QString next = segs.at(i + 1).toLower();
            if ((next == QStringLiteral("v") || next == QStringLiteral("r")
                 || next == QStringLiteral("reel"))
                && isMediaId(segs.at(i + 2))) {
                return true;
            }
        }
    }
    return false;
}

QString hostKey(VideoCache::Host host)
{
    switch (host) {
    case VideoCache::YouTube:
        return QStringLiteral("youtube");
    case VideoCache::Vimeo:
        return QStringLiteral("vimeo");
    case VideoCache::Loom:
        return QStringLiteral("loom");
    case VideoCache::Descript:
        return QStringLiteral("descript");
    case VideoCache::TikTok:
        return QStringLiteral("tiktok");
    case VideoCache::X:
        return QStringLiteral("x");
    case VideoCache::Instagram:
        return QStringLiteral("instagram");
    case VideoCache::Facebook:
        return QStringLiteral("facebook");
    case VideoCache::DirectFile:
        return QStringLiteral("direct");
    case VideoCache::LocalFile:
        return QStringLiteral("local");
    case VideoCache::NotAVideo:
        break;
    }
    return {};
}

bool isSafeCacheName(const QString &name)
{
    if (name.isEmpty() || name == QStringLiteral(".") || name == QStringLiteral(".."))
        return false;
    if (name.contains(QLatin1Char('/')) || name.contains(QLatin1Char('\\')))
        return false;
    return true;
}

QString fileUrlFor(const QString &absolutePath)
{
    return QUrl::fromLocalFile(QFileInfo(absolutePath).absoluteFilePath())
        .toString(QUrl::FullyEncoded);
}

QString localVideoPath(const QString &raw, const QString &deckDir)
{
    const QString trimmed = raw.trimmed();
    const QUrl parsed = parseUrl(trimmed);
    if (parsed.isLocalFile())
        return parsed.toLocalFile();

    QString path = trimmed;
    if (path.startsWith(QStringLiteral("~/")))
        path = QDir::homePath() + path.mid(1);
    else if (path == QStringLiteral("~"))
        path = QDir::homePath();

    if (QDir::isAbsolutePath(path))
        return path;
    if (deckDir.isEmpty())
        return {};
    return QDir(deckDir).absoluteFilePath(path);
}

QString encodedUrl(const QString &url)
{
    return QString::fromUtf8(QUrl::toPercentEncoding(url));
}

QString iframeSrc(const QString &html)
{
    static const QRegularExpression re(
        QStringLiteral("<iframe[^>]*\\ssrc=[\"']([^\"']+)[\"']"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch m = re.match(html);
    return m.hasMatch() ? m.captured(1) : QString();
}

QString tiktokVideoIdFromHtml(const QString &html)
{
    static const QRegularExpression re(
        QStringLiteral("data-video-id=[\"'](\\d+)[\"']"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch m = re.match(html);
    return m.hasMatch() ? m.captured(1) : QString();
}

QString mediaUrlFromOEmbed(const QJsonObject &obj)
{
    const QString url = obj.value(QStringLiteral("url")).toString();
    if (!url.isEmpty()) {
        const QUrl parsed(url);
        if (isVideoPath(parsed.path()))
            return url;
    }
    static const QRegularExpression srcRe(
        QStringLiteral("src=[\"'](https?://[^\"']+\\.(mp4|webm|mov)[^\"']*)[\"']"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch m = srcRe.match(obj.value(QStringLiteral("html")).toString());
    return m.hasMatch() ? m.captured(1) : QString();
}

} // namespace

struct VideoCache::Private {
    QNetworkAccessManager nam;
    QNetworkReply *reply = nullptr;
    QFile downloadFile;
    QString downloadDest;
    QStringList queue;
    QStringList failed;
    QString current;
    QJsonObject index;
    int done = 0;
    int total = 0;
    bool busy = false;
    enum Phase { Idle, OEmbed, Media, Poster } phase = Idle;
};

VideoCache::VideoCache(QObject *parent)
    : QObject(parent)
    , d(new Private)
{
    d->nam.setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
}

VideoCache::~VideoCache()
{
    d->busy = false;
    d->queue.clear();
    if (d->reply) {
        disconnect(d->reply, nullptr, this, nullptr);
        d->reply->abort();
        d->reply->deleteLater();
        d->reply = nullptr;
    }
    if (d->downloadFile.isOpen()) {
        const QString name = d->downloadFile.fileName();
        d->downloadFile.close();
        QFile::remove(name);
    }
    delete d;
}

void VideoCache::setDeckDir(const QString &deckDir)
{
    m_deckDir = deckDir;
}

QString VideoCache::cacheDir() const
{
    if (m_deckDir.isEmpty())
        return {};
    return QDir(m_deckDir).filePath(QStringLiteral(".omapresent-cache"));
}

QJsonObject VideoCache::readIndex() const
{
    const QString dir = cacheDir();
    if (dir.isEmpty())
        return {};
    QFile file(QDir(dir).filePath(QStringLiteral("index.json")));
    if (!file.open(QIODevice::ReadOnly))
        return {};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    return doc.isObject() ? doc.object() : QJsonObject();
}

bool VideoCache::writeIndex(const QJsonObject &index) const
{
    const QString dir = cacheDir();
    if (dir.isEmpty())
        return false;
    if (!QDir().mkpath(dir))
        return false;
    QSaveFile file(QDir(dir).filePath(QStringLiteral("index.json")));
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(QJsonDocument(index).toJson(QJsonDocument::Indented));
    return file.commit();
}

VideoCache::Host VideoCache::hostFor(const QString &url)
{
    const QString trimmed = url.trimmed();
    if (looksLikeUrl(trimmed)) {
        const QUrl parsed = parseUrl(trimmed);
        if (!parsed.isValid())
            return NotAVideo;

        const QString scheme = parsed.scheme().toLower();
        if (scheme == QStringLiteral("mailto") || scheme == QStringLiteral("javascript")
            || scheme == QStringLiteral("data")) {
            return NotAVideo;
        }

        if (!youtubeId(parsed).isEmpty())
            return YouTube;
        if (!vimeoId(parsed).isEmpty())
            return Vimeo;
        if (!loomId(parsed).isEmpty())
            return Loom;
        if (!descriptId(parsed).isEmpty())
            return Descript;
        if (!tiktokId(parsed).isEmpty())
            return TikTok;
        if (!xStatusId(parsed).isEmpty())
            return X;
        if (!instagramCode(parsed).isEmpty())
            return Instagram;
        if (isFacebookVideo(parsed))
            return Facebook;

        if (isVideoPath(parsed.path())) {
            if (scheme == QStringLiteral("file"))
                return LocalFile;
            if (scheme == QStringLiteral("http") || scheme == QStringLiteral("https"))
                return DirectFile;
        }
        return NotAVideo;
    }

    // Relative or absolute local video path, matching src/renderer/media.js.
    if (!trimmed.isEmpty() && isVideoPath(trimmed) && !hasScheme(trimmed))
        return LocalFile;
    return NotAVideo;
}

bool VideoCache::isBareUrlLine(const QString &line)
{
    return looksLikeUrl(line.trimmed());
}

QStringList VideoCache::extractUrls(const QString &slideMarkdown)
{
    QStringList urls;
    if (slideMarkdown.isEmpty())
        return urls;

    const QStringList lines = slideMarkdown.split(QRegularExpression(QStringLiteral("\\r?\\n")));
    bool inFence = false;
    QString fence;

    for (const QString &raw : lines) {
        const QString trimmed = raw.trimmed();
        if (trimmed.startsWith(QStringLiteral("```")) || trimmed.startsWith(QStringLiteral("~~~"))) {
            const QString marker = trimmed.left(3);
            if (!inFence) {
                inFence = true;
                fence = marker;
            } else if (trimmed.startsWith(fence)) {
                inFence = false;
                fence.clear();
            }
            continue;
        }
        if (inFence)
            continue;
        if (isBareUrlLine(trimmed))
            urls.append(trimmed);
    }
    return urls;
}

QString VideoCache::embedUrlFor(const QString &url)
{
    const Host host = hostFor(url);
    const QUrl parsed = parseUrl(url);
    switch (host) {
    case YouTube: {
        const QString id = youtubeId(parsed);
        return id.isEmpty()
            ? QString()
            : QStringLiteral("https://www.youtube.com/embed/") + id;
    }
    case Vimeo: {
        QString hash;
        const QString id = vimeoId(parsed, &hash);
        if (id.isEmpty())
            return {};
        QString embed = QStringLiteral("https://player.vimeo.com/video/") + id;
        if (!hash.isEmpty())
            embed += QStringLiteral("?h=") + hash;
        return embed;
    }
    case Loom: {
        const QString id = loomId(parsed);
        return id.isEmpty()
            ? QString()
            : QStringLiteral("https://www.loom.com/embed/") + id;
    }
    case Descript: {
        const QString id = descriptId(parsed);
        return id.isEmpty()
            ? QString()
            : QStringLiteral("https://share.descript.com/embed/") + id;
    }
    case TikTok: {
        const QString id = tiktokId(parsed);
        return id.isEmpty()
            ? QString()
            : QStringLiteral("https://www.tiktok.com/embed/v2/") + id;
    }
    case X: {
        const QString id = xStatusId(parsed);
        return id.isEmpty()
            ? QString()
            : QStringLiteral("https://platform.twitter.com/embed/Tweet.html?id=") + id;
    }
    case Instagram: {
        QString kind;
        const QString id = instagramCode(parsed, &kind);
        if (id.isEmpty())
            return {};
        return QStringLiteral("https://www.instagram.com/") + kind + QLatin1Char('/')
            + id + QStringLiteral("/embed/");
    }
    case Facebook: {
        const QString href = parsed.toString();
        return QStringLiteral("https://www.facebook.com/plugins/video.php?href=")
            + encodedUrl(href);
    }
    case DirectFile:
        return parsed.toString();
    case LocalFile:
        return parsed.toString();
    case NotAVideo:
        break;
    }
    return {};
}

QString VideoCache::oEmbedUrlFor(const QString &url)
{
    const Host host = hostFor(url);
    const QString encoded = encodedUrl(parseUrl(url).toString());
    switch (host) {
    case YouTube:
        return QStringLiteral("https://www.youtube.com/oembed?format=json&url=") + encoded;
    case Vimeo:
        return QStringLiteral("https://vimeo.com/api/oembed.json?url=") + encoded;
    case Loom:
        return QStringLiteral("https://www.loom.com/v1/oembed?url=") + encoded;
    case Descript:
        return QStringLiteral("https://share.descript.com/oembed?url=") + encoded;
    case TikTok:
        return QStringLiteral("https://www.tiktok.com/oembed?url=") + encoded;
    case X:
        return QStringLiteral("https://publish.twitter.com/oembed?url=") + encoded;
    case Instagram:
        return QStringLiteral("https://api.instagram.com/oembed?url=") + encoded;
    case Facebook:
        return QStringLiteral("https://www.facebook.com/plugins/video/oembed.json?url=")
            + encoded;
    case DirectFile:
    case LocalFile:
    case NotAVideo:
        break;
    }
    return {};
}

QString VideoCache::cacheKey(const QString &url)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(url.trimmed().toUtf8(), QCryptographicHash::Sha256).toHex());
}

QJsonObject VideoCache::describe(const QString &url) const
{
    const QString trimmed = url.trimmed();
    const Host host = hostFor(trimmed);
    QString embed = embedUrlFor(trimmed);
    QString cachedFile;
    QString poster;
    QString title;
    int width = 0;
    int height = 0;

    const QJsonObject entry = readIndex().value(trimmed).toObject();
    const QString dir = cacheDir();
    if (!dir.isEmpty() && !entry.isEmpty()) {
        title = entry.value(QStringLiteral("title")).toString();
        width = entry.value(QStringLiteral("width")).toInt();
        height = entry.value(QStringLiteral("height")).toInt();
        const QString storedEmbed = entry.value(QStringLiteral("embedUrl")).toString();
        if (!storedEmbed.isEmpty())
            embed = storedEmbed;

        const QString fileName = entry.value(QStringLiteral("file")).toString();
        if (isSafeCacheName(fileName)) {
            const QString path = QDir(dir).filePath(fileName);
            if (QFileInfo::exists(path) && QFileInfo(path).size() > 0)
                cachedFile = fileUrlFor(path);
        }
        const QString posterName = entry.value(QStringLiteral("poster")).toString();
        if (isSafeCacheName(posterName)) {
            const QString path = QDir(dir).filePath(posterName);
            if (QFileInfo::exists(path) && QFileInfo(path).size() > 0)
                poster = fileUrlFor(path);
        }
    }

    if (cachedFile.isEmpty() && host == LocalFile) {
        const QString local = localVideoPath(trimmed, m_deckDir);
        if (!local.isEmpty() && QFileInfo::exists(local) && QFileInfo(local).size() > 0)
            cachedFile = fileUrlFor(local);
        else
            embed.clear();
    }

    bool vertical = false;
    if (width > 0 && height > width) {
        vertical = true;
    } else if (host == TikTok) {
        vertical = true;
    } else if (host == YouTube) {
        const QString path = parseUrl(trimmed).path().toLower();
        vertical = path.contains(QStringLiteral("/shorts/"));
    } else if (host == Instagram) {
        const QString path = parseUrl(trimmed).path().toLower();
        vertical = path.contains(QStringLiteral("/reel"));
    }

    QString status;
    if (!cachedFile.isEmpty())
        status = QStringLiteral("cached");
    else if (!embed.isEmpty())
        status = QStringLiteral("embed");
    else
        status = QStringLiteral("qr");

    QJsonObject out;
    out.insert(QStringLiteral("host"), hostKey(host));
    out.insert(QStringLiteral("embedUrl"), embed);
    out.insert(QStringLiteral("cachedFile"), cachedFile);
    out.insert(QStringLiteral("poster"), poster);
    out.insert(QStringLiteral("title"), title);
    out.insert(QStringLiteral("width"), width);
    out.insert(QStringLiteral("height"), height);
    out.insert(QStringLiteral("vertical"), vertical);
    out.insert(QStringLiteral("status"), status);
    return out;
}

static QNetworkRequest mediaRequest(const QUrl &url, int timeoutMs)
{
    QNetworkRequest req(url);
    req.setMaximumRedirectsAllowed(8);
    req.setRawHeader("User-Agent", "Omapresent/1.0");
    req.setRawHeader("Accept", "application/json, */*;q=0.8");
    req.setTransferTimeout(timeoutMs);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    return req;
}

void VideoCache::prefetch(const QStringList &urls)
{
    if (d->busy)
        return;

    QStringList unique;
    for (const QString &u : urls) {
        const QString t = u.trimmed();
        if (t.isEmpty() || unique.contains(t))
            continue;
        unique.append(t);
    }

    if (cacheDir().isEmpty()) {
        emit prefetchProgress(0, unique.size());
        emit prefetchFinished(unique);
        return;
    }

    QDir().mkpath(cacheDir());
    d->queue = unique;
    d->failed.clear();
    d->done = 0;
    d->total = unique.size();
    d->busy = true;
    d->index = readIndex();
    d->phase = Private::Idle;
    emit prefetchProgress(0, d->total);
    prefetchNext();
}

void VideoCache::prefetchNext()
{
    while (d->busy) {
        if (d->queue.isEmpty()) {
            writeIndex(d->index);
            d->busy = false;
            d->phase = Private::Idle;
            emit prefetchFinished(d->failed);
            emit cacheChanged();
            return;
        }

        d->current = d->queue.takeFirst();
        const Host host = hostFor(d->current);

        auto finishSync = [this](bool failed) {
            if (failed && !d->failed.contains(d->current))
                d->failed.append(d->current);
            d->done += 1;
            emit prefetchProgress(d->done, d->total);
        };

        if (host == NotAVideo) {
            finishSync(false);
            continue;
        }

        const QJsonObject existing = d->index.value(d->current).toObject();
        const QString existingFile = existing.value(QStringLiteral("file")).toString();
        bool haveFile = false;
        if (isSafeCacheName(existingFile)) {
            const QString path = QDir(cacheDir()).filePath(existingFile);
            haveFile = QFileInfo::exists(path) && QFileInfo(path).size() > 0;
        }
        if (haveFile || !existing.value(QStringLiteral("fetchedAt")).toString().isEmpty()) {
            finishSync(false);
            continue;
        }

        if (host == LocalFile) {
            const QString source = localVideoPath(d->current, m_deckDir);
            const QFileInfo info(source);
            if (!info.exists() || info.size() <= 0) {
                finishSync(true);
                continue;
            }
            const QString name = cacheKey(d->current) + QLatin1Char('.')
                + videoExtension(info.fileName());
            const QString dest = QDir(cacheDir()).filePath(name);
            if (!QFileInfo::exists(dest)) {
                if (!QFile::copy(source, dest)) {
                    finishSync(true);
                    continue;
                }
            }
            QJsonObject entry = existing;
            entry.insert(QStringLiteral("file"), name);
            entry.insert(QStringLiteral("poster"), existing.value(QStringLiteral("poster")).toString());
            entry.insert(QStringLiteral("title"), info.completeBaseName());
            entry.insert(QStringLiteral("width"), existing.value(QStringLiteral("width")).toInt());
            entry.insert(QStringLiteral("height"), existing.value(QStringLiteral("height")).toInt());
            entry.insert(QStringLiteral("fetchedAt"),
                         QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
            d->index.insert(d->current, entry);
            writeIndex(d->index);
            finishSync(false);
            continue;
        }

        if (host == DirectFile) {
            const QUrl source = parseUrl(d->current);
            if (source.isLocalFile()) {
                finishSync(true);
                continue;
            }
            const QString name = cacheKey(d->current) + QLatin1Char('.')
                + videoExtension(source.path());
            d->downloadDest = QDir(cacheDir()).filePath(name);
            if (QFileInfo::exists(d->downloadDest) && QFileInfo(d->downloadDest).size() > 0) {
                QJsonObject entry = existing;
                entry.insert(QStringLiteral("file"), QFileInfo(d->downloadDest).fileName());
                entry.insert(QStringLiteral("fetchedAt"),
                             QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
                d->index.insert(d->current, entry);
                writeIndex(d->index);
                finishSync(false);
                continue;
            }
            const QString part = d->downloadDest + QStringLiteral(".part");
            d->downloadFile.setFileName(part);
            if (!d->downloadFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                finishSync(true);
                continue;
            }
            d->phase = Private::Media;
            d->reply = d->nam.get(mediaRequest(source, 120000));
            connect(d->reply, &QNetworkReply::readyRead, this, [this]() {
                if (d->reply)
                    d->downloadFile.write(d->reply->readAll());
            });
            connect(d->reply, &QNetworkReply::finished, this, &VideoCache::onReplyFinished);
            return;
        }

        const QString oembed = oEmbedUrlFor(d->current);
        if (oembed.isEmpty()) {
            finishSync(embedUrlFor(d->current).isEmpty());
            continue;
        }
        d->phase = Private::OEmbed;
        d->reply = d->nam.get(mediaRequest(QUrl(oembed), 15000));
        connect(d->reply, &QNetworkReply::finished, this, &VideoCache::onReplyFinished);
        return;
    }
}

void VideoCache::onReplyFinished()
{
    QNetworkReply *reply = d->reply;
    d->reply = nullptr;
    if (!reply) {
        prefetchNext();
        return;
    }
    reply->deleteLater();

    auto failCurrent = [this]() {
        if (!d->failed.contains(d->current))
            d->failed.append(d->current);
        if (d->downloadFile.isOpen()) {
            const QString name = d->downloadFile.fileName();
            d->downloadFile.close();
            QFile::remove(name);
        }
        d->phase = Private::Idle;
        d->done += 1;
        emit prefetchProgress(d->done, d->total);
        prefetchNext();
    };

    auto succeedCurrent = [this]() {
        d->phase = Private::Idle;
        d->done += 1;
        emit prefetchProgress(d->done, d->total);
        prefetchNext();
    };

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const bool httpFail = reply->error() != QNetworkReply::NoError || status >= 400;
    if (httpFail) {
        // A missing thumbnail (or optional media URL after a successful oEmbed)
        // must not turn a working embed into a failure.
        const bool optional = d->phase == Private::Poster
            || (d->phase == Private::Media
                && !d->index.value(d->current).toObject()
                        .value(QStringLiteral("fetchedAt"))
                        .toString()
                        .isEmpty());
        if (optional)
            succeedCurrent();
        else
            failCurrent();
        return;
    }

    if (d->phase == Private::Media || d->phase == Private::Poster) {
        d->downloadFile.write(reply->readAll());
        d->downloadFile.close();
        if (d->downloadFile.size() <= 0) {
            QFile::remove(d->downloadFile.fileName());
            failCurrent();
            return;
        }
        QFile::remove(d->downloadDest);
        if (!QFile::rename(d->downloadFile.fileName(), d->downloadDest)) {
            QFile::remove(d->downloadFile.fileName());
            failCurrent();
            return;
        }
        QJsonObject entry = d->index.value(d->current).toObject();
        const QString baseName = QFileInfo(d->downloadDest).fileName();
        if (d->phase == Private::Media)
            entry.insert(QStringLiteral("file"), baseName);
        else
            entry.insert(QStringLiteral("poster"), baseName);
        entry.insert(QStringLiteral("fetchedAt"),
                     QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
        d->index.insert(d->current, entry);
        writeIndex(d->index);
        succeedCurrent();
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    if (!doc.isObject()) {
        failCurrent();
        return;
    }
    const QJsonObject oembed = doc.object();
    QJsonObject entry = d->index.value(d->current).toObject();
    const QString title = oembed.value(QStringLiteral("title")).toString();
    if (!title.isEmpty())
        entry.insert(QStringLiteral("title"), title);
    entry.insert(QStringLiteral("width"), oembed.value(QStringLiteral("width")).toInt());
    entry.insert(QStringLiteral("height"), oembed.value(QStringLiteral("height")).toInt());
    entry.insert(QStringLiteral("fetchedAt"),
                 QDateTime::currentDateTimeUtc().toString(Qt::ISODate));

    const QString html = oembed.value(QStringLiteral("html")).toString();
    QString resolvedEmbed = iframeSrc(html);
    if (resolvedEmbed.isEmpty()) {
        const QString tiktokIdFromHtml = tiktokVideoIdFromHtml(html);
        if (!tiktokIdFromHtml.isEmpty())
            resolvedEmbed = QStringLiteral("https://www.tiktok.com/embed/v2/") + tiktokIdFromHtml;
    }
    if (!resolvedEmbed.isEmpty())
        entry.insert(QStringLiteral("embedUrl"), resolvedEmbed);
    else if (entry.value(QStringLiteral("embedUrl")).toString().isEmpty())
        entry.insert(QStringLiteral("embedUrl"), embedUrlFor(d->current));

    d->index.insert(d->current, entry);
    writeIndex(d->index);

    const QString media = mediaUrlFromOEmbed(oembed);
    const QString thumb = oembed.value(QStringLiteral("thumbnail_url")).toString();
    if (!media.isEmpty()) {
        const QString name = cacheKey(d->current) + QLatin1Char('.')
            + videoExtension(QUrl(media).path());
        d->downloadDest = QDir(cacheDir()).filePath(name);
        d->downloadFile.setFileName(d->downloadDest + QStringLiteral(".part"));
        if (d->downloadFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            d->phase = Private::Media;
            d->reply = d->nam.get(mediaRequest(QUrl(media), 120000));
            connect(d->reply, &QNetworkReply::readyRead, this, [this]() {
                if (d->reply)
                    d->downloadFile.write(d->reply->readAll());
            });
            connect(d->reply, &QNetworkReply::finished, this, &VideoCache::onReplyFinished);
            return;
        }
    }
    if (!thumb.isEmpty() && QUrl(thumb).isValid()) {
        QString ext = QFileInfo(QUrl(thumb).path()).suffix().toLower();
        if (ext.isEmpty())
            ext = QStringLiteral("jpg");
        const QString name = cacheKey(d->current) + QStringLiteral("-poster.") + ext;
        d->downloadDest = QDir(cacheDir()).filePath(name);
        d->downloadFile.setFileName(d->downloadDest + QStringLiteral(".part"));
        if (d->downloadFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            d->phase = Private::Poster;
            d->reply = d->nam.get(mediaRequest(QUrl(thumb), 15000));
            connect(d->reply, &QNetworkReply::readyRead, this, [this]() {
                if (d->reply)
                    d->downloadFile.write(d->reply->readAll());
            });
            connect(d->reply, &QNetworkReply::finished, this, &VideoCache::onReplyFinished);
            return;
        }
    }

    succeedCurrent();
}
