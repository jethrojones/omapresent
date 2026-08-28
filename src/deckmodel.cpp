#include "deckmodel.h"

#include <QJsonArray>
#include <QStringList>

namespace {

// Spec §4.9: "up to ~8 bindings per deck".
constexpr int maxRecallBindings = 8;

bool isBlank(const QString &line)
{
    return line.trimmed().isEmpty();
}

// The text of a separator: exactly three dashes, optionally followed by a
// `{...}` recall tag. `----`, `- - -`, `***` and `___` are not separators.
bool isSeparatorBody(const QString &trimmed)
{
    if (!trimmed.startsWith(QStringLiteral("---")))
        return false;
    const QString rest = trimmed.mid(3).trimmed();
    return rest.isEmpty()
        || (rest.startsWith(QLatin1Char('{')) && rest.endsWith(QLatin1Char('}')));
}

bool isLineComment(const QString &line)
{
    return line.trimmed().startsWith(QStringLiteral("//"));
}

// `// ---` marks the slide that follows as a draft (spec §4.3). Returns the
// separator text with the `//` removed, or an empty string when the line is
// not a drop marker.
QString dropMarkerBody(const QString &line)
{
    const QString trimmed = line.trimmed();
    if (!trimmed.startsWith(QStringLiteral("//")))
        return {};
    const QString rest = trimmed.mid(2).trimmed();
    return isSeparatorBody(rest) ? rest : QString();
}

// --- Fenced code blocks ---------------------------------------------------
// Fences matter twice over: `---` inside one is not a separator, and `//` or
// `<!-- -->` inside one is example code, not a comment.

// Leading indent that still counts as part of a fence line (CommonMark allows
// three spaces; a fourth would make it an indented code block).
int fenceIndent(const QString &line)
{
    int i = 0;
    while (i < 3 && i < line.size() && line.at(i) == QLatin1Char(' '))
        ++i;
    return i;
}

bool isFenceOpener(const QString &line, QChar *marker, int *length)
{
    const int start = fenceIndent(line);
    if (start >= line.size())
        return false;
    const QChar candidate = line.at(start);
    if (candidate != QLatin1Char('`') && candidate != QLatin1Char('~'))
        return false;

    int run = 0;
    while (start + run < line.size() && line.at(start + run) == candidate)
        ++run;
    if (run < 3)
        return false;

    // A backtick fence's info string may not itself contain a backtick.
    const QString info = line.mid(start + run);
    if (candidate == QLatin1Char('`') && info.contains(QLatin1Char('`')))
        return false;

    *marker = candidate;
    *length = run;
    return true;
}

bool isFenceCloser(const QString &line, QChar marker, int length)
{
    const int start = fenceIndent(line);
    int run = 0;
    while (start + run < line.size() && line.at(start + run) == marker)
        ++run;
    if (run < length)
        return false;
    return line.mid(start + run).trimmed().isEmpty();
}

// --- Comment spans --------------------------------------------------------

enum class Span { None, Percent, Html };

QString spanCloser(Span span)
{
    return span == Span::Percent ? QStringLiteral("%%") : QStringLiteral("-->");
}

// Drops the tail of a span that opened on an earlier line. Returns false when
// this line is entirely inside the comment.
bool closeSpan(QString *text, Span span)
{
    const QString closer = spanCloser(span);
    const int end = text->indexOf(closer);
    if (end < 0)
        return false;
    *text = text->mid(end + closer.size());
    return true;
}

// Removes every `%%...%%` and `<!-- ... -->` span that opens on this line. A
// span left open records itself in *openSpan so the next line continues inside
// the comment; an unterminated span therefore swallows the rest of the file,
// which is what both Obsidian and HTML do.
QString removeSpans(const QString &line, Span *openSpan)
{
    QString text = line;
    int from = 0;
    forever {
        const int percent = text.indexOf(QStringLiteral("%%"), from);
        const int html = text.indexOf(QStringLiteral("<!--"), from);
        if (percent < 0 && html < 0)
            break;

        const bool percentFirst = html < 0 || (percent >= 0 && percent < html);
        const int start = percentFirst ? percent : html;
        const Span span = percentFirst ? Span::Percent : Span::Html;
        const QString closer = spanCloser(span);
        const int end = text.indexOf(closer, start + (percentFirst ? 2 : 4));
        if (end < 0) {
            *openSpan = span;
            text.truncate(start);
            break;
        }
        text.remove(start, end + closer.size() - start);
        from = start;
    }
    return text;
}

// One run of body lines between two separators, before it becomes a Slide.
struct Segment {
    QString recallKey;
    bool skipInFlow = false;
    bool dropped = false;
    // Original line numbers this segment owns, separator line included. Used
    // for cursor -> slide sync, so every line lands somewhere.
    int spanStart = 0;
    int spanEnd = 0;
    // Indices into the cleaned body.
    QVector<int> lines;
};

// A source line after comment removal, still carrying where it came from.
struct CleanLine {
    QString text;
    int sourceLine = 0;
    bool inFence = false;
};

// The one comment-stripping pass. Lines that a comment emptied are dropped
// outright (so `a\n// note\nb` stays one paragraph) but the line numbers of
// everything else are preserved exactly, which is what editor sync needs.
QVector<CleanLine> cleanLines(const QStringList &lines, int firstSourceLine)
{
    QVector<CleanLine> out;
    out.reserve(lines.size());

    Span span = Span::None;
    QChar fenceMarker;
    int fenceLength = 0;

    for (int i = 0; i < lines.size(); ++i) {
        const QString &raw = lines.at(i);
        const int sourceLine = firstSourceLine + i;

        if (fenceLength > 0) {
            out.append(CleanLine{raw, sourceLine, true});
            if (isFenceCloser(raw, fenceMarker, fenceLength))
                fenceLength = 0;
            continue;
        }

        QString text = raw;
        bool edited = false;

        if (span != Span::None) {
            edited = true;
            if (!closeSpan(&text, span))
                continue;
            span = Span::None;
        } else if (isLineComment(raw)) {
            // The splitter consumes `// ---` itself; every other `//` line goes.
            if (!dropMarkerBody(raw).isEmpty())
                out.append(CleanLine{raw, sourceLine, false});
            continue;
        }

        const QString before = text;
        text = removeSpans(text, &span);
        edited = edited || text != before;

        // Only a line a comment emptied disappears; an authored blank line is
        // structure and stays.
        if (edited && text.trimmed().isEmpty())
            continue;

        out.append(CleanLine{text, sourceLine, false});
        isFenceOpener(text, &fenceMarker, &fenceLength);
    }
    return out;
}

QStringList sourceLines(const QString &text)
{
    QString normalized = text;
    normalized.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    normalized.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    return normalized.split(QLatin1Char('\n'));
}

// --- Frontmatter ----------------------------------------------------------

// Everything after an unquoted ` #` is a YAML comment. The spec's frontmatter
// block is written with such comments, so decks copied from it must parse.
QString withoutTrailingComment(const QString &value)
{
    for (int i = 0; i < value.size(); ++i) {
        if (value.at(i) == QLatin1Char('#') && (i == 0 || value.at(i - 1).isSpace()))
            return value.left(i).trimmed();
    }
    return value;
}

QVariant scalarValue(const QString &raw)
{
    const QString value = raw.trimmed();
    if (value.startsWith(QLatin1Char('"')) || value.startsWith(QLatin1Char('\''))) {
        const int end = value.indexOf(value.at(0), 1);
        if (end > 0)
            return value.mid(1, end - 1);
    }

    const QString bare = withoutTrailingComment(value);
    if (bare.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0)
        return true;
    if (bare.compare(QStringLiteral("false"), Qt::CaseInsensitive) == 0)
        return false;
    return bare;
}

} // namespace

