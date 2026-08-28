#include "webbundle.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QMap>
#include <QRegularExpression>
#include <QSet>
#include <QUrl>

#include <algorithm>
#include <functional>

namespace {

// The published page loads from `file://` as often as from a web server: the
// promise in spec §9 is a bundle you can unzip on a laptop and open. A browser
// refuses to fetch an ES module from a file:// page (CORS, origin `null`) but
// happily runs a plain script, a stylesheet and a relative image. So the
// renderer's module graph is flattened into one classic assets/render.js, and
// nothing on either page is fetched at runtime — the deck JSON is inlined.

QString htmlEscaped(const QString &text)
{
    QString escaped;
    escaped.reserve(text.size());
    for (const QChar character : text) {
        switch (character.unicode()) {
        case '&': escaped += QStringLiteral("&amp;"); break;
        case '<': escaped += QStringLiteral("&lt;"); break;
        case '>': escaped += QStringLiteral("&gt;"); break;
        case '"': escaped += QStringLiteral("&quot;"); break;
        default: escaped += character;
        }
    }
    return escaped;
}

// A colour we are willing to paste into a stylesheet. colors.toml comes from
// disk and the palette carries whatever it held, so the bundle only ever emits
// hex it recognises.
bool isHexColour(const QString &value)
{
    static const QRegularExpression hex(QStringLiteral("^#[0-9a-fA-F]{3,8}$"));
    return hex.match(value).hasMatch();
}

// Family names only: a `font:` frontmatter value goes into a CSS declaration.
QString cssFontFamily(const QString &font)
{
    QString cleaned;
    for (const QChar character : font) {
        if (character.isLetterOrNumber() || character == u' ' || character == u'-')
            cleaned += character;
    }
    cleaned = cleaned.simplified();
    if (cleaned.isEmpty())
        return {};
    return QStringLiteral("\"%1\", ").arg(cleaned);
}

// ASCII only: these become file names inside a bundle that gets uploaded,
// unzipped and served, and every layer of that has its own opinion about
// anything else.
QString slugged(const QString &text)
{
    QString slug;
    for (const QChar character : text) {
        if (character.unicode() < 128 && character.isLetterOrNumber())
            slug += character.toLower();
        else if (!slug.isEmpty() && !slug.endsWith(u'-'))
            slug += u'-';
    }
    while (slug.endsWith(u'-'))
        slug.chop(1);
    return slug;
}

// The deck JSON lives in a <script type="application/json"> block, so no
// sequence in it may close that block early. \u escapes keep it valid JSON.
QString jsonForScriptTag(const QJsonObject &object)
{
    QString text = QString::fromUtf8(
        QJsonDocument(object).toJson(QJsonDocument::Compact));
    text.replace(QStringLiteral("<"), QStringLiteral("\\u003c"));
    text.replace(QStringLiteral("&"), QStringLiteral("\\u0026"));
    return text;
}

// An absolute local path for a deck JSON value, or empty when the value is not
// a local file: an unresolved asset, a remote URL, a data: URI.
QString localPathFor(const QString &value, const QString &deckDir)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty())
        return {};
    if (trimmed.startsWith(QStringLiteral("file:"), Qt::CaseInsensitive))
        return QUrl(trimmed).toLocalFile();
    if (trimmed.contains(QStringLiteral("://")) ||
        trimmed.startsWith(QStringLiteral("data:"), Qt::CaseInsensitive) ||
        trimmed.startsWith(QStringLiteral("qrc:"), Qt::CaseInsensitive))
        return {};
    if (QDir::isAbsolutePath(trimmed))
        return trimmed;
    if (deckDir.isEmpty())
        return {};
    return QDir(deckDir).absoluteFilePath(trimmed);
}

// Marks a module specifier the loader has to point at a blob: URL. Nothing
// else in a script can look like it, and it never survives into a running page.
const char kModuleMarker[] = "omapresent:module/";

// `text` as a JavaScript string literal.
QString jsString(const QString &text)
{
    QString quoted;
    quoted.reserve(text.size() + 2);
    quoted += u'"';
    for (const QChar character : text) {
        switch (character.unicode()) {
        case '"': quoted += QStringLiteral("\\\""); break;
        case '\\': quoted += QStringLiteral("\\\\"); break;
        case '\n': quoted += QStringLiteral("\\n"); break;
        case '\r': quoted += QStringLiteral("\\r"); break;
        case '\t': quoted += QStringLiteral("\\t"); break;
        // U+2028 and U+2029 are line terminators to a JavaScript parser.
        case 0x2028: quoted += QStringLiteral("\\u2028"); break;
        case 0x2029: quoted += QStringLiteral("\\u2029"); break;
        default:
            if (character.unicode() < 0x20)
                quoted += QStringLiteral("\\u%1").arg(int(character.unicode()), 4, 16, QLatin1Char('0'));
            else
                quoted += character;
        }
    }
    quoted += u'"';
    return quoted;
}

