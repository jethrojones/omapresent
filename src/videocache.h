#pragma once

// VideoCache — spec §4.8. Recognises video/embed URLs, resolves them through
// each host's oEmbed endpoint, and caches the underlying media into
// <deck-dir>/.omapresent-cache/ so a prepared deck presents offline.
//
// Owner: the media agent. Contract frozen.

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>

class VideoCache : public QObject {
    Q_OBJECT

public:
    enum Host {
        NotAVideo,   // -> the renderer draws a QR code instead (spec §4.8)
        YouTube, Vimeo, Loom, Descript, TikTok, X, Instagram, Facebook,
        DirectFile,  // .mp4 / .webm / .mov URL
        LocalFile
    };
    Q_ENUM(Host)

    explicit VideoCache(QObject *parent = nullptr);
    ~VideoCache() override;

    void setDeckDir(const QString &deckDir);
    // <deckDir>/.omapresent-cache, or empty when the deck is unset or the
    // cache path is a symlink / would write outside the deck (SEC-003).
    QString cacheDir() const;

    // What the renderer needs for one URL, resolved from cache when possible:
    //   { "host": "youtube", "embedUrl": "...", "cachedFile": "file:///... or empty",
    //     "poster": "file:///... or empty", "title": "...", "width": 0, "height": 0,
    //     "vertical": false, "status": "cached|embed|qr" }
    // Never blocks on the network: returns the cached answer, or status "embed"
    // while prefetch runs.
    QJsonObject describe(const QString &url) const;

    // Spec §4.8 "Prepare for offline": resolve and download every URL that the
    // host allows. Emits prefetchProgress() then prefetchFinished().
    // `urls` that cannot be fetched degrade to "embed" then "qr" and are
    // reported once in prefetchFinished(failed).
    Q_INVOKABLE void prefetch(const QStringList &urls);

    // --- Pure helpers, directly unit-tested -------------------------------
    static Host hostFor(const QString &url);
    // True when a whole line, on its own, is a bare URL (spec §4.8): the
    // renderer turns it into a player when hostFor() != NotAVideo, else a QR.
    // A relative video filename (`clip.webm`) is not a URL — `.webm` is a
    // file extension, not a TLD. hostFor() still returns LocalFile for it.
    static bool isBareUrlLine(const QString &line);
    // Every bare-URL line in a slide, in document order, plus whole-line
    // local video paths so describe()/prefetch() see them.
    static QStringList extractUrls(const QString &slideMarkdown);
    // The host's embed URL for a watch/share URL, e.g. a YouTube watch link to
    // its /embed/<id> form. Empty when the host is not recognised.
    static QString embedUrlFor(const QString &url);
    // The host's oEmbed endpoint for `url`. Empty when there is none (direct
    // files, local files, unrecognised hosts). Does not hit the network.
    static QString oEmbedUrlFor(const QString &url);
    // SHA-256 hex of the trimmed URL; cache files are named from this so a
    // re-prefetch of the same URL is a no-op.
    static QString cacheKey(const QString &url);

signals:
    void prefetchProgress(int done, int total);
    void prefetchFinished(const QStringList &failed);
    void cacheChanged();

private:
    void prefetchNext();
    void onReplyFinished();
    QJsonObject readIndex() const;
    bool writeIndex(const QJsonObject &index) const;

    QString m_deckDir;
    struct Private;
    Private *d = nullptr;
};