DeckModel::DeckModel(QObject *parent) : QObject(parent) {}

void DeckModel::setSource(const QString &text)
{
    m_source = text;
    parse();
    emit deckChanged();
}

QString DeckModel::source() const
{
    return m_source;
}

QString DeckModel::frontmatterRaw() const
{
    return m_frontmatterRaw;
}

QVariantMap DeckModel::frontmatter() const
{
    return m_frontmatter;
}

QVector<Slide> DeckModel::slides() const
{
    return m_slides;
}

int DeckModel::slideCount() const
{
    return m_slides.size();
}

int DeckModel::slideIndexForLine(int line) const
{
    if (line < 0 || line >= m_lineToSlide.size())
        return -1;
    return m_lineToSlide.at(line);
}

QJsonObject DeckModel::toJson() const
{
    QJsonArray slides;
    int flowIndex = 0;
    for (const Slide &slide : m_slides) {
        QJsonObject object;
        object.insert(QStringLiteral("index"), slide.skipInFlow ? -1 : flowIndex++);
        object.insert(QStringLiteral("markdown"), slide.markdown);
        object.insert(QStringLiteral("recallKey"), slide.recallKey);
        object.insert(QStringLiteral("skip"), slide.skipInFlow);
        object.insert(QStringLiteral("sourceStartLine"), slide.sourceStartLine);
        object.insert(QStringLiteral("sourceEndLine"), slide.sourceEndLine);
        slides.append(object);
    }

    QJsonObject deck;
    deck.insert(QStringLiteral("frontmatter"), QJsonObject::fromVariantMap(m_frontmatter));
    deck.insert(QStringLiteral("slides"), slides);
    return deck;
}