// The specifier in an `import x from "…"`, an `export … from "…"`, a bare
// `import "…"` or a dynamic `import("…")`. Minified vendor builds put all of
// those mid-line, so this matches on the token rather than the line. A quoted
// string inside code that happens to look like one is harmless: the bundle only
// ever acts on specifiers that resolve to a file the renderer actually ships.
const QRegularExpression &moduleSpecifierPattern()
{
    static const QRegularExpression pattern(
        QStringLiteral(R"((?:\bfrom|\bimport)\s*\(?\s*(['"])([^'"\n]*)\1)"));
    return pattern;
}

// True when a script uses ES module syntax, and so cannot be loaded with a
// plain <script src>.
bool looksLikeModule(const QString &source)
{
    const QStringList lines = source.split(u'\n');
    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.startsWith(QStringLiteral("import ")) ||
            trimmed.startsWith(QStringLiteral("import{")) ||
            trimmed.startsWith(QStringLiteral("import'")) ||
            trimmed.startsWith(QStringLiteral("import\"")) ||
            trimmed.startsWith(QStringLiteral("export ")) ||
            trimmed.startsWith(QStringLiteral("export{")) ||
            trimmed.startsWith(QStringLiteral("export*")))
            return true;
    }
    // A minified build is one long line ending in `export{…}`.
    return source.contains(QStringLiteral("export{")) ||
           source.contains(QStringLiteral("export {")) ||
           source.contains(QStringLiteral("export default"));
}

// Every renderer file, keyed by its path relative to the renderer directory.
// Qt resources and a source checkout both walk the same way.
QStringList rendererFiles(const QString &rendererDir)
{
    QStringList found;
    // Walk the cleaned path: QDirIterator echoes back whatever root it was
    // given, and a caller's "build-tests/../src/renderer" must still match.
    const QString root = QDir::cleanPath(rendererDir);
    const QString prefix = root + u'/';
    QDirIterator it(root, QDir::Files | QDir::Readable, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        if (path.startsWith(prefix))
            found += path.mid(prefix.size());
    }
    found.sort();
    return found;
}

const char *kBundleCss = R"CSS(/* Omapresent published deck — the chrome around the shared renderer.
   Every colour comes from assets/theme.css. There are no styling knobs here
   (spec §1.2, §4.6); this file only places the parts the renderer does not own:
   the notes subtitles, the small chrome bar, and the long read's measure. */

html, body { height: 100%; }

