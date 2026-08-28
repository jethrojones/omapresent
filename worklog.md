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

### 2026-08-27 18:16 PT — media (Grok 4.6)
**Task:** T13 packaging — make the package actually build and install
**Did:** Taking over `pkgbuild/` from skill-docs. Starting by correcting `depends` against `pacman -Si` (there is no `qt6-quickcontrols2` package) and the `install -D` glob, then running `makepkg` in a scratch dir.
**Tests:** not yet
**Next:** Fix PKGBUILD, run makepkg, inspect the installed tree, namcap if available.
**NEEDS:** nothing

### 2026-08-27 18:19 PT — media (Grok 4.6)
**Task:** T13 packaging — make the package actually build and install
**Did:**
- Dropped `qt6-quickcontrols2` (not an Arch package; Quick Controls 2 lives in `qt6-declarative`). Spec §12 still names it; logged as a discrepancy. Added `hicolor-icon-theme`, `qt6-webchannel`, `ttf-ia-writer`. No `yt-dlp`, no bundled fonts.
- Fixed `install -Dm644 skill/reference/*.md dest/` (invalid with several sources) to `install -m644 -t dest files`.
- Kept local-checkout `source=()` / `cd "$startdir/.."` because there is no tagged release yet and Omarchy local-source packages are consumed as a tree, not a tarball. `build()` now qmake6+make into `$srcdir/build` so it does not share the agents' `build/` directory. Comment in the PKGBUILD says when to switch to `git+#tag=v$pkgver`.
- `makepkg -f -d` in `/tmp/omapresent-makepkg-*` succeeded (`-d` because `gst-plugins-bad`/`ugly` exist in extra but are not installed here and sudo needs a password; they stay in `depends` so a real pacman install pulls them). Package `omapresent-0.1.0-1-x86_64.pkg.tar.zst`.
- Installed tree verified: `/usr/bin/omapresent`, `LICENSE`, `NOTICE`, `welcome.md`, whole `skill/` (SKILL.md + four reference files), `.desktop` with `MimeType=text/markdown;`, scalable icon. No fonts, no yt-dlp.
- `.install` guards gtk-update-icon-cache / update-desktop-database with `command -v` and `|| true`. `post_remove` refreshes those caches (the files are already gone).
- `namcap` is in extra but not installed; running the unpacked extra package needs `pyalpm`. Manual stand-in: every `depends` name exists via `pacman -Si`; ELF `NEEDED` maps to `qt6-base`, `qt6-declarative`, `qt6-webengine` (plus glibc/gcc). `qt6-multimedia` / gstreamer / `ttf-ia-writer` / portal are runtime, not SONAMEs, kept because spec §12 / appearance DBus / codecs.
**Tests:** `bash -n` PKGBUILD and `.install`; `desktop-file-validate` clean; `makepkg --printsrcinfo` ok; package listing as above.
**Next:** done. Waiting for the next task.
**NEEDS:** nothing

### 2026-08-27 19:19 PT — renderer (Codex GPT-5.6 Sol)
**Task:** T2 shared renderer bundle — gate checkpoint
**Did:** Replaced the renderer stubs with the full DOM API, offline Markdown/KaTeX/QR bundle, theme-only CSS, pure parser/layout/media modules, PDF/read modes, and a standalone browser fixture. Autoreview findings were verified and fixed for navigation, pagination, recall export, long-read order, media control, forced QR, code classification, overview scrolling, and tall images.
**Tests:** 35 renderer tests pass. Chromium fixture and QR checks pass. A five-slide PDF fixture paginates to six pages. `./bin/build` passes. The shared test build stops in `tests/tst_integration.cpp` because `Slide` has no `operator==`.
**Next:** Retry `./bin/build && ./bin/test` after the integration owner repairs its compile error, then commit the renderer files.
**NEEDS:** The integration owner must remove `QList<Slide>::indexOf` / direct list comparison or add the frozen-contract-safe equality support. The backend owner must add bare local video filenames to `VideoCache::extractUrls()` so `describe()` can resolve them; the renderer already uses resolved asset entries and refuses broken raw paths.

### 2026-08-27 19:24 PT — renderer (Codex GPT-5.6 Sol)
**Task:** T2 shared renderer bundle — handoff
**Did:** Committed the complete renderer as `152abdd`. The bundle now supplies the frozen page API, QWebChannel state path, all closed-set layouts, fragments, scrolling, recall/blank/overview, offline Markdown/KaTeX/QR rendering, themed CSS, PDF/web output, vendor licences, and the standalone fixture. Final bounded Autoreview is clean.
**Tests:** 35 renderer tests pass. Chromium fixture, forced-QR, long-read order, and six-page PDF pagination checks pass. `./bin/build` passes. Full `./bin/test` remains blocked before execution by the unrelated `Slide` equality compile error in uncommitted `tests/tst_integration.cpp`.
**Next:** Integration owner fixes its compile error and reruns the shared gate. Renderer work is committed and ready.
**NEEDS:** Same two cross-owner items above: integration test equality compile fix; backend extraction of bare local video filenames.

### 2026-08-27 19:39 PT — publish (Codex GPT-5.6 Sol)
**Task:** T6 web publish and provider layer
**Did:** Implemented byte-preserving publish config, slugging, here.now create/update/history/revert/auth/claim/domain flows, safe private publishing, command publishing, and S3 Signature V4 uploads. Publishing now owns a bundle snapshot and fails closed for invalid access, unreadable config, and private modes that a provider cannot enforce. Final Autoreview is clean.
**Tests:** `tests/tst_publisher.cpp` — 12 cases pass with no network. An isolated T6 checkout passes `./bin/build && ./bin/test`. The shared `./bin/build` passes. The shared `./bin/test` cannot compile uncommitted `tests/tst_integration.cpp` because `Slide` has no `operator==`.
**Next:** T6 is complete. Retry the shared test gate after the integration owner repairs its compile error.
**NEEDS:** The integration owner must remove the `QList<Slide>` equality uses or add frozen-contract-safe equality support.

### 2026-08-27 21:10 PT — renderer (Codex GPT-5.6 Sol)
**Task:** T2 review 1 — rendered PDF list width and continuation footer
**Did:** Reproduced the welcome-deck PDF and inspected pages 2, 3, and 7 as images. Confirmed that list width collapses because `27ch` resolves on the small list container, and that the footer ends at content height on a continuation page.
**Tests:** Repro currently produces 53 PDF pages. Visual baseline captured under `/tmp/renderer-review-*.png` and `/tmp/renderer-footer-07.png`.
**Next:** Fix full-width list layout with a rendered-width regression test, then correct footer pagination and re-export.
**NEEDS:** nothing

### 2026-08-27 21:12 PT — publish (Codex GPT-5.6 Sol)
**Task:** T6 local here.now protocol integration coverage
**Did:** Started a loopback-only mock-server test for create, upload, refresh, finalize, access, authentication, failure, and command-provider environment flows.
**Tests:** not yet
**Next:** Add the local endpoint seam, drive the full state machine, then run the shared build and test gate.
**NEEDS:** nothing

### 2026-08-27 21:20 PT — present (Claude Opus 5)
**Task:** T9 present mode
**Did:**
- Implemented `src/presentation.cpp` in full. C++ holds the authoritative
  position of the talk in a new pure `DeckNavigator` and drives every window
  through the frozen page API, so the audience cannot drift from the presenter.
  Fragment counts and scroll fractions are the renderer's to know, so they are
  learned from its state events and remembered per slide (spec §4.7).
- Two separate top-level windows, `src/AudienceWindow.qml` and
  `src/PresenterWindow.qml`. The presenter carries the current slide, a real
  next-slide preview (a third renderer view, so it looks like what will appear),
  the notes as formatted Markdown, a click-to-reset elapsed timer, a wall clock,
  x/count, the recall bindings and the `Ctrl+?` sheet.
- Monitor assignment is the pure `assignOutputs()`; hotplug is wired to
  `screenAdded`/`screenRemoved`/`primaryScreenChanged`, and windows drop out of
  fullscreen to move, which is what makes a mid-talk projector land correctly.
- All of §5.2 through one `Presentation::handleKey`, called from both windows,
  so the two behave identically and the dispatch is unit-testable.
- Idle inhibit and DND are RAII holders (`IdleInhibit`, `DoNotDisturbHold`)
  released in the destructor as well as `stop()`, so a crash path still gives
  the desktop back. DND reads the prior state first through
  makoctl / swaync / dunstctl / `omarchy-toggle-notification-silencing`; idle
  uses the ScreenSaver DBus inhibit (which hypridle itself serves) plus
  `omarchy-toggle-idle`, and only touches a stay-awake it did not already find.
  Starting twice does not stack either one.
