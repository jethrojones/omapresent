# Omapresent build worklog

Every agent appends here. Newest entries at the bottom. See `AGENTS.md` §5 for
the format. Never rewrite an existing entry.

---

### 2026-08-27 17:55 PT — orchestrator (Claude Opus 5)
**Task:** T0 repo baseline and contracts
**Did:**
- `git init`; imported `github.com/omacom/omawrite` @ `8f98892` verbatim as the
  base commit, then renamed every `omawrite`/`Omawrite` identifier to
  `omapresent`/`Omapresent` (spec §15 milestone 0).
- `LICENSE` keeps DHH's copyright line and adds Jethro's; wrote `NOTICE`
  crediting Omawrite as the starting point.
- Dropped the bundled iA Writer Mono S font files and their `resources.qrc`
  entries — spec §12/§14 says depend on the system family, do not bundle.
- Froze the interfaces every other task compiles against: `src/deckmodel.h`,
  `src/assetindex.h`, `src/omarchytheme.h`, `src/videocache.h`,
  `src/publisher.h`, `src/presentation.h`, plus `docs/renderer-contract.md`
  for the C++ ↔ renderer JSON and the `window.omapresent` page API.
- Added compiling stubs for each so the tree is green from commit one.
- `omapresent.pro` now pulls in `webenginequick webchannel multimedia network`.
- Replaced the single-suite `QTEST_MAIN` harness with a multi-suite runner
  (`tests/testrunner.h` + `tests/main.cpp`) so agents add suites without
  colliding; `bin/test` now runs the C++ binary **and** `node --test` over
  `tests/renderer/`.
**Tests:** `./bin/build` OK. `./bin/test` OK — 12 inherited cases pass, 5 new
suites registered and skipping.
**Next:** Spawn the wave-1 agents.
**NEEDS:** nothing

### 2026-08-27 17:49 PT — publish (Codex GPT-5.6 Sol)
**Task:** T6 web publish and provider layer
**Did:** Loaded the task contract and started the provider implementation.
**Tests:** Baseline tests not run yet.
**Next:** Implement config helpers, provider flows, and the network-free test suite.
**NEEDS:** nothing

### 2026-08-27 17:50 PT — skill-docs (Gemini 3.7 Flash)
**Task:** T7 agent skill, welcome deck, packaging and docs
**Did:** Reviewed spec (§7, §11, §12, §13, §14.5), contracts, and Omarchy system skill path precedents. Starting work on welcome deck, agent skill with full reference docs, packaging files, and README.
**Tests:** Baseline `./bin/build && ./bin/test` clean.
**Next:** Author `welcome/welcome.md` demonstrating all features and layout rules.
**NEEDS:** nothing
### 2026-08-27 17:51 PT — assets (Gemini 3.7 Flash)
**Task:** T4 asset index and image resolution
**Did:** Read kickoff brief `tasks/T4-assets.md` and spec §4.5/§15. Verified baseline `./bin/build && ./bin/test`. Starting implementation of `AssetIndex` resolution steps 1-5, recursive file index, watchers, size hint parsing, reference extraction, and `tst_assetindex` test suite.
**Tests:** Baseline `./bin/build && ./bin/test` passing.
**Next:** Implement `AssetIndex` in `src/assetindex.cpp` and full unit test coverage in `tests/tst_assetindex.cpp`.
**NEEDS:** nothing

### 2026-08-27 17:50 PT — renderer (Codex GPT-5.6 Sol)
**Task:** T2 shared renderer bundle
**Did:** Read the frozen renderer contract, task brief, required spec sections, and frontend design rules. Confirmed ownership is limited to `src/renderer/**`, `tests/renderer/**`, and append-only worklog entries.
**Tests:** Baseline not run yet.
**Next:** Build the pure parsing, layout, and media modules with table-driven tests, then add the DOM shell and offline vendor bundle.
**NEEDS:** nothing

