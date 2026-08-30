# Renderer contract

One HTML/CSS/JS bundle in `src/renderer/` renders **all four** outputs — live
preview, the presentation windows, PDF export and web publish (spec §3). The
C++ side never renders Markdown. This file is the frozen interface between
them. Change it only by changing this document first.

## 1. The deck document

C++ builds one JSON object and hands it to the page. `DeckModel::toJson()`
produces the `frontmatter` / `slides` half; the caller merges in the rest.

```jsonc
{
  "mode": "preview" | "present" | "pdf" | "web",
  "frontmatter": {              // spec §4.4, every key optional
    "title": "Quarterly Review",
    "author": "Jethro Jones",
    "date": "2026-09-01",
    "theme": "gruvbox",
    "font": "IBM Plex Sans",
    "aspect": "16:9",
    "root": "~/Documents/aibrain",
    "header": "Optimization Doc",
    "footer": "{title} — {slide}/{count}",
    "slide-numbers": true,
    "progress": true,
    "publish": { "slug": "q3-review", "provider": "herenow", "access": "link" }
  },
  "slides": [
    {
      "index": 0,               // position in the linear flow, -1 when skipped
      "markdown": "# Hello\n\n![[a.png]]\n",   // comments already stripped
      "recallKey": "q",         // "" when unbound
      "skip": false,            // true for `--- {q, skip}`
      "sourceStartLine": 12,    // 0-based, into the ORIGINAL file
      "sourceEndLine": 18
    }
  ],
  "assets": {                   // AssetIndex::resolveAll()
    "budget.png": "file:///home/jethro/Pictures/budget.png",
    "missing.png": ""           // empty -> placeholder + "missing:" tag
  },
  "media": {                    // VideoCache::describe() per bare URL
    "https://youtu.be/abc": {
      "host": "youtube", "embedUrl": "https://www.youtube.com/embed/abc",
      "cachedFile": "", "poster": "", "title": "", "vertical": false,
      "status": "embed"         // "cached" | "embed" | "qr"
    }
  },
  "palette": { /* OmarchyTheme::palette(), see src/omarchytheme.h */ },
  "backgroundImage": "file:///home/jethro/.local/state/omarchy/current/background",
  "textScale": 1.0
}
```

Everything is optional except `mode` and `slides`. The renderer must draw
something sane when a key is missing — a deck opened before the theme loads
still renders.

## 2. Page API

`src/renderer/render.html` defines exactly one global, `window.omapresent`:

```js
window.omapresent = {
  render(deckJson),            // full replace, keeps slide + scroll if it can
  update(deckJson),            // live edit: same, explicitly preserves position
  goto(slideIndex),            // clamps; resets fragments to 0
  next(),                      // next fragment, else next slide -> state event
  previous(),
  scrollBy(deltaPixels),       // within the current slide (spec §4.7)
  setScroll(fraction),         // 0..1, used to mirror presenter -> audience
  showRecall(key), hideRecall(),
  setBlank(mode),              // "black" | "white" | ""   (spec §5.2 B / W)
  setOverview(on),             // spec §5.2 O
  playPause(),                 // spec §4.8 Space targets the first player
  focusNextMedia(),            // Tab
  role: "audience" | "presenter" | "editor" | "export",
  onState: null,               // assigned by the host, see below
};
```

The host assigns `window.omapresent.onState = fn`. The renderer calls it after
every state change with:

```jsonc
{ "slideIndex": 3, "slideCount": 12, "fragment": 1, "fragmentCount": 4,
  "scrollFraction": 0.0, "scrollable": true, "recall": "",
  "blank": "", "overview": false, "heading": "Where we are",
  "notesHtml": "<p>...</p>", "nextSlideHtml": "<section>...</section>",
  "recallKeys": ["q", "1"], "mediaCount": 0 }
```

### `omapresentHost.embedBase(callback)` — added for T34

The bridge object also answers one question, and only when asked:

```js
window.omapresentHost.embedBase(base => { /* "http://127.0.0.1:<port>/<token>/" */ });
```

`base` is a loopback origin the host serves one bundled page from, or `""` when
it has none. A hosted video player refuses to configure when the page embedding
it has an opaque origin — `qrc:` and `file:` both are — and answers "Error 153"
instead, so the renderer builds that player inside a frame served from `base`.

Two properties matter and are tested:

- It is a **method, not a property**. A WebChannel sends every property at
  handshake, which would start the host's server when a deck opens. A method is
  answered only when the renderer asks, and it asks only after the reader has
  clicked Play — so spec §4.8's "nothing autoplays" and finding SEC-002's "no
  request on open" both still hold.
- It answers **asynchronously, through the callback**. A WebChannel method does
  not return its value.

