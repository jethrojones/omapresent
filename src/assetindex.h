#pragma once

// AssetIndex — spec §4.5, "it just finds it". Turns an image reference written
// in any accepted form into an absolute path on disk, using a recursive index
// of the resolution root.
//
// Owner: the assets agent. Contract frozen.

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>

class AssetIndex : public QObject {
    Q_OBJECT

public:
    explicit AssetIndex(QObject *parent = nullptr);
    ~AssetIndex() override;

    // Directory of the open deck file. Step 1 of resolution is relative to it.
    void setDeckDir(const QString &deckDir);
    QString deckDir() const;

    // The `root:` frontmatter folder, recursively indexed and watched.
    // Empty resets it to deckDir().
    void setRoot(const QString &root);
    QString root() const;

    // Absolute path of the resolved file, or an empty string when nothing
    // matches. Follows the exact order in spec §4.5:
    //   1. relative to deckDir  2. ~ / $HOME / env expansion, absolute
    //   3. filename search of the index (shortest/closest wins)
    //   4. case-insensitive retry of 3
    // A reference may be "budget.png", "~/Pictures/budget.png", "../img/a.png"
    // or an http(s) URL (returned unchanged for the cache layer to fetch).
    QString resolve(const QString &reference) const;

    // { reference -> "file:///abs/path" } for every input; unresolved
    // references map to an empty string so the renderer can show the
    // missing-asset placeholder (spec §4.5 step 5).
    QJsonObject resolveAll(const QStringList &references) const;

    // --- Pure helpers, directly unit-tested -------------------------------
    // Every image reference in a slide's markdown, in document order, in all
    // the forms of spec §4.5: ![[x]], ![alt](x), and a bare path alone on a
    // line. Size hints are stripped: "photo.png|600" yields "photo.png".
    static QStringList extractReferences(const QString &slideMarkdown);
    // True when a whole line, on its own, should be read as an image path:
    // it contains a '/' or ends in a known image extension.
    static bool looksLikeImageReference(const QString &line);
    // Splits "photo.png|600" / "photo.png|main" into the reference and its
    // hint. `maxWidthPx` is 0 when absent; `isMain` marks the bento hero.
    static void parseSizeHint(const QString &reference, QString *bareReference,
                              int *maxWidthPx, bool *isMain);
    // The shortest form of `absolutePath` that still resolves uniquely against
    // this index — what drag-and-drop inserts (spec §4.5).
    QString shortestUniqueReference(const QString &absolutePath) const;

    // Waits for any in-flight background directory walk to complete (useful in tests).
    void waitForIndex(int timeoutMs = 5000) const;

signals:
    // The watched tree changed; anything showing images should re-resolve.
    void indexChanged();

private:
    friend class AssetIndexScanWorker;
    void rebuild();
    void applyScanResult(quint64 generation,
                         QHash<QString, QStringList> newByName,
                         QStringList allDirs);

    QString m_deckDir;
    QString m_root;
    // lowercased filename -> absolute paths, in shortest-path-first order
    QHash<QString, QStringList> m_byName;
    struct Private;
    Private *d = nullptr;
};
