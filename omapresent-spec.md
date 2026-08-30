# Omapresent — Build Specification

> A dead-simple, Markdown-only presentation app for Omarchy. Open any Markdown
> file and it is already a presentation. No styling, no slide masters, no
> layout fiddling — the structure of your writing *is* the deck.

This document is the build brief. Hand it to a coding agent (e.g. inside Herdr)
to implement. It is written to be buildable top-to-bottom; the **Open decisions**
section near the end lists the few calls left to the implementer or to Jethro.

**Repo & licence.** Source lives at `github.com/jethrojones/omapresent`, public,
**MIT**. Start from Omawrite's source (`github.com/omacom/omawrite`, also MIT,
© 2026 David Heinemeier Hansson) so Omapresent inherits Omarchy's conventions for
free — keep DHH's copyright line in `LICENSE` and add `Copyright (c) 2026 Jethro
Jones`. Everything shipped (app, renderer bundle, welcome deck, agent skill) is
MIT.

---

## 1. Philosophy

1. **The Markdown is the source of truth.** Any `.md` file is a valid deck with
   zero changes. A file with no `---` is one long scrollable slide.
2. **You never style anything.** It is beautiful by default because it inherits
   the user's Omarchy theme. There are no colour, font, or alignment knobs in
   the document. Everything is centered and it looks right.
3. **Writing structure = slide structure.**
   - Headings, indented outline lines, code, tables, block quotes, math, images,
     and lists are what the **audience** sees.
   - Plain paragraph prose is the **speaker notes** — audience never sees it.
4. **Never shrink to fit.** Text keeps its size. If a slide has more content
   than fits, the slide scrolls (↑/↓) rather than compressing. Left/right move
   between slides; up/down scroll within one.
5. **Media just works.** A path to a local image displays it. A bare URL to a
   video becomes an embedded player. A bare URL on its own becomes a QR code.
6. **It is also a real editor**, not just a viewer — in the spirit of Omawrite.

## 2. Reference points

| Source | What to take from it |
| --- | --- |
| **Omawrite** (`github.com/omacom/omawrite`) | The template for *everything* structural: Qt Quick + C++ Qt 6, `.pro` build, `bin/build`/`bin/install`/`bin/test`, `pkgbuild/PKGBUILD`, system dark/light via `org.freedesktop.appearance` portal + DBus, file-watching with external-change warning, unsaved-draft recovery, text-scale awareness, tiny keyboard-first UI, `Ctrl+?` shortcut sheet. Match its minimalism and its file layout. |
| **iA Presenter** | Markdown-only decks; headings/structure drive slides; body text as speaker notes; publish to a scrolling web page *and* a deck; optional customizable header/footer per slide. |
| **Obsidian + presentation plugins** | `![[image.png]]` embeds resolved by filename against an indexed folder; `[[wikilinks]]`; `%%comments%%`; the "it just finds the file" ergonomics. |

## 3. Tech stack & architecture

Mirror Omawrite's stack and repo shape.

- **Language / UI:** C++17 + Qt 6 Quick (QML). Deps: `qt6-base`, `qt6-declarative`,
  `qt6-quickcontrols2`, `qt6-webengine`, `qt6-multimedia`.
- **Build:** qmake project file `omapresent.pro` (+ `tests/tests.pro`), driven by
  `bin/build`, `bin/install`, `bin/test`.
- **Editor pane:** native QML `TextArea` with a `QSyntaxHighlighter` subclass for
  Markdown (port/extend Omawrite's `markdownhighlighter.*`).
- **Renderer:** a single **HTML/CSS/JS renderer running in `QtWebEngine`**, shared
  by *all four* outputs — live preview, the presentation window, PDF export, and
  web publish. This guarantees the preview, the projector, the PDF, and the
  published page are pixel-identical, and gives us Markdown parsing
  (`markdown-it` + plugins), math (KaTeX), bento image layout (CSS grid), and web
  video embeds for free. The renderer is a bundled local asset bundle; **no
  network at runtime except** explicit video pre-fetch and publish.
- **C++ backend responsibilities:**
  - File IO, file-watching, draft recovery (from Omawrite).
  - The **asset index**: watch the resolution root, resolve image references.
  - **Theme bridge**: read Omarchy `colors.toml`, watch for theme changes,
    expose a palette + background path to the renderer as JSON.
  - **Video pre-fetch**: resolve and cache embedded web videos for offline use.
  - **PDF**: `QWebEnginePage::printToPdf`.
  - **Publish**: pluggable provider upload (see §9).
  - **Presentation windowing**: spawn the presenter + audience windows, pick
    monitors, inhibit idle, set Do-Not-Disturb.
- **Parsing pipeline:** C++ splits the raw file into `frontmatter`, an ordered
  list of `slides` (raw Markdown each), and a `notes`/`screen` classification is
  done in the renderer. C++ passes: raw slide Markdown, resolved asset map
  (`ref -> file:// path or cache path`), palette JSON, and mode
  (`preview | present | pdf | web`).