**Tests:** `tests/tst_presentation.cpp` — 44 cases, all green: navigation across
fragments and slide boundaries, the digit-then-Enter jump including past the end,
recall show/hide restoring slide + fragment + scroll exactly, scroll memory per
slide, and monitor assignment over six output lists. Both QML windows were also
instantiated for real against a live `Presentation` with WebEngine initialised,
and load clean.
**Next:** done with T9.
**NEEDS:**
- `app-shell`: register `PresenterWindow.qml` and `AudienceWindow.qml` in
  `src/resources.qrc`, and call `backend.presentation()->setQmlEngine(&engine)`
  in `main.cpp`. Without the qrc entries present mode logs a component error and
  opens nothing; without the engine it builds a private `QQmlEngine` instead.
- `app-shell`: `src/PreviewPane.qml` declares `WebEngineScript { ... }` and
  assigns `userScripts.collection`. On Qt 6.11 that type is `isCreatable: false`
  — instantiating it fails with "Element is not creatable", which takes the
  whole component down, so the preview pane never loads and never gets state
  events. I hit the same thing and moved to `runJavaScript(bridgeScript)` at
  `LoadSucceededStatus`, which is equivalent for a page that never navigates.
- `publish`: `WebBundle` cannot see the renderer when its directory is passed
  with a `..` in it. `rendererFiles()` compares `QDirIterator` paths (uncleaned,
  e.g. `.../build-tests/../src/renderer/render.js`) against a
  `QDir::cleanPath()`ed prefix, so nothing ever matches and `build()` fails with
  "no render.js". That is 1 of the 3 `WebBundleTest` failures currently red in
  the shared tree; the other two are its expectations against the renderer's new
  output. None of them are mine — `tst_presentation` is 44/44.

### 2026-08-27 21:05 PT — doc-model (Claude Opus 5)
**Task:** T12 end-to-end integration tests
**Did:** Added `tests/tst_integration.cpp` (21 cases), the fixture decks under
`tests/integration/`, and a small node bridge
(`tests/integration/renderer-answers.mjs`) that runs the *real* renderer ES
modules and hands their answers back to the C++ test, so the two sides are
compared against shipping code rather than a copy of its rules. One line added
to `tests/tests.pro`.

The welcome deck is the primary fixture: 25 slides, parses with no warnings,
frontmatter including the nested `publish:` map, one recall key (`q`) bound
once, no slide empty of both audience content and notes, and exactly the five
images it deliberately leaves missing to demonstrate the placeholder. Which
lines count as images is decided by asking the renderer, since it is the side
that decides what reaches the screen.

Seam fixtures, each a file someone will really open: CRLF throughout, a UTF-8
BOM, a final separator with no trailing newline, an unclosed fence, a
frontmatter-only deck, a deck with no separators, a `root:` that does not
exist, and the same filename at two depths under one root (shortest wins, and
`shortestUniqueReference` round-trips back to the deeper file).

Two changes in my own files: `DeckModel` now ignores a leading U+FEFF while
parsing — `QString::fromUtf8` drops it so the ordinary read path never sees
one, but a path that keeps it should not silently hide the frontmatter, and
`source()` still returns the text untouched. `Slide` gained `operator==` and a
`QDebug` operator so a failed comparison prints the slide rather than an
address.

**Tests:** `tests/tst_integration.cpp` — 21 cases, all green. `./bin/build`
passes. `./bin/test`: every suite green except `WebBundleTest` (3 failures,
`buildsAgainstTheRealRenderer` reports "no render.js in .../src/renderer" while
that file exists) — the webbundle agent's own source and suite, both being
edited as I write this, and unrelated to my files.

**Heads-up:** my files were staged and ready to commit when commit `4703b94`
("Record the first end-to-end run…") swept them into itself, so `src/deckmodel.*`,
`tests/integration/**`, `tests/tst_integration.cpp` and the `tests/tests.pro`
line are committed there rather than under a commit of mine. Content is intact;
I have not touched that commit. Worth a reminder that `git add -A` picks up
other agents' staged work.

**NEEDS:** three disagreements between components. Each is pinned by a test that
records *both* answers, so the suite stays green and a fix on either side fails
loudly and has to be acknowledged.

1. **`AssetIndex::looksLikeImageReference` treats any line containing `/` or
   `\` as an image path.** So prose, table rows and display math in the welcome
   deck become image references: `$$e^{i\pi} + 1 = 0$$`, `Omapresent natively
   recognizes YouTube, Vimeo, … X/Twitter, …`, `| \`B\` / \`W\` | Black out /
   White out audience display`. Renderer's `parseBareImage` requires no
   whitespace unless the line starts with `./`, `~/`, `$VAR/` or `file://`. 15
   phantom references in the manual alone, and some of them resolve against
   whatever happens to exist on the machine. Already filed by the orchestrator
   as `tasks/review/assetindex-prose-bug.md` — same bug, found independently.
   Pinned by `IntegrationTest::assetIndexReadsProseAsImagePaths` (`extras.size()
   == 15`).

2. **`AssetIndex::extractReferences` matches `![[...]]` and `![](...)` inside
   inline code spans, and does not skip the `qr:` prefix.** The renderer's
   `parseObsidianImage` requires the embed to be the whole line and explicitly
   excludes `qr:`. So the manual's own syntax documentation yields
   `figure.png`, `diagram.png` (twice) and `qr:https://...` as images to
   resolve. Pinned by
   `IntegrationTest::rendererAgreesOnInlineCodeAndQrReferences`.

3. **The renderer reads a bare local video filename as a web URL.**
   `urlFromLine()` in `src/renderer/media.js` accepts a schemeless domain, and
   `.webm` / `.mov` / `.txt` look like TLDs — so `clip.webm` alone on a line
   parses as `https://clip.webm`, gets no video host, and draws a **QR code**
   instead of playing the file. `VideoCache::hostFor("clip.webm")` says
   `LocalFile`. The tell is that `clip.mp4` behaves *differently* — the digit in
   `mp4` fails the TLD pattern, so it plays. `./clip.webm` is read correctly by
   both, which is what makes the bare form a trap rather than a policy. Spec
   §4.8 lists `.mp4`/`.webm`/`.mov` together, so the three should behave alike.
   Pinned by `IntegrationTest::rendererReadsBareVideoFilenamesAsWebUrls`.

   Minor, not pinned: the renderer's `parseBareImage("// a comment")` returns an
   image reference, because `//` matches its explicit-path prefix. Harmless
   today only because `DeckModel` strips `//` lines before the renderer sees
   them — worth knowing if anything ever feeds it raw file text.

### 2026-08-27 21:15 PT — media (Grok 4.6)
**Task:** T5 follow-up — `clip.webm` C++ vs renderer disagreement
**Did:** Decided `clip.webm` is a **local video file**, not a URL and not an image. `hostFor` stays `LocalFile`. `isBareUrlLine` stays **false** — a relative filename is not a URL; the renderer was only calling it one because `urlFromLine()` treats `.webm` as a TLD (`https://clip.webm` → QR). `extractUrls` now also returns whole-line local video paths so `describe()`/`prefetch()` (and Backend, which iterates `extractUrls`) see `clip.webm`. `clip.webm` is not an image: `AssetIndex::looksLikeImageReference` requires `/` or an image extension, and `.webm` is neither. `./clip.webm` *is* an image under that slash rule — see NEEDS.
**Tests:** `tst_videocache` — `clip.webm` isBareUrlLine false, host local, extractUrls includes it. Running `./bin/build && ./bin/test`.
**Next:** waiting on the renderer `urlFromLine` fix below; then both sides report `isBareUrlLine=false`, `host=local`.
**NEEDS:**

Contract sentence for `docs/renderer-contract.md` (please land this):

> A whole line that is only a relative or absolute path ending in `.mp4`, `.webm` or `.mov` is a local video (`host: "local"`). It is not a URL (`isBareUrlLine` is false — those suffixes are file extensions, not TLDs, so `clip.webm` is not `https://clip.webm`) and not an image. `VideoCache::extractUrls` still returns these lines so `describe()` / `prefetch()` see them.

Renderer (`src/renderer/media.js`): in `urlFromLine`, return `null` for a schemeless value that matches `DIRECT_VIDEO_EXTENSIONS` *before* the schemeless-domain regex. After that, `isBareUrlLine("clip.webm")` is false and `videoHostFor("clip.webm")` is `"local"`, matching C++. Same for `.mov`. `notes.txt` should also stop parsing as `https://notes.txt` (same TLD trap).

Renderer (`deckparse.js`) / assets: `parseBareImage` and `AssetIndex::looksLikeImageReference` treat any slash path as an image, so `./clip.webm` is currently both a local video and an image. Exclude `.mp4`/`.webm`/`.mov` from image detection.