QString DeckModel::stripComments(const QString &text)
{
    QStringList out;
    const QVector<CleanLine> cleaned = cleanLines(sourceLines(text), 0);
    out.reserve(cleaned.size());
    for (const CleanLine &line : cleaned)
        out.append(line.text);
    return out.join(QLatin1Char('\n'));
}

bool DeckModel::isSeparatorLine(const QString &line, const QString &previousLine,
                                const QString &nextLine)
{
    // Callers pass an empty string for "no such line", which reads as blank:
    // a separator on the first or last line of a file still counts.
    if (!isBlank(previousLine) || !isBlank(nextLine))
        return false;
    return isSeparatorBody(line.trimmed());
}

void DeckModel::parseSeparatorTag(const QString &line, QString *recallKey, bool *skipInFlow)
{
    if (recallKey)
        recallKey->clear();
    if (skipInFlow)
        *skipInFlow = false;

    const QString trimmed = line.trimmed();
    const int open = trimmed.indexOf(QLatin1Char('{'));
    const int close = trimmed.lastIndexOf(QLatin1Char('}'));
    if (open < 0 || close <= open)
        return;

    const QStringList tokens = trimmed.mid(open + 1, close - open - 1).split(QLatin1Char(','));
    for (const QString &token : tokens) {
        const QString word = token.trimmed();
        if (word.isEmpty())
            continue;
        if (word.compare(QStringLiteral("skip"), Qt::CaseInsensitive) == 0) {
            if (skipInFlow)
                *skipInFlow = true;
        } else if (word.size() == 1 && word.at(0).isLetterOrNumber()) {
            if (recallKey && recallKey->isEmpty())
                *recallKey = word;
        }
        // Anything else is a typo in the tag; ignore it rather than guess.
    }
}

QVariantMap DeckModel::parseFrontmatter(const QString &yaml)
{
    QVariantMap result;
    // A key with an empty value is only a parent once an indented line follows,
    // so it is held back until we know which it is.
    QString pendingKey;
    QVariantMap pendingChildren;

    const auto flushPending = [&] {
        if (pendingKey.isEmpty())
            return;
        if (pendingChildren.isEmpty())
            result.insert(pendingKey, QString());
        else
            result.insert(pendingKey, pendingChildren);
        pendingKey.clear();
        pendingChildren.clear();
    };

    for (const QString &raw : sourceLines(yaml)) {
        const QString trimmed = raw.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#')))
            continue;

        const int colon = trimmed.indexOf(QLatin1Char(':'));
        if (colon <= 0)
            continue; // Not `key: value` — read what we can and drop the rest.

        const QString key = trimmed.left(colon).trimmed();
        const QString rest = trimmed.mid(colon + 1).trimmed();
        const bool indented = raw.startsWith(QLatin1Char(' ')) || raw.startsWith(QLatin1Char('\t'));

        if (indented && !pendingKey.isEmpty()) {
            pendingChildren.insert(key, scalarValue(rest));
            continue;
        }

        flushPending();
        if (withoutTrailingComment(rest).isEmpty())
            pendingKey = key;
        else
            result.insert(key, scalarValue(rest));
    }
    flushPending();

    return result;
}

