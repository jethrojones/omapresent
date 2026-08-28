#pragma once

// WebBundle — spec §9. Produces the self-contained static site that Publisher
// uploads: two views of the same deck, both from the shared renderer in `web`
// mode, offline-capable and themed.
//
// Owner: the webbundle agent. Contract frozen.
//
// This class writes files. It never touches the network — fetching is the
// video cache's job, uploading is the publisher's.

#include <QHash>
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

    // Where the renderer bundle is read from. Defaults to ":/renderer", the
    // compiled-in copy of src/renderer/. Tests point it at the source tree.
    void setRendererDir(const QString &rendererDir);
    QString rendererDir() const;

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

    // --- Pure helpers, directly unit-tested -------------------------------
    // The name a source file gets inside media/: the source's own stem, slugged,
    // plus a short digest of its absolute path. Stable across rebuilds, and two
    // "budget.png" files from different directories do not collide.
    static QString mediaFileName(const QString &sourcePath);

    // Every relative module specifier a source imports, re-exports or loads
    // dynamically, in the order it appears. Bare specifiers are left out:
    // there is no package resolution in a bundle.
    static QStringList moduleImports(const QString &source);

    // `source` with each specifier in `replacements` swapped for its value.
    // The renderer's modules are loaded from blob: URLs made at run time (see
    // webbundle.cpp), because a browser refuses to fetch an ES module from a
    // page opened off the disk, and a published deck has to open off a disk.
    static QString withModuleImports(const QString &source,
                                     const QHash<QString, QString> &replacements);

signals:
    void progress(int done, int total, const QString &what);

private:
    // Sets lastError(), undoes everything this build wrote, and returns false.
    bool fail(const QString &message);
    void rollback();
    bool ensureDirectory(const QString &relativeDir);
    bool writeFile(const QString &relativePath, const QByteArray &data);
    bool copyFile(const QString &sourcePath, const QString &relativePath);
    // Packages src/renderer/ into assets/render.js and copies the rest.
    bool buildRenderer();
    // assets/render.js: the renderer's modules as text, plus the few lines that
    // turn them into blob: URLs and start the entry module, which is last.
    QByteArray rendererLoader(const QStringList &order,
                              const QHash<QString, QString> &sources) const;
    // Every local file the deck references, mapped to its name in media/.
    void collectMedia();
    // The deck JSON as one page sees it: media rewritten to paths relative to
    // that page, `mode` "web", and `view` "deck" or "read".
    QJsonObject deckForPage(const QString &prefix, const QString &view) const;
    QByteArray themeCss() const;
    QByteArray page(const QString &view) const;
    QString deckTitle() const;

    QJsonObject m_deck;
    QString m_deckDir;
    QStringList m_files;
    qint64 m_totalBytes = 0;
    QString m_lastError;
    struct Private;
    Private *d = nullptr;
};