T12: drop `rendererReadsBareVideoFilenamesAsWebUrls` once the renderer matches; it pins the bug.

### 2026-08-27 21:16 PT — theme (Grok 4.6)
**Task:** T3 live-reload verification
**Did:** Confirmed `setDirectoriesForTest` is the state-root hook. Watcher already covers `current/` so a `theme` symlink swap is visible (the target watch is not). Reload now keeps the previous palette when `colors.toml` is missing or empty mid-session instead of falling through to the dark default. Added tests that repoint a fake `current/theme` symlink, collapse the switch burst to one `themeChanged()`, keep colours when the theme dir or `colors.toml` vanishes, and assert `installedThemes()` lists a duplicated name once with the user copy winning.
**Tests:** OmarchyThemeTest 29/29, including `liveReloadFollowsThemeSymlink`, `liveReloadDebouncesSymlinkBurst`, `keepsPaletteWhenLiveThemeVanishes`, `keepsPaletteWhenColorsTomlUnreadable`. `./bin/build` green. Official `./bin/test` still has one failure in `IntegrationTest::assetIndexReadsProseAsImagePaths` (not our file).
**Next:** live-reload gap closed.
**NEEDS:** nothing

### 2026-08-27 21:17 PT — reviewer (Codex GPT-5.6 Sol)
**Task:** T14 adversarial review of risky paths
**Did:** Loaded the vault context, task scope, shared-tree rules, and current worktree state. Started a read-only review of publish, file-copy, cache, network, and hostile-input paths.
**Tests:** Not run yet.
**Next:** Read the named spec sections and trace each risky path. Add only reproducible findings and T14-owned tests.
**NEEDS:** nothing

### 2026-08-27 21:44 PT — present (Claude Opus 5)
**Task:** Review — AssetIndex prose-as-image and inline-code/qr references
(inherited `src/assetindex.cpp` and `tests/tst_assetindex.cpp` from `assets`)
**Did:** Two fixes, both the same class of bug — documentation *about* the
syntax being read as a use of it.
- `looksLikeImageReference` no longer accepts any line containing a slash.
  A known image extension still accepts on its own, because spec §4.5 says a
  path with spaces needs no escaping and that is the only signal that can carry
  `./img/chart with spaces.png`. Without an extension the line now has to be a
  single token rooted like a path (`/`, `./`, `../`, `~/`), carry no prose
  punctuation, and name something after the last slash. A root is what separates
  a path from two words joined by a slash.
- `extractReferences` blanks out inline code spans before matching (offsets
  preserved, so result order is unchanged) and skips the `qr:` prefix in both
  the `![[…]]` and `![](…)` forms, matching the renderer's `parseObsidianImage`.
  Fenced blocks were already handled.
**Deliberate narrowing:** `sub/photo` — a relative path with neither a root nor
an extension — is no longer a bare-line image reference. It is structurally
identical to `and/or`, so there is no rule that keeps one and drops the other.
`sub/photo.png` and `./sub/photo` both still work. The two cases that changed
were the only positives in the old suite that relied on it.
**Tests:** `tests/tst_assetindex.cpp` — 24 cases green, including the explicit
positive/negative table from the review note and a new
`extractReferencesIgnoresInlineCodeAndQr` covering backticked example syntax,
double-backtick spans, an unclosed backtick, and `qr:` in both embed forms.
Measured against the real manual: `welcome/welcome.md` went from 20 extracted
references to 5 — exactly its five real `![[…]]` embeds, with all 15 phantoms
gone. `IntegrationTest::welcomeImagesResolveOrAreDeliberatelyMissing` is green.
`./bin/build` passes.
**Next:** done. Waiting for the next task.
**NEEDS:**
- `doc-model`: `tests/tst_integration.cpp` pins the old behaviour on purpose
  ("so a fix on the AssetIndex side fails here and has to be acknowledged"), so
  both now fail. Acknowledging it here with the exact new answers — I did not
  touch your file:
  - `rendererAgreesOnInlineCodeAndQrReferences` (~L583): `lines.at(0)`,
    `lines.at(1)` and `lines.at(2)` now all extract `QStringList()`. `lines.at(3)`
    is unchanged at `{"inline.png"}` — an inline image inside a speaker note is
    still worth resolving, because the presenter and web roles render notes as
    formatted Markdown. C++ and the renderer now agree on all four.
  - `assetIndexReadsProseAsImagePaths` (~L336): `looksLikeImageReference(
    "Recognises YouTube, Vimeo, TikTok, X/Twitter and Facebook.")` is now false,
    `extractReferences("![[qr:https://example.com]]")` is now `QStringList()`,
    and `extras.size()` is now **0**, not 15. The `inlineCodePhantoms` list and
    the loop that explains each extra have nothing left to explain.
- Not mine, still red in the shared tree:
  `IntegrationTest::rendererReadsBareVideoFilenamesAsWebUrls` (`clip.webm`) —
  the bare-local-video item the renderer agent already raised for the backend
  owner's `VideoCache::extractUrls()`.
- `assets` (or whoever lands it): `src/assetindex.h` line 50-51 still documents
  the old rule, "it contains a '/' or ends in a known image extension". The
  behaviour is now the narrower one above; the header comment should follow. I
  left the header alone since the handover named only the two files.

### 2026-08-27 21:19 PT — renderer (Codex GPT-5.6 Sol)
**Task:** T2 review 1 — rendered PDF defects and local video parity
**Did:** Made lists use the full content width. Rounded each PDF slide to full physical pages before print, so the footer appears only on the final page and sits at its bottom. Made bare `.mp4`, `.webm`, and `.mov` names local videos before schemeless URL detection. Added a Chromium width regression and media parity cases.
**Tests:** Renderer 37/37 green. Real Qt export is 42 pages, down from 53 in this tree. Visually checked pages 2, 3, and 7; lists are full width and the continuation footer is on the bottom edge. `./bin/build` passes. Full `./bin/test` is red only in three T12 tests that pin the old asset and media disagreements.
**Next:** Commit the renderer files after T12 updates its stale assertions, then rerun the full gate.
**NEEDS:** T12 must replace or remove `assetIndexReadsProseAsImagePaths`, `rendererReadsBareVideoFilenamesAsWebUrls`, and the old phantom expectations in `rendererAgreesOnInlineCodeAndQrReferences`. The renderer and VideoCache now agree that `clip.webm` is local and not a bare URL. Land the local-video sentence already proposed in the media worklog in `docs/renderer-contract.md`.

### 2026-08-27 21:25 PT — webbundle (Claude Opus 5)
**Task:** T11 the static web bundle
**Did:** Implemented `src/webbundle.cpp`. `build()` writes `index.html` (slides,
arrow/space/Home/End keys, swipe, speaker notes as a toggleable subtitle track
fed from `onState.notesHtml`, slide counter, progress bar) and
`read/index.html` (masthead, article measure, slides flowed as sections), the
two cross-linked. Both set `mode: "web"` and `data-op-view`, so the renderer
picks role `web` itself and flows notes as body text in the long read — that
attribute is already honoured in `render.js:415`, so no contract change was
needed.

The load path is the one thing worth knowing here. Chromium refuses to fetch an
ES module from a `file://` page (CORS, origin `null`) — measured, not assumed —
so a bundle that has to open by double-clicking `index.html` cannot use
`<script type="module" src=…>`. Inline modules, plain external scripts,
external CSS and relative images all work, and `fetch()` does not. So:
`assets/render.js` is a plain script carrying the renderer's modules as text,
turning each into a blob: URL and starting the entry module (`window.
omapresentReady`, which the page chrome awaits); the deck JSON is inlined into
each page; CSS and media stay ordinary relative files. I first wrote a
source-rewriting flattener and threw it away — `vendor/markdown-it.mjs` ends in
a mid-line `export { ir as default }` and `katex.mjs` in a 200-character export
list, and rewriting those by hand is a bug farm. Blob URLs let the module
system do it, so vendored bytes travel unmodified.

Also: media copied to `media/<slug>-<8 hex of the source path>`, so names are
stable across rebuilds and two `budget.png` from different folders coexist;
palette baked into `assets/theme.css` as custom properties (one theme, no
`prefers-color-scheme` — a published deck keeps the theme it was written in);
an unresolvable or remote image becomes the missing-asset placeholder rather
than a link off the bundle, while a hosted video's `embedUrl` stays, since it
is the only way to play an uncached embed; `files()`/`totalBytes()` are exactly
what landed on disk; a failure rolls back precisely what it wrote and leaves a
pre-existing directory alone. `render.html`, `renderer.qrc` and `package.json`
do not travel — how the renderer is built is not a published deck's business.
`vendor/LICENSES.md` and the three `LICENSE.*` files do travel, verbatim, into
`assets/vendor/`.