### Suggested repo layout

```
omapresent/
├── README.md
├── omapresent.pro
├── bin/{build,install,test}
├── src/
│   ├── main.cpp
│   ├── backend.{h,cpp}          # app state, open/save, recovery
│   ├── deckmodel.{h,cpp}        # frontmatter + slide splitting
│   ├── assetindex.{h,cpp}       # image resolution / fuzzy find
│   ├── omarchytheme.{h,cpp}     # colors.toml reader + watcher
│   ├── videocache.{h,cpp}       # oEmbed resolve + offline media prefetch
│   ├── publisher.{h,cpp}        # pluggable publish providers (herenow/command/s3)
│   ├── presentation.{h,cpp}     # window + monitor + idle/DND control
│   ├── markdownhighlighter.{h,cpp}
│   ├── systemtheme.{h,cpp}      # from Omawrite (portal dark/light)
│   ├── Main.qml                 # editor + preview shell
│   ├── PresenterWindow.qml
│   ├── AudienceWindow.qml
│   └── renderer/               # the shared HTML/CSS/JS bundle
│       ├── render.html
│       ├── render.js           # markdown-it + plugins, layout engine
│       ├── deck.css            # theme variables consumed here
│       └── vendor/{markdown-it,katex,qrcode}...
├── fonts/                       # only if a future Omarchy drops iA Writer S (see §14)
├── skill/                       # the agent skill (§11), MIT, installed by the pkg
│   ├── SKILL.md
│   └── reference/{document-model.md,publish-toml.md,settings-toml.md,recipes.md}
├── welcome/welcome.md           # first-run deck / manual (§7)
├── tests/
├── LICENSE                      # MIT — DHH's line + Jethro's
├── NOTICE                       # credits Omawrite as the starting point
└── pkgbuild/{PKGBUILD,omapresent.desktop,omapresent.install,omapresent.svg}
```

---

## 4. The document model

### 4.1 Slides & separators

- A slide break is a line containing exactly `---` **with a blank line both
  before and after it**. Anything else (`---` mid-paragraph, a Setext `Heading\n---`,
  `***`, `___`) is **not** a break.
- **Frontmatter:** if the very first line of the file is `---`, everything up to
  the next `---` is YAML frontmatter for the *whole file* (see §4.4). There is no
  per-slide frontmatter.
- **Headings never start a slide.** Only a separator does. One slide may contain
  many `#` headings; that is normal and renders as a tall scrolling slide.
- A file with no separators = a single slide (possibly very long, scrollable).

### 4.2 What the audience sees vs. speaker notes

| Element | Audience screen | Speaker notes |
| --- | --- | --- |
| ATX/Setext headings `#`–`######` | ✅ | — |
| Indented lines (tab / 4-space) under content — "outline" | ✅ (as sub-points) | — |
| Bulleted / numbered lists | ✅, **revealed one item at a time** | — |
| Fenced & indented code blocks | ✅ (syntax highlighted) | — |
| Tables | ✅ | — |
| Block quotes | ✅ | — |
| Math (`$…$`, `$$…$$`) | ✅ (KaTeX) | — |
| Images (any form, see §4.5) | ✅ | — |
| Video / embed URLs (§4.7) | ✅ (player) | — |
| Bare URL alone on a line (§4.8) | ✅ (QR code) | — |
| **Plain paragraph prose** | — | ✅ |
| Link/emphasis/`code` spans inside any of the above | rendered inline | — |

- A slide whose only content is prose (no heading/media/list) shows **blank** on
  the audience screen; the prose still appears in the presenter view and as the
  subtitle track of the published deck.
- Notes render as formatted Markdown in the presenter view (not raw).

### 4.3 Comments

- A line beginning with `//` (optionally preceded by whitespace) is a **comment**:
  removed before parsing, shown nowhere. `// # Later` hides that heading.
- A `//` on the separator line of a slide (`// ---`) **excludes the entire
  following slide** from the deck (draft slide).
- `%%…%%` (Obsidian) and `<!-- … -->` (HTML) are also treated as comments.

### 4.4 Frontmatter keys (all optional)