// --- Parsing --------------------------------------------------------------

void DeckModel::parse()
{
    m_frontmatterRaw.clear();
    m_frontmatter.clear();
    m_slides.clear();

    const QStringList lines = sourceLines(m_source);
    m_lineToSlide.fill(-1, lines.size());

    // Frontmatter, spec §4.4: only when the very first line is `---`, running
    // to the next `---`. That closing line is not a slide separator.
    int bodyStart = 0;
    if (!lines.isEmpty() && lines.first().trimmed() == QStringLiteral("---")) {
        for (int i = 1; i < lines.size(); ++i) {
            if (lines.at(i).trimmed() != QStringLiteral("---"))
                continue;
            m_frontmatterRaw = QStringList(lines.mid(1, i - 1)).join(QLatin1Char('\n'));
            m_frontmatter = parseFrontmatter(m_frontmatterRaw);
            bodyStart = i + 1;
            break;
        }
    }

    const QVector<CleanLine> body = cleanLines(lines.mid(bodyStart), bodyStart);

    QVector<Segment> segments;
    segments.append(Segment{});
    segments.last().spanStart = bodyStart;
    segments.last().spanEnd = lines.size() - 1;

    for (int i = 0; i < body.size(); ++i) {
        const CleanLine &line = body.at(i);
        if (!line.inFence) {
            const QString marker = dropMarkerBody(line.text);
            const QString candidate = marker.isEmpty() ? line.text : marker;
            const QString previous = i > 0 ? body.at(i - 1).text : QString();
            const QString next = i + 1 < body.size() ? body.at(i + 1).text : QString();

            if (isSeparatorLine(candidate, previous, next)) {
                segments.last().spanEnd = line.sourceLine - 1;

                Segment segment;
                segment.dropped = !marker.isEmpty();
                segment.spanStart = line.sourceLine;
                segment.spanEnd = lines.size() - 1;
                parseSeparatorTag(candidate, &segment.recallKey, &segment.skipInFlow);
                segments.append(segment);
                continue;
            }
            if (!marker.isEmpty())
                continue; // A `// ---` that is not a separator is just a comment.
        }
        segments.last().lines.append(i);
    }

    int bindings = 0;
    for (const Segment &segment : segments) {
        if (segment.dropped)
            continue;

        int first = 0;
        int last = segment.lines.size() - 1;
        while (first <= last && isBlank(body.at(segment.lines.at(first)).text))
            ++first;
        while (last >= first && isBlank(body.at(segment.lines.at(last)).text))
            --last;
        if (first > last)
            continue; // Nothing between these separators: no slide.

        QStringList markdown;
        markdown.reserve(last - first + 1);
        for (int i = first; i <= last; ++i)
            markdown.append(body.at(segment.lines.at(i)).text);

        Slide slide;
        slide.markdown = markdown.join(QLatin1Char('\n')) + QLatin1Char('\n');
        slide.recallKey = segment.recallKey;
        slide.skipInFlow = segment.skipInFlow;
        slide.sourceStartLine = body.at(segment.lines.at(first)).sourceLine;
        slide.sourceEndLine = body.at(segment.lines.at(last)).sourceLine;

        if (!slide.recallKey.isEmpty() && bindings >= maxRecallBindings) {
            // Drop the whole tag, not just the key: a skipped slide with no key
            // left to pop it would be unreachable for the rest of the talk.
            qWarning("DeckModel: ignoring recall tag {%s} on the slide at line %d; "
                     "a deck binds at most %d keys.",
                     qUtf8Printable(slide.recallKey), slide.sourceStartLine, maxRecallBindings);
            slide.recallKey.clear();
            slide.skipInFlow = false;
        } else if (!slide.recallKey.isEmpty()) {
            ++bindings;
        }

        for (int line = segment.spanStart; line <= segment.spanEnd; ++line) {
            if (line >= 0 && line < m_lineToSlide.size())
                m_lineToSlide[line] = m_slides.size();
        }
        m_slides.append(slide);
    }
}
