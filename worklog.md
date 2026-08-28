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