```yaml
---
title: Quarterly Review          # used for the title slide + <title> + publish slug
author: Jethro Jones
date: 2026-09-01
theme: gruvbox                    # override the live Omarchy theme for this deck
font: "IBM Plex Sans"             # override body font (must resolve on system)
aspect: "16:9"                    # export/PDF canvas; on-screen present fills the display
root: ~/Documents/aibrain        # image-resolution root (default: the deck's folder)
header: "Optimization Doc"        # optional persistent header on every slide
footer: "{title} — {slide}/{count}"   # optional persistent footer; supports tokens
slide-numbers: true              # show slide numbers (off by default)
progress: true                   # show a thin progress bar (off by default)
publish:                         # publish settings (see §9); all optional
  slug: q3-review                #   default: slug derived from title, else filename
  title: "Q3 Review — Optimization Doc"   # default: frontmatter title, else first heading
  provider: herenow              #   herenow | <name of a provider in the config, §9>
  access: link                   #   link | public | password | restricted
---
```

Footer/header tokens: `{title}` `{author}` `{date}` `{slide}` `{count}` `{heading}`.

**No title slide is auto-generated.** A deck starts on its first authored slide.
If you want a title slide, write one (`# My Talk` as the first slide). The
`title` / `author` / `date` keys are only used for the footer/header tokens, the
window title, and — for the published version — the page `<title>` and URL slug,
which *are* auto-derived when not given.

### 4.5 Images — resolution ("it just finds it")

Accepted forms, all equivalent:

- `![[budget.png]]` — Obsidian embed
- `![alt](budget.png)` — standard Markdown
- `![[~/Pictures/budget.png]]`, `![[/abs/path/budget.png]]`, `![[../img/budget.png]]`
- **A bare path on its own line** with no Markdown wrapper: `~/Pictures/budget.png`
  or `./img/budget.png`. Bare paths must contain a `/` **or** end in a known
  image extension to be treated as an image (so a lone word is not misread).

Resolution order for a non-absolute, non-relative reference:

1. Exact path relative to the deck file's directory.
2. `~` / `$HOME` / env-var expansion, then absolute path.
3. **Filename search** against the asset index (the `root` folder, recursive,
   default = deck's folder). Shortest / closest match wins, like Obsidian.
4. Case-insensitive retry of step 3.
5. **Not found → placeholder:** render the current Omarchy theme's desktop
   background (`~/.local/state/omarchy/current/background`) as the slide image,
   with a small unobtrusive "missing: `budget.png`" tag in the corner (present
   mode hides the tag; preview/PDF/editor show it).

Handle gracefully: spaces in paths (no `%20` needed), animated GI/APNG (autoplay,
loop, not "media" for spacebar), EXIF rotation, HEIC/TIFF/RAW/SVG/PDF-as-image
(convert or fall to placeholder), broken symlink / permission denied / unmounted
drive (→ placeholder), huge images (downscale for display, keep original for PDF),
`http(s)://` image URLs (download into the video/asset cache, offline-safe),
Linux case-sensitivity. Size hint: `![[photo.png|600]]` (max width px) and
`![[photo.png|main]]` (bento hero, §4.6).

Drag-and-drop a file into the editor inserts `![[shortest-unambiguous-name]]` at
the cursor — the shortest form that still resolves uniquely; full path if needed.
Wayland drops arrive as percent-encoded `file://` URIs in `text/uri-list`.

### 4.6 Layout grammar (derived from arrangement — enumerated, deterministic)

A slide is a vertical stack of **blocks** (runs of lines separated by a blank
line). Blocks are centered horizontally. The stack is centered vertically **if it
fits**; otherwise it is top-aligned and the slide scrolls.

| Arrangement | Rendering |
| --- | --- |
| Heading alone on the slide | Large centered title, vertically centered — a title slide. |
| Heading, then image on the **next line (no blank line)** | Tight unit: image directly under the heading, both centered, image near-full width. |
| Heading, **blank line**, image | Two separate centered blocks with generous spacing between them. |
| Image alone on the slide | Fills the content width (up to the display width); height scales; if it overflows vertically the slide scrolls. Never cropped. |
| Several image refs on **consecutive lines** (one block) | **Bento tiling** (CSS grid), arranged "the Omarchy way": 2 → side by side, 3 → row, 4 → 2×2, 5–6 → mosaic, 7 → a balanced 4+3 two-row grid. Eight or more fall back to a vertical stack. If one image has `|main`, it becomes a large central tile with the others sized around it (the macOS-keynote "feature grid" look). |
| Images separated by blank lines | Stacked vertically, each full width, slide scrolls. |
| Indented lines under a heading | Static outline: indent = hierarchy depth, shown all at once. |
| Bulleted / numbered list | Shown as a list, **revealed one item at a time** on → / Space; nested items reveal with their parent's children in sequence. After the last item, → advances to the next slide. |
| Code block / table / quote / math | Rendered as its own centered block at full size. |
| Prose paragraph | Not rendered on the audience screen (it is a note). |

There are no other layout modes and no way to override them from the document —
this is deliberate.

### 4.7 Long / scrolling slides

- On-screen present mode: content keeps its size; the slide is a scroll surface.
- ↑/↓ (and PgUp/PgDn, mouse wheel) scroll the current slide.
- **The audience screen mirrors the presenter's scroll position live.**
- Scroll position is remembered when navigating away and back within a session.
- This is the intended way to hold a slide with lots of material while you are
  not ready to move on — a "wall of text" that you scroll through is fine.

### 4.8 Video & web embeds

- A URL **alone on a line** from a recognised video host → an inline player
  sized to the slide (respects vertical video). Recognised at launch: **YouTube,
  Vimeo, Loom, Descript, TikTok, X / Twitter, Instagram, Facebook**, plus direct
  `.mp4`/`.webm`/`.mov` URLs and local video files. No generic `yt-dlp` fallback —
  a URL from an unrecognised host is treated as a QR code (below), not a video.
- A URL alone on a line that is **not** a recognised video → rendered as a
  **QR code** (`qrcode` JS lib) at a large size **with the full URL shown
  beneath it** on both the audience and presenter screens, so everyone can see
  where it points. Explicit `\`\`\`qr` fenced block, or `![[qr:https://…]]`,
  force a QR code.
- **Playback:** `Space` plays/pauses the focused player. Nothing autoplays on
  slide entry. After a video ends, `Space` advances the slide. Arrows always
  navigate regardless of media. If a slide has multiple players, `Space` targets
  the first; `Tab` moves focus between them.
- **Offline:** on save, and on an explicit "Prepare for offline" action, resolve
  each embed via the host's oEmbed endpoint and download the underlying media
  into `<deck-dir>/.omapresent-cache/` where the host allows it. In present mode,
  play the cached file if present; fall back to the live embed; fall back to a
  QR code of the URL if neither works. Private / age-restricted / DRM /
  geo-blocked videos that cannot be fetched degrade to the live embed then the
  QR code, with a one-time warning at save.

### 4.9 Recall / overlay slides

Some slides you want to jump to at any moment during a talk — a running-gag
quote, a QR survey link, a diagram you keep returning to.

- Tag the **separator line**: `--- {q}` binds the slide that *follows* it to the
  `q` key. Any single letter/number works; up to ~8 bindings per deck.
- In present mode, pressing a bound key renders that slide as a **full-screen
  overlay** on top of the current slide (current slide dimmed behind). Press the
  same key, `Esc`, or `Space` to dismiss and return exactly where you were.
- By default a recall slide is *also* in the normal left/right flow. `--- {q, skip}`
  keeps it poppable but removes it from the linear sequence.

### 4.10 Editor conveniences

- **Triple-return inserts a slide break.** When the user presses `Return` a third
  consecutive time (creating two blank lines) at the end of the document flow,
  replace the trailing blank lines with `\n\n---\n\n` and place the cursor on the
  new slide. This is an *editor* behaviour only; triple blank lines carry no
  meaning at render time.
- `Ctrl+B` / `Ctrl+I` / `Ctrl+K` insert bold / italic / link (from Omawrite).
- Drag-drop image → `![[name]]` (§4.5).
- Live two-way: editing updates the preview and any running presentation
  immediately, holding slide + scroll position where possible.

---

## 5. Present mode

### 5.1 Windows & monitors

- "Present" opens **two separate top-level windows** (so Hyprland tiling / window
  rules treat them independently, and either can be screen-shared or captured in
  OBS on its own):
  - **Audience window** — headings/media/lists only, themed, fills its output.
  - **Presenter window** — current slide (scaled), next-slide preview, rendered
    speaker notes, elapsed timer (click to reset), wall clock, current
    slide x/count, and the list of recall-key bindings.
- Monitor assignment: if ≥2 outputs, audience → the external/non-primary output
  fullscreen, presenter → the other. If 1 output, audience fills it and
  `N` toggles a notes overlay on the same screen.
- Projector hotplug mid-talk is handled: re-evaluate outputs and move windows.
- The audience window is a normal shareable window — sharing just that window in
  a video call or OBS gives a clean full-frame capture.

### 5.2 Keys (present mode)

| Key | Action |
| --- | --- |
| → / Space | Next fragment; if none, next slide. Space plays/pauses media first. |
| ← | Previous fragment / previous slide |
| ↑ / ↓ / PgUp / PgDn / wheel | Scroll the current slide (audience mirrors) |
| Home / End | First / last slide |
| digits then `Enter` | Jump to slide number |
| `F` | Toggle fullscreen (audience) |
| `B` | Black the audience screen (toggle) |
| `W` | White the audience screen (toggle) |
| `O` | Slide overview grid; arrows + `Enter` to pick |
| `N` | Toggle notes overlay (single-monitor mode) |
| bound letter/number | Show / hide that recall-slide overlay (§4.9) |
| `Esc` | Exit present mode (both windows) |
| `Ctrl+?` | Shortcut reference |

### 5.3 Environment

- **Inhibit idle** (`hypridle` / `org.freedesktop.ScreenSaver` inhibit) for the
  duration of the presentation; restore on exit.
- **Enable Do-Not-Disturb** (`omarchy` / `makoctl` / notification portal) while
  presenting; restore prior state on exit.
- Transitions between slides are **instant cuts** — no animation.

---

## 6. Theme integration

Omapresent is, by default, wearing the same clothes as the rest of the desktop.

- **Live source:** `~/.local/state/omarchy/current/theme/colors.toml` and
  `~/.local/state/omarchy/current/theme.name`. Missing background placeholder
  uses `~/.local/state/omarchy/current/background`.
- **Both `colors.toml` shapes must be supported:**
  - Rich named form (e.g. `catppuccin`, `gruvbox`): `mode`, `accent`,
    `background`, `foreground`, `muted`, `selection`, `*_background`,
    `*_foreground`, `red|orange|yellow|green|cyan|blue|magenta|brown`, `bright_*`.
  - Terminal form (e.g. `gold-rush`): `accent`, `foreground`, `background`,
    `cursor`, `selection_*`, `color0`–`color15`. Derive semantic roles from the
    16 colours when named keys are absent.
- **Mapping to the deck:** `background` → slide background; `foreground` →
  headings/text; `accent` → links, active fragment, progress bar, QR modules;
  `muted`/`dark_foreground` → footer/header, slide numbers, code comments; the
  ANSI hues → code syntax highlighting.
- **Live reload:** install an Omarchy hook
  (`omarchy hook install theme-set <omapresent-refresh>`) *and* watch the
  `current/theme` symlink. On change, re-read and repaint every open window
  (editor, preview, presenter, audience) without losing position.
- **Per-deck override:** `theme:` frontmatter loads
  `~/.config/omarchy/themes/<name>/colors.toml` or the stock
  `/usr/share/omarchy/themes/<name>/colors.toml` instead of the live theme. The
  override applies **only inside Omapresent's own windows** (editor, preview,
  presenter, audience) — it never re-themes the rest of the desktop.
