# T2 — The shared renderer bundle

**Agent:** `renderer` · **Spec:** §3, §4.2, §4.6, §4.7, §4.8, §6, §15 milestone 1

## Files you own
- `src/renderer/**` (everything: `render.html`, `render.js`, `deck.css`,
  `vendor/`, `renderer.qrc`, plus any modules you add)
- `tests/renderer/**`

Nothing else. Do not touch `src/*.cpp`, `*.h`, `*.qml` or the `.pro` files — if
`renderer.qrc` needs new entries, add them there yourself (it is yours), and log
a `NEEDS:` line if `omapresent.pro` itself needs a change.

## What to build

**Read `docs/renderer-contract.md` first — it is the frozen interface.** One
HTML/CSS/JS bundle renders all four outputs: live preview, the presentation
windows, PDF export and web publish. That is the whole architectural bet
(spec §14.1): preview, projector, PDF and published page are pixel-identical
because they are literally the same code.

### Structure it for testing
Put the pure logic in ES modules with no DOM access, so `node --test` can
import them directly:
- `deckparse.js` — block splitting, screen-vs-notes classification (§4.2),
  fragment counting.
- `layout.js` — the layout grammar (§4.6), including bento arrangement.
- `media.js` — bare-URL classification, video host recognition, QR decisions
  (§4.8). Mirror the host list in `src/videocache.h`.
`render.js` is the thin DOM shell over those, and defines `window.omapresent`.

### The layout grammar (§4.6) is a closed set
Implement the table exactly, and nothing beyond it. There are no styling knobs
in the document — that is deliberate (§1.2). The cases:
- Heading alone → large centered title, vertically centered.
- Heading then image on the **next line, no blank line** → one tight unit.
- Heading, blank line, image → two blocks with generous spacing.
- Image alone → fills content width, never cropped, scrolls if it overflows.
- Several image refs on **consecutive lines** → bento CSS grid: 2 side by side,
  3 in a row, 4 as 2×2, 5–6 as a mosaic. `|main` makes one the hero tile with
  the others sized around it.
- Images separated by blank lines → stacked vertically, full width, scrolls.
- Indented lines under a heading → static outline, indent = depth, all at once.
- Bulleted/numbered list → **revealed one item at a time**; nested items reveal
  with their parent's children in sequence; after the last item the next `→`
  advances the slide.
- Code / table / quote / math → its own centered block at full size.
- Prose paragraph → **not on the audience screen**; it is a speaker note.

### Never shrink to fit (§1.4, §4.7)
Text keeps its size, always. A slide with too much content becomes a scroll
surface: the stack is vertically centered when it fits, top-aligned when it does
not. `←`/`→` move between slides, `↑`/`↓` scroll within one. Report
`scrollFraction` in the state event so the audience window mirrors the
presenter's scroll live.

### Media (§4.8)
A URL alone on a line from a recognised host → an inline player sized to the
slide, respecting vertical video. Nothing autoplays on slide entry. Any other
bare URL → a **large QR code with the full URL printed beneath it**, on both
audience and presenter. ```qr fences and `![[qr:...]]` force a QR. A URL inside
a notes paragraph stays an ordinary link, not a QR.

### Theme (§6, contract §5)
`deck.css` uses the listed CSS custom properties and **no hard-coded colours**.
`render.js` sets them from `deck.palette`. Missing images render the
`backgroundImage` with a small "missing: name.png" tag in the corner — hidden in
`present` mode, shown in preview/PDF/editor.

### Vendored, offline
`markdown-it` (+ the plugins you need), `katex`, and a QR generator go in
`src/renderer/vendor/` as committed files with `LICENSES.md` recording each
licence and version. **No CDN, no network at runtime, no build step that
requires the network to rebuild the app.** Prefer vendoring the distributed
single-file builds.

## Tests
`tests/renderer/*.test.mjs`, run by `node --test` through `bin/test`. Cover
classification, every row of the §4.6 table, fragment counting including nested
lists, scroll/fit decisions, and each media/QR branch. Table-driven is fine and
preferred.

## Done when
`./bin/build && ./bin/test` pass, the renderer opens standalone in a browser
against a fixture deck, and your worklog entry is appended.