### 2026-08-27 17:52 PT — assets (Gemini 3.7 Flash)
**Task:** T4 asset index and image resolution
**Did:** Implemented `AssetIndex` in `src/assetindex.cpp`: resolution order steps 1-5 (relative to deckDir, tilde/env expansion, shortest-path filename search against recursive index, Linux case-insensitive retry, empty fallback for placeholder), resolveAll for renderer contract, looksLikeImageReference, parseSizeHint, extractReferences (excluding code blocks and non-image words), shortestUniqueReference, and recursive filesystem watching with debounced rebuild.
**Tests:** `tests/tst_assetindex.cpp` — 21 test cases covering all spec §4.5 and task requirements. `./bin/build && ./bin/test` all green.
**Next:** Done with T4.
**NEEDS:** nothing

### 2026-08-27 17:53 PT — skill-docs (Gemini 3.7 Flash)
**Task:** T7 agent skill, welcome deck, packaging and docs
**Did:**
- Created `welcome/welcome.md`: a complete, interactive presentation manual demonstrating every document rule, all 10 §4.6 layout grammar arrangements, bento grids, image resolution, media/QR, recall overlays (`--- {q}`), theming, PDF export, and publish settings, with design rationales in plain-prose speaker notes.
- Created Agent Skill in `skill/SKILL.md` (Agent Skills spec compliant) and comprehensive reference docs in `skill/reference/`: `document-model.md`, `publish-toml.md`, `settings-toml.md`, and `recipes.md`, documenting schemas derived from headers, single-key patching rules, and the strict safety boundary requiring user confirmation before running `omapresent publish`.
- Updated `pkgbuild/PKGBUILD` with exact Qt6 & GStreamer dependencies, no `yt-dlp`, no bundled fonts (depends on system `iA Writer S`), installing `welcome.md`, `skill/`, `LICENSE`, `NOTICE`, desktop file, and icon.
- Updated `pkgbuild/omapresent.desktop` and `pkgbuild/omapresent.install` (icon cache and desktop database updates).
- Created `pkgbuild/omapresent.svg`: clean pineapple-on-projector SVG icon, legible at 48px.
- Rewrote `README.md` for Omapresent with project pitch, philosophy, install instructions, full §13 keyboard reference, and Omawrite/MIT credits.
- Researched Omarchy skills convention (§14.5): Omarchy symlinks system skills from `/usr/share/omarchy/default/agents/skills/` into `~/.claude/skills/`, `~/.agents/skills/`, `~/.codex/skills/`, `~/.pi/agent/skills/`.
**Tests:** `bash -n pkgbuild/PKGBUILD` and `bash -n pkgbuild/omapresent.install` syntax valid. `./bin/build && ./bin/test` clean and all green (33 passed, 4 skipped stubs).
**Next:** Done with T7.
**NEEDS:** Backend/app first-run step to symlink `/usr/share/omapresent/skill/` into `${XDG_DATA_HOME:-~/.local/share}/omarchy/skills/omapresent` or `~/.claude/skills/omapresent`.

### 2026-08-27 17:55 PT — skill-docs (Gemini 3.7 Flash)
**Task:** Review 1 (T7 skill-docs)
**Did:**
- Removed `theme: default` from `welcome/welcome.md` frontmatter so the deck inherits the live desktop Omarchy theme on launch.
- Added explicit demonstration of per-deck `theme: <name>` frontmatter syntax in a fenced block on the theming slide, explaining that it overrides palette for that deck only without altering the desktop.
- Added a dedicated Mathematical Notation slide in `welcome/welcome.md` demonstrating both display math (`$$...$$`) and inline math (`$...$`) with KaTeX vector typography.
- Audited the entire §4.2 table and verified every row (headings, outlines, lists, code, tables, block quotes, math, images, video embeds, bare-URL QR codes, and prose speaker notes) has live demonstration in `welcome.md`.
**Tests:** `./bin/check-skill-sync` passes. `./bin/build && ./bin/test` pass clean (33 passed, 4 skipped stubs).
**Next:** Done with Review 1.
**NEEDS:** nothing

