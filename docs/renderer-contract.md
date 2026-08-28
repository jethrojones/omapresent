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

C++ receives it through `QWebEngineScript` / `runJavaScript`; the host wires
`onState` to a `qt.webChannelTransport`-free callback via
`page->runJavaScript()` polling is NOT acceptable — use a `QWebChannel` object
registered as `omapresentHost` with a single slot `state(QString json)`, and
the renderer calls `omapresentHost.state(JSON.stringify(s))` when it exists.

## 3. Screen vs. notes

The renderer classifies each block per spec §4.2. Audience content: headings,
indented outline lines, lists, code, tables, block quotes, math, images,
video embeds, bare-URL QR codes. Speaker notes: plain paragraph prose. In
`presenter` and `web` roles notes are rendered as formatted Markdown; in
`audience`, `pdf` and `export` they are omitted entirely.

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

## 6. Testability

`render.js` must keep its pure logic — comment stripping is C++'s job, but
block splitting, screen/notes classification, bento arrangement, fragment
counting and URL/QR decisions are the renderer's — in ES modules under
`src/renderer/` that `node --test` can import directly with no DOM. Tests live
in `tests/renderer/*.test.mjs` and run via `bin/test`.

## 7. Offline

No network at runtime. Vendored `markdown-it`, `katex` and a QR library live
in `src/renderer/vendor/` with their licences in
`src/renderer/vendor/LICENSES.md`. The only network calls in the whole app are
the explicit video pre-fetch (spec §4.8) and publish (spec §9), both in C++.
