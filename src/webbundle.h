#pragma once

// WebBundle — spec §9. Produces the self-contained static site that Publisher
// uploads: two views of the same deck, both from the shared renderer in `web`
// mode, offline-capable and themed.
//
// Owner: the webbundle agent. Contract frozen.
//
// This class writes files. It never touches the network — fetching is the
// video cache's job, uploading is the publisher's.

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>

class WebBundle : public QObject {
    Q_OBJECT

public:
    explicit WebBundle(QObject *parent = nullptr);
    ~WebBundle() override;

    // The deck JSON of docs/renderer-contract.md §1, with mode "web".
    void setDeck(const QJsonObject &deckJson);
    // Where the deck lives, for resolving relative assets and the video cache.
    void setDeckDir(const QString &deckDir);

    // Writes the bundle into `outputDir`, creating it if needed, and returns
    // true on success. The directory is left usable by `file://` alone —
    // opening index.html from disk must work with no server.
    //
    // Layout:
    //   index.html          the deck view (spec §9.1)
    //   read/index.html     the long read (spec §9.2)
    //   assets/deck.css     the renderer's stylesheet
    //   assets/render.js    the renderer bundle
    //   assets/vendor/...   markdown-it, KaTeX, QR — as vendored
    //   media/...           every image and cached video the deck references,
    //                       copied in and rewritten to relative paths
    Q_INVOKABLE bool build(const QString &outputDir);

    // Files written by the last successful build(), relative to outputDir.
    // Publisher uploads exactly this list.
    QStringList files() const;

    // Bytes written by the last successful build().
    qint64 totalBytes() const;

    QString lastError() const;

signals:
    void progress(int done, int total, const QString &what);

private:
    QJsonObject m_deck;
    QString m_deckDir;
    QStringList m_files;
    qint64 m_totalBytes = 0;
    QString m_lastError;
    struct Private;
    Private *d = nullptr;
};