### 2026-08-27 17:56 PT — assets (Gemini 3.7 Flash)
**Task:** Review 1 (T4 assets)
**Did:**
- Moved recursive directory walk off-thread via `QThreadPool` worker (`AssetIndexScanWorker`), keeping `resolve()` non-blocking and continuously responsive from the previous index until new results land.
- Added `AssetIndex::waitForIndex()` helper for deterministic synchronization in unit tests.
- Added 2,048 directory limit cap to `QFileSystemWatcher` with depth-sorted priority (nearer root first) and single warning log on cap or inotify exhaustion.
- Added unit tests in `tests/tst_assetindex.cpp` verifying continuous old-index resolution during background rebuilds (`resolveMaintainsOldIndexDuringRebuild`) and directory watch capping (`directoryWatchingCapsAtLimit`).
**Tests:** `tests/tst_assetindex.cpp` — 23 test cases, all green. `./bin/build && ./bin/test` pass clean (35 passed, 4 skipped stubs).
**Next:** Done with Review 1.
**NEEDS:** nothing

### 2026-08-27 17:57 PT — theme (Grok 4.6)
**Task:** T3 Omarchy theme bridge
**Did:** Starting. Inspected both `colors.toml` shapes across `/usr/share/omarchy/themes/` (rich named) and `~/.config/omarchy/themes/` (terminal `color0`–`color15`), plus `omarchy-theme-color`'s alias cascade so derivation matches the desktop.
**Tests:** not yet.
**Next:** `parseColorsToml`, contrast helpers, live reload, override, tests.
**NEEDS:** nothing


### 2026-08-27 18:05 PT — app-shell (Claude Opus 5)
**Task:** T8 app shell — preview pane, WebEngine host, PDF export, CLI
**Did:** Read AGENTS.md, the brief, spec §3/§4.10/§8/§10/§13/§15 and
docs/renderer-contract.md. Confirmed the Omarchy agent-skill precedent on this
machine for the routed NEEDS: `/usr/share/omarchy/default/agents/skills/<name>`
is symlinked (absolute) into `~/.claude/skills`, `~/.agents/skills`,
`~/.codex/skills` and `~/.pi/agent/skills`. Starting implementation.
**Tests:** Baseline `./bin/build && ./bin/test` green before I touched anything.
**Next:** renderhost.{h,cpp}, then Backend wiring, then Main.qml split.
**NEEDS:** nothing yet

### 2026-08-27 18:06 PT — media (Grok 4.6)
**Task:** T5 video, embeds and the offline cache
**Did:** Read AGENTS.md, tasks/T5-media.md, spec §4.8/§14.3/§14.4/§15.5 and docs/renderer-contract.md §1. Starting VideoCache: host recognition, bare-URL extraction, describe() from disk, then prefetch into `.omapresent-cache/`.
**Tests:** not written yet
**Next:** hostFor / isBareUrlLine / extractUrls / embedUrlFor with a table-driven suite that never touches the network.
**NEEDS:** nothing

### 2026-08-27 17:58 PT — skill-docs (Gemini 3.7 Flash)
**Task:** T10 settings.toml support
**Did:** Starting T10: implementing `Settings` in `src/settings.cpp` and unit test suite in `tests/tst_settings.cpp`.
**Tests:** Baseline `./bin/build && ./bin/test` passing.
**Next:** Implement `Settings::defaults()`, parsing via `Publisher::parseToml`, atomic patching via `Publisher::patchToml`, enum validation, file watching, and comprehensive unit tests.
**NEEDS:** nothing


### 2026-08-27 18:02 PT — present (Claude Opus 5)
**Task:** T9 present mode
**Did:** Read `AGENTS.md`, `tasks/T9-present-mode.md`, `docs/renderer-contract.md`
and spec §5/§4.7/§4.9/§15. Confirmed baseline builds. Settled the design: C++
holds the authoritative deck position and drives every window through the frozen
page API, so the two windows cannot drift; `RenderHost` (app-shell) supplies the
`omapresentHost` bridge and `callScript`.
**Tests:** Baseline `./bin/build && ./bin/test` not re-run yet — other agents are
mid-flight in the tree.
**Next:** Header additions, `DeckNavigator`, the two windows, then the suite.
**NEEDS:** nothing yet