- **Enumerate installed themes** from both those directories for a theme picker.
- **Projector legibility floor:** if the resolved text/background pair is below
  a WCAG-AA-ish contrast ratio on the **audience** window, nudge the text
  lightness until it clears the floor. Presenter/preview/PDF keep the exact
  theme.
- **Fonts:** prefer the `font:` override → the system UI font → **iA Writer
  Quattro S** (body) / **iA Writer Mono S** (code) — both already shipped on
  Omarchy and SIL OFL licensed. See §14.

---

## 7. First-run / welcome deck (this is also the manual)

- On first launch (and via **Help → How Omapresent works**), open a bundled
  `welcome.md` deck.
- It is a real Omapresent deck that **explains the whole app by being it**:
  the document model, every layout rule, the comment syntax, image resolution,
  video/QR behaviour, recall slides, all present-mode shortcuts, the frontmatter
  keys, theming, export, and publish — each demonstrated live on its own slide,
  with the design rationale in the speaker notes.
- Ships read-only from `/usr/share/omapresent/welcome.md`; "Edit a copy" drops it
  in `~/`.

---

## 8. PDF export

- **Slides only** (no notes handout variant for v1).
- Canvas = `aspect:` frontmatter (default `16:9`), landscape.
- A slide taller than one page **paginates across multiple pages** — no scaling.
- Fragment slides render fully expanded (all bullets shown).
- Recall overlays export as ordinary slides in document order.
- Implemented via `QWebEnginePage::printToPdf` against the shared renderer in
  `pdf` mode.
- `Ctrl+P` opens the system print dialog (as Omawrite).

---

## 9. Web publish

Two artifacts from one command, both themed and beautiful:

1. **Deck view** — the slides as presented, arrow-key / swipe navigable, with the
   **speaker notes shown as subtitles** beneath each slide (toggleable).
2. **Long read** — a single scrolling page: headings, media, lists and notes
   flowed together as a well-set article in the deck's theme.

Both are produced by the shared renderer in `web` mode as a **static bundle**
(HTML + CSS + JS + assets + cached videos), self-contained and offline-capable.

### Hosting is pluggable

In keeping with Omarchy's "it's your machine" ethos, **where a deck publishes is
a config choice**, not a lock-in. Omapresent ships with one built-in provider
(`herenow`) and reads additional providers from
`~/.config/omapresent/publish.toml`:

```toml
default = "herenow"

[providers.herenow]
type = "herenow"
api_key = "hn_xxx"              # blank/omitted → anonymous 24h links
domain = "omapresent.com"      # optional; a custom domain you added in here.now
mount_prefix = "/presentations" # optional; deck lands at <domain>/presentations/<slug>

[providers.mybox]
type = "command"               # generic escape hatch: run any command
# The static bundle dir is passed as $OMAPRESENT_BUNDLE, the slug as $OMAPRESENT_SLUG.
# Whatever the command prints on stdout as the last line is shown as the live URL.
publish = "rsync -a --delete $OMAPRESENT_BUNDLE/ me@host:/var/www/decks/$OMAPRESENT_SLUG/ && echo https://decks.example.com/$OMAPRESENT_SLUG"

[providers.s3]
type = "s3"                    # native S3-compatible target
endpoint = "https://s3.us-west-002.backblazeb2.com"
bucket = "my-decks"
prefix = "presentations/"
base_url = "https://decks.example.com"
```

Provider types for v1: **`herenow`**, **`command`** (run anything), **`s3`**
(any S3-compatible bucket + CDN base URL). Per-deck override via the frontmatter
`publish.provider` key.

### The built-in `herenow` provider — confirmed capabilities

Verified against `https://here.now/openapi.json`:

- **Publish flow:** `POST /api/v1/publish` (returns `slug`, `siteUrl`,
  `versionId`, `claimToken`, and a `presignedUploads[]` array of `{path, url}`) →
  PUT each bundle file to its presigned S3 URL → `POST /api/v1/publish/{slug}/finalize`
  with `{versionId}`. `POST /api/v1/publish/{slug}/uploads/refresh` re-presigns if
  URLs expire. Finalize is idempotent by `versionId`.
- **Updates & rollback:** `PUT /api/v1/publish/{slug}` stages a new version;
  `GET /api/v1/publish/{slug}/versions` and
  `POST /api/v1/publish/{slug}/versions/{versionId}/restore` (instant pointer
  flip) give history and rollback — map these to "republish" and "revert" in the
  UI.
- **Anonymous:** publish with no `Authorization` header → live `{slug}.here.now`
  URL that expires in 24h; `claimToken` + `POST /api/v1/publish/{slug}/claim`
  attaches it to an account later. This is the zero-config "quick share" default.
- **Authenticated:** `Authorization: Bearer <api_key>`. Key is obtained via
  `POST /api/auth/agent/request-code` (email) → `POST /api/auth/agent/verify-code`;
  Omapresent can run this flow in Preferences.
- **Custom domain + pretty paths — `omapresent.com/presentations/<slug>` IS
  possible.** Add the domain once via `POST /api/v1/domains` (returns DNS records
  to set), then for each deck call `POST /api/v1/mounts` with
  `{domain: "omapresent.com", mount_path: "/presentations/<slug>", slug}`. Multiple
  Sites share one host under different `mount_path`s — exactly the layout Jethro
  wants. (`primary-domain` is the simpler single-Site redirect; `mounts` is the
  multi-deck one.)
- **Access control:** `PATCH /api/v1/publish/{slug}/access` with
  `mode ∈ {anyone_with_link, password, restricted, account_members}`,
  `allowedEmails[]`, `allowedDomains[]`. `restricted` requires a claimed Site.
- **Analytics** (`GET /api/v1/publishes/{slug}/analytics`) and **Site Data**
  (form/survey collections at `/.herenow/data/:collection`) are available if we
  later want view counts or an audience-response feature on published decks.

### Settings surface (Preferences → Publish)

- Provider picker + "Sign in to here.now" (runs the email code flow).
- Custom domain field → offers to add it and shows the DNS records to set.
- Default access mode; optional password.
- Everything is also editable directly in `~/.config/omapresent/publish.toml`.

---

## 10. Editor app behaviour (inherited from Omawrite)

- `Ctrl+S` save (XDG portal picker when untitled), `Ctrl+Shift+S` save as,
  `Ctrl+O` open, `Ctrl+N` new window, `Ctrl+P` print, `Ctrl+Z`/`Ctrl+Shift+Z`/`Ctrl+Y`
  undo/redo, `Ctrl+F` find, `Ctrl+H` find & replace, `Super+F` fullscreen,
  `Ctrl+?` shortcuts.
- **New in Omapresent:** `Ctrl+Return` present from current slide, `Ctrl+E`
  export PDF, `Ctrl+Shift+P` publish, `F5` present from start.
- Unsaved-draft recovery after an abnormal exit.
- Watch the open file; warn before an external change overwrites unsaved work.
- Follow desktop text scaling (`omarchy display text size` / GNOME
  `text-scaling-factor`); reflow without restart.