body {
    margin: 0;
    background: var(--op-background, #111);
    color: var(--op-foreground, #eee);
    font-family: var(--op-font-body, system-ui, sans-serif);
    font-size: calc(1rem * var(--op-text-scale, 1));
    -webkit-text-size-adjust: 100%;
}

a { color: var(--op-accent, #8ab); }

.op-chrome {
    display: flex;
    align-items: center;
    gap: 0.75rem;
    font-size: 0.8rem;
    color: var(--op-muted, #999);
}

.op-chrome button {
    font: inherit;
    color: inherit;
    background: none;
    border: 1px solid var(--op-selection, #444);
    border-radius: 4px;
    padding: 0.15rem 0.5rem;
    cursor: pointer;
}

.op-chrome button:hover,
.op-chrome button:focus-visible { border-color: var(--op-accent, #8ab); }
.op-chrome button[aria-pressed="true"] { color: var(--op-foreground, #eee); }
.op-chrome a { text-decoration: none; }
.op-chrome a:hover { text-decoration: underline; }

.op-failure {
    margin: 4rem auto;
    max-width: 32rem;
    padding: 0 1.25rem;
    color: var(--op-muted, #999);
    text-align: center;
}

/* ---- deck view --------------------------------------------------------- */

[data-op-view="deck"] body {
    display: flex;
    flex-direction: column;
    overflow: hidden;
}

[data-op-view="deck"] #deck {
    flex: 1 1 auto;
    min-height: 0;
    position: relative;
}

[data-op-view="deck"] .op-chrome {
    position: fixed;
    top: 0;
    right: 0;
    padding: 0.6rem 0.9rem;
    opacity: 0.3;
    transition: opacity 0.15s ease-in-out;
    z-index: 2;
}

[data-op-view="deck"] .op-chrome:hover,
[data-op-view="deck"] .op-chrome:focus-within { opacity: 1; }

/* Speaker notes as subtitles beneath the slide (spec §9.1). */
.op-notes {
    flex: 0 0 auto;
    margin: 0;
    padding: 0.85rem 1.5rem 1.05rem;
    max-height: 32vh;
    overflow-y: auto;
    background: var(--op-dark-background, #000);
    color: var(--op-muted, #999);
    border-top: 1px solid var(--op-selection, #333);
    font-size: 0.95rem;
    line-height: 1.5;
}

.op-notes[hidden] { display: none; }
.op-notes > :first-child { margin-top: 0; }
.op-notes > :last-child { margin-bottom: 0; }

#op-progress {
    flex: 0 0 auto;
    height: 2px;
    background: var(--op-selection, #333);
}

#op-progress-bar {
    height: 100%;
    width: 0;
    background: var(--op-accent, #8ab);
    transition: width 0.15s ease-out;
}

/* ---- long read --------------------------------------------------------- */

[data-op-view="read"] body { overflow-y: auto; }

.op-masthead,
[data-op-view="read"] #deck {
    max-width: 38rem;
    margin: 0 auto;
    padding: 0 1.25rem;
}

.op-masthead {
    padding-top: 4rem;
    padding-bottom: 2rem;
    border-bottom: 1px solid var(--op-selection, #333);
    margin-bottom: 3rem;
}

.op-masthead h1 {
    margin: 0 0 0.5rem;
    font-size: 2.2rem;
    line-height: 1.15;
}

.op-masthead p {
    margin: 0 0 1rem;
    color: var(--op-muted, #999);
    font-size: 0.9rem;
}

[data-op-view="read"] #deck {
    padding-bottom: 6rem;
    line-height: 1.65;
}

/* The renderer sizes a slide to a screen. In the long read a slide is just a
   section of the article, so it stops being a viewport and starts flowing. */
[data-op-view="read"] #deck section,
[data-op-view="read"] #deck .op-slide {
    display: block;
    width: auto;
    height: auto;
    min-height: 0;
    padding: 0;
    margin: 0 0 3.5rem;
    text-align: left;
}

[data-op-view="read"] #deck section + section,
[data-op-view="read"] #deck .op-slide + .op-slide {
    border-top: 1px solid var(--op-selection, #333);
    padding-top: 3.5rem;
}

[data-op-view="read"] #deck h1,
[data-op-view="read"] #deck h2,
[data-op-view="read"] #deck h3 {
    line-height: 1.2;
    margin: 2.4rem 0 1rem;
}

[data-op-view="read"] #deck p,
[data-op-view="read"] #deck ul,
[data-op-view="read"] #deck ol,
[data-op-view="read"] #deck table { margin: 0 0 1.3rem; }

[data-op-view="read"] #deck img,
[data-op-view="read"] #deck video,
[data-op-view="read"] #deck iframe,
[data-op-view="read"] #deck canvas,
[data-op-view="read"] #deck svg {
    display: block;
    max-width: 100%;
    height: auto;
    margin: 2rem auto;
}

[data-op-view="read"] #deck pre {
    overflow-x: auto;
    padding: 1rem;
    background: var(--op-dark-background, #000);
    border-radius: 4px;
}

[data-op-view="read"] #deck code,
[data-op-view="read"] #deck pre { font-family: var(--op-font-mono, ui-monospace, monospace); }

[data-op-view="read"] #deck blockquote {
    margin: 1.5rem 0;
    padding-left: 1rem;
    border-left: 3px solid var(--op-accent, #8ab);
    color: var(--op-muted, #999);
}

[data-op-view="read"] #deck table { width: 100%; border-collapse: collapse; }

[data-op-view="read"] #deck th,
[data-op-view="read"] #deck td {
    border-bottom: 1px solid var(--op-selection, #333);
    padding: 0.4rem 0.6rem;
    text-align: left;
}

[data-op-view="read"] .op-chrome {
    max-width: 38rem;
    margin: 0 auto;
    padding: 1rem 1.25rem 0;
}

[data-op-view="read"] .op-footer {
    max-width: 38rem;
    margin: 0 auto;
    padding: 2rem 1.25rem 4rem;
    border-top: 1px solid var(--op-selection, #333);
    color: var(--op-muted, #999);
    font-size: 0.85rem;
}
)CSS";

const char *kBundleJs = R"JS(/* Omapresent published deck — page chrome around the shared renderer.
   The renderer draws the deck (docs/renderer-contract.md §2); this file drives
   it: navigation, the notes subtitles, and the link between the two views.
   Plain script, no imports, no fetches: it has to run from an unzipped
   folder with no server behind it. */
(function () {
    "use strict";

    var view = document.documentElement.getAttribute("data-op-view") || "deck";
    var deck = JSON.parse(document.getElementById("op-deck").textContent);
    var frontmatter = deck.frontmatter || {};

    function stored(key) {
        try { return window.localStorage.getItem("omapresent." + key); }
        catch (error) { return null; }
    }

    function remember(key, value) {
        try { window.localStorage.setItem("omapresent." + key, value); }
        catch (error) { /* a private window, or a page with no storage */ }
    }

    function fail(message) {
        var note = document.createElement("p");
        note.className = "op-failure";
        note.textContent = message;
        document.body.appendChild(note);
    }

    function startRead(api) {
        api.render(deck);
    }

    function startDeck(api) {
        var notes = document.getElementById("op-notes");
        var notesBody = document.getElementById("op-notes-body");
        var toggle = document.getElementById("op-notes-toggle");
        var counter = document.getElementById("op-counter");
        var progress = document.getElementById("op-progress");
        var progressBar = document.getElementById("op-progress-bar");
        var showNotes = stored("notes") !== "off";
        var hasNotes = false;

        if (frontmatter.progress !== true) progress.hidden = true;
        if (frontmatter["slide-numbers"] === false) counter.hidden = true;

        function applyNotes() {
            notes.hidden = !(showNotes && hasNotes);
            toggle.setAttribute("aria-pressed", showNotes ? "true" : "false");
        }

        function toggleNotes() {
            showNotes = !showNotes;
            remember("notes", showNotes ? "on" : "off");
            applyNotes();
        }

        api.onState = function (state) {
            var count = state.slideCount || 0;
            counter.textContent = (state.slideIndex + 1) + " / " + count;
            if (count > 0)
                progressBar.style.width = ((state.slideIndex + 1) / count * 100) + "%";
            notesBody.innerHTML = state.notesHtml || "";
            hasNotes = !!(state.notesHtml && state.notesHtml.trim());
            applyNotes();
        };

        toggle.addEventListener("click", toggleNotes);
        document.getElementById("op-previous").addEventListener("click", function () {
            api.previous();
        });
        document.getElementById("op-next").addEventListener("click", function () {
            api.next();
        });

        document.addEventListener("keydown", function (event) {
            if (event.metaKey || event.ctrlKey || event.altKey) return;
            var key = event.key;
            if (key === "ArrowRight" || key === "ArrowDown" || key === "PageDown" ||
                key === " " || key === "Enter") {
                api.next();
            } else if (key === "ArrowLeft" || key === "ArrowUp" || key === "PageUp" ||
                       key === "Backspace") {
                api.previous();
            } else if (key === "Home") {
                api.goto(0);
            } else if (key === "End") {
                api.goto((deck.slides || []).length - 1);
            } else if (key === "n" || key === "N") {
                toggleNotes();
            } else if (key === "f" || key === "F") {
                if (document.fullscreenElement) document.exitFullscreen();
                else document.documentElement.requestFullscreen();
            } else {
                return;
            }
            event.preventDefault();
        });

        // Swipe: a horizontal drag that beats both the noise floor and any
        // vertical intent, so scrolling a long slide never changes slide.
        var touchX = 0;
        var touchY = 0;
        document.addEventListener("touchstart", function (event) {
            touchX = event.changedTouches[0].clientX;
            touchY = event.changedTouches[0].clientY;
        }, { passive: true });
        document.addEventListener("touchend", function (event) {
            var dx = event.changedTouches[0].clientX - touchX;
            var dy = event.changedTouches[0].clientY - touchY;
            if (Math.abs(dx) < 50 || Math.abs(dx) < Math.abs(dy)) return;
            if (dx < 0) api.next();
            else api.previous();
        }, { passive: true });

        applyNotes();
        api.render(deck);
    }

    function start() {
        var api = window.omapresent;
        if (!api || typeof api.render !== "function") {
            fail("This deck could not start: the renderer did not load.");
            return;
        }
        if (view === "read") startRead(api);
        else startDeck(api);
    }

    // assets/render.js starts the renderer's entry module asynchronously, so
    // both it and the document have to be ready before the deck is driven.
    function whenReady() {
        Promise.resolve(window.omapresentReady).then(start, function (error) {
            fail("This deck could not start: " + error);
        });
    }

    if (document.readyState === "loading")
        document.addEventListener("DOMContentLoaded", whenReady);
    else
        whenReady();
})();
)JS";

} // namespace

struct WebBundle::Private {
    QString rendererDir = QStringLiteral(":/renderer");
    QString outputRoot;
    bool outputRootExisted = false;
    // Absolute paths of everything this build made, for the rollback that keeps
    // a failure from leaving half a bundle behind.
    QStringList writtenPaths;
    QStringList createdDirs;
    // Absolute source path -> "media/<stable-name>".
    QMap<QString, QString> media;
    // Bundle-relative paths, in the order the pages must load them.
    QStringList styleSheets;
    QStringList classicScripts;
};

WebBundle::WebBundle(QObject *parent) : QObject(parent), d(new Private) {}

WebBundle::~WebBundle()
{
    delete d;
}

void WebBundle::setDeck(const QJsonObject &deck)
{
    m_deck = deck;
}

void WebBundle::setDeckDir(const QString &dir)
{
    m_deckDir = dir;
}

void WebBundle::setRendererDir(const QString &rendererDir)
{
    d->rendererDir = rendererDir.isEmpty() ? QStringLiteral(":/renderer") : rendererDir;
}

QString WebBundle::rendererDir() const
{
    return d->rendererDir;
}

QStringList WebBundle::files() const
{
    return m_files;
}

qint64 WebBundle::totalBytes() const
{
    return m_totalBytes;
}

QString WebBundle::lastError() const
{
    return m_lastError;
}

QString WebBundle::mediaFileName(const QString &sourcePath)
{
    const QFileInfo info(sourcePath);
    QString stem = slugged(info.completeBaseName());
    if (stem.isEmpty())
        stem = QStringLiteral("asset");
    stem.truncate(48);

    QString suffix = slugged(info.suffix());
    if (!suffix.isEmpty())
        suffix.prepend(u'.');

    // The digest is of the source path, not the bytes: the same picture keeps
    // the same name when it is edited, so republishing does not churn every
    // file, and two "budget.png" in different folders never collide.
    const QString digest = QString::fromLatin1(
        QCryptographicHash::hash(QDir::cleanPath(info.absoluteFilePath()).toUtf8(),
                                 QCryptographicHash::Sha256)
            .toHex()
            .left(8));

    return QStringLiteral("%1-%2%3").arg(stem, digest, suffix);
}

QStringList WebBundle::moduleImports(const QString &source)
{
    QStringList specifiers;
    QRegularExpressionMatchIterator matches = moduleSpecifierPattern().globalMatch(source);
    while (matches.hasNext()) {
        const QString specifier = matches.next().captured(2);
        if (specifier.startsWith(u'.'))
            specifiers += specifier;
    }
    return specifiers;
}

QString WebBundle::withModuleImports(const QString &source,
                                     const QHash<QString, QString> &replacements)
{
    QString rewritten;
    rewritten.reserve(source.size());
    qsizetype copied = 0;

    QRegularExpressionMatchIterator matches = moduleSpecifierPattern().globalMatch(source);
    while (matches.hasNext()) {
        const QRegularExpressionMatch match = matches.next();
        const auto replacement = replacements.constFind(match.captured(2));
        if (replacement == replacements.constEnd())
            continue;
        rewritten += source.mid(copied, match.capturedStart(2) - copied);
        rewritten += *replacement;
        copied = match.capturedEnd(2);
    }
    rewritten += source.mid(copied);
    return rewritten;
}

bool WebBundle::fail(const QString &message)
{
    m_lastError = message;
    rollback();
    m_files.clear();
    m_totalBytes = 0;
    return false;
}

void WebBundle::rollback()
{
    for (const QString &path : std::as_const(d->writtenPaths))
        QFile::remove(path);

    // Deepest first, so a directory is empty by the time we reach it.
    QStringList dirs = d->createdDirs;
    std::sort(dirs.begin(), dirs.end(),
              [](const QString &a, const QString &b) { return a.size() > b.size(); });
    for (const QString &path : std::as_const(dirs))
        QDir().rmdir(path);

    d->writtenPaths.clear();
    d->createdDirs.clear();
}

bool WebBundle::ensureDirectory(const QString &relativeDir)
{
    QDir root(d->outputRoot);
    const QString absolute = relativeDir.isEmpty()
        ? d->outputRoot
        : QDir::cleanPath(root.filePath(relativeDir));
    if (QFileInfo::exists(absolute))
        return QFileInfo(absolute).isDir();

    if (!root.mkpath(relativeDir.isEmpty() ? QStringLiteral(".") : relativeDir))
        return false;

    // Remember every level we made, so a rollback can unmake exactly those.
    QString walked = d->outputRoot;
    const QStringList parts = relativeDir.split(u'/', Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        walked = QDir::cleanPath(walked + u'/' + part);
        if (!d->createdDirs.contains(walked))
            d->createdDirs += walked;
    }
    return true;
}

bool WebBundle::writeFile(const QString &relativePath, const QByteArray &data)
{
    const QString parent = QFileInfo(relativePath).path();
    if (!ensureDirectory(parent == QStringLiteral(".") ? QString() : parent))
        return fail(QStringLiteral("Could not create %1/%2 — check that %3 is writable.")
                        .arg(d->outputRoot, parent, d->outputRoot));

    const QString absolute = QDir(d->outputRoot).filePath(relativePath);
    QFile file(absolute);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return fail(QStringLiteral("Could not write %1: %2").arg(absolute, file.errorString()));
    if (file.write(data) != data.size())
        return fail(QStringLiteral("Could not write %1: %2").arg(absolute, file.errorString()));
    file.close();

    d->writtenPaths += absolute;
    m_files += relativePath;
    m_totalBytes += data.size();
    return true;
}

bool WebBundle::copyFile(const QString &sourcePath, const QString &relativePath)
{
    QFile source(sourcePath);
    if (!source.open(QIODevice::ReadOnly))
        return fail(QStringLiteral("Could not read %1: %2")
                        .arg(sourcePath, source.errorString()));
    return writeFile(relativePath, source.readAll());
}

bool WebBundle::buildRenderer()
{
    const QString rendererRoot = QDir::cleanPath(d->rendererDir);
    const QStringList sources = rendererFiles(rendererRoot);
    if (!sources.contains(QStringLiteral("render.js")))
        return fail(QStringLiteral("The renderer bundle is missing: no render.js in %1.")
                        .arg(rendererRoot));

    // Depth-first from render.js, so a module is created after everything it
    // imports and the loader below can wire them up in one pass.
    QStringList order;
    QSet<QString> seen;
    QHash<QString, QString> marked;
    bool readable = true;
    std::function<void(const QString &)> visit = [&](const QString &relative) {
        if (seen.contains(relative) || !readable)
            return;
        seen += relative;

        QFile file(QDir(rendererRoot).filePath(relative));
        if (!file.open(QIODevice::ReadOnly)) {
            fail(QStringLiteral("Could not read the renderer's %1: %2")
                     .arg(relative, file.errorString()));
            readable = false;
            return;
        }
        const QString source = QString::fromUtf8(file.readAll());

        const QString dir = QFileInfo(relative).path();
        QHash<QString, QString> replacements;
        for (const QString &specifier : moduleImports(source)) {
            const QString target = QDir::cleanPath(
                dir == QStringLiteral(".") ? specifier : dir + u'/' + specifier);
            // A specifier that is not one of the renderer's own files is left
            // exactly as written; there is nothing here to point it at.
            if (!sources.contains(target))
                continue;
            visit(target);
            replacements.insert(specifier, QString::fromLatin1(kModuleMarker) + target);
        }

        marked.insert(relative, withModuleImports(source, replacements));
        order += relative;
    };
    visit(QStringLiteral("render.js"));
    if (!readable)
        return false;

    if (!writeFile(QStringLiteral("assets/render.js"), rendererLoader(order, marked)))
        return false;

    // Everything else travels verbatim: stylesheets and vendored libraries the
    // pages link, licences and fonts they do not.
    for (const QString &relative : sources) {
        if (order.contains(relative))
            continue;
        if (relative.endsWith(QStringLiteral(".html")))
            continue;   // render.html is the in-app entry point; the bundle has its own
        // Build-time files: how the renderer is compiled in and how its own
        // tests run is nobody's business once a deck is published.
        if (relative.endsWith(QStringLiteral(".qrc")) ||
            relative.endsWith(QStringLiteral(".map")) ||
            QFileInfo(relative).fileName() == QStringLiteral("package.json"))
            continue;

        const QString destination = QStringLiteral("assets/") + relative;

        if (relative.endsWith(QStringLiteral(".js")) || relative.endsWith(QStringLiteral(".mjs"))) {
            QFile file(QDir(rendererRoot).filePath(relative));
            if (!file.open(QIODevice::ReadOnly))
                return fail(QStringLiteral("Could not read the renderer's %1: %2")
                                .arg(relative, file.errorString()));
            const QByteArray script = file.readAll();
            // A module nothing imported is either already inside the loader or
            // unused, and there is no way to load it on its own. A plain script
            // is a vendored library expected as a global, so the pages load it
            // ahead of the renderer.
            if (looksLikeModule(QString::fromUtf8(script)))
                continue;
            if (!writeFile(destination, script))
                return false;
            d->classicScripts += destination;
            continue;
        }

        if (!copyFile(QDir(rendererRoot).filePath(relative), destination))
            return false;
        if (relative.endsWith(QStringLiteral(".css")))
            d->styleSheets += destination;
    }

    // deck.css after the vendored sheets it overrides, then the bundle's chrome.
    std::sort(d->styleSheets.begin(), d->styleSheets.end(),
              [](const QString &a, const QString &b) {
                  const bool aDeck = a == QStringLiteral("assets/deck.css");
                  const bool bDeck = b == QStringLiteral("assets/deck.css");
                  if (aDeck != bDeck)
                      return bDeck;
                  return a < b;
              });
    return true;
}

// The renderer is an ES module graph, and a browser will not fetch an ES module
// from a page opened off the disk — which is exactly how a published bundle
// gets read. So the modules travel inside this script as text, and the loader
// turns each one into a blob: URL, rewriting the marked specifiers to the URLs
// of the modules already made. The module system does the rest, so a minified
// vendor build with a default export behaves exactly as it does in the app.
QByteArray WebBundle::rendererLoader(const QStringList &order,
                                     const QHash<QString, QString> &sources) const
{
    QStringList js;
    js += QStringLiteral(
        "/* Omapresent — the renderer of src/renderer/, packaged so a published\n"
        "   deck runs from a folder as well as from a server. Generated: edit\n"
        "   the modules, not this. */");
    js += QStringLiteral("(function () {");
    js += QStringLiteral("    \"use strict\";");
    js += QStringLiteral("    var marker = %1;").arg(jsString(QString::fromLatin1(kModuleMarker)));
    js += QStringLiteral("    var order = [");
    for (const QString &name : order)
        js += QStringLiteral("        %1,").arg(jsString(name));
    js += QStringLiteral("    ];");
    js += QStringLiteral("    var sources = {");
    for (const QString &name : order)
        js += QStringLiteral("        %1: %2,").arg(jsString(name), jsString(sources.value(name)));
    js += QStringLiteral("    };");
    js += QStringLiteral("    var urls = {};");
    js += QStringLiteral("    for (var i = 0; i < order.length; i++) {");
    js += QStringLiteral("        var source = sources[order[i]];");
    js += QStringLiteral("        for (var name in urls)");
    js += QStringLiteral("            source = source.split(marker + name).join(urls[name]);");
    js += QStringLiteral("        urls[order[i]] = URL.createObjectURL(");
    js += QStringLiteral("            new Blob([source], { type: \"text/javascript\" }));");
    js += QStringLiteral("    }");
    js += QStringLiteral("    // The pages wait on this before they drive the renderer.");
    js += QStringLiteral("    window.omapresentReady = import(urls[order[order.length - 1]]);");
    js += QStringLiteral("})();");
    js += QString();
    return js.join(u'\n').toUtf8();
}

void WebBundle::collectMedia()
{
    d->media.clear();

    auto take = [this](const QString &value) {
        const QString path = localPathFor(value, m_deckDir);
        if (path.isEmpty() || d->media.contains(path))
            return;
        const QFileInfo info(path);
        if (!info.isFile() || !info.isReadable())
            return;   // a stale or missing asset: the renderer draws the placeholder
        d->media.insert(path, QStringLiteral("media/") + mediaFileName(path));
    };

    const QJsonObject assets = m_deck.value(QStringLiteral("assets")).toObject();
    for (auto it = assets.constBegin(); it != assets.constEnd(); ++it)
        take(it.value().toString());

    const QJsonObject media = m_deck.value(QStringLiteral("media")).toObject();
    for (auto it = media.constBegin(); it != media.constEnd(); ++it) {
        const QJsonObject description = it.value().toObject();
        take(description.value(QStringLiteral("cachedFile")).toString());
        take(description.value(QStringLiteral("poster")).toString());
    }

    take(m_deck.value(QStringLiteral("backgroundImage")).toString());
}

QJsonObject WebBundle::deckForPage(const QString &prefix, const QString &view) const
{
    auto rewrite = [this, &prefix](const QString &value) {
        const QString path = localPathFor(value, m_deckDir);
        const QString name = d->media.value(path);
        // Anything that did not make it into media/ — an unresolved reference,
        // a remote image we cannot vendor — becomes the missing-asset
        // placeholder rather than a link off the bundle.
        return name.isEmpty() ? QString() : prefix + name;
    };

    QJsonObject deck = m_deck;
    deck.insert(QStringLiteral("mode"), QStringLiteral("web"));
    deck.insert(QStringLiteral("view"), view);

    QJsonObject assets = deck.value(QStringLiteral("assets")).toObject();
    for (const QString &key : assets.keys())
        assets.insert(key, rewrite(assets.value(key).toString()));
    deck.insert(QStringLiteral("assets"), assets);

    QJsonObject media = deck.value(QStringLiteral("media")).toObject();
    for (const QString &key : media.keys()) {
        QJsonObject description = media.value(key).toObject();
        description.insert(QStringLiteral("cachedFile"),
                           rewrite(description.value(QStringLiteral("cachedFile")).toString()));
        description.insert(QStringLiteral("poster"),
                           rewrite(description.value(QStringLiteral("poster")).toString()));
        media.insert(key, description);
    }
    deck.insert(QStringLiteral("media"), media);

    deck.insert(QStringLiteral("backgroundImage"),
                rewrite(deck.value(QStringLiteral("backgroundImage")).toString()));
    return deck;
}

QByteArray WebBundle::themeCss() const
{
    const QJsonObject palette = m_deck.value(QStringLiteral("palette")).toObject();
    const QJsonObject frontmatter = m_deck.value(QStringLiteral("frontmatter")).toObject();

    QStringList lines;
    lines += QStringLiteral(
        "/* Generated from the deck's palette (spec §6). A published deck keeps\n"
        "   the theme it was written in; it does not follow the reader's\n"
        "   desktop, and there is no second theme invented for it here. */");
    lines += QStringLiteral(":root {");

    const QString mode = palette.value(QStringLiteral("mode")).toString();
    if (mode == QStringLiteral("light") || mode == QStringLiteral("dark"))
        lines += QStringLiteral("    color-scheme: %1;").arg(mode);

    for (auto it = palette.constBegin(); it != palette.constEnd(); ++it) {
        if (it.key() == QStringLiteral("mode"))
            continue;
        if (it.key() == QStringLiteral("ansi")) {
            const QJsonArray ansi = it.value().toArray();
            for (int i = 0; i < ansi.size(); ++i) {
                const QString colour = ansi.at(i).toString();
                if (isHexColour(colour))
                    lines += QStringLiteral("    --op-ansi-%1: %2;").arg(i).arg(colour);
            }
            continue;
        }
        const QString colour = it.value().toString();
        if (isHexColour(colour)) {
            QString name = it.key();
            name.replace(u'_', u'-');
            lines += QStringLiteral("    --op-%1: %2;").arg(name, colour);
        }
    }

    lines += QStringLiteral("    --op-font-body: %1system-ui, -apple-system, sans-serif;")
                 .arg(cssFontFamily(frontmatter.value(QStringLiteral("font")).toString()));
    lines += QStringLiteral(
        "    --op-font-mono: ui-monospace, SFMono-Regular, Menlo, monospace;");

    const double scale = m_deck.value(QStringLiteral("textScale")).toDouble(1.0);
    lines += QStringLiteral("    --op-text-scale: %1;")
                 .arg(scale > 0.0 ? scale : 1.0, 0, 'g', 4);
    lines += QStringLiteral("}");
    lines += QString();
    return lines.join(u'\n').toUtf8();
}

QString WebBundle::deckTitle() const
{
    const QJsonObject frontmatter = m_deck.value(QStringLiteral("frontmatter")).toObject();
    const QJsonObject publish = frontmatter.value(QStringLiteral("publish")).toObject();

    QString title = publish.value(QStringLiteral("title")).toString().trimmed();
    if (title.isEmpty())
        title = frontmatter.value(QStringLiteral("title")).toString().trimmed();
    if (title.isEmpty())
        title = publish.value(QStringLiteral("slug")).toString().trimmed();
    if (title.isEmpty())
        title = QStringLiteral("Presentation");
    return title;
}

QByteArray WebBundle::page(const QString &view) const
{
    const bool read = view == QStringLiteral("read");
    const QString prefix = read ? QStringLiteral("../") : QString();
    const QString title = deckTitle();
    const QJsonObject frontmatter = m_deck.value(QStringLiteral("frontmatter")).toObject();

    QStringList head;
    for (const QString &sheet : std::as_const(d->styleSheets))
        head += QStringLiteral("<link rel=\"stylesheet\" href=\"%1%2\">").arg(prefix, sheet);

    QStringList scripts;
    for (const QString &script : std::as_const(d->classicScripts))
        scripts += QStringLiteral("<script src=\"%1%2\"></script>").arg(prefix, script);

    QStringList html;
    html += QStringLiteral("<!doctype html>");
    html += QStringLiteral("<html lang=\"en\" data-op-view=\"%1\">").arg(read ? QStringLiteral("read") : QStringLiteral("deck"));
    html += QStringLiteral("<head>");
    html += QStringLiteral("<meta charset=\"utf-8\">");
    html += QStringLiteral(
        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1, "
        "viewport-fit=cover\">");
    html += QStringLiteral("<title>%1</title>").arg(htmlEscaped(title));
    html += QStringLiteral("<link rel=\"stylesheet\" href=\"%1assets/theme.css\">").arg(prefix);
    html += head;
    html += QStringLiteral("<link rel=\"stylesheet\" href=\"%1assets/bundle.css\">").arg(prefix);
    html += QStringLiteral("</head>");
    html += QStringLiteral("<body>");

    if (read) {
        const QString author = frontmatter.value(QStringLiteral("author")).toString();
        const QString date = frontmatter.value(QStringLiteral("date")).toString();
        QStringList byline;
        if (!author.isEmpty())
            byline += htmlEscaped(author);
        if (!date.isEmpty())
            byline += htmlEscaped(date);

        html += QStringLiteral("<nav class=\"op-chrome\">");
        html += QStringLiteral("<a href=\"../index.html\">&larr; View as slides</a>");
        html += QStringLiteral("</nav>");
        html += QStringLiteral("<header class=\"op-masthead\">");
        html += QStringLiteral("<h1>%1</h1>").arg(htmlEscaped(title));
        if (!byline.isEmpty())
            html += QStringLiteral("<p>%1</p>").arg(byline.join(QStringLiteral(" &middot; ")));
        html += QStringLiteral("</header>");
        html += QStringLiteral("<main id=\"deck\"></main>");
        html += QStringLiteral("<footer class=\"op-footer\">");
        html += QStringLiteral("<a href=\"../index.html\">View as slides</a>");
        html += QStringLiteral("</footer>");
    } else {
        html += QStringLiteral("<nav class=\"op-chrome\">");
        html += QStringLiteral(
            "<button id=\"op-previous\" type=\"button\" aria-label=\"Previous slide\">"
            "&#8249;</button>");
        html += QStringLiteral("<span id=\"op-counter\"></span>");
        html += QStringLiteral(
            "<button id=\"op-next\" type=\"button\" aria-label=\"Next slide\">"
            "&#8250;</button>");
        html += QStringLiteral(
            "<button id=\"op-notes-toggle\" type=\"button\" aria-pressed=\"true\">"
            "Notes</button>");
        html += QStringLiteral("<a href=\"read/index.html\">Read as article &rarr;</a>");
        html += QStringLiteral("</nav>");
        html += QStringLiteral("<main id=\"deck\"></main>");
        html += QStringLiteral(
            "<figure class=\"op-notes\" id=\"op-notes\" hidden>"
            "<div id=\"op-notes-body\"></div></figure>");
        html += QStringLiteral("<div id=\"op-progress\"><div id=\"op-progress-bar\"></div></div>");
    }

    html += QStringLiteral("<noscript><p class=\"op-failure\">This deck needs "
                           "JavaScript to render.</p></noscript>");
    html += QStringLiteral("<script type=\"application/json\" id=\"op-deck\">%1</script>")
                .arg(jsonForScriptTag(deckForPage(prefix, read ? QStringLiteral("read")
                                                               : QStringLiteral("deck"))));
    html += scripts;
    html += QStringLiteral("<script src=\"%1assets/render.js\"></script>").arg(prefix);
    html += QStringLiteral("<script src=\"%1assets/bundle.js\"></script>").arg(prefix);
    html += QStringLiteral("</body>");
    html += QStringLiteral("</html>");
    html += QString();
    return html.join(u'\n').toUtf8();
}

bool WebBundle::build(const QString &outputDir)
{
    m_files.clear();
    m_totalBytes = 0;
    m_lastError.clear();
    d->writtenPaths.clear();
    d->createdDirs.clear();
    d->styleSheets.clear();
    d->classicScripts.clear();

    if (m_deck.isEmpty())
        return fail(QStringLiteral("There is no deck to publish."));
    if (outputDir.trimmed().isEmpty())
        return fail(QStringLiteral("No output directory was given."));

    d->outputRoot = QDir::cleanPath(QFileInfo(outputDir).absoluteFilePath());
    d->outputRootExisted = QFileInfo::exists(d->outputRoot);
    if (d->outputRootExisted && !QFileInfo(d->outputRoot).isDir())
        return fail(QStringLiteral("%1 is a file, not a directory.").arg(d->outputRoot));
    if (!QDir().mkpath(d->outputRoot))
        return fail(QStringLiteral("Could not create the bundle directory %1. "
                                   "Check that its parent exists and is writable.")
                        .arg(d->outputRoot));
    if (!d->outputRootExisted)
        d->createdDirs += d->outputRoot;

    collectMedia();

    if (!buildRenderer())
        return false;

    // The renderer's own file count is only known once it has been flattened,
    // so the first progress report is already against the real total.
    const int total = m_files.size() + int(d->media.size()) + 5;
    int done = m_files.size();
    auto step = [this, &done, total](const QString &what) {
        emit progress(++done, total, what);
    };
    emit progress(done, total, QStringLiteral("the renderer"));

    if (!writeFile(QStringLiteral("assets/theme.css"), themeCss()))
        return false;
    step(QStringLiteral("assets/theme.css"));

    if (!writeFile(QStringLiteral("assets/bundle.css"), QByteArray(kBundleCss)))
        return false;
    step(QStringLiteral("assets/bundle.css"));

    if (!writeFile(QStringLiteral("assets/bundle.js"), QByteArray(kBundleJs)))
        return false;
    step(QStringLiteral("assets/bundle.js"));

    for (auto it = d->media.constBegin(); it != d->media.constEnd(); ++it) {
        if (!copyFile(it.key(), it.value()))
            return false;
        step(it.value());
    }

    if (!writeFile(QStringLiteral("index.html"), page(QStringLiteral("deck"))))
        return false;
    step(QStringLiteral("index.html"));

    if (!writeFile(QStringLiteral("read/index.html"), page(QStringLiteral("read"))))
        return false;
    step(QStringLiteral("read/index.html"));

    m_files.sort();
    return true;
}
