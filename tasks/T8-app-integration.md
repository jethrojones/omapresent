# T8 — App shell: preview pane, WebEngine host, PDF export

**Agent:** `app-shell` · **Spec:** §3, §4.10, §8, §10, §13, §15 milestone 1 & 7

## Files you own
- `src/main.cpp`, `src/backend.cpp`, `src/backend.h`
- `src/Main.qml`, `src/EditorMutations.js`, `src/resources.qrc`
- `src/markdownhighlighter.cpp`, `src/markdownhighlighter.h`
- `omapresent.pro`
- `tests/tst_omapresent.cpp`
- A new `src/renderhost.{h,cpp}` if you want one (you own it)

Nothing else. In particular: do **not** edit `src/deckmodel.*`,
`src/assetindex.*`, `src/omarchytheme.*`, `src/videocache.*`,
`src/publisher.*`, `src/presentation.*`, or anything under `src/renderer/` —
other agents are writing those right now. They currently compile as stubs;
build against their headers and the real behaviour will appear underneath you.

## What to build

### 1. Wire the pieces into `Backend`
Own instances of `DeckModel`, `AssetIndex`, `OmarchyTheme`, `VideoCache`,
`Publisher` and `Presentation`, expose them to QML, and keep them fed:
- editor text changes → `DeckModel::setSource`
- file opened → `AssetIndex::setDeckDir`, and `setRoot` from the `root:`
  frontmatter key
- `theme:` frontmatter → `OmarchyTheme::setOverrideTheme`
- `OmarchyTheme::themeChanged` → repaint every open surface **without losing
  slide or scroll position**

### 2. The live preview pane
Split `Main.qml` into the editor `TextArea` (keep everything Omawrite gave us:
highlighting, typography, find/replace, drafts, external-change warning) and a
live HTML preview in a `WebEngineView` loading `qrc:/renderer/render.html`.

Build the deck JSON of `docs/renderer-contract.md` §1 and push it on every
edit through `update()`, not `render()`, so position holds. Debounce
sensibly — a keystroke should not reparse a 200-slide deck synchronously.

Register a `QWebChannel` object named `omapresentHost` with a single slot
`state(QString json)`; the renderer calls it on every state change. That is how
C++ learns the current slide, fragment and scroll position. Do **not** poll with
`runJavaScript`.

### 3. Editor conveniences (§4.10)
- **Triple-return inserts a slide break**: on the third consecutive `Return`,
  replace the trailing blank lines with `\n\n---\n\n` and put the cursor on the
  new slide. Editor behaviour only — three blank lines mean nothing at render
  time.
- Keep `Ctrl+B` / `Ctrl+I` / `Ctrl+K`.
- **Drag-and-drop an image** → insert `![[shortest-unambiguous-name]]` at the
  cursor via `AssetIndex::shortestUniqueReference`. Wayland drops arrive as
  percent-encoded `file://` URIs in `text/uri-list` — decode them, and handle
  paths with spaces.

### 4. New shortcuts (§10, §13)
`F5` present from the start · `Ctrl+Return` present from the current slide ·
`Ctrl+E` export PDF · `Ctrl+Shift+P` publish. Keep every Omawrite binding.
Update the `Ctrl+?` sheet to the full §13 reference.

### 5. PDF export (§8)
`QWebEnginePage::printToPdf` against the shared renderer in `pdf` mode. Slides
only, no notes handout. Canvas from the `aspect:` frontmatter, default `16:9`,
landscape. A slide taller than one page **paginates across pages — never
scale**. Fragment slides render fully expanded. Recall overlays export as
ordinary slides in document order. `Ctrl+P` still opens the system print dialog.

### 6. Session state (§10)
Reopening a deck restores the last slide and scroll position, per-file, under
`~/.local/state/omapresent/`. Follow desktop text scaling and reflow without a
restart (Omawrite already does the detection — carry it into the renderer as
`textScale` in the deck JSON).

### 7. CLI (§11)
`omapresent <file>`, `omapresent present <file>`,
`omapresent export --pdf <file>`, `omapresent publish <file> [--provider X]`.
`publish` must confirm before uploading unless given an explicit
`--yes`, because it sends the deck to an external host.

## Tests
Extend `tests/tst_omapresent.cpp` (it already has 12 passing cases from
Omawrite — keep them green). Add coverage for the triple-return transform, the
`text/uri-list` decode including a percent-encoded path with spaces, the deck
JSON you hand the renderer, and CLI argument parsing. Anything needing a real
`WebEngineView` is out of scope for the unit suite; test the JSON and the text
transforms instead.

## Done when
`./bin/build && ./bin/test` pass, the app opens a Markdown file and shows a
live preview beside the editor, and your worklog entry is appended.