**Tests:** `tests/tst_webbundle.cpp` — 24 cases, all green: both views and their
cross-links, `files()` compared against a walk of the output, exact progress
totals, image copying and relative rewriting (including the `../` the long read
needs), same-named images from different directories, placeholders, embed URLs
kept, a leak scan for `qrc:`/`file://`/the author's absolute path/`http` over
every generated file, stable rebuild names, rebuild-in-place, palette baking
including a colours.toml value that is not a colour, module packaging, the two
pure helpers (`moduleImports`, `withModuleImports`) against minified and
multi-line spellings, `</script>` in deck markdown, and four failure paths
(no deck, no renderer, unwritable parent, unwritable output — the last two
asserting no partial bundle). One case builds against the real `src/renderer/`.

End-to-end: built `welcome/welcome.md` into a temp dir (33 files, 7.9 MB, of
which KaTeX is 7.4) and opened both pages in Chromium from `file://` — 25
slides, notes rendering as subtitles, 53 notes flowed as body text in the long
read, and no `qrc:`, absolute path or remote asset reference in any generated
file.

**Next:** T11 is done. Happy to take `Backend::buildWebBundle()` next if the
orchestrator wants it moved onto this class.

**NEEDS:**
- `src/backend.cpp` (app-shell): `Backend::buildWebBundle()` still hand-rolls a
  bundle — it copies `:/renderer` into a temp dir, writes `deck.json` beside it
  and copies `render.html` to `index.html`. That produces one view, no long
  read, no media, no theme, and it cannot open off a disk. It should be
  `WebBundle` with `setDeck(deckDocument("web"))`, `setDeckDir(...)`,
  `build(dir)`, and `files()` handed to `Publisher::publish()`. Two lines.
- `tests/tst_integration.cpp` (integration): three cases are red in the tree and
  none of them are mine — they are that file's own pinned tripwires firing
  because the fixes they were watching for landed.
  `assetIndexReadsProseAsImagePaths` still asserts that
  `looksLikeImageReference("… X/Twitter …")` is true, but the uncommitted
  `src/assetindex.cpp` now requires a rooted path token, which is the behaviour
  the case's comment asked for; `rendererReadsBareVideoFilenamesAsWebUrls` and
  `rendererAgreesOnInlineCodeAndQrReferences` are the same story after
  `1348e97`. They need re-pinning to the fixed behaviour by their owner. I left
  them alone per AGENTS.md §1.
- `src/renderer/vendor/LICENSES.md` (renderer): two small inaccuracies now that
  a published bundle exists. It names `katex.min.css`, but the file on disk and
  in the bundle is `katex.css`; and "Each file is the upstream distribution
  build, unmodified" is worth a sentence saying that a published bundle carries
  these same bytes embedded as text inside `assets/render.js`, with the licence
  files copied verbatim beside it. The bytes are unmodified — only the
  renderer's own relative import specifiers are rewritten, and the vendored
  libraries have none — but a reader of that file should not have to infer it.

### 2026-08-27 21:21 PT — theme (Grok 4.6)
**Task:** T3 — is the contrast floor wired to the audience window?
**Did:** Traced `OmarchyTheme::palette()` into the deck JSON. `ensureContrast` is never called outside `omarchytheme.cpp` / `tst_omarchytheme.cpp`. `Backend::deckDocument` always passes `m_theme.palette()` into `RenderHost::composeDeck` for preview, present, pdf and web. `Presentation::setDeck` stores that one palette and `applyTo` `render`/`update`s the same `d->deck` for audience, presenter and next-slide preview. AudienceWindow.qml chrome also reads `presentation.palette` verbatim. Spec §6's audience-only floor does not exist in practice. Added `OmarchyTheme::paletteForRole(palette, role)` — `audience` nudges `foreground`/`muted`/`dark_foreground`/`accent` against `background`; presenter/preview/pdf/web/export/editor get the exact theme.
**Tests:** `paletteForRoleFloorsAudienceOnly`, `paletteForRoleLeavesExactThemeForOtherRoles`. OmarchyThemeTest 31/31. `./bin/build` green.
**Next:** waiting on app-shell and present to call the helper.
**NEEDS:** app-shell (`src/backend.cpp` `deckDocument`, and PDF/web callers): keep `m_theme.palette()` for preview, pdf and web. present (`src/presentation.cpp` `setDeck` / `applyTo`): the audience `WebEngineView` must receive a deck whose `palette` is `OmarchyTheme::paletteForRole(exact, "audience")`. Presenter window, next-slide preview, and `AudienceWindow.qml` chrome currently share one unfloored object — only the audience page (and any audience chrome that draws text) should get the floored copy. Do not floor the presenter, preview pane, PDF, or published web bundle.

### 2026-08-27 22:06 PT — present (Claude Opus 5)
**Task:** Review — local video is not also an image (renderer contract §3a)
**Did:** `looksLikeImageReference` now returns false for a bare line ending in
`.mp4`, `.webm` or `.mov`, before either of the image tests runs. That closes
the overlap §3a names: `./clip.webm` used to satisfy rule 1 as a local video and
rule 3 as an image at the same time, because the rooted-path test does not care
what the extension is. `extractReferences` inherits it through its bare-line
branch, which is the path a deck actually takes.
**Scope I did not take, deliberately:** explicit `![[clip.webm]]` and
`![](clip.webm)` embeds still yield a reference. §3a governs "a line whose
entire content is one reference", and `VideoCache::extractUrls` only ever
returns whole trimmed lines (`isBareUrlLine(trimmed) || hostFor(trimmed)`), so
stripping video extensions from the embed forms too would leave
`![[clip.webm]]` picked up by nobody and resolving to nothing. The renderer's
`parseObsidianImage` keeps them as references as well, so the two sides still
agree. Say the word if you want them dropped and the pickup moved to VideoCache.
**Tests:** `tests/tst_assetindex.cpp` — 24 cases green. Added `clip.webm` and
`./clip.webm` as the review asked, plus `clip.mp4`, `./clip.mp4`,
`~/Videos/talk.mov`, `/srv/media/keynote.MP4`, `../shared/demo.WebM` (case
insensitive) and `./clip.webm|600` (size hint stripped first) to the negative
table, and the same four through `extractReferences` including a video and an
image on adjacent lines. `welcome/welcome.md` still extracts exactly its 5 real
embeds. `./bin/build && ./bin/test` both pass, whole tree green — integration is
39/39, including `rendererReadsBareVideoFilenamesAsWebUrls` and both tests that
were pinning the old AssetIndex behaviour.
**Next:** done. Waiting for the next task.
**NEEDS:** nothing

### 2026-08-27 21:22 PT — media (Grok 4.6)
**Task:** T5 follow-up — offline video path end to end
**Did:** Added three cases in `tests/tst_videocache.cpp` that walk the real sequence with no network: a deck dir named `Deck café テスト` with a tiny local `clip.webm` as a bare line (ffmpeg if present, else an `ftyp` box), prefetch into `.omapresent-cache/`, `describe()` `file://` round-trip after deleting the original, `index.json` parsed by a second `VideoCache`, a stale index entry whose file is gone falling to `embed`/`qr` (not `cached`), prefetch of `http://127.0.0.1:<closed-port>/clip.mp4` finishing in `failed` within 5s, and every written file staying under the cache dir.
**Tests:** VideoCacheTest 113 cases. `./bin/build && ./bin/test` pass (full tree green).
**Next:** done. Waiting for the next task.
**NEEDS:** nothing

### 2026-08-27 22:40 PT — doc-model (Claude Opus 5)
**Task:** T13 live-sync tests, plus rewriting the T12 pins whose bugs are fixed
**Did:** Added `tests/tst_livesync.cpp` (23 cases) for the editor-to-everything
seam of spec §4.10. It drives the real path — `DeckModel::setSource` →
`toJson()` → `DeckNavigator::setDeck` — plus `RenderHost`, so it needs no
compositor and no window. One line added to `tests/tests.pro`. I did not touch
`src/backend.cpp` or `src/renderhost.cpp`.

Covered: editing the current slide, another slide, or adding one after it all
hold the index; scroll position survives an edit inside the slide and is
remembered per slide across edits; deleting down to fewer slides clamps;
deleting everything leaves no dangling index and typing the deck back recovers;
`slideIndexForLine` round-trips inside every slide, is never non-monotonic
across a file, and returns -1 for frontmatter and for the lines of a `// ---`
draft slide; removing the blank line above a separator merges two slides and the
navigator clamps rather than dangling; and every keystroke on the way to typing
`---` parses to something sane, because the preview re-renders on all of them.