### 2026-08-27 18:10 PT — theme (Grok 4.6)
**Task:** T3 Omarchy theme bridge
**Did:** Implemented the Omarchy theme bridge. `parseColorsToml` handles both `colors.toml` shapes (rich named catppuccin/gruvbox and terminal gold-rush `color0`–`color15`), `#rgb`/`0x`/bare-hex, quoted and unquoted values, TOML tables, and never fails — missing keys follow `omarchy-theme-color`'s cascade (ANSI aliases, orange←yellow, brown mixed 50% toward black, brights mixed 20% toward white, dark_background mixed 25% toward black). `contrastRatio` is WCAG sRGB→linear luminance; `ensureContrast` walks HSL lightness only. Live reload watches `current/`, the theme dir, and `colors.toml`, debounced 150ms, emitting `themeChanged()` once per actual change. `setOverrideTheme` resolves user then system dirs and never writes the desktop. `installedThemes()` is sorted and deduplicated with user winning.
**Tests:** `tests/tst_omarchytheme.cpp` — 19 cases, all green in isolation (21 including init/cleanup). Official `./bin/build` is currently red in other agents' files (`backend.cpp` Command enum, `presentation` moc); theme objects compile and the suite passes via a one-suite binary.
**Next:** Done with T3.
**NEEDS:** nothing

### 2026-08-27 18:20 PT — webbundle (Claude Opus 5)
**Task:** T11 the static web bundle
**Did:** Read `AGENTS.md`, `tasks/T11-web-bundle.md`, `docs/renderer-contract.md`
and spec §9. Measured the one thing that decides the design: Chromium blocks
`<script type="module" src=…>` from `file://` (CORS, origin `null`), while inline
modules, classic external scripts, external CSS and relative images all load
fine. So a bundle that "opens from `file://` with no server" cannot reference the
renderer's ES modules as files — the module graph has to be inlined into the
page. That is what I am building.
**Tests:** Baseline `./bin/build && ./bin/test` not yet re-run; several agents are
mid-flight in the tree.
**Next:** `webbundle.cpp`: renderer inlining, media copy with stable names,
themed `index.html` + `read/index.html`, then `tests/tst_webbundle.cpp`.
**NEEDS:** nothing yet

### 2026-08-27 19:25 PT — doc-model (Claude Opus 5)
**Task:** T1 document model
**Did:** Implemented `DeckModel` in `src/deckmodel.cpp` — frontmatter (§4.4),
separators (§4.1), comments (§4.3), recall tags (§4.9), line mapping, `toJson()`.
Pipeline is: normalise line endings → lift frontmatter (only when line 0 is
`---`) → one comment-stripping pass over the body that also tracks fences →
split on separators → assemble slides. Comment stripping removes whole lines
(so `a\n// note\nb` stays one paragraph) but every surviving line keeps its
original line number, which is what the editor sync needs.

Decisions the spec left open, all commented in the code:
- **Empty slides are dropped.** A trailing `---`, two separators in a row, or an
  empty file yield no slide rather than a blank one.
- **A 9th recall tag loses the whole tag, not just the key** — clearing the key
  but keeping `skip` would leave a slide with no way to reach it. Logged with
  `qWarning` as the brief asks.
- **`sourceStartLine`/`sourceEndLine` are the content range** (first to last
  non-blank line). `slideIndexForLine` maps the wider span a slide *owns* — its
  separator line through the line before the next one — so a cursor on a blank
  line or a separator still resolves to a slide. Frontmatter lines and the lines
  of a `// ---` draft slide return -1.
- **An unterminated `%%` or `<!--` runs to end of file**, matching Obsidian and
  HTML.
- `--- {qq}` and other unparseable tag words are ignored rather than guessed.

`src/deckmodel.h` gained two private members only — `void parse()` and
`QVector<int> m_lineToSlide`. No existing declaration changed.
**Tests:** `tests/tst_deckmodel.cpp` — 84 cases, no `QSKIP`. Covers every
separator shape (Setext, `***`/`___`/`- - -`/`----`, missing blank line, inside
back-tick and tilde fences of length ≥ 3 with info strings, a short fence that
cannot close a long one), both frontmatter shapes plus malformed YAML, all three
comment forms inline and multi-line, comments left alone inside fences, the
`// ---` draft slide and its non-shifting of neighbouring recall bindings, the
recall-tag grammar, the 8-binding cap, CRLF input, and a 44-line realistic deck
asserting exact slide count, markdown, recall keys, flow indices and line ranges.
`./bin/build && ./bin/test` pass.
Also removed a stale `build-tests/moc_videocache.cpp` that a Qt 5.15 moc had
written into the shared build dir; it made every test build fail with "the moc
has changed too much". It is a generated artifact, so make regenerated it with
Qt 6 and the tree went green. Worth knowing if it comes back: something in the
tree is invoking Qt 5's `moc`/`qmake` rather than `qmake6`.
**Next:** nothing on T1.
**NEEDS:** Nothing blocking. One note for whoever builds present mode: if two
separators bind the same key, `DeckModel` keeps both slides and both keys — the
spec does not say which wins, so the key handler needs a rule (first match is
the obvious one).

