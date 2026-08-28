#pragma once

// DeckModel — splits a raw Markdown file into frontmatter plus an ordered list
// of slides. It is the single authority for spec §4.1 (separators), §4.3
// (comments) and §4.4 (frontmatter). It deliberately does NOT decide what is
// audience content and what is a speaker note: that classification lives in the
// renderer (src/renderer/render.js), so preview, present, PDF and web agree.
//
// Owner: the doc-model agent. Contract frozen — add members, never change the
// meaning or signature of what is already here.

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QVector>

struct Slide {
    // Slide markdown with comments already stripped, trimmed of the blank
    // lines that surrounded its separators.
    QString markdown;
    // Recall binding from the separator line, e.g. `--- {q}` gives "q".
    // Empty when the slide is not bound to a key. Single letter or digit.
    QString recallKey;
    // True for `--- {q, skip}`: poppable by key, absent from the linear flow.
    bool skipInFlow = false;
    // 0-based line numbers in the ORIGINAL source, for editor<->slide sync.
    int sourceStartLine = 0;
    int sourceEndLine = 0;
};

class DeckModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(int slideCount READ slideCount NOTIFY deckChanged)

public:
    explicit DeckModel(QObject *parent = nullptr);

    // Reparses from scratch. Emits deckChanged().
    void setSource(const QString &text);
    QString source() const;

    // Raw YAML text between the opening and closing `---`, empty when absent.
    QString frontmatterRaw() const;
    // Parsed frontmatter. Only the keys in spec §4.4 are interpreted; unknown
    // keys are preserved as strings. Nested `publish:` becomes a QVariantMap
    // under key "publish".
    QVariantMap frontmatter() const;

    // Slides in document order, INCLUDING skipInFlow ones. Slides dropped by a
    // `// ---` separator (spec §4.3) are absent entirely.
    QVector<Slide> slides() const;
    int slideCount() const;

    // 0-based index of the slide containing a source line, -1 if none.
    Q_INVOKABLE int slideIndexForLine(int line) const;

    // The document handed to the renderer. Shape is frozen — see
    // docs/renderer-contract.md. Callers merge in "assets", "palette",
    // "backgroundImage" and "mode" before handing it to the web engine.
    QJsonObject toJson() const;

    // --- Pure helpers, directly unit-tested -------------------------------
    // Removes `//` line comments, `%%...%%` spans and `<!-- ... -->` spans.
    // A `// ---` separator line is NOT removed here: it is a marker consumed by
    // the splitter, which drops the slide that follows it.
    static QString stripComments(const QString &text);
    // True when `line` is a slide separator, given its neighbours. Callers pass
    // the previous and next raw lines; both must be blank (or absent at the
    // file edges) for the separator to count. `***` / `___` never count.
    static bool isSeparatorLine(const QString &line, const QString &previousLine,
                               const QString &nextLine);
    // Parses `--- {q, skip}` into ("q", true). Returns ("", false) for a plain
    // `---`. Whitespace inside the braces is insignificant.
    static void parseSeparatorTag(const QString &line, QString *recallKey, bool *skipInFlow);
    // The YAML subset used by frontmatter: scalars, quoted strings, one level
    // of nesting, `true`/`false` booleans. Never throws.
    static QVariantMap parseFrontmatter(const QString &yaml);

signals:
    void deckChanged();

private:
    struct Private;
    QString m_source;
    QString m_frontmatterRaw;
    QVariantMap m_frontmatter;
    QVector<Slide> m_slides;
};