- Reopening a deck restores the last slide + scroll position (per-file state in
  `~/.local/state/omapresent/`).
- Multi-window; each window one deck.

---

## 11. Agent skill

Omapresent ships a **Claude Code / agent skill** so an AI agent can help someone
author and maintain decks without the person learning every rule in §4.

- **Contents of the skill:**
  - The full document model — separators, comments, screen-vs-notes rules, the
    layout grammar, image resolution, recall slides, video/QR behaviour,
    frontmatter keys — condensed to a reference an agent can act on.
  - How to edit the two TOML config files safely:
    `~/.config/omapresent/publish.toml` (providers, domains, access) and
    `~/.config/omapresent/settings.toml` (editor/present preferences) — with the
    schema, valid enum values, and "read the file, patch one key, keep the rest"
    guidance. Never rewrite a file wholesale; preserve comments and unknown keys.
  - Recipes: "turn this note into a deck", "add a recall slide", "make a bento
    image slide", "wire up publishing to my own S3 bucket", "prepare this deck
    for an offline venue".
  - How to invoke the app from a shell: `omapresent <file>`, `omapresent present
    <file>`, `omapresent export --pdf <file>`, `omapresent publish <file>
    [--provider <name>]`.
  - The safety line: editing a user's deck content and config is fine; publishing
    (`omapresent publish`) sends the deck to an external host and must be
    confirmed by the user first.
- **Format:** a `SKILL.md` with YAML frontmatter (`name: omapresent`,
  `description:` covering "create / edit / publish Omapresent decks and config"),
  plus any reference files, following the Agent Skills spec.
- **Location in the repo:** `skill/` at the repo root.
- **Auto-install on package install.** `pkgbuild/omapresent.install` (the
  pacman `.install` hook) copies the skill into the user-level skills directory
  on `post_install` / `post_upgrade` and removes it on `post_remove`. Target:
  `${XDG_DATA_HOME:-~/.local/share}/omarchy/skills/omapresent/` if Omarchy
  defines a skills path, otherwise `~/.claude/skills/omapresent/`. Because pacman
  hooks run as root, the hook installs to `/usr/share/omapresent/skill/` and
  drops a one-line Omarchy migration / first-run step that symlinks it into the
  invoking user's skills dir (mirror however Omawrite would handle a per-user
  asset — confirm the exact Omarchy-blessed mechanism during build).
- The skill is MIT, versioned in the same repo, and kept in sync with the app —
  the build's CI checks that every frontmatter key and TOML field named in the
  skill still exists in the code.

---

## 12. Packaging