The renderer asks only for a hosted embed (`media[url].status` "embed" on a
recognised host). Cached files, local files, direct video URLs and other
providers are built without asking, and never start the server. When `base` is
empty and the page's own origin is not `http(s)`, the renderer draws a QR code
and a link instead (spec §4.8's last fallback).

This is additive: `state(QString json)` remains the slot the renderer calls
after every state change, and nothing about it changes.

C++ receives it through `QWebEngineScript` / `runJavaScript`; the host wires
`onState` to a `qt.webChannelTransport`-free callback via
`page->runJavaScript()` polling is NOT acceptable — use a `QWebChannel` object
registered as `omapresentHost` whose slot for this is `state(QString json)`, and
the renderer calls `omapresentHost.state(JSON.stringify(s))` when it exists.
That object answers one further call, `embedBase(callback)`, described in §2a;
`state` remains the only slot the renderer pushes state through.

## 3. Screen vs. notes

The renderer classifies each block per spec §4.2. Audience content: headings,
indented outline lines, lists, code, tables, block quotes, math, images,
video embeds, bare-URL QR codes. Speaker notes: plain paragraph prose. In
`presenter` and `web` roles notes are rendered as formatted Markdown; in
`audience`, `pdf` and `export` they are omitted entirely.

## 3a. What a bare line is

A line whose entire content is one reference is classified once, and both sides
of the app must agree. The order is settled:

1. **Local video** — a relative or absolute path ending in `.mp4`, `.webm` or
   `.mov` is a local video, `host: "local"`. It is **not** a URL: those suffixes
   are file extensions, not TLDs, so `clip.webm` is not `https://clip.webm`, and
   `isBareUrlLine` is false for it. It is **not** an image either — image
   detection must exclude the video extensions, or `./clip.webm` would be both.
   `VideoCache::extractUrls` still returns these lines so `describe()` and
   `prefetch()` see them.
2. **Bare URL** — a whole line that is a single `http(s)` URL. A recognised
   video host becomes a player; anything else becomes a QR code with the URL
   printed beneath it (spec §4.8).
3. **Image** — a bare path per spec §4.5. A line only counts when it is
   plausibly one path: prose that merely contains a `/` (`and/or`,
   `X/Twitter`, a sentence mentioning `~/Documents`) is prose, and prose is a
   speaker note.
4. Otherwise it is ordinary Markdown.

`AssetIndex` (C++) and `deckparse.js` / `media.js` (renderer) implement this
same order. `tests/tst_integration.cpp` asserts they agree; when they disagree,
that suite is the arbiter and this section is what it is arbitrating against.

## 4. Layout grammar

Implement spec §4.6 exactly and no more. There are no document-level styling
knobs. Blocks are runs of lines separated by a blank line; blocks are centered
horizontally, the stack is centered vertically when it fits and top-aligned
with a scroll surface when it does not (spec §4.7).

## 5. Theme

`deck.css` consumes CSS custom properties only — never hard-coded colours:

```css
--op-background --op-foreground --op-accent --op-muted --op-selection
--op-dark-background --op-dark-foreground
--op-red --op-orange --op-yellow --op-green --op-cyan --op-blue
--op-magenta --op-brown  (and --op-bright-* for each)
--op-ansi-0 ... --op-ansi-15
--op-font-body --op-font-mono --op-text-scale
```

`render.js` sets them from `deck.palette` on every `render`/`update`.

An unresolved image uses `backgroundImage` as its visual fallback in preview,
presentation, PDF and both web views. The `missing:` tag is hidden only in
present mode. A missing or rejected background leaves the themed background
colour in place and keeps the tag in every other mode.

For a published bundle, ordinary local media must resolve inside the canonical
deck directory or configured asset `root`. There is one narrow wallpaper
exception. `backgroundImage` may name exactly
`$XDG_STATE_HOME/omarchy/current/background` (normally
`~/.local/state/omarchy/current/background`). The publisher accepts a regular
file there only when the state root and `current` are real directories and
their canonical paths remain nested. It accepts a symlink there only when the
real target remains inside the real, non-symlink `current/theme` directory. It
rejects every other outside path and rewrites it to empty. This rule keeps the
theme background without allowing a deck asset, parent link, or replaced
wallpaper link to package an arbitrary outside file.

## 6. Testability

`render.js` must keep its pure logic — comment stripping is C++'s job, but
block splitting, screen/notes classification, bento arrangement, fragment
counting and URL/QR decisions are the renderer's — in ES modules under
`src/renderer/` that `node --test` can import directly with no DOM. Tests live
in `tests/renderer/*.test.mjs` and run via `bin/test`.

## 7. Offline

No network at runtime. Vendored `markdown-it`, `katex` and a QR library live
in `src/renderer/vendor/` with their licences in
`src/renderer/vendor/LICENSES.md`. Rendering a deck must make zero requests.

An `http(s)` image in `assets` and an uncached remote video remain inert data.
The renderer draws a themed load or play button without putting the remote URL
in an `img`, `video`, or `iframe` source. It assigns that source only after the
reader activates the matching button. This explicit action is the only page
runtime network path. PDF output keeps the inert button.

The C++ network paths remain the explicit video pre-fetch (spec §4.8) and
publish (spec §9). A published bundle retains a remote image URL only so the
same explicit load action works there. Opening the bundle offline still renders
the complete deck shell and never attempts that URL.