**Two decisions**, asserted rather than assumed, both with the reasoning in the
test:
- **Deleting the slide the presenter is on keeps the index**, so they land on
  the slide that *followed* the deleted one — exactly where pressing Right would
  have taken them. Moving backwards would rewind the talk, and blanking the
  screen would be worse than either. It is also what every list UI does when you
  delete the selected row.
- **A separator line belongs to the slide it introduces**, so a caret parked on
  `---` presents from the slide below it — the one the author is about to write,
  not the one they just finished. This was already my T1 behaviour; it is now
  written down as a decision with a reason.

**Also:** rewrote the two T12 pins whose bugs the owners have now fixed, so they
assert the fix instead of the defect, keeping every input case:
`assetIndexReadsProseAsImagePaths` → `assetIndexTellsProseFromImagePaths` (now
data-driven: prose with a slash is prose, paths are paths, and per the new
contract §3a a local video is never also an image), and
`rendererReadsBareVideoFilenamesAsWebUrls` →
`videoFilenamesBehaveIdenticallyOnBothSides`.

The third pin had **changed shape rather than being fixed**, which is why it was
worth reading both answers instead of assuming. Everything agrees now except an
inline `![alt](x)` inside a paragraph, and that difference is correct rather
than a bug: contract §3 renders notes as formatted Markdown in the presenter and
web roles, so an inline image in a note really is drawn there and its file
really does need resolving. `AssetIndex` must collect it; the renderer's block
classifier is right that the line is not an image *block*. That now has its own
test, `inlineImagesInProseAreResolvedButNotDrawnAsBlocks`, saying so — no defect
filed.

**Tests:** `./bin/build` passes. `./bin/test` fully green — every C++ suite
(LiveSyncTest 23, IntegrationTest 39, DeckModelTest 84) and all 38 renderer
tests, 0 failures.