### 2026-08-27 18:09 PT — theme (Grok 4.6)
**Task:** T3 Qt 6 getHslF compile fix
**Did:** `ensureContrast` no longer calls `QColor::getHslF` with `qreal*` pointers. It now reads `hslHueF()` / `hslSaturationF()` / `lightnessF()` (float) and writes lightness with `setHslF`. Same walk, same hue preservation.
**Tests:** `./bin/build && ./bin/test` pass. OmarchyThemeTest 21/21.
**Next:** T3 remains done.
**NEEDS:** nothing

### 2026-08-27 18:11 PT — media (Grok 4.6)
**Task:** T5 video, embeds and the offline cache
**Did:** Implemented `VideoCache` (spec §4.8). `hostFor` recognises YouTube / Vimeo / Loom / Descript / TikTok / X / Instagram / Facebook (real URL shapes, tracking params, http/https, www, trailing slashes), plus direct `.mp4`/`.webm`/`.mov` URLs and local video files. Unrecognised hosts and homepages without a video id are `NotAVideo` (QR). `isBareUrlLine` / `extractUrls` ignore prose and fenced code. `describe()` never hits the network: cache hit → `status: "cached"`, else constructed embed → `"embed"`, else `"qr"`. `prefetch` copies local files into `<deck-dir>/.omapresent-cache/` named by SHA-256 of the URL, writes `index.json`, and is a no-op on retry; oEmbed/download runs only on user-initiated prefetch. Index `file`/`poster` names cannot escape the cache dir.
**Tests:** `tests/tst_videocache.cpp` — 109 cases, no network. `./bin/build && ./bin/test` pass.
**Next:** done. Waiting for the next task.
**NEEDS:** renderer `src/renderer/media.js` host *list* matches (`youtube`, `vimeo`, `loom`, `descript`, `tiktok`, `x`, `instagram`, `facebook`, `direct`, `local`). Remaining drift, if you want lockstep:
- `isBareUrlLine` in C++ also accepts `file://`, `mailto:`, `www.` and schemeless `youtu.be`/`youtube.com`; media.js is http(s) only.
- C++ `hostFor` requires a video/post id (so `https://youtube.com/` is QR); media.js treats any URL on those domains as the host.
- C++ also maps `fb.com` and preserves Vimeo privacy hashes as `?h=` on the embed URL.
- `describe()` host keys and embed URLs for TikTok / X / Instagram / Facebook / Descript are in `embedUrlFor`; media.js currently only special-cases YouTube / Vimeo / Loom and otherwise returns the original URL.
If any of those should change, say so and I will match.

### 2026-08-27 18:14 PT — theme (Grok 4.6)
**Task:** T3 review 1 — ensureContrast wrong direction
**Did:** `ensureContrast` no longer picks a walk direction from `relativeLuminance(bg) < 0.5`. Mid-grey `#808080` (L=0.216) made that walk *up* to white (ratio 3.95, never 4.5) even though black on the same background is 5.32. It now walks both ends of the lightness axis, takes the first colour that clears `minRatio`, prefers the direction that keeps the original lighter/darker relationship when both work, and if neither clears returns the end with the higher ratio. Derivation still matches `omarchy-theme-color` (ANSI aliases, orange←yellow, brown mixed 50% toward black, brights 20% toward white).
**Tests:** Added mid-grey `#767676` on `#808080`, a luminance sweep including mid-tones, an unreachable 7.0 floor that returns the best available, and a preference case on `#767676` where both ends clear 4.5. Then `./bin/build && ./bin/test`.
**Next:** Done with review 1.
**NEEDS:** nothing