- Binary / package / command: **`omapresent`**.
- Source: `github.com/jethrojones/omapresent`, public, **MIT** (`LICENSE` keeps
  DHH's line from Omawrite and adds `Copyright (c) 2026 Jethro Jones`). Add a
  short `NOTICE` crediting Omawrite as the starting point. Repo topics /
  README badge: `open-source`, `mit`, `omarchy`.
- `pkgbuild/PKGBUILD` targeting the Omarchy package repo; `omapresent.desktop`
  ("Omapresent", `MimeType=text/markdown;`), `omapresent.install`,
  `omapresent.svg` icon (placeholder pineapple-on-projector; Jethro to replace).
- Requirements: `qt6-base qt6-declarative qt6-quickcontrols2 qt6-webengine
  qt6-multimedia`; `gst-plugins-{base,good,bad,ugly}` for local video codecs.
  No `yt-dlp`.
- Fonts: **do not bundle** — depend on the iA Writer S family already present on
  Omarchy (`iA Writer Quattro S` / `Duo S` / `Mono S`, all SIL OFL) and degrade
  through the fallback stack.

---

## 13. Full keyboard reference (goes in `Ctrl+?` and welcome deck)

*Editor:* `Ctrl+S` save · `Ctrl+Shift+S` save as · `Ctrl+O` open · `Ctrl+N` new
window · `Ctrl+P` print · `Ctrl+E` export PDF · `Ctrl+Shift+P` publish ·
`Ctrl+Z` / `Ctrl+Shift+Z` / `Ctrl+Y` undo/redo · `Ctrl+F` find · `Ctrl+H`
replace · `Ctrl+B` / `Ctrl+I` / `Ctrl+K` bold/italic/link · `Super+F` fullscreen
· `F5` present from start · `Ctrl+Return` present from current slide.

*Present:* `→`/`Space` next · `←` back · `↑`/`↓` scroll · `Home`/`End` first/last
· digits+`Enter` jump · `F` fullscreen · `B` black · `W` white · `O` overview ·
`N` notes overlay · bound key = recall slide · `Esc` exit.

---

## 14. Open decisions (for the implementer)

1. **Renderer = QtWebEngine HTML/CSS** shared across preview/present/PDF/web.
   Confirmed direction; flagged only because it is the biggest architectural
   commitment and pulls in Chromium (contradicts "tiny", but buys video, math,
   bento, and export parity in one move). The alternative — native QML rendering
   plus a separate export path — is a lot more code for a worse result.
2. **Fonts.** Rely on the system iA Writer S family (`iA Writer Quattro S`,
   `Duo S`, `Mono S`) — already installed on Omarchy and SIL OFL, so it satisfies
   both "fonts Omarchy ships with" and "open source". Do **not** bundle copies.
   Only fall back to bundling IBM Plex Sans/Mono if a future Omarchy release
   drops those fonts from the default install.
3. **TikTok / X / Instagram / Facebook embeds** rely on each platform's public
   oEmbed/embed endpoint; several of these actively fight embedding and change
   often. Treat them as best-effort: embed when it works, else QR-code the URL.
   No `yt-dlp` or scraping.
4. **oEmbed at runtime** needs a network call the first time a web video slide is
   built. Cache the resolved embed/media in `.omapresent-cache/` so a prepared
   deck presents fully offline.
5. **Per-user skill install from a root pacman hook.** The `.install` hook runs
   as root and cannot know which user to install the skill for. Confirm the
   Omarchy-blessed mechanism — a first-run step in the app that symlinks
   `/usr/share/omapresent/skill/` into the user's skills dir, an Omarchy
   migration, or an XDG autostart shim. Whatever Omawrite-style precedent exists
   wins.

### Settled in the design conversation

- Everything is centered; no per-document styling knobs.
- Headings never break slides — only a blank-line-wrapped `---` (or the editor's
  triple-return) does.
- Body prose = speaker notes; headings/outline/lists/code/tables/quotes/math/
  media = audience screen.
- `//`, `%%…%%`, `<!-- -->` are comments; `// ---` drops the next slide.
- No per-slide frontmatter; file-level frontmatter only, all keys optional.
- Recall slides are tagged on the **separator line**: `--- {q}`.
- Bare URL alone on a slide line → QR code **with the URL printed beneath it**;
  a URL inside a notes paragraph stays an ordinary link.
- **No auto-generated title slide** for presentations; the published version
  *does* auto-derive its page `<title>` and URL slug.
- `theme:` override applies to Omapresent's windows only, never the desktop.
- Bundled themes are not shipped — Omapresent enumerates the themes already
  installed under `/usr/share/omarchy/themes` and `~/.config/omarchy/themes`.
- Hosting is pluggable (`~/.config/omapresent/publish.toml`); `herenow` is the
  default provider and `omapresent.com/presentations/<slug>` is achievable via
  here.now domain + mounts.
- Public repo `github.com/jethrojones/omapresent`, MIT, started from Omawrite's
  MIT source to match Omarchy conventions.
- An MIT agent skill ships in `skill/` and auto-installs with the package, so
  agents can author decks and safely patch the TOML config files.

---

## 15. Suggested build milestones

0. **Repo** — create `github.com/jethrojones/omapresent` (public, MIT); import
   Omawrite's source as the base commit, keep its `LICENSE`, add Jethro's
   copyright line + `NOTICE`.
1. **Skeleton** — adapt Omawrite's structure; editor + live HTML preview of a
   single-slide document; Omarchy theme bridge (both `colors.toml` shapes) with
   live reload.
2. **Document model** — frontmatter, `---` splitting, comments, screen/notes
   classification, headings + prose + lists.
3. **Images** — asset index, full resolution chain, missing→background
   placeholder, bento layout + `|main`, drag-drop insert.
4. **Present mode** — two windows, monitor assignment, all keys, fragment
   reveal, scroll mirroring, idle-inhibit + DND.
5. **Media** — local video; YouTube / Vimeo / Loom / Descript embeds, then
   best-effort TikTok / X / Instagram / Facebook; QR generation with URL caption;
   offline cache.
6. **Recall slides**, overview grid, header/footer, slide numbers, progress.
7. **Export** — PDF (paginated), then web publish (deck + long read) with the
   pluggable provider layer (`herenow` first, then `command` / `s3`).
8. **Agent skill** — write `skill/SKILL.md` + references; wire auto-install into
   `pkgbuild/omapresent.install`; add the CI check that skill ↔ code stay in sync.
9. **Welcome deck** as the manual; `Ctrl+?` sheet; polish; PKGBUILD; publish to
   the Omarchy package repo.

---

*Created by Claude Sonnet 5 on 2026-08-27 17:10 PT on ombee. Updated 2026-08-27
17:25 PT — theme override scope, pluggable hosting + confirmed here.now API,
video provider list, QR captions, no auto title slide, agent skill, MIT repo.*