**Heads-up:** as in T12, my staged files were swept into another agent's commit
before mine ran — this time `0b23d2c` ("Exercise the offline video cache on a
real deck"). Content is intact and I have not touched that commit, but two
agents are now running `git add -A`.

**NEEDS:** one defect, for whoever owns `src/presentation.cpp`.

**`DeckNavigator::setDeck` follows the slide *number*, not the slide.** It only
clamps the index, so an insertion or deletion *above* the presenter silently
moves them onto different content. Insert a slide at position 2 while presenting
slide 7 and the audience jumps to what was slide 6 — mid-talk, with no
keystroke. Deleting above does the same in reverse. Pinned by
`LiveSyncTest::insertingASlideBeforeTheCurrentOneMovesTheContentAway` and
`deletingASlideBeforeTheCurrentOneMovesTheContentAway`; both are named after the
defect and should be renamed when it is fixed.

The fix is reachable from where the bug is, and the tests assert that too: the
slide that was on screen is still present in the deck `setDeck` receives, just
at a different flow index, so matching the outgoing slide's `markdown` against
the incoming slides and moving the index to where it went would do it. Falling
back to the current clamp when no match is found handles the case where the
current slide is the one being edited. `skippedSlidesDoNotShiftTheFlowIndex`
already passes and should keep passing — a `{q, skip}` slide is outside the flow,
so inserting one never moves anybody.

### 2026-08-27 21:26 PT — renderer (Codex GPT-5.6 Sol)
**Task:** T2 final verification — host state and fragment reveal
**Did:** Committed the PDF layout fixes and local-video parity. Added a Chromium interaction test that drives all four fragments on the fixture list, checks nested DOM order and computed visibility, confirms the fifth action advances to slide 3, and receives each state as serialized JSON through the bridge-shaped host callback. Verified the native path by stopping the real app in `RenderHost::state`; the stack entered through `QMetaObjectPublisher` and `Qt6WebChannel`.
**Tests:** `./bin/build && ./bin/test` passes. Renderer 38/38 green. Real PDF pages were inspected earlier; list width and continuation footers are correct.
**Next:** T2 is complete.
**NEEDS:** The app-shell owner must commit the current `src/PreviewPane.qml` bridge fix. The old committed version creates an uncreatable `WebEngineScript`; the current working-tree version runs `bridgeScript()` after load and is the version that passed the real QWebChannel check.

### 2026-08-27 21:29 PT — reviewer (Codex GPT-5.6 Sol)
**Task:** T14 adversarial review of risky paths
**Did:** Wrote `docs/review-findings.md` with five ranked findings. Added `SecurityTest` with passing command-injection and hostile-input checks plus expected failures for asset-root disclosure, cache-root writes, and bundle-output writes through symlinks. Verified the publish confirmation, upload snapshot, session-state, cache-name, packaging, and skill safety paths.
**Tests:** `./bin/build && ./bin/test` pass. `SecurityTest` has 7 passes, 3 expected failures, and 0 unexpected failures. All 38 renderer tests pass. Autoreview ran in local mode with Codex at high reasoning and returned no actionable findings.
**Next:** T14 is complete.
**NEEDS:** SEC-001 to AssetIndex/WebBundle owners; SEC-002 to renderer/app-shell owner; SEC-003 to VideoCache owner; SEC-004 and defense-in-depth SEC-005 to WebBundle owner. See `docs/review-findings.md` for exact reproductions and severity.

### 2026-08-27 21:32 PT — publish (Codex GPT-5.6 Sol)
**Task:** T6 local here.now protocol integration coverage
**Did:** Added a loopback-only here.now endpoint seam and an in-process HTTP server. The suite drives anonymous and authenticated create, presigned PUT, expired-URL refresh, finalize, access, private placeholder, failure, and command-provider flows. It asserts request bodies, signature queries, headers, ordering, idempotent version handling, and no work before explicit `publish()`.
**Tests:** `PublisherTest` — 20 passed. `./bin/build && ./bin/test` pass. Final Autoreview is clean.
**Next:** T6 is complete.
**NEEDS:** The spec names `account_members`, but documented frontmatter has no account-members value. Current here.now also requires password changes through `/metadata`, not `/access`; tests follow the live API.

### 2026-08-28 06:18 PT — root (Codex GPT-5.6 Sol)
**Task:** SEC-001 asset-root symlink disclosure
**Did:** Started the inherited AssetIndex and WebBundle security fix. Confirmed that indexed symlinks and bundle media copies currently trust paths without canonical-root checks.
**Tests:** Not run yet.
**Next:** Reject canonical targets outside the deck or asset root. Keep symlinks that resolve inside those roots.
**NEEDS:** The reviewer must remove the SEC-001 `QEXPECT_FAIL` after the fix makes its security check pass.

### 2026-08-28 06:20 PT — media (Grok 4.6)
**Task:** SEC-003 cache-directory symlink
**Did:** `VideoCache::cacheDir()` now resolves the deck canonically and returns empty (with `qWarning`) if `.omapresent-cache` is a symlink, is not a directory, or its canonical path is not inside the deck. `prefetch`/`writeIndex` re-check after `mkpath` so a raced symlink is not written through. Local `describe()` still answers from the original file; we just refuse to write outside the deck.
**Tests:** `VideoCacheTest::prefetchRefusesSymlinkCacheDir` passes. Full VideoCacheTest 114/114. `SecurityTest::videoCacheSymlinkCannotLeaveTheDeckDirectory` is now an **XPASS** (the pin is the only red).
**Next:** done. Waiting for the next task.
**NEEDS:** reviewer — please remove the `QEXPECT_FAIL` on `SecurityTest::videoCacheSymlinkCannotLeaveTheDeckDirectory`. SEC-003 is fixed; that expected-failure now fails as an unexpected pass.

### 2026-08-28 06:21 PT — renderer (Codex GPT-5.6 Sol)
**Task:** SEC-002 deferred remote media, then long-read typography review
**Did:** Confirmed that `mediaElement()` assigns HTTP sources to direct video and hosted iframe elements while the deck opens. Remote poster URLs can also request data before play. Started a source-based gate that keeps cached and local files eager but defers all HTTP media until the reader acts.
**Tests:** Not run yet.
**Next:** Add a browser regression for direct video and hosted iframe sources. Then fix the read-mode type branch from `tasks/review/longread-typography.md`.
**NEEDS:** Publish owner must keep the generated long-read bundle CSS in sync with the renderer CSS change.

### 2026-08-28 06:28 PT — reviewer (Codex GPT-5.6 Sol)
**Task:** T14 follow-up review of CLI, first-run skill links, and settings patches
**Did:** Started the second adversarial pass. Converted the SEC-003 expected failure to a normal regression assertion after the cache-symlink fix landed.
**Tests:** Not run yet.
**Next:** Check the CLI failure paths, skill-directory symlinks, hostile settings patches, and the live SEC-001 and SEC-005 fixes.
**NEEDS:** nothing

### 2026-08-28 06:24 PT — root (Codex GPT-5.6 Sol)
**Task:** SEC-001 asset-root symlink disclosure checkpoint
**Did:** AssetIndex now filters and re-checks canonical targets. WebBundle now accepts media only when its canonical target stays in the deck or asset root. Inside-root symlinks remain supported.
**Tests:** AssetIndexTest 26/26 passes. WebBundleTest has no failures. The SEC-001 security assertion is now an XPASS because its review pin remains.
**Next:** Run the corrected WebBundle symlink cases, Autoreview, and full verification after the review pin is removed.
**NEEDS:** reviewer — remove the SEC-001 `QEXPECT_FAIL` in `tests/tst_security.cpp`; the disclosure assertion now passes.

### 2026-08-28 06:26 PT — theme (Grok 4.6)
**Task:** Live verification of present mode on Wayland (HDMI-A-1, gold-rush)
**Did:** Ran `./build/omapresent present welcome/welcome.md` on the real Hyprland session. Did not switch the desktop theme. Present mode never appeared.

**What actually happened**
1. **Two windows?** No. `hyprctl clients` showed one `class=omapresent` window, title `welcome.md - Omapresent`, size 463×519, workspace 8, not fullscreen. That is the editor. No `audienceWindow`, no `Presenter - Omapresent`.
2. **Single-monitor audience fill + N overlay?** Untestable — present windows never opened. On one output the code would close the presenter and keep audience only (`Presentation::assignMonitors`); we never got that far.
3. **Presenter chrome** (current slide, next-slide preview, formatted notes, timer, clock, slide x/count, recall keys): not shown. N overlay (notes-only) is the single-screen substitute and also never appeared.
4. **Arrows / Space fragments / scroll mirror:** keys were sent at the editor (the only omapresent window). Welcome deck did not change on disk. No audience to mirror.
5. **Recall `Q` (`--- {q}` in welcome.md):** overlay never appeared.
6. **B / W / O / Esc:** B/W/O had no present-mode effect. Esc did not quit; the process was SIGTERM'd so it would not sit on the desktop.
7. **§6 contrast floor:** not visible. Code path in `presentation.cpp` does call `paletteForRole` for the audience deck, but no audience window loaded. Live theme is gold-rush `#d9d9d9` on `#121212` (already ~15:1), so even a working audience would likely match the presenter unless a weaker theme is used.

**Cause (stderr):** `omapresent: present mode: qrc:/AudienceWindow.qml: No such file or directory`. `src/AudienceWindow.qml` and `src/PresenterWindow.qml` exist on disk; `src/resources.qrc` does not list them, so `createWindow("qrc:/AudienceWindow.qml")` fails and `start()` leaves only the editor.

**Screenshots** (full 4K shots are the existing tiled desktop; the omapresent window is the editor):
- `/tmp/omapresent-present-verify/01-window-3-welcome.md_-_Omapresent.png` — editor, "Opened welcome.md", 1999 words
- `/tmp/omapresent-present-verify/01-launch-full.png` — full HDMI-A-1
- `/tmp/omapresent-present-verify/run3.stderr` — contains the QML miss (line with `qrc:/AudienceWindow.qml`)
- `/tmp/omapresent-present-verify/windows-launch.txt` — hyprctl dump

**Tests:** not a unit-test change. Present mode was not exercised beyond the failed window create.
**Next:** blocked on app-shell packing the two QML files.
**NEEDS:** app-shell (`src/resources.qrc`): add `AudienceWindow.qml` and `PresenterWindow.qml` so `qrc:/AudienceWindow.qml` and `qrc:/PresenterWindow.qml` resolve. Present already creates them from those URLs (T9 worklog asked for this). Until that lands, `omapresent present` cannot open either present-mode window. Do not need a theme change for a first visual of the contrast floor — gold-rush already clears 4.5; pick a mid-grey theme later if you want a visible delta.

### 2026-08-28 06:28 PT — root (Codex GPT-5.6 Sol)
**Task:** SEC-001 asset-root symlink disclosure
**Did:** Closed both trust points. AssetIndex filters canonical targets at scan time and resolve time. WebBundle accepts media only from the canonical deck or asset root and re-checks before copy. Symlinks that stay inside an allowed root remain supported.
**Tests:** AssetIndexTest 26/26. WebBundleTest 26/26. SecurityTest SEC-001 passes after the reviewer removed its pin. `./bin/build && ./bin/test` passes. Autoreview found no in-scope issue.
**Next:** Fix the generated long-read bundle typography with the renderer owner's read-mode CSS.
**NEEDS:** nothing

### 2026-08-28 06:36 PT — root (Codex GPT-5.6 Sol)
**Task:** Long-read article typography
**Did:** Reset the generated read-view stylesheet from projector layout to article flow. Added a readable heading and body scale, flowed speaker notes as prose, removed slide chrome, stacked article media, and kept separate 16:9 and 9:16 player frames. The deck view remains unchanged.
**Tests:** WebBundleTest 27/27. `./bin/build && ./bin/test` passes. A moved `file://` bundle rendered cleanly in Chromium at 1280x900 and 430x900. It has no `file:///`, `qrc:`, or `/home/jethro` references. Final Autoreview is clean after fixing its vertical-video finding.
**Next:** Return to SEC-004 streaming copies and SEC-005 output-root hardening.
**NEEDS:** nothing

### 2026-08-28 06:40 PT — root (Codex GPT-5.6 Sol)
**Task:** SEC-004 streaming copies and SEC-005 output-root symlinks
**Did:** Started the remaining inherited WebBundle findings. Confirmed that copyFile still reads a whole source and ensureDirectory still follows an existing output symlink.
**Tests:** Not run yet.
**Next:** Stream fixed-size chunks and validate every output path below the canonical bundle root.
**NEEDS:** The reviewer must remove the SEC-005 `QEXPECT_FAIL` when its assertion starts to pass.

### 2026-08-28 06:42 PT — reviewer (Codex GPT-5.6 Sol)
**Task:** T14 follow-up review of CLI, first-run skill links, and settings patches
**Did:** Added SEC-006 for CLI publish continuing after a directory or unreadable file fails to open, SEC-007 for a symlinked agent skills directory, and SEC-008 for a shadowed key in a duplicate settings table. Added passing checks for publish confirmation, missing files, unknown providers, no-frontmatter publish, existing skill content, long and malformed settings text, unknown-key preservation, and newline escaping. Converted the fixed SEC-001 and SEC-003 pins to normal assertions. SEC-005 remains an expected failure while its owner works on it.
**Tests:** `./bin/build && ./bin/test` passes. SecurityTest has 15 passes, 5 expected-failure assertions, and no unexpected results. All 39 renderer tests pass. Autoreview accepted and drove three test-hardening changes, then finished clean with no actionable findings.
**Next:** Convert SEC-005 when its fix passes. Route SEC-006 through SEC-008 to their owners.
**NEEDS:** Backend owner — SEC-006 and SEC-007. Settings or publish owner — SEC-008. WebBundle owner — SEC-005 pin remains live.

### 2026-08-28 06:53 PT — renderer (Codex GPT-5.6 Sol)
**Task:** SEC-002 deferred remote media and long-read renderer typography
**Did:** Committed SEC-002 as `aa4331a`. Remote video and embed sources now enter the DOM only after play. Cached and local video remain eager. Added an explicit themed play control, keyboard support, hosted autoplay requests, and correct media state for the desktop Space shortcut. Matched publish commit `47f9fa6` with a `#deck`-scoped read branch that uses article type and measure, flows speaker notes as prose, and leaves the slide view unchanged.
**Tests:** Real Qt open-deck repro made zero HTTP requests. Renderer 41/41. `./bin/build && ./bin/test` passes. Chromium images pass at 1280×900 and 430×900 for the moved `file://` bundle. Both isolated Autoreview gates are clean after their accepted fixes.
**Next:** Commit the long-read renderer half.
**NEEDS:** nothing

### 2026-08-28 06:44 PT — reviewer (Codex GPT-5.6 Sol)
**Task:** SEC-005 output-root regression pin
**Did:** Converted `bundleOutputSymlinkCannotLeaveTheOutputRoot` from an expected failure to a normal assertion after the WebBundle fix made it pass. Updated the review report to match.
**Tests:** `./bin/build && ./bin/test` passes. SecurityTest has 15 passes, 4 expected-failure assertions, and no unexpected results. Final Autoreview found no actionable issue.
**Next:** SEC-001, SEC-003, and SEC-005 are now normal regression guards.
**NEEDS:** nothing

### 2026-08-28 06:52 PT — root (Codex GPT-5.6 Sol)
**Task:** SEC-004 streaming copies and SEC-005 output-root symlinks
**Did:** WebBundle now copies files with a fixed 1 MiB buffer. It rejects symlinked output roots, directories, and final files. It verifies each directory against the canonical output root and commits files atomically with QSaveFile.
**Tests:** WebBundleTest 30/30, including a sparse multi-chunk file and both output-symlink forms. SEC-005 passes as a normal security test. `./bin/build && ./bin/test` passes. Final Autoreview is clean.
**Next:** All inherited WebBundle and AssetIndex review findings are complete.
**NEEDS:** nothing

### 2026-08-28 06:53 PT — reviewer (Codex GPT-5.6 Sol)
**Task:** Fix SEC-006, SEC-007, and SEC-008
**Did:** Started the routed fixes. Confirmed that the CLI ignores `open()` failure, first run accepts a symlinked skills directory, and `patchToml()` returns after the first duplicate key.
**Tests:** Not run yet.
**Next:** Add one checked command-file load path, reject escaped skill directories, patch the effective last duplicate key, and convert each security pin.
**NEEDS:** nothing

### 2026-08-28 06:55 PT — renderer (Codex GPT-5.6 Sol)
**Task:** Renderer review closeout
**Did:** Committed the long-read renderer branch as `22624ab`. SEC-002 is `aa4331a`. The publish and renderer halves now agree, and the final images show an article read view with an unchanged deck view.
**Tests:** `./bin/build && ./bin/test` passes. Renderer 41/41. SEC-002 and long-read Autoreview passes are clean.
**Next:** Renderer review work is complete.
**NEEDS:** nothing

### 2026-08-28 07:01 PT — renderer (Codex GPT-5.6 Sol)
**Task:** Long-read article scale and section rhythm
**Did:** Kept all article-body rules inside `#deck`. Set h1/h2/h3 to 2.1/1.65/1.3rem against the 1rem body. Reduced section separation from 7rem to 4rem around the rule. The WebBundle masthead remains larger at 2.6rem and owns only content outside `#deck`.
**Tests:** Chromium wide and 430px images pass. Renderer 41/41. `./bin/build && ./bin/test` passes. Isolated Autoreview is clean.
**Next:** Commit the renderer refinement.
**NEEDS:** nothing

### 2026-08-28 07:02 PT — renderer (Codex GPT-5.6 Sol)
**Task:** Long-read article scale committed
**Did:** Committed the renderer refinement as `d912ff3`. The WebBundle and renderer ownership boundary is now explicit and has no body-rule duplication.
**Tests:** `./bin/build && ./bin/test` passes. Renderer 41/41. Autoreview is clean.
**Next:** Renderer long-read work is complete.
**NEEDS:** nothing

### 2026-08-28 06:57 PT — renderer (Codex GPT-5.6 Sol)
**Task:** Long-read article scale and section rhythm
**Did:** Accepted the ownership split. Renderer owns only `#deck`. WebBundle owns the masthead, read navigation, and footer. Started a smaller article heading scale and a tighter section separator.
**Tests:** Not run yet.
**Next:** Render wide and narrow moved bundles, then run renderer and shared gates.
**NEEDS:** WebBundle can keep the masthead at 2.6rem. The renderer h1 target is 2.1rem.

### 2026-08-28 06:58 PT — root (Codex GPT-5.6 Sol)
**Task:** Spec §8 CLI PDF export verification
**Did:** Started real-PDF checks for canvas geometry, tall-slide pagination and scale, expanded fragments, recall-slide order, and Ctrl+P.
**Tests:** Not run yet.
**Next:** Add a CLI-driven export suite that checks Poppler geometry and text output.
**NEEDS:** nothing

### 2026-08-28 09:30 PT — doc-model (Claude Opus 5)
**Task:** T14 resource + contrast integration tests, and a spec-deviations note
**Did:** Six cases added to `tests/tst_integration.cpp` (now 45), one
`RESOURCES` line added to `tests/tests.pro`, and `docs/spec-deviations.md`
written. Nothing else touched; I did not go near `src/backend.cpp` or
`src/main.cpp`.

**The resource seam.** `everyQrcPathNamedInCppResolves` walks `src/*.cpp` for
`qrc:/...` string literals and asks Qt whether each resolves. The resources are
now linked into the test binary, so it is the real resource system answering
rather than a guess about the mapping — that is what the one added line to
`tests/tests.pro` is for. `everyQrcEntryExistsOnDisk` parses both `.qrc` files
and checks the reverse, and `everyQmlFileIsRegisteredAsAResource` catches the
mistake at the moment it is made: a `.qml` written into `src/` and never listed.

Why it matters: present mode never opened at all until today, because
`AudienceWindow.qml` and `PresenterWindow.qml` were written and unit-tested but
never added to `src/resources.qrc`. Every suite stayed green, because a unit
test reads the file from disk and the running app reads it from a resource, and
until now nothing put those two facts in the same room. All three tests have a
guard against passing vacuously — the scan asserts it found the paths it expects,
and the QML check asserts `:/ThisWindowDoesNotExist.qml` really is absent, so a
resource system that answered yes to everything would be caught.

**The contrast floor.** `onlyTheAudienceGetsTheContrastFloor` uses a gruvbox-ish
palette whose foreground genuinely fails 4.5:1 against its background, and
asserts the audience copy clears the floor on all four text keys while the
background and mode are untouched, and that `preview`, `presenter`, `pdf`,
`web`, `export` and `editor` come back byte-identical.
`aThemeThatAlreadyClearsTheFloorIsUntouched` covers the no-op case, and
`presentationHandsTheAudienceItsOwnPalette` runs it end to end through
`RenderHost::composeDeck` and `Presentation::setDeck`, checking that the deck
the audience window is sent carries the nudged palette while everything else
carries the theme exactly as written, and that only the colours differ.

**`docs/spec-deviations.md`** records three decisions, each with what the spec
says, what reality is, what the code does and why — deliberately separate from
`docs/review-findings.md`, which is for bugs:
- **D-001** §9 names an `account_members` access mode that no §4.4 frontmatter
  value can reach. The four documented `access:` values collapse to three API
  modes, with `link` and `public` both meaning `anyone_with_link`; `public` is
  honoured rather than rejected because §4.4 documents it.
- **D-002** §9 sets a publish password through `PATCH .../access`; the live API
  wants `PATCH .../metadata`. The tests follow the live API, because the server
  is the authority for its own shape.
- **D-003** §12 requires `qt6-quickcontrols2`, which is not an Arch package —
  Quick Controls and Quick Dialogs ship inside `qt6-declarative`. The PKGBUILD
  drops it and adds `qt6-webchannel`, which §12 does not mention but the
  renderer bridge needs.

**Tests:** `./bin/build` passes. `./bin/test` fully green — every C++ suite
(IntegrationTest 45, DeckModelTest 84, LiveSyncTest 23) and all 41 renderer
tests, 0 failures.
**Next:** nothing assigned.
**NEEDS:** nothing new. The T13 navigator defect
(`DeckNavigator::setDeck` follows the slide number, not the slide) is still
open and still pinned by two `LiveSyncTest` cases named after it.

### 2026-08-28 07:02 PT — renderer (Codex GPT-5.6 Sol)
**Task:** Long-read article scale closeout
**Did:** Final renderer files are ready. The article body stays inside `#deck`. H1/h2/h3 are 2.1/1.65/1.3rem. Section separation is 4rem total. The WebBundle masthead stays at 2.6rem outside `#deck`.
**Tests:** Chromium wide and 430px images pass. Renderer 41/41. `./bin/build && ./bin/test` passes. Isolated Autoreview is clean.
**Next:** Commit the renderer refinement.
**NEEDS:** nothing

### 2026-08-28 07:06 PT — root (Codex GPT-5.6 Sol)
**Task:** Spec §8 CLI PDF export verification
**Did:** Added a real CLI export suite. It checks default 16:9, 4:3, and 16:10 page boxes; multi-page tall slides with unchanged word geometry; fully expanded fragments; skipped and normal recall slides in document order; and the Ctrl+P QPrintDialog route. No product code changed.
**Tests:** ExportTest 9/9. A four-page tall-slide PDF also passed visual review. `./bin/build && ./bin/test` passes. Isolated Autoreview is clean.
**Next:** Keep these as §8 regression guards.
**NEEDS:** nothing

### 2026-08-28 07:35 PT — webbundle (Claude Opus 5)
**Task:** tasks/review/longread-typography.md — the long read was set for a projector
**Did:** Two causes, and the type scale was only one of them.

*Two owners.* `bundle.css` carried a near-copy of the renderer's read-mode
article rules — column, section flow, heading sizes, code, blockquote, tables —
which lost to `deck.css` on specificity anyway. Agreed the split with the
renderer agent through the orchestrator before editing: renderer owns everything
inside `#deck`, I own the page around it. Deleted my ~120 duplicated lines. One
owner, no cascade fight.

*The page around the article never opted out of the projector.* `deck.css` sets
every heading centred inside `24ch` at `clamp(3rem, 8.5vw, 8.5rem)` and every
`p`/`li`/`blockquote` at `clamp(1.65rem, 3.2vw, 3.4rem)`. `.op-masthead`,
`.op-chrome`, `.op-footer` and the deck view's `.op-notes` track all sit outside
`#deck`, where the read branch never reaches them — so they were being set as
slides. That is why the masthead was centred, broke early at 24ch, and drew its
rule to the outer column width so it overhung the prose it introduced. Each now
opts out by name at the article's own text measure (38rem less its two 1.25rem
gutters), with the document title one step above the article's `h1`
(`clamp(2.3rem, 6vw, 2.6rem)` against their `2.1rem`) at every width.

The same leak was setting the deck view's speaker-note subtitles at projector
size. Fixed here too.

**Tests:** 30 cases, all green. Replaced the inherited
`longReadResetsProjectorTypographyIntoArticleFlow`, which asserted the literal
source text of `deck.css` — the renderer's file to reformat, so it broke on
`22624ab` without anything regressing. It now asserts what is mine: that
`bundle.css` styles the page and never `#deck` (the assertion that keeps the
duplication from creeping back), that the masthead opts out by name, and that
each page carries the `data-op-view` flag the renderer keys its article branch
off. §9.2's note promotion itself is only observable in a rendered DOM — the
deck JSON is inlined in both pages, so note text is in both files either way —
so it is verified in a browser, below, not from C++.

**Verified by looking.** Built `welcome/welcome.md`, copied the bundle to an
unrelated path, rendered from `file://` with no server, at 1280 and 420 wide.
Screenshots in `~/Pictures/omapresent/webbundle-verification/`:
- `before-read.png` — masthead centred, rule overhanging, projector scale
- `final-read-top.png`, `after-read-tall.png` — the article: masthead aligned to
  the measure, note prose flowed as body text, code, lists, media down the page
- `final-read-narrow.png` — 420px, gutters and hierarchy intact
- `after-deck.png` — deck view, no regression from the deletions
- `after-deck-notes-probe.png` — the NEEDS below, demonstrated

**Next:** nothing outstanding on my side.

**NEEDS:**
- `src/renderer/deck.css` (renderer): spec §9.1's notes-as-subtitles is invisible
  in a published deck view. `deck.css:59-64` gives `.op-current-slide` and
  `.op-recall-overlay` `height: 100vh`. In the app the page is only the deck so
  100vh and "fill the container" coincide; in a bundle the page is chrome +
  `#deck` + the notes track + the progress bar, and a 100vh slide pushes the
  last two past the viewport where `body { overflow: hidden }` clips them. The
  DOM is correct — track populated, not hidden, `data-role="web"` so
  `deck.css:513` shows it — it is purely laid out off-screen. Evidence rather
  than a guess: changing only that declaration to `height: 100%` in a *copy* of
  a built bundle brings the track back correctly sized
  (`after-deck.png` vs `after-deck-notes-probe.png`). I did not change it in the
  repo: `height: 100%` may not resolve in the app, where `#deck` is
  `min-height: 100%` rather than a definite height, so the right form is the
  renderer's call. Anything container-relative works for the bundle.
- Observation, no action asked: since the SEC-001 trusted-roots work a bundle no
  longer carries `media/` for `welcome.md` — the missing-image placeholder used
  to travel as the desktop background (spec §4.5 step 5) and now does not, which
  took the bundle from 33 files / 7.9 MB to 32 / 1.27 MB. Shipping the author's
  wallpaper to viewers may well be the wrong default, so this may be correct as
  it stands; flagging it only because §4.5 step 5 still describes the old
  behaviour and nobody has decided which one wins for a published deck.

### 2026-08-28 07:10 PT — orchestrator (Claude Opus 5)
**Task:** Catch-up log. Jethro asked whether everything is going in the worklog;
it was not. Agents have logged well (62 entries), but I had exactly one entry
here from setup, and had been putting my own findings in
`docs/acceptance.md` instead. That file is the right place for the
requirement-by-requirement record, but it is not this narrative, and the two
should not have diverged. What follows is the orchestrator's half, backfilled
from commits and verification runs. Logging here as I go from now on.

**Coordination and infrastructure**
- Froze the interfaces before any agent started (six headers plus
  `docs/renderer-contract.md`), each with a compiling stub, so ten agents could
  work in one tree without waiting on each other.
- Added `flock` to `bin/build` / `bin/test` after realising eight agents sharing
  one `build/` would corrupt each other's object files and produce failures
  belonging to nobody.
- `bin/test` never built the application, so three CLI security suites skipped
  themselves everywhere including CI. Fixed; running them immediately surfaced
  SEC-006.
- Landed `docs/renderer-contract.md` §3a settling what a bare line is
  (local video → URL → image → prose) after three components each decided
  independently and drifted.
- Vendored markdown-it, KaTeX and qrcode-generator myself when that work was
  blocking the renderer's critical path; verified all three import and run
  offline. This collided briefly with the renderer agent's own plan for
  `vendor/`, which it reconciled — my fault for not announcing it first.
- Brokered the long-read CSS ownership split between `renderer` (inside `#deck`)
  and `webbundle` (masthead/chrome/footer outside it) when they could not
  address each other directly.

**Fixes I made myself, because the owning agent's provider had died**
- `src/resources.qrc`: registered `AudienceWindow.qml` and
  `PresenterWindow.qml`. They were written, reviewed and unit-tested and never
  registered, so `omapresent present` silently opened nothing. Present mode did
  not exist.
- `src/presentation.cpp`: `closeWindows()` deleted the windows synchronously
  from inside `handleKey`, which QML invokes on those very windows — Qt calls
  that fatal, so every Esc aborted with a core dump. `deleteLater` now.
- `src/presentation.cpp`: `stop()` released the idle and DND holds *after*
  `closeWindows()`, so on the crashing path they never ran and a talk left the
  user's notifications switched off. Released first now.
- `src/omarchytheme.cpp`: one-line Qt 6 `getHslF` signature fix, after the tree
  sat red for ~15 minutes blocking every agent's verification.
- `src/assetindex.h`, `src/renderer/vendor/LICENSES.md`: stale comments.
- Recovered and committed uncommitted work from four agents that hit their
  provider session limit mid-task, after verifying it built and passed.

**Verification I did rather than delegated** (detail in `docs/acceptance.md`)
- Ran the app. Nobody had. Editor, present mode, PDF export, published bundle.
- Reintroduced the exact `resources.qrc` defect to prove the new regression
  guard actually catches it. It does.
- Proved SEC-002 with a listening HTTP server: zero requests on deck open.
- Proved SEC-001 with a symlink escaping an asset root: refused, while a
  symlink inside the root still resolves.
- Proved SEC-006 against the real CLI: directory, unreadable and missing files
  all exit 1 before the publish path.
- Checked PDF geometry by hand (`4:3` → 960×720, `16:9` → 960×540) and found
  four `ExportTest` failures were test-harness bugs, not product bugs — and said
  so, to stop an agent "fixing" correct behaviour into a red test.
- Live DND cycle: off → on for the presentation → off after Esc.

**Provider attrition, which became the limiting factor rather than the work**
- agy (Gemini): individual quota exhausted, ~7 days. Lost `assets` and
  `skill-docs` mid-task.
- claude: session limit; lost all four agents at once, recovered ~01:30.
- grok: weekly limit reached, offered to upgrade or buy credits. That is
  Jethro's spending decision, not mine, so I dismissed it unselected and treated
  both grok agents as gone.
- codex: still running throughout.
Work was redistributed each time, and file ownership handed back deliberately
when agents returned so they would not collide with their stand-ins.

**Next:** route the deck-view notes defect the webbundle agent just reported;
release `src/main.cpp` / `src/backend.cpp` back to app-shell once the reviewer
is clear of them.
**NEEDS:** nothing

### 2026-08-28 07:07 PT — reviewer (Codex GPT-5.6 Sol)
**Task:** Close SEC-006 through SEC-008 and finalize the review report
**Did:** Committed the three fixes as `3e54048`. Released `src/main.cpp` and `src/backend.cpp` back to app-shell. Recast all eight findings as fixed historical results with commit status, original reproductions, and live regression coverage. Kept both passed-check sections.
**Tests:** SecurityTest 15/15 and renderer 41/41 pass. Autoreview accepted one present-mode recovery fix, then finished clean. A later shared run was red only in an active, unrelated presentation/export change.
**Next:** App-shell can edit its files. The review report is complete.
**NEEDS:** The active presentation/export owner must return the shared full suite to green.
