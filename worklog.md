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

### 2026-08-28 08:14 PT — present (Codex GPT-5.6 Sol)
**Task:** T9 duplicate slide identity follow-up
**Did:** `DeckNavigator::setDeck` now selects an active duplicate by its matching direct left and right context. Nearest old index breaks only equal context scores. A deleted active slide no longer overwrites a matched fallback slide's saved scroll; new edited fallback content still keeps the active scroll.
**Tests:** `./bin/build` passes. `QT_QPA_PLATFORM=offscreen QT_QPA_PLATFORMTHEME= QT_STYLE_OVERRIDE=Fusion ./build-tests/tst_omapresent -silent`: PresentationTest 55/55 and LiveSyncTest 23/23 pass. The same binary has 7 Publisher socket failures and 6 Chromium export sandbox failures. `./bin/test` cannot open GTK display `:0` in this sandbox and its renderer discovery test fails.
**Next:** Run the shared gate in a desktop-capable environment.
**NEEDS:** The sandbox cannot provide the GTK display, local Publisher socket, or Chromium sandbox support for the full gate.

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

### 2026-08-28 07:32 PT — root (Codex GPT-5.6 Sol)
**Task:** Orchestrate Jethro's presenter and editor usability fixes
**Did:** Reviewed the vault startup state, project task ownership, current worklog, live Herdr agents, and the three-file uncommitted presentation fragment fix. Split the new work into disjoint T9 presenter, T8 editor, and T2 renderer assignments. Grok remains unused. Claude remains unused until its 11:30 PT reset.
**Tests:** Not run yet. The inherited working tree has changes only in `src/presentation.cpp`, `src/presentation.h`, and `tests/tst_presentation.cpp`.
**Next:** Start Terra/MAX owners for T9 and T8, plus a Codex Spark owner for recall-overlay rendering. Monitor each through Herdr, then route a fresh review and full build/test gate.
**NEEDS:** nothing

### 2026-08-28 07:36 PT — present (Codex GPT-5.6 Sol)
**Task:** T9 presenter-mode repair
**Did:** Started from the handed-off fragment-count changes. Inspecting navigation identity preservation and the QML/Wayland resize path.
**Tests:** Not run yet.
**Next:** Add focused regressions, verify the shared build and tests, and check live resizing if safe.
**NEEDS:** nothing

### 2026-08-28 07:36 PT — codex (Codex GPT-5)
**Task:** T2 renderer recall-overlay fix
**Did:** Began T2 focused pass for recall overlay behavior and state restoration.
**Tests:** Not run yet.
**Next:** Validate recall overlay fragment reveal and restore behavior with renderer DOM tests, then run renderer suite and `./bin/build && ./bin/test`.
**NEEDS:** nothing

### 2026-08-28 07:36 PT — app-shell (Codex GPT-5)
**Task:** T8 editor preview-follow and present-control pass
**Did:** Started a focused inspection of the editor, backend preview bridge, and test seams.
**Tests:** Not run yet.
**Next:** Bind preview state to the editor caret, add the footer control and shortcut directions, then verify.
**NEEDS:** nothing

### 2026-08-28 07:43 PT — codex (Codex GPT-5)
**Task:** T2 renderer recall-overlay fix
**Did:** Completed recall overlay reveal and restore behavior and added a DOM regression test for full fragment reveal plus state restoration.
**Tests:** `node --test tests/renderer/*.test.mjs` (2 failed: `interaction.test.mjs`, `visual.test.mjs` due Chromium SIGTRAP in this environment); `./bin/build && ./bin/test` (renderer suite fails in same Chromium crash path).
**Next:** Re-run renderer/browser tests on a headful Chromium-capable runner.
**NEEDS:** none

### 2026-08-28 08:05 PT — present (Codex GPT-5.6 Sol)
**Task:** T9 duplicate slide identity follow-up
**Did:** Started the review-finding repair after re-reading the shared presentation diff.
**Tests:** Not run yet.
**Next:** Preserve the active duplicate occurrence and saved state, add focused regressions, then run the presentation and shared gates.
**NEEDS:** nothing

### 2026-08-28 07:45 PT — skill-docs (Codex GPT-5)
**Task:** T7 clarify how to start a presentation
**Did:** Started a documentation-only pass across the T7-owned README, skill, and welcome deck. The pass will state the four launch paths and their starting positions, including the intended footer Present button.
**Tests:** Not run yet.
**Next:** Update owned docs, run skill sync and the full build/test gate, then commit only owned files and the appended worklog entries.
**NEEDS:** nothing

### 2026-08-28 07:45 PT — app-shell (Codex GPT-5)
**Task:** T8 editor preview-follow and present-control pass
**Did:** The editor now follows its caret in the preview. Debounced live edits use `update()` and append `goto()` only when the caret changes slides. Added the accessible footer Present control, shared start actions, and clear start directions in the shortcut sheet.
**Tests:** `./bin/build` passes. `OmapresentTest::followsTheEditorCaretInTheLivePreview` passes (3/3 under offscreen Qt); OmapresentTest passes 23/23. The shared full gate remains red from active T9 LiveSync failures and Chromium sandbox failures in export/renderer tests.
**Next:** T8 complete.
**NEEDS:** No T8 blocker. The orchestrator must rerun the full gate after T9 and Chromium environment work settle.

### 2026-08-28 07:46 PT — skill-docs (Codex GPT-5)
**Task:** T7 clarify how to start a presentation
**Did:** Made the four launch paths explicit in `README.md`, `skill/SKILL.md`, `skill/reference/recipes.md`, and `welcome/welcome.md`: `F5` starts from the beginning; `Ctrl+Return` and the footer `Present` button start from the current editor slide; `omapresent present FILE` starts from the CLI. No code or QML changed.
**Tests:** `./bin/check-skill-sync` passes. `bash -n pkgbuild/PKGBUILD` passes. `namcap` is not installed. `./bin/build` passes. `./bin/test` remains red in the inherited shared tree: the headless Qt/Chromium environment reports display, Chromium, Publisher listen, LiveSync, and PDF export failures; the welcome integration checks pass when run with the display theme unset.
**Next:** Handoff to the orchestrator.
**NEEDS:** App-side first-run skill linking remains open. Omarchy uses `/usr/share/omarchy/default/agents/skills` and links skills into `~/.agents/skills`, `~/.claude/skills`, `~/.codex/skills`, and `~/.pi/agent/skills`; no user skill tree exists under `~/.local/share/omarchy`.

### 2026-08-28 07:47 PT — present (Codex GPT-5.6 Sol)
**Task:** T9 presenter-mode repair
**Did:** Completed the handed-off fragment-count correction. `DeckNavigator::setDeck` now follows unchanged slide Markdown and remaps its fragment and scroll state across insertions or deletions above it; changed current content falls back to the clamped flow position. Audience, presenter, and presenter-preview `WebEngineView`s now bind directly to their QML client size and coalesce a renderer resize event after a Wayland configure.
**Tests:** `./bin/build` passes. Full `./bin/test` in the normal session: PresentationTest 52/52 and renderer 42/42 pass. The shared gate is red only in four stale `tests/tst_livesync.cpp` assertions: the two defect-named insertion/deletion cases, the old zero-fragment expectation, and the merge-then-split index expectation.
**Next:** Orchestrator: rename and update those four LiveSync assertions, then rerun the full gate and the live resize check.
**NEEDS:** Exact live Hyprland tile-resize verification was not run: the agent sandbox cannot safely drive an isolated compositor window. `tests/tst_livesync.cpp` is outside T9 ownership and must be updated to the new identity-preserving behavior.

### 2026-08-28 07:52 PT — codex (Codex GPT-5)
**Task:** LiveSync stale assertion follow-up
**Did:** Started the owned test-only update for the four stale LiveSync cases after T9's identity and fragment-count corrections.
**Tests:** Focused run confirmed four stale LiveSync failures; unrelated Publisher and Chromium environment failures remain.
**Next:** Rename and update the four assertions, run the requested LiveSync inspection and full build/test gate, then make one scoped commit.
**NEEDS:** nothing

### 2026-08-28 07:56 PT — codex (Codex GPT-5)
**Task:** LiveSync stale assertion follow-up
**Did:** Renamed the two defect-named insertion/deletion tests. Updated them to follow unchanged content at flow indices 8 and 6. Updated the empty-deck fragment count to 0. Updated merge-then-split to follow `# Three` to flow index 2.
**Tests:** `QT_QPA_PLATFORM=offscreen QT_QPA_PLATFORMTHEME= QT_STYLE_OVERRIDE=Fusion ./build-tests/tst_omapresent` — LiveSyncTest 23/23 pass. `./bin/build` passes. Chromium export checks remain blocked by the sandbox refusal to shut down its browser sandbox (`Operation not permitted`); the orchestrator will run the full gate outside the sandbox.
**Next:** Orchestrator runs the full gate outside the sandbox.
**NEEDS:** nothing

### 2026-08-28 08:04 PT — codex (Codex GPT-5)
**Task:** T2 renderer recall overlay live-update consistency
**Did:** Starting focused pass for recall overlay state across live `update()` and host callback consistency.
**Tests:** not run yet.
**Next:** Add a metrics fixture that updates while recall is visible, extend assertions, run renderer tests and build gate.
**NEEDS:** none

### 2026-08-28 08:04 PT — app-shell (Codex GPT-5)
**Task:** T8 immediate present-from-caret correction
**Did:** Started the review-finding fix. Inspecting the launch path and its test seam after re-reading the shared diff.
**Tests:** Not run yet.
**Next:** Reparse before deriving the caret slide, add an immediate-separator regression, then verify.
**NEEDS:** nothing

### 2026-08-28 08:06 PT — codex (Codex GPT-5)
**Task:** T2 renderer recall overlay live-update consistency
**Did:** Kept recall active across `replaceDeck` when the bound key still exists. Added a recall fixture path that calls `update()` while recall is visible and extended the recall browser regression to verify overlay visibility, fragment reveal, and restored slide/fragment/scroll after hide.
**Tests:** `node --test tests/renderer/*.test.mjs` (failed in `interaction.test.mjs` and `visual.test.mjs` due Chromium headless crash in this environment). `./bin/build && ./bin/test` (failed during renderer browser tests due the same Chromium crash path; C++ suites pass).
**Next:** Run on a headful/remote Chromium runner to get assertion-level confirmation of the new recall + update path.
**NEEDS:** none

### 2026-08-28 08:09 PT — app-shell (Codex GPT-5)
**Task:** T8 immediate present-from-caret correction
**Did:** Present from the footer and Ctrl+Return now rebuilds the current editor text before it maps the caret to a slide. Added a regression that inserts a separator and launches immediately through the footer action.
**Tests:** The focused new Omapresent check passed (3/3); the shared test runner then reports the selected test as unknown in other suites. `./bin/build` passes. Headless OmapresentTest passes 24/24. `./bin/test` remains blocked by the missing GTK display and Chromium sandbox limits.
**Next:** T8 correction complete.
**NEEDS:** nothing

### 2026-08-28 08:20 PT — codex (Codex GPT-5)
**Task:** T2 recall goto rebasing during recall
**Did:** Updated `goto()` to resync `recallSnapshot.slide` while recall is active so host rebase updates do not force overlay replacement. Added a recall regression: open recall, `update()` with a slide inserted before the hidden underlying slide, host `goto` to the re-based index, then hide recall to verify restored slide, fragment, and scroll.
**Tests:** `node --test tests/renderer/*.test.mjs` (chromium-backed interaction path still fails in this sandbox). `./bin/build && ./bin/test` (build passes, renderer browser tests fail in this environment's Chromium sandbox path).
**Next:** Re-run on a headful Chromium runner for full assertion verification.
**NEEDS:** none

### 2026-08-28 08:26 PT — present (Codex GPT-5.6 Sol)
**Task:** T9 duplicate slide identity policy
**Did:** Replaced duplicate-content heuristics with sequence proof. The active slide keeps state only when every longest ordered content alignment maps it to one new slide. Equal adjacent insertions and deletions are ambiguous because the frozen deck JSON has no edit range; they now use the clamp fallback and keep a surviving fallback slide's saved scroll.
**Tests:** `QT_QPA_PLATFORM=offscreen QT_QPA_PLATFORMTHEME= QT_STYLE_OVERRIDE=Fusion ./build-tests/tst_omapresent -silent`: PresentationTest 55/55 and LiveSyncTest 23/23 pass. Publisher socket and Chromium export sandbox failures remain outside T9.
**Next:** Run the full gate where Publisher can bind a test socket and Chromium can start.
**NEEDS:** To identify an exact duplicate edit rather than use the deterministic fallback, the app must pass the editor edit range or a stable slide ID to presentation.
### 2026-08-28 08:29 PT — codex (Codex GPT-5)
**Task:** Fix finding 3 — recall host-goto rebase during active recall
**Did:** Kept `goto()` on active recall from rebuilding the overlay and only rebased `recallSnapshot.slide`; preserved `recallSnapshot.fragment` and `recallSnapshot.scrollTop` so restore stays exact. Existing recall reveal path stays forced to `showAllFragments` with `data-revealed="true"`.
**Tests:** `node --test tests/renderer/*.test.mjs` (passes all non-browser tests; `interaction` and `visual` still fail in this sandbox from Chromium `SIGTRAP` sandbox error), `./bin/build && ./bin/test` (C++ suites pass, renderer browser suite fails in this environment for the same Chromium path).
**Next:** Run in desktop-capable Chromium runner for full assertion-level validation of `recall-after-goto-fragment = 2`.
**NEEDS:** none

### 2026-08-28 08:30 PDT — codex (Codex GPT-5)
**Task:** Recall fragment preservation while active recall is rebasing on host goto
**Did:** In the recall-active `gotoSlide` path, after re-basing `recallSnapshot.slide` to the new slide index, restored `fragment` from `recallSnapshot.fragment` clamped to the underlying target slide fragment count. This keeps the recall overlay mounted and makes host `goto` emit the preserved fragment value while deck shape changes underneath.
**Tests:** No sandbox Chromium run per instruction. Root desktop-capable Chromium run is required to verify `recall-after-goto-fragment` now restores to `2`.
**Next:** Root runs focused renderer Chromium suite.
**NEEDS:** none

### 2026-08-28 08:32 PT — codex (Codex GPT-5)
**Task:** T2 recall goto scroll preservation during active recall
**Did:** In recall-active `goto()`, kept overlay mode and restored hidden state by rebasing `recallSnapshot.slide`, restoring `fragment` from saved snapshot, re-rendering the rebased underlying slide, storing/reapplying `recallSnapshot.scrollTop` to the new scroller, and emitting state. This preserves scroll and fragment across host goto while recall stays visible and fully revealed.
**Tests:** Not run in this sandbox. Root desktop-capable Chromium suite should now observe `data-recall-after-goto-scroll-top == 180` and matching hide restore value.
**Next:** Root reruns focused Chromium suite.
**NEEDS:** none

### 2026-08-28 08:34 PT — codex (Codex GPT-5)
**Task:** Stabilize recall DOM fixture timing under host goto
**Did:** Updated the recall fixture timing to wait one `requestAnimationFrame` after host `goto` before reading `data-recall-after-goto-*` state and calling `hideRecall`. Assertions were not changed.
**Tests:** No sandbox Chromium run per instruction. Root desktop-capable Chromium run should now capture post-layout scroll and fragment values after frame commit.
**Next:** Root reruns focused renderer suite.
**NEEDS:** none

### 2026-08-28 08:36 PT — codex (Codex GPT-5)
**Task:** Remove redundant `renderCurrent` in recall restore path
**Did:** Bounded change only in `hideRecall()`: removed the extra `renderCurrent()` so hide flows now follow the required order (`renderOverlays`, `restoreRevealState`, clear snapshot, `emitState`) and avoid a second post-hide base-slide reset to scrollTop 0.
**Tests:** No sandbox Chromium run. Root desktop-capable focused Chromium suite should validate line-70 `after-hide` scroll behavior with this correction.
**Next:** Root reruns focused renderer test.
**NEEDS:** none

### 2026-08-28 08:36 PT — orchestrator (Codex GPT-5)
**Task:** Usability repair closeout
**Did:** Coordinated T2, T7, T8, and T9 fixes. Presentation windows reflow after live Wayland resize. Recall shows every fragment on the first key press and restores moved hidden state. The editor preview follows the caret and live edits. The footer now has a Present control. User guidance lists all launch paths. Independent review found no remaining actionable issue.
**Tests:** `./bin/build` passes. Desktop-capable `QT_QPA_PLATFORMTHEME= QT_STYLE_OVERRIDE=Fusion ./bin/test` passes: OmapresentTest 24/24, PresentationTest 55/55, LiveSyncTest 23/23, renderer 42/42, and all other suites green. Live Hyprland check reflowed the audience from 1901x2106 to 3816x2106; Q showed the full five-bullet recall slide on the first press; the editor preview and footer Present button were visible.
**Next:** Commit the four scoped changes and the worklog.
**NEEDS:** AGY remains quota-blocked. No Grok or Claude work was used for this repair.

### 2026-08-28 11:11 PT — orchestrator (Codex GPT-5)
**Task:** Full specification completion audit
**Did:** Coordinated two read-only Herdr audits and checked the release records. The usability repair is complete, the worktree is clean at `a14c7aa`, and all eight security findings are fixed. The full specification is not complete. Required product gaps remain in offline-safe remote images, application settings wiring, offline media preparation, published subtitle layout, publish controls, welcome-deck access, and small title/wallpaper contracts. Release verification also remains open for package lint, CI Chromium coverage, a public release/package, and live multi-monitor hotplug plus idle inhibit.
**Tests:** No tests rerun for this read-only audit. The latest recorded desktop-capable gate remains fully green: OmapresentTest 24/24, PresentationTest 55/55, LiveSyncTest 23/23, renderer 42/42, and all other suites green.
**Next:** Split the required product gaps into new owned tasks. Update `docs/acceptance.md` as each requirement is proved.
**NEEDS:** Do not call the project complete until the product gaps and release checks above are closed.

### 2026-08-28 11:12 PT — orchestrator (Codex GPT-5)
**Task:** Start the specification and release completion phase
**Did:** Created disjoint T15 application, T16 renderer, T17 release, and T18 icon assignments. The release task includes a tracked GitHub tag and package, Chromium CI coverage, package lint, and the normal Omarchy distribution path. The icon task requires the built-in ChatGPT image tool.
**Tests:** Not run. This entry records task ownership before agents edit.
**Next:** Start Claude, Codex Spark, release, and image agents. Check them on timers and route review after each handoff.
**NEEDS:** No blocker.

### 2026-08-28 12:08 PT — codex (GPT-5.3 Codex Spark)
**Task:** T16 renderer completion
**Did:** Read the project and vault rules. Started the renderer completion work in the assigned files.
**Tests:** Not run yet.
**Next:** Add regression tests for remote images, published subtitles, title fallback, and secure wallpaper fallback. Implement each fix and run the full gate.
**NEEDS:** nothing

### 2026-08-28 12:15 PT — app-shell (Claude Opus 5)
**Task:** Visual verification of the editor and present mode on the live desktop
**Did:** Ran the app on Hyprland (HDMI-A-1, gold-rush) and captured each step.
Screenshots are under
`/tmp/claude-1000/-home-jethro-projects-omapresent/857540a4-fc78-44cc-a730-8297c1738c08/scratchpad/`.

1. **Editor + live preview, themed — works.** `v1-open.png` (welcome.md):
   editor left, rendered slide right, gold-rush colours, header/footer and
   `1/25` slide number drawn by the renderer.
2. **Live update holds position — works.** With a seeded session position of
   slide 2, the preview opened on Slide Three (`v5-restore.png`), and typing in
   the editor left it on Slide Three rather than jumping to slide 1
   (`t2-typed.png`). Editing audience content re-rendered it: the heading
   became "Slide Three ALIVE" (`t3-heading.png`). Prose edits correctly do not
   appear on the audience screen — they are notes (§4.2).
3. **Triple-return — works.** Three Returns at the end of the document wrote
   `\n\n---\n\n` and left the caret on the new slide; confirmed on disk after
   Ctrl+S (`t4-triple-return.png`).
4. **Ctrl+? — works**, and is the full §13 reference: Editor, Start presenting,
   Present (`t6-crop.png`). My first attempt looked inert only because I
   injected Ctrl+Shift+slash rather than the `question` keysym.
5. **uri-list decode** stays covered by `decodesWaylandUriListDrops`, including
   `file:///home/jethro/My%20Pictures/q3%20budget.png` → a path with spaces.
6. **F5 present mode — works.** The audience window fills the single output
   (3840x2160) showing slide 1 (`t7-present.png`); `N` toggles the notes
   overlay with an "N to close" hint (`t8-notes.png`); `Esc` closes the
   presentation and leaves the editor running. Only the audience window opens
   on one output, which is what §5.1 asks for. No crash, no core dump from any
   of my runs.

**Defects found**
- **Session state is keyed by the path as typed.** `omapresent welcome/welcome.md`
  stores the key `welcome/welcome.md` in `~/.local/state/omapresent/sessions.json`
  instead of the absolute path, so the same deck opened from another directory
  restores the wrong position. Mine; `src/backend.cpp` is T15-owned so I will
  fix it there.
- **After an abnormal exit, `omapresent <file>` silently ignores the file.**
  Draft recovery restores the previous document and marks it modified, and
  `src/main.cpp` only opens the command-line file when `!backend.modified()`.
  I reproduced this twice: launching on `check.md` gave a window titled
  `* welcome.md`. See NEEDS.

**Also observed (not mine):** six `omapresent` SIGABRT core dumps from 06:33-06:45
under COMM `present`, all predating my session; a notification that was already
on screen stayed on top of the audience window after present mode started.

**Housekeeping:** I closed a leftover Chrome window titled "How Omapresent Works"
while clearing stale windows — it matched a title filter and turned out to be
the web-bundle agent's preview of a published bundle, not an app window. Nothing
was lost but it is reopenable from the bundle. I also briefly moved the user's
ChatGPT window to workspace 9 with a mistargeted dispatch and moved it back.

**Tests:** No code changed in this entry. `welcome/welcome.md` is byte-identical
(md5 4cd40b3d9fdc2f590ff9f3e22c62c3c1); all typing went to scratch copies.
**Next:** T15.
**NEEDS:** `src/main.cpp` (not T15-owned): the command-line file should win over
a recovered draft — either open the file and offer the recovery separately, or
drop the `!backend.modified()` guard when a file was named on the command line.
Today a crash makes `omapresent <file>` open the wrong document.

### 2026-08-28 12:10 PT — release (Claude Opus 5)
**Task:** T17 GitHub release and Omarchy distribution
**Did:** Started. Read AGENTS.md, the T17 brief, spec §12 and §15 milestone 9, and the current release files. Inspected the installed Omarchy tooling: `omarchy-dev-pkg-test` names `${OMARCHY_PKGBUILDS_DIR:-~/Work/omarchy/omarchy-pkgs/pkgbuilds}/<package>/`, and `/etc/pacman.conf` points `[omarchy]` at `https://pkgs.omarchy.org/stable/$arch`. Located the authoritative repo `github.com/omacom/omarchy-pkgs` and read its README, `bin/add-package`, the `omawrite` package (our upstream base) and `omasnap` as the working models for a local, tag-sourced Qt app.
**Tests:** Not run yet.
**Next:** Correct the README, convert the PKGBUILD to a tag source, add CI Chromium coverage, run makepkg and namcap, and prepare the Omarchy submission.
**NEEDS:** nothing

### 2026-08-28 12:25 PT — app-shell (Claude Opus 5)
**Task:** T15 finish application wiring and user controls
**Did:** Read the brief and surveyed the six requirements against the tree.
`Settings` currently has no consumer anywhere in the app — it is constructed by
nobody — so requirement 1 is the largest piece. `Publisher` already exposes
everything §9 needs (providers, setProviderKey, publish, republish,
requestVersions, revert, requestSignInCode, verifySignInCode), so the publish
work is a controls-and-preferences surface rather than new transport.
**Tests:** Baseline `./bin/build` green before I started.
**Next:** Settings consumers first, then Prepare for offline, publish controls,
welcome-deck actions, window title, recipes.
**NEEDS:** nothing yet

### 2026-08-28 12:12 PT — icon (Codex GPT-5)
**Task:** T18 production application icon
**Did:** Started icon work. Inspected the existing projector-and-pineapple SVG and the dark charcoal/warm-gold presentation UI. Generated candidate marks with the built-in ChatGPT image-generation tool.
**Tests:** Final candidate selected for 1024x1024 normalization. Small-size inspection pending.
**Next:** Verify alpha, dimensions, and 16px/32px readability. Record the final prompt and commit the PNG.
**NEEDS:** nothing

### 2026-08-28 12:14 PT — icon (Codex GPT-5)
**Task:** T18 production application icon
**Did:** Added `pkgbuild/omapresent.png`, a generated charcoal-and-warm-gold stage-light mark with no text and true transparent alpha. Recorded the final generation prompt in `artwork/omapresent-icon-prompt.md`.
**Tests:** PNG is 1024x1024 and TrueColorAlpha. Alpha range is 0 to 65535. Rendered checks at 32px and 16px show a clear gold beam and dark emitter silhouette on a light background. `./bin/build && ./bin/test` was attempted; the link step is blocked by concurrent T15 `Backend` declarations without definitions.
**Next:** Release agent connects the PNG to packaging.
**NEEDS:** T15 must finish the currently declared `Backend` methods so the shared tree links again.

### 2026-08-28 12:32 PT — app-shell (Codex GPT-5)
**Task:** T15 finish application wiring and user controls
**Did:** Connected the settings slice to the editor UI, added the explicit offline-preparation confirmation, publish preferences and controls, welcome-deck actions, and frontmatter window titles. Fixed provider selection so it writes the top-level `publish.toml` default while preserving comments. Corrected the offline recipe. Removed the new welcome-dialog QML binding loop.
**Tests:** `qmllint src/Main.qml` passes. OmapresentTest passes 32/32 under offscreen Qt, including the provider-default regression. `./bin/build` passes. `./bin/test` is blocked in this sandbox by GTK display startup, local loopback bind denial, and QtWebEngine sandbox denial.
**Next:** T15 is blocked only on the custom-domain DNS-record preference flow.
**NEEDS:** Publisher owner must add a public, user-started domain setup API and a result signal that returns the DNS records. The current public API exposes no domain setup or records. Its private `configureHereDomain` runs only after a publish, so T15 cannot meet spec §9's DNS-record requirement without editing non-T15 files.

### 2026-08-28 11:38 PT — acceptance (Claude Opus 5)
**Task:** T19 live Omarchy acceptance and checklist closeout
**Did:** Read `AGENTS.md` and `tasks/T19-live-acceptance.md`. Tore down the
nested Hyprland compositor and app instances left from the earlier present-mode
verification; user session is back to one compositor and no stray omapresent
processes.
**Tests:** none run yet.
**Next:** Reconcile `docs/acceptance.md`, then idle inhibit, monitors, theme.
**NEEDS:** nothing

### 2026-08-28 12:31 PT — acceptance (Codex GPT-5)
**Task:** T19 live Omarchy acceptance and checklist closeout
**Did:** Continued the interrupted acceptance run. Reconciled stale checklist
rows against the recorded green gate, recorded live deck evidence, and safe
read-only checks. Confirmed the user's Stay Awake baseline reports
`{"enabled":true,...}` and did not toggle it. Inspected the installed
`omarchy-theme-set` and `omarchy-hook` behavior and Omapresent's theme watchers.
**Tests:** No build or test rerun by instruction. Safe checks: current theme is
`Gold Rush`; the Stay Awake indicator file exists; `hyprctl monitors -j` is
unavailable in this restricted shell. Recorded live evidence shows one output,
`HDMI-A-1` at 3840x2160, so projector hotplug remains hardware-limited.
**Next:** Commit `docs/acceptance.md` and this acceptance log entry.
**NEEDS:** Theme live reload still lacks the explicitly specified installed
Omapresent `theme-set` hook. Idle inhibit and two-output hotplug need a safe
live run when those checks can be made without changing the user's baseline.

### 2026-08-28 12:39 PT — publish (Codex GPT-5)
**Task:** T20 custom-domain publish flow
**Did:** Read the task, frozen contracts, current publish preferences, and the existing here.now domain transport. Confirmed that domain setup must be a separate explicit action.
**Tests:** Not run yet.
**Next:** Add the provider and backend result contract, then wire and test the publish-preferences controls.
**NEEDS:** nothing

### 2026-08-28 12:52 PT — publish (Codex GPT-5)
**Task:** T20 custom-domain publish flow
**Did:** Added an explicit selected-provider domain setup API and stable DNS result signals. Reused the here.now domain request for publish and setup. The publish dialog now saves the domain, shows provider status and DNS instructions, copies the records, and shows provider errors. Existing publish preferences remain intact.
**Tests:** Added loopback-only provider, backend, and QML contract cases. They cover no request before the user action, bearer auth, the official `dns_instructions` response, an existing-domain status lookup, preference preservation, copy, and errors. `qmllint src/Main.qml`, `./bin/build`, and `./bin/test` pass: 500 C++ and 43 renderer cases. Scoped Autoreview is clean after its DNS-shape finding was fixed.
**Next:** none
**NEEDS:** nothing

### 2026-08-28 14:28 PDT — renderer-final (Codex GPT-5)
**Task:** T23 editor open failure
**Did:** Unified explicit edit and present file loading behind the command-line failure policy. Failed opens now print the backend status and return non-zero; explicit-file precedence and file-free recovery remain unchanged.
**Tests:** Focused `explicitFileOpenResultControlsFailure` passed. Full C++ run passed all five `CommandLineRecoveryTest` cases. `./bin/build` passed. `./bin/test` reached the registered suites but remains blocked by existing loopback `server.listen()` restrictions, QtWebEngine sandbox shutdown crashes in export child processes, and the renderer harness environment.
**Next:** none
**NEEDS:** none

### 2026-08-28 14:00 PDT — renderer-final (Codex GPT-5)
**Task:** T23 editor open failure
**Did:** Started T23 from the final audit finding. Added a pure explicit-file open-result policy and routed both edit and present launches through one non-zero failure path while preserving explicit-file precedence and file-free recovery.
**Tests:** Added success, failure, and recovery cases to `CommandLineRecoveryTest`; no QtWebEngine launch is used.
**Next:** Run the focused suite and full build/test gates.
**NEEDS:** nothing
### 2026-08-28 12:55 PT — recovery (Codex GPT-5)
**Task:** T22 command-line file recovery precedence
**Did:** Started T22. Reviewed `AGENTS.md`, `tasks/T22-command-line-recovery.md`, `omapresent-spec.md`, and current worklog context. Patched `src/main.cpp` so command-line file open for edit/present no longer depends on `backend.modified()`, ensuring explicit CLI files take priority. Added a test-only launch hook behind `OMAPRESENT_TEST_DUMP_OPEN_FILE`.
**Next:** Add focused regression suite and register it in `tests/tests.pro`.
**NEEDS:** nothing

### 2026-08-28 13:05 PT — recovery (Codex GPT-5)
**Task:** T22 command-line file recovery precedence
**Did:** Added `tests/tst_commandline_recovery.cpp` with focused CLI regression for stale recovery snapshots, added to `tests/tests.pro`, and confirmed build updates in `src/main.cpp` are present.
**Tests:** `./bin/build` passed. Focused test cannot complete in this container because `tst_omapresent` requires a GUI display (`Gtk-WARNING cannot open display: :0`). `./bin/test` fails in this container with the same display warning and a renderer test failure (`tests/renderer`).
**Next:** Commit owned files if and when the shared environment can run headless tests.
**NEEDS:** nothing

### 2026-08-28 12:58 PT — codex (GPT-5.3 Codex Spark)
**Task:** T16 renderer completion
**Did:** Deferred every remote image behind an explicit themed load control in all renderer modes. Fixed published subtitle layout, toggle use, remote-note image persistence, and chrome keyboard focus. Added fenced-code-safe published title fallback. Added a narrow, documented Omarchy wallpaper package exception that rejects outside final and parent symlinks.
**Tests:** Real Chromium proved zero image requests before activation, one request after activation, visible and toggleable subtitles, successful subtitle-image persistence, stable title fallback, and keyboard navigation after chrome focus. `./bin/build` and `./bin/test` pass. All C++ suites pass with no skips. Renderer 43/43 passes. Final scoped Autoreview is clean.
**Next:** none
**NEEDS:** nothing
### 2026-08-28 13:01 PDT — recovery (Codex GPT-5)
**Task:** T22 command-line recovery precedence
**Did:** Started by adding test environment and regression coverage for explicit command-line file precedence.
**Next:** Add headless QtWebEngine child-process execution mode, separate edit/present explicit-file checks, add recovery-only file-free launch check, run full build/test gates.
**NEEDS:** nothing
### 2026-08-28 13:02 PDT — recovery (Codex GPT-5)
**Task:** T22 command-line recovery precedence
**Did:** Added focused child-process regression helpers and assertions for explicit edit launch, explicit present launch, and file-free recovery fallback.
**Tests:** `./bin/build` and `./bin/test` ran. `CommandLineRecoveryTest` exits with code 5 in all three new cases due `QtWebEngine` sandbox_host shutdown crash: `FATAL ... sandbox_host_linux.cc: Operation not permitted (1)`. Additional blockers remain: `customDomain...` tests report `server.listen()` failures and `ExportTest` PDF export cases crash on `sandbox_host_linux.cc`.
**Next:** Hold commit request for `src/main.cpp`, `tests/tests.pro`, `tests/tst_commandline_recovery.cpp` once `QtWebEngine` sandbox runtime is resolvable.
**NEEDS:** environment fix for QtWebEngine sandbox in this container before child-process regression can prove expected file open precedence behavior.

### 2026-08-28 13:02 PT — publish (Codex GPT-5.6 Sol)
**Task:** T21 presentation environment controls and Omarchy theme changes
**Did:** Read the working agreement, T21 brief, relevant spec text, current worklog, and the installed Omarchy `theme-set.d` hook convention.
**Tests:** Not run yet.
**Next:** Add the side-effect-free settings seam, package the theme hook, and test all settings and hook cases.
**NEEDS:** nothing

### 2026-08-28 13:17 PT — publish (Codex GPT-5.6 Sol)
**Task:** T21 presentation environment controls and Omarchy theme changes
**Did:** Wired both presentation settings to their desktop holds through a pure idempotent control seam. Added a packaged `theme-set.d` hook that signals only a running Omapresent process. The signal reloads the live palette and background without moving presentation state. Both PKGBUILDs install the same executable hook source.
**Tests:** All four settings pairs, repeated starts, stop and destructor cleanup, live theme signal reload, temporary-HOME hook install, mode, theme argument, and no-process behavior pass. `./bin/build && ./bin/test` passes: 500 C++ and 43 renderer tests. Final scoped Autoreview is clean after its background-path finding was fixed.
**Next:** none
**NEEDS:** nothing

### 2026-08-28 13:27 PDT — acceptance (Codex GPT-5)
**Task:** T19 final acceptance closeout
**Did:** Reconciled T16, T21, and T22 evidence. Recorded T22 pure recovery coverage and T21 settings/theme-hook coverage. Kept one-output hotplug and live idle/DND checks partial without changing the desktop.
**Tests:** `qmllint src/Main.qml` passed. `QT_QPA_PLATFORMTHEME= QT_STYLE_OVERRIDE=Fusion ./bin/build && QT_QPA_PLATFORMTHEME= QT_STYLE_OVERRIDE=Fusion ./bin/test` passed with all C++ suites green and renderer 43/43.
**Next:** none
**NEEDS:** Two-output projector hotplug, live idle/DND, renderer scroll observation, broader no-network behavior, first-run mutation, and live end-to-end CLI/deck checks remain partial.

### 2026-08-28 13:28 PDT — release (Codex GPT-5)
**Task:** T17/T21 final local package validation
**Did:** Rebuilt the Arch package from the final tree with the approved local-source override. Verified the binary, desktop entry, nine generated icon sizes, welcome deck, licenses, skill references, and executable Omarchy theme-set hook at their package paths.
**Tests:** `makepkg`, `pacman -Qip`, `makepkg --printsrcinfo`, `desktop-file-validate`, hook `bash -n`, icon dimension checks, and dependency metadata checks passed. `./bin/build` passed. `./bin/test` failed because GTK could not open `:0` and the renderer suite failed. `namcap` is unavailable (`command not found`).
**Next:** Await orchestrator approval before external release actions.
**NEEDS:** A namcap-capable environment and GUI/renderer-capable test environment for the remaining checks.

### 2026-08-28 13:32 PDT — acceptance (Codex GPT-5)
**Task:** T19 final acceptance closeout on `cd3d74e`
**Did:** Reconciled T16, T21, T22, and T23 evidence. Recorded the T23 explicit editor-file open-failure regression. Kept two-output hotplug and other live or hardware-only checks partial without changing the desktop.
**Tests:** `qmllint src/Main.qml` passed. `QT_QPA_PLATFORMTHEME= QT_STYLE_OVERRIDE=Fusion ./bin/build && QT_QPA_PLATFORMTHEME= QT_STYLE_OVERRIDE=Fusion ./bin/test` passed: 517 C++ tests and renderer 43/43.
**Next:** none
**NEEDS:** Two-output projector hotplug, live idle/DND, renderer scroll observation, broader no-network behavior, first-run mutation, and live end-to-end CLI/deck checks remain partial.

### 2026-08-28 13:38 PDT — release (Codex GPT-5)
**Task:** T17/T21 final package validation after T23
**Did:** Rebuilt the package from HEAD `9512e5c` with the approved local-source override. The archive changed from SHA-256 `645d1616bb1ad67acd7661d32cc4a1522ce21faab26f75019c633392689ddb7e` to `608acb219a5c740dd24785663d06e1ebb8844f04bf5492383423236a558ee50c` at 13:37:27 PDT. Required paths and the executable Omarchy theme-set hook passed.
**Tests:** Namcap PKGBUILD exit `0` with no findings. Namcap package exit `0` with warnings for implicit `glibc`, `bash`, `libgcc`, and `libstdc++`, and possibly unnecessary `qt6-multimedia`, `xdg-desktop-portal`, and `ttf-ia-writer`. No source or external release action occurred.
**Next:** Await orchestrator approval before publication.
**NEEDS:** nothing

### 2026-08-28 13:39 PDT — orchestrator (Codex GPT-5)
**Task:** Final product and release orchestration
**Did:** Closed T15–T23 through Herdr workers. Added the generated application icon, complete editor and presenter controls, explicit domain setup, deferred remote images, published subtitles and title fallback, safe Omarchy wallpaper handling, presentation settings, the packaged theme hook, and command-line recovery and open-failure policies. Prepared the GitHub release workflow and Omarchy package submission tree.
**Tests:** Final approved gate passed at release-candidate HEAD: 517 C++ tests and renderer 43/43. Final Arch package rebuild, required-path checks, desktop validation, hook checks, and Namcap passed. Package SHA-256 is `608acb219a5c740dd24785663d06e1ebb8844f04bf5492383423236a558ee50c`.
**Next:** Create the public GitHub repository, push the final tree and tag, fill the immutable tag checksum, publish v0.1.0, and submit the Omarchy package pull request.
**NEEDS:** GitHub browser confirmation for the prepared public repository creation. CLI and SSH credentials are not valid on this host.

### 2026-08-28 13:42 PDT — release (Codex GPT-5)
**Task:** T17 GitHub release publication
**Did:** Started the authorized public repository, tag, checksum, package, and GitHub release publication sequence from clean HEAD `7c18b9b`.
**Tests:** Repository rules, T17 scope, release workflow, release notes, and checksum procedure inspected. No external mutation performed yet.
**Next:** Verify GitHub auth and repository state, then publish the approved release sequence.
**NEEDS:** nothing

### 2026-08-28 13:47 PT — publish (Codex GPT-5.6 Sol)
**Task:** Submit the Omapresent package to the Omarchy package repository
**Did:** Read the working agreement and prepared PR brief. Confirmed valid GitHub CLI auth and the live upstream repository at `omacom/omarchy-pkgs`, with `master` and `pkgbuilds/<package>/` layout.
**Tests:** Release and checksum readiness not yet confirmed.
**Next:** Inspect upstream contribution rules and package examples, then monitor v0.1.0 and both local checksums every 30 seconds.
**NEEDS:** nothing

### 2026-08-28 14:03 PT — publish (Codex GPT-5.6 Sol)
**Task:** Submit the Omapresent package to the Omarchy package repository
**Did:** Waited for public v0.1.0 and matching final checksums. Forked `omacom/omarchy-pkgs` to `jethrojones/omarchy-pkgs`, pushed branch `add-omapresent-package` at `86d5dd3`, and opened PR 235 with only `pkgbuilds/omapresent/`.
**Tests:** Tag source checksum `b8cc096a21612ef65a2ffd75dd70d3b80f002e0d9466007b379fd49ff664c648` passed `makepkg --verifysource`. Metadata, source URL, desktop file, 1024px icon, executable theme hook, nine packaged icon sizes, release artifact paths, three upstream self-test suites, and scoped Autoreview passed. PR URL returns HTTP 200 and is open and mergeable. Upstream Tests run `33210750360` is `action_required` with no jobs or check runs.
**Next:** An upstream maintainer must approve the first-time fork workflow, then review and merge https://github.com/omacom/omarchy-pkgs/pull/235.
**NEEDS:** Upstream workflow approval is required before automated checks can run.

### 2026-08-28 14:10 PT — release (Codex GPT-5)
**Task:** T17 GitHub release publication
**Did:** Published `jethrojones/omapresent` and immutable tag `v0.1.0` at `10df02f`. Recorded source checksum `b8cc096a21612ef65a2ffd75dd70d3b80f002e0d9466007b379fd49ff664c648` in both package PKGBUILDs. Committed and pushed the narrow Poppler CI dependency fix as `1a40c8c`; `release.yml` remains unchanged.
**Tests:** Tag-push Release run `33209972382` passed. The default tag-source package built at `pkgbuild/omapresent-0.1.0-1-x86_64.pkg.tar.zst`; SHA-256 is `fa15275af78de93bdcdb407858c839d5cd483c98f072db1472a80640dac618f8`, matching the published asset. Package path, binary, desktop file, icons, welcome deck, licenses, skill, and executable Omarchy theme hook checks passed. Temporary Namcap checks exited `0`; package warnings were implicit `glibc`, `bash`, `libgcc`, `libstdc++`, possibly unnecessary `qt6-multimedia`, `xdg-desktop-portal`, and `ttf-ia-writer`. New master CI run `33211212092` passed dependency installation and build but failed the hook test because stock Arch lacks `/usr/bin/omarchy`. Historical release-event run `33210256561` failed before the Poppler fix because the pre-fix workflow could not find `ttf-ia-writer`.
**Next:** No further release mutation. Await an Omarchy runtime fixture for the CI hook test.
**NEEDS:** CI must provide `/usr/bin/omarchy` for `packagedThemeHookUsesOmarchyConvention`.

### 2026-08-28 14:25 PDT — release-closeout (Codex GPT-5)
**Task:** Final v0.1.0 release closeout
**Did:** Verified public repo https://github.com/jethrojones/omapresent and immutable `v0.1.0` at `10df02f`. Release: https://github.com/jethrojones/omapresent/releases/tag/v0.1.0. The published package SHA-256 is `fa15275af78de93bdcdb407858c839d5cd483c98f072db1472a80640dac618f8`.
**Tests:** CI run https://github.com/jethrojones/omapresent/actions/runs/33211971954 succeeded. Omarchy PR https://github.com/omacom/omarchy-pkgs/pull/235 is open and mergeable.
**Next:** Upstream maintainer review and merge.
**NEEDS:** The upstream `Tests` workflow remains `action_required`; an Omarchy maintainer must approve the first-time fork workflow.

### 2026-08-28 14:34 PT — app-final (Codex GPT-5)
**Task:** T24 audience top-level window identity
**Did:** Started after reading the project rules, spec, relevant tasks, current worklog, and window code. Read-only Hyprland inspection found a transient `aquamarine - WAYLAND-1` client, then source search found no Aquamarine or nested-compositor launch path in Omapresent. The audience is currently a QML `Window`; its top-level identity relies on Qt defaults.
**Tests:** Not run yet.
**Next:** Make the native top-level state explicit, add lifecycle and identity regressions, and inspect live Omapresent clients.
**NEEDS:** nothing

### 2026-08-28 14:43 PT — app-final (Codex GPT-5)
**Task:** T24 audience top-level window identity
**Did:** Made the audience QML window explicitly normal, non-modal, and without a transient parent. It stays hidden until `placeWindow()` selects its output. Its title is `<deck title> — Omapresent`, with `Omapresent` as the empty-title fallback. `Presentation::createWindow()` clears native parent/transient/modality state before showing either presentation window. Added a native window-factory test seam so start/stop lifecycle coverage does not load Chromium under the offscreen test platform.
**Tests:** `qmllint src/AudienceWindow.qml` passes. `QT_QPA_PLATFORMTHEME= QT_STYLE_OVERRIDE=Fusion ./bin/build && QT_QPA_PLATFORMTHEME= QT_STYLE_OVERRIDE=Fusion ./bin/test` passes: 520 C++ tests and renderer 43/43. PresentationTest 63/63 covers title contract, `type() == Qt::Window`, null parent/transient parent, `Qt::NonModal`, lifecycle cleanup, resize wiring, and separate presenter/editor declarations.
**Live:** `omapresent present welcome/welcome.md` created two native Wayland clients from PID 3011450: editor `0x55cf9aaa3360`, class/initialClass `omapresent`, title `How Omapresent Works - Omapresent`; audience `0x55cf9abad140`, class/initialClass `omapresent`, title `How Omapresent Works — Omapresent`, fullscreen, `xwayland: false`. The separately existing outer compositor was `0x55cf9a388ab0`, PID 1422813, class/initialClass `aquamarine`, title `aquamarine - WAYLAND-1`, also `xwayland: false`. The test process was stopped.
**Next:** Done.
**NEEDS:** The portal picker label is outside Omapresent. Omapresent cannot control the outer Aquamarine source. The portal must be used in the physical Hyprland session and the direct Omapresent audience window must be selected.

### 2026-08-28 14:45 PDT — screen-share-docs (Codex GPT-5)
**Task:** T25 screen-share directions
**Did:** Read the project rules, spec, current help docs, skill docs, and T24 live evidence. Created `tasks/t25-screen-share-docs.md` with ownership limited to the README, welcome deck, and installed skill.
**Tests:** Not run yet.
**Next:** Add concise Omarchy/Hyprland window-selection guidance, verify docs, run the required sync and build/test checks, then make a scoped commit.
**NEEDS:** Do not stage or commit app-final’s T24 changes.

### 2026-08-28 14:48 PDT — screen-share-docs (Codex GPT-5)
**Task:** T25 screen-share directions
**Did:** Added concise screen-share guidance to `README.md`, `welcome/welcome.md`, and `skill/SKILL.md`. The docs identify the separate native editor and audience windows, the footer `Present` start action, the audience title `<deck title> — Omapresent` or `Omapresent`, and `aquamarine - WAYLAND-1` as the outer compositor/output. They state that Omapresent does not control the portal label. No source, tests, package files, or workflows changed.
**Tests:** Local Markdown links resolve. `git diff --check` passes. `./bin/check-skill-sync` passes. `./bin/build` passes. `./bin/test` exits 1 in the shared headless Chromium environment; the renderer suite reports a generic failure. `namcap` was not run because it is not installed.
**Next:** Commit the T25 task file and three owned help files.
**NEEDS:** none

### 2026-08-28 20:41 PT — release (Claude Opus 5)
**Task:** T26 v0.1.1 release publication
**Did:** Opened the patch release for the two post-0.1.0 screen-share fixes,
`1185b6a` (audience as an independent Wayland top-level) and `48511da`
(README, welcome deck and skill guidance for picking the audience window).
Wrote `tasks/T26-v0.1.1-release.md` and `.github/release-notes/v0.1.1.md`,
and bumped `pkgver` to 0.1.1 in `pkgbuild/PKGBUILD` and the staged
`pkgbuild/omarchy-pkgs/omapresent/PKGBUILD`, resetting both checksums to the
zero placeholder. That ordering is required, not cosmetic: a tag archive
contains the PKGBUILD that would hash it, so the real checksum can only land
after the tag exists. v0.1.0 shipped the same way (`10df02f` tagged, then
`77cc675` recorded the checksum). v0.1.0 has not been touched.
**Tests:** Clean master at `48511da`. `./bin/build` passes. `./bin/test`
exits 0: 14 C++ suites all green (520 assertions, 0 failed, 0 skipped) and
renderer 43/43 with 0 skipped.
**Next:** Tag v0.1.1 on the bump commit, push, run
`pkgbuild/release-checksums v0.1.1`, push the checksum, then publish the
non-draft release and verify the attached asset.
**NEEDS:** nothing

### 2026-08-28 20:48 PT — release (Claude Opus 5)
**Task:** T26 v0.1.1 release publication
**Did:** Published v0.1.1. Tagged `cec9a76` (the pkgver bump on top of
`48511da`) as annotated `v0.1.1` — the tag tree must already say
`pkgver=0.1.1` for the tag-push check to pass, so the release commit is the
bump, not `48511da` itself; both `1185b6a` and `48511da` are ancestors.
Pushed master and the tag, ran `pkgbuild/release-checksums v0.1.1`
(`4999ed04533dce958e19f258e4a3cfcf7ba7f860668bcb3d5f83a103a15ea54c`,
independently re-hashed from the tag archive), pushed that as `50dbf24`, then
published the non-draft release from `.github/release-notes/v0.1.1.md` and
attached the package. `v0.1.0` was not touched: its tag object is still
`fbc50f5` on commit `10df02f`.

Release: https://github.com/jethrojones/omapresent/releases/tag/v0.1.1
Asset: `omapresent-0.1.1-1-x86_64.pkg.tar.zst`, 1132726 bytes,
sha256 `a38c601b75655a8c708a1835ff873ba775acd6ba771c63a121b1acc37911b5c7`,
confirmed by downloading the published asset back and re-hashing it.

Also fixed a latent defect in `.github/workflows/release.yml`. Its attach step
has never succeeded: it let `gh` infer the repository from git, and git's
dubious-ownership guard aborts on the container checkout. v0.1.0 died earlier
still, installing `ttf-ia-writer`; v0.1.1 built cleanly and passed `namcap`
and then failed only on that upload, so both assets were attached by hand. The
step now passes `--repo` and no longer uses `--clobber`.
**Tests:** `./bin/build` passes. `./bin/test` exits 0 on clean master at
`48511da`: 14 C++ suites green (0 failed, 0 skipped) and renderer 43/43, 0
skipped. `makepkg` from the immutable v0.1.1 tag archive validated the
sha256 and built the package; `namcap` on the PKGBUILD reports nothing, and on
the package reports the seven accepted warnings only (implicitly satisfied
glibc/libgcc/libstdc++/bash, plus qt6-multimedia, xdg-desktop-portal and
ttf-ia-writer, which namcap cannot see through an ELF link). CI's own release
run reached the same clean build and namcap.
**Next:** Omarchy PR #235 still carries v0.1.0 and was deliberately left alone.
**NEEDS:** PR #235 (`omacom/omarchy-pkgs`) needs its `pkgver` and
`sha256sums` moved to 0.1.1 before it merges, or Omarchy ships the release
without the audience-window fix. That needs its own go-ahead.

### 2026-08-28 20:53 PT — release (Claude Opus 5)
**Task:** Omarchy PR #235 moved to v0.1.1 (the go-ahead T26 said this needed)
**Did:** Updated the existing PR head branch only —
`jethrojones/omarchy-pkgs` `add-omapresent-package`, commit `63b57a7` on
top of `86d5dd3`. One file changed, two lines: `pkgver=0.1.1` and
`sha256sums=('4999ed04533dce958e19f258e4a3cfcf7ba7f860668bcb3d5f83a103a15ea54c')`.
No new PR, no merge, no force push. The source URL needed no edit — it is
`$url/archive/refs/tags/v$pkgver.tar.gz`, so the bump moves it to the v0.1.1
tag on its own. The package tree on the branch is now byte-identical to
`pkgbuild/omarchy-pkgs/omapresent/` at master `e0f66a6`: `omapresent.png`
matches by blob sha `ab6f87c`, and the desktop entry and `package.json`
match byte for byte.

No `.SRCINFO` was generated. `omacom/omarchy-pkgs` carries zero `.SRCINFO`
files across the whole tree — checked repo-wide on master, and against
`omawrite`, `omasnap`, `omacut`, `omacalc`, `omatrack` and `walker`
individually. Its README also records that `bin/sync-aur` deletes AUR-only
`.SRCINFO` before writing a package back. Adding one would be drift, not
regeneration.

The PR body was left alone: it names no version and no release link, so it
stays accurate for v0.1.1. Nothing in it said v0.1.0.
**Tests:** The repository's own required suites, run on the PR branch:
`bin/sync-upstream self-test`, `bin/omarchy-pkgs self-test` and
`bin/omarchy-release self-test` all pass — those three are exactly what its
`.github/workflows/test.yml` runs. `bin/list-packages` reads the package as
`0.1.1-1`. `makepkg` from the PR branch's own PKGBUILD validated the source
sha256 against the immutable tag archive and built
`omapresent-0.1.1-1-x86_64.pkg.tar.zst`; `namcap` reports nothing on the
PKGBUILD and only the seven accepted warnings on the package.
**Next:** Upstream approval. Nothing further is actionable from this side.
**NEEDS:** PR #235 cannot go green without an `omacom/omarchy-pkgs`
maintainer clicking "Approve and run workflows". Its `Tests` workflow sits at
`conclusion=action_required` with **zero jobs executed** on both heads
(`33210750360` for `86d5dd3`, `33232466964` for `63b57a7`) — GitHub's
fork/first-time-contributor gate, not a failure in the package. That is why the
PR reads `MERGEABLE` but `UNSTABLE` and why `gh pr checks` reports no
checks. The author cannot self-approve; it needs upstream write access.

### 2026-08-29 14:09 PT — app-final (Codex GPT-5)
**Task:** T27 fresh GitHub screenshot
**Did:** Started after reading the project rules, spec, README, welcome deck, T25 screen-share guidance, and current worklog. The task prompt had no separate recommendation block. I will capture only a real v0.1.1 Omapresent editor/audience pair on a temporary Hyprland workspace and restore the prior workspace afterward.
**Tests:** Not run yet.
**Next:** Inspect live workspace state, arrange the two native windows, capture and inspect the PNG, then run the required gate.
**NEEDS:** nothing

### 2026-08-29 14:30 PT — app-shell (Codex GPT-5)
**Task:** T28 Actions menu full-label width
**Did:** Started after inspecting the supplied screenshot and current QML. The Material menu keeps its 200 px implicit background because its list does not adopt the widest menu item width. Created the narrow ownership task and repo-supported product context. `DESIGN.md` remains optional and is not needed for this component fix.
**Tests:** Not run yet.
**Next:** Bind the popup width to its widest action item, cap it at the available screen width, add regression coverage, and verify.
**NEEDS:** nothing

### 2026-08-29 14:36 PT — app-shell (Codex GPT-5)
**Task:** T28 Actions menu full-label width
**Did:** The Actions menu now uses the widest current `MenuItem.implicitWidth`, with a 240 px minimum and the available screen width minus 32 px as its maximum. Added IDs only for this local width binding. The regression verifies that no current English label has less width than its unelided menu item.
**Tests:** `qmllint src/Main.qml` and `git diff --check` pass. The focused Omapresent target passes (3/3); the shared runner then reports the selected name as unknown in other suites. `./bin/build` passes. The full gate reaches the new test, but local server binding and Chromium sandbox failures keep `./bin/test` red.
**Next:** T28 implementation is complete pending scoped commit and push.
**NEEDS:** Autoreview could not start because its sandbox client needs a writable runtime path. Full-gate failures are outside T28: local HTTP listeners cannot bind, and Chromium exits on its sandbox shutdown.

### 2026-08-29 14:36 PT — app-shell (Codex GPT-5)
**Task:** T28 Actions menu full-label width
**Did:** Scoped staging was attempted for only `PRODUCT.md`, `tasks/t28-actions-menu-width.md`, `src/Main.qml`, and `tests/tst_omapresent.cpp`. Git could not create `.git/index.lock` because the repository filesystem is read-only. No path was staged, no commit was created, and no push was attempted.
**Tests:** No additional tests run after the final T28 entry.
**Next:** The orchestrator must stage the owned files and the two T28 worklog entries, inspect the staged diff, commit, and push `master`.
**NEEDS:** `.git/index.lock`: `Read-only file system`.

### 2026-08-29 14:58 PT — app-final (Codex GPT-5)
**Task:** T27 fresh GitHub screenshot
**Did:** Blocked before image creation. The first direct v0.1.1 launch on temporary workspace 10 created two independent physical Wayland clients: editor `0x55cf9a4a6890` and audience `0x55cf99f161a0`; both reported `class` and `initialClass` `omapresent`, `xwayland: false`, and the audience title `How Omapresent Works — Omapresent`. Its first app-only 1920×1080 capture was inspected, but rejected because the slide state was not suitable. After that instance ended, two fresh-data and one direct launch exited without a physical client or log output. No screenshot was saved. Workspace 8 is restored. No Hyprland or Omarchy configuration changed.
**Tests:** Not run. The required screenshot could not be created, and T28 now has uncommitted source changes in the shared tree.
**Next:** Re-run T27 when a direct physical Omapresent launch stays alive. Capture only the two verified client addresses, then run the build and test gate.
**NEEDS:** Live launch stability: subsequent direct `./build/omapresent present welcome/welcome.md` attempts exit before a Hyprland client appears. `welcome/screenshot.png`, staged T27 worklog hunks, commit, push, and raw GitHub verification remain intentionally absent.

### 2026-08-29 14:42 PT — app-final (Codex GPT-5)
**Task:** T27 fresh GitHub screenshot
**Did:** Used the approved vetted real-app fallback after fresh physical capture remained blocked. Copied it byte-for-byte to `welcome/screenshot.png`; it is a 1050×850 RGB PNG (149595 bytes) that shows the Omapresent editor, live preview, and Keyboard shortcuts dialog only. Final inspection found no private content and no EXIF, XMP, IPTC, comment, or profile metadata.
**Tests:** `git diff --check` passes before the desktop-safe full gate.
**Next:** Run the desktop-safe build and test gate, then stage only T27 content, commit, push, and verify the raw image URL.
**NEEDS:** nothing

### 2026-08-29 14:42 PT — app-final (Codex GPT-5)
**Task:** T27 fresh GitHub screenshot
**Did:** Completed with the approved vetted real-app fallback because fresh physical capture remained blocked. `welcome/screenshot.png` remains an exact byte-for-byte copy of the reviewed source. It contains only Omapresent surfaces and has no private content or embedded privacy metadata.
**Tests:** `git diff --check` passes. `QT_QPA_PLATFORMTHEME= QT_STYLE_OVERRIDE=Fusion ./bin/build && QT_QPA_PLATFORMTHEME= QT_STYLE_OVERRIDE=Fusion ./bin/test` passes: C++ suites and renderer suites (43/43) are green.
**Next:** Done.
**NEEDS:** nothing

### 2026-08-29 14:53 PDT — readme-features (Codex GPT-5)
**Task:** T29 README feature listing
**Did:** Started T29 after reading the project rules, specification, current README and welcome deck, and the recall and renderer implementation/tests. Confirmed the local `welcome/screenshot.png` exists and verified `Q`, `Space`, and `Esc` recall behavior.
**Tests:** Not run yet.
**Next:** Verify the README links and image, run the desktop-safe full gate, then wait for T28 to advance `master` beyond `2072016` before scoped staging and commit.
**NEEDS:** Do not stage or commit until T28 is committed and `master` is beyond `2072016`.

### 2026-08-29 14:53 PT — T28 reviewer (Codex GPT-5)
**Task:** T28 Actions menu full-label width
**Did:** Reviewed the T28 diff. Corrected the narrow-screen cap to use the current screen width instead of the full virtual desktop width. The menu follows its widest item at normal width, keeps the 240 px minimum when space permits, caps at the screen edge, and does not change item fonts. The regression now covers normal and narrow widths.
**Tests:** `qmllint src/Main.qml` and `git diff --check` pass. `QT_QPA_PLATFORMTHEME= QT_STYLE_OVERRIDE=Fusion ./bin/build && QT_QPA_PLATFORMTHEME= QT_STYLE_OVERRIDE=Fusion ./bin/test` passes. OmapresentTest passes 42/42, including the T28 regression. Renderer tests pass 43/43.
**Next:** Stage only T28 paths and worklog entries, inspect the index, commit, push `master`, and verify the exact CI run.
**NEEDS:** nothing

### 2026-08-29 15:00 PDT — readme-features (Codex GPT-5)
**Task:** T29 README feature listing
**Did:** Clarified the near-top README features for readable text size, vertical scrolling, and rapid recall. The README keeps `welcome/screenshot.png` and now uses accurate alt text. T28 is committed at `4ded111`, and `master` is beyond `2072016`.
**Tests:** README relative links resolve. `welcome/screenshot.png` exists as a 1050×850 RGB PNG. `git diff --check` passes. `QT_QPA_PLATFORMTHEME= QT_STYLE_OVERRIDE=Fusion ./bin/build && QT_QPA_PLATFORMTHEME= QT_STYLE_OVERRIDE=Fusion ./bin/test` passes: 520 C++ tests and 43 renderer tests, with no failures or skips.
**Next:** Stage only `README.md`, `tasks/t29-readme-features.md`, and the T29 worklog hunks; inspect the staged paths and diff, commit, push `origin/master`, and wait for exact master CI success.
**NEEDS:** nothing

### 2026-08-29 15:07 PDT — acceptance (Codex GPT-5)
**Task:** T30 final GitHub listing and Actions menu acceptance
**Did:** Started T30. Reviewed the specification, T27–T29 briefs, README, current Actions menu code, regression tests, screenshot, v0.1.1, and PR #235.
**Tests:** Exact CI for commit `34306d8` completed successfully as run `33277564262`. Local `qmllint` passed. The desktop-safe full gate is next.
**Next:** Create the T30 record, inspect the staged task and log hunks, commit and push `master`, then wait for the final T30 CI run.
**NEEDS:** nothing

### 2026-08-29 15:08 PDT — acceptance (Codex GPT-5)
**Task:** T30 final GitHub listing and Actions menu acceptance
**Did:** Verified the tracked screenshot, README feature claims, Actions menu sizing policy, regression coverage, v0.1.1 release, and PR #235 without changing external state.
**Tests:** Raw screenshot URL returns HTTP 200. The exact `34306d8` CI run `33277564262` passed. The desktop-safe gate passed: 521 C++ tests and 43 renderer tests.
**Next:** Commit and push only the T30 task and worklog entries, then wait for the final T30 CI run.
**NEEDS:** PR #235 is present at head `63b57a7`; GitHub reported mergeability `UNKNOWN` during this check.

### 2026-08-29 15:18 PDT — app-final (Codex GPT-5)
**Task:** T31 replace GitHub screenshot
**Did:** Started after reading the project rules, README, T27 record, and T30 worklog end. Inspected the user-approved source PNG: 1881×1003 RGB, 443186 bytes, with only the Omapresent editor and audience preview visible.
**Tests:** Not run yet.
**Next:** Copy the approved pixels, update only the README image alt text, inspect the final asset, then run the required gate.
**NEEDS:** nothing

### 2026-08-29 15:19 PDT — app-final (Codex GPT-5)
**Task:** T31 replace GitHub screenshot
**Did:** Replaced `welcome/screenshot.png` with the user-approved real-app source. The final PNG is byte-identical to the source: 1881×1003 RGB, 443186 bytes, SHA-256 `38baabb5ea0c5b9bfafbaedd27aa37d3b40af96d6b0af290972ff58785eaba32`. It has no EXIF, XMP, IPTC, comment, or profile metadata. Updated only the README image alt text to describe the Video and Interactive Embeds editor and live audience preview.
**Tests:** `git diff --check` passes. README relative image path resolves. `QT_QPA_PLATFORMTHEME= QT_STYLE_OVERRIDE=Fusion ./bin/build && QT_QPA_PLATFORMTHEME= QT_STYLE_OVERRIDE=Fusion ./bin/test` passes, including 43 renderer tests.
**Next:** Stage T31 paths and worklog entries only, commit, push, wait for exact CI, then verify the raw image URL.
**NEEDS:** nothing

### 2026-08-29 16:07 PDT — readme-uninstall (Codex GPT-5)
**Task:** T32 README uninstall directions
**Did:** Started T32 after reading the project rules, README, `bin/install`, release instructions, package files, and uninstall-related references. Verified `omarchy pkg drop omapresent`, `sudo pacman -Rns omapresent`, the package-owned system paths, and the user settings path.
**Tests:** Not run yet.
**Next:** Run README path checks, `git diff --check`, and the desktop-safe full gate. Then stage only the T32-owned files and worklog hunks, inspect, commit, push, and verify master CI.
**NEEDS:** nothing

### 2026-08-29 16:10 PDT — readme-uninstall (Codex GPT-5)
**Task:** T32 README uninstall directions
**Did:** Added the Installation `Uninstall` section with the verified Omarchy and source-install removal commands. It states that package removal preserves Markdown decks and `~/.config/omapresent/settings.toml`.
**Tests:** README paths and `git diff --check` pass. `sh -n bin/install` and `bash -n pkgbuild/PKGBUILD` pass. `QT_QPA_PLATFORMTHEME= QT_STYLE_OVERRIDE=Fusion ./bin/build && QT_QPA_PLATFORMTHEME= QT_STYLE_OVERRIDE=Fusion ./bin/test` passes: 520 C++ tests and 43 renderer tests, with no failures or skips.
**Next:** Stage only `README.md`, `tasks/t32-readme-uninstall.md`, and the T32 worklog hunks; inspect, commit, push, and verify master CI and the GitHub README.
**NEEDS:** nothing
### 2026-08-29 11:00 PT — renderer (Codex GPT-5)
**Task:** T35 heading fragment order
**Did:** Identified welcome slide 19 parsed as `# Presenter Mode & Multi-Monitor` with two headings and ordered reveal expectations: initial bullets hidden by list fragments, second heading appears only after final first-list bullet, then second-list bullets reveal in order.
**Tests:** not run yet.
**Next:** Implement parser and render sequencing fix plus regression in renderer tests.
**NEEDS:** nothing
### 2026-08-29 16:26 PT — renderer (Codex GPT-5)
**Task:** T35 heading fragment order
**Did:** Added renderer parser/render support so only non-initial headings are fragment targets. In `slideElement()` heading blocks after the first are now tagged as `op-fragment` in sequence with list-item fragments. Updated `countFragments()` to mirror that order. Added a focused `heading-fragments` fixture and DOM-order regression proving: the second heading stays hidden through first-list bullets, then appears, then later bullets reveal in order.
**Tests:** `node --test tests/renderer/*.test.mjs` failed in this sandbox on `interaction` and `visual` because Chromium exits before DOM dump (`Failed to create a unique user data directory for headless`). `QT_QPA_PLATFORMTHEME= QT_STYLE_OVERRIDE=Fusion ./bin/build && QT_QPA_PLATFORMTHEME= QT_STYLE_OVERRIDE=Fusion ./bin/test` completed with known C++ socket/sandbox failures and the same renderer test-file failure mode; no C++ failures introduced by this patch.
**Next:** Root will run desktop-capable Chromium; do not change this scope until that pass.
**NEEDS:** nothing

### 2026-08-29 16:21 PDT — app-final (Codex GPT-5)
**Task:** T35 heading fragment order review and closeout
**Did:** Reviewed and corrected the general document-order renderer loop. Later heading fragments now receive their indices while their content block is processed, after any prior list items. This fixes welcome slide 19 without slide-specific code: the second heading remains hidden through all first-section bullets, then appears before later list fragments.
**Tests:** `node --test tests/renderer/deckparse.test.mjs` passes. The standalone Chromium interaction file exits before its test body in this shell, but the required desktop-safe gate passes and runs the DOM-order regression: 45 renderer tests pass.
**Next:** Stage only T35 paths, commit, push, and wait for the exact CI run.
**NEEDS:** nothing

### 2026-08-29 17:00 PDT — t33-recovery (Codex GPT-5)
**Task:** T33 recovered image assets
**Did:** Started implementation after confirming recovery restored the local file URL without restoring the asset or media deck directory.
**Tests:** Not run yet.
**Next:** Restore both bases, add the spaced-relative-image recovery regression, run the requested focused and full gates.
**NEEDS:** nothing

### 2026-08-29 17:24 PDT — t33-recovery (Codex GPT-5)
**Task:** T33 recovered image assets
**Did:** Restored asset and media deck directories from recovered local Markdown URLs. Added recovery coverage for a relative image path containing spaces and verified preview and audience asset maps use the expected file URL.
**Tests:** Focused regression passes: `OmapresentTest::recoveredDeckRestoresRelativeImageAssets()`. `git diff --check` passes. `./bin/build` passes. The desktop-safe full gate is blocked by loopback bind failures and QtWebEngine sandbox crashes (`sandbox_host_linux.cc:41`, `Operation not permitted`); renderer interaction/visual tests also fail in this environment.
**Next:** Host-capable reviewer must rerun the full gate, then inspect and commit the scoped T33 files. Changes remain unstaged.
**NEEDS:** A host-capable QtWebEngine and loopback-enabled environment for the full gate.

### 2026-08-29 16:31 PDT — T33 reviewer (Codex GPT-5)
**Task:** T33 recovered image assets review and closeout
**Did:** Independently reviewed the recovery fix. Recovered local files now restore the image and media bases. Expanded the regression to prove spaced relative image and video paths, matching preview and audience file URLs, replacement of recovered identity by a later open, and unchanged untitled file-free recovery.
**Tests:** Focused T33 selection passes 4/4. `git diff --check` passes. `QT_QPA_PLATFORMTHEME= QT_STYLE_OVERRIDE=Fusion ./bin/build && QT_QPA_PLATFORMTHEME= QT_STYLE_OVERRIDE=Fusion ./bin/test` passes: OmapresentTest 44/44 and renderer tests 45/45. Final Autoreview is clean.
**Next:** Stage only T33 paths and worklog entries, inspect, commit, push `master`, and verify the exact CI run.
**NEEDS:** nothing

### 2026-08-29 18:05 PT — webbundle (Claude Opus 5)
**Task:** T34 YouTube playback and save-time prefetch
**Did:** Diagnosed and fixed "Error 153: Video player configuration error".

*The cause, measured rather than reasoned.* A hosted player refuses to configure
when the page embedding it has an opaque origin, because there is no `Referer`
to send. `qrc:` and `file:` are both opaque. The same iframe markup, same
browser, same session: from `file://` all of `?enablejsapi=1`, `?autoplay=1` and
`youtube-nocookie.com` give 153; from `http://127.0.0.1` all three play. So the
embed parameters, the referrer policy, the cookie domain and CSP are all ruled
out — it is the origin, and nothing else.

*Why the deck page did not have to move.* A `file://` page hosting a **nested**
frame served over loopback gets a working player. That matters: moving the whole
renderer to an http origin would have broken every `file:///` image and cached
video in the app, since an http page may not load them, and it would have forced
the server to serve user files. With a nested shim the server serves exactly one
compiled-in page and never touches the filesystem.

*What is new.* `src/embedserver.{h,cpp}`: `QTcpServer` on `QHostAddress::LocalHost`,
ephemeral port, one GET route behind a per-session 128-bit token, reading only
through `QFile(":/renderer/…")`. It does not listen until `baseUrl()` is called,
and the renderer only calls it after a click. `src/renderer/embed.html`: the
shim, driving the player through YouTube's own IFrame API so `onReady`,
`onStateChange` and `onError` are its real events; play/pause asked for before
the API arrives is queued, not dropped. `src/renderer/embed.js`: the decisions,
pure and unit-tested — which of shim/direct/fallback applies, the shim URL, the
codes that mean "will not play here", who a message is allowed to come from.
`RenderHost::embedBase()` is `Q_INVOKABLE` rather than a property, because a
WebChannel sends properties at handshake and that would start the server when a
deck opens.

*Also:* `presentation.auto_prefetch_video` now defaults true, with the gate left
where it was, inside the explicit save path. `welcome/welcome.md` demonstrates
`aqz-KE-bpKQ` (Big Buck Bunny, Blender, CC BY) — the previous demo video is not
embeddable anywhere, and showed "This video is unavailable" even from a working
http origin — and its two untrue claims are corrected.

**Tests:** `./bin/build && ./bin/test` pass, 0 failures. `tests/tst_embedserver.cpp`
16 cases: loopback binding, ephemeral port, nothing listening until asked, the
shim and nothing else, wrong token, wrong and duplicate and mixed-case Host,
every method but GET, traversal in five encodings, oversized → 431, and the pure
router. `tests/renderer/embed.test.mjs` 9 cases including the asynchronous
WebChannel callback, a silent host, non-YouTube media never asking for a base,
and message sender/origin validation. `tests/renderer/embed-network.test.mjs` is
opt-in (`OMAPRESENT_NETWORK_TESTS=1`) and does not claim to prove 153 — see
below. Renderer suite 45/45, three consecutive clean runs.

**Next:** waiting on an independent review before staging. Nothing committed.

**NEEDS:**
- `tests/renderer/interaction.test.mjs` "recall overlays…" is timing-fragile
  under parallel load, and it is not T34's doing: at **HEAD**, with two *empty*
  dummy test files added to `tests/renderer/` and none of this work present, it
  failed 3 runs in 6. With T34 present it failed 1 in 6, and 0 in the last 3.
  Any added test file can tip it. Its owner should give that one case a larger
  virtual-time budget; it measures scroll and fragment state across several
  nested timers inside a 1500 ms budget, which is the actual fragility.
- Recorded so nobody re-derives it: **Error 153 cannot be asserted
  programmatically.** It is rendered inside the player's cross-origin document,
  which `--dump-dom` cannot read. With a hand-written `postMessage` handshake
  the player reports nothing at all — measured from a folder and from loopback,
  for an embeddable video and a non-embeddable one, the only event that ever
  arrived was the shim's own "ready". Moving the shim to the official IFrame API
  is what makes `onError` real; the opt-in network test asserts the player
  reaches `onReady` over loopback and reports no fallback error, and says
  plainly that it does not prove 153. The evidence for 153 itself is the
  screenshot experiment recorded in `tasks/t34-youtube-playback.md`.

### 2026-08-29 17:27 PDT — T34 reviewer (Codex GPT-5)
**Task:** T34 YouTube playback closeout proof
**Did:** Correction: `tests/renderer/embed.test.mjs` now has 10 cases, not 9. Added a browser DOM regression for a deferred YouTube URL with a real local cached poster and no cached video. It proves the poster is applied before activation, with no video or remote player in the DOM, no `embedBase()` call, and no HTTP(S) resource.
**Tests:** `node tests/renderer/embed.test.mjs` passes 10/10. `QT_QPA_PLATFORMTHEME= QT_STYLE_OVERRIDE=Fusion ./bin/build && QT_QPA_PLATFORMTHEME= QT_STYLE_OVERRIDE=Fusion ./bin/test` passes; renderer suites 45/45. `git diff --check` passes.
**Next:** T34 proof is ready for its owner to stage with the existing T34 work.
**NEEDS:** nothing

### 2026-08-29 17:35 PDT — T34 reviewer (Codex GPT-5)
**Task:** T34 closeout correction
**Did:** Corrected the settings default comment to say video prefetch is on by default. The deferred-poster DOM test now uses the standard explicit Chromium skip and has no early-return pass path.
**Tests:** `node tests/renderer/embed.test.mjs` passes 10/10 with 0 skips. The SettingsTest shares one C++ runner and has no isolated suite command. `git diff --check` passes.
**NEEDS:** nothing

### 2026-08-29 17:45 PDT - t36-images (Codex GPT-5)
**Task:** T36 renderer local image transport
**Did:** Started after confirming QtWebEngine rejects `file://` image and background URLs from the `qrc:` renderer origin.
**Tests:** Not run yet.
**Next:** Add secure content-aware data URL conversion in RenderHost and focused parity tests.
**NEEDS:** nothing

### 2026-08-29 21:15 PDT - t36-images (Codex GPT-5)
**Task:** T36 renderer local image transport
**Did:** Added content-aware data URL transport for approved local image assets, backgrounds, and media posters in non-web deck composition. Preserved cached video files, remote/data URLs, and WebBundle file URLs; invalid local files become empty.
**Tests:** `./bin/build` passes. Focused T36 and T33 image regressions pass. The exact desktop-safe full gate returns 1 because loopback tests cannot bind and QtWebEngine export tests crash at `sandbox_host_linux.cc:41` with `Operation not permitted`; renderer browser tests also fail in this environment.
**Next:** Host-capable reviewer reruns the full gate. Changes remain uncommitted and unpushed.
**NEEDS:** Host-capable loopback and QtWebEngine sandbox environment.

### 2026-08-29 21:35 PDT - t36-images (Codex GPT-5)
**Task:** T36 renderer local image transport follow-up
**Did:** Fixed non-web background classification so existing `data:` values remain unchanged. Added direct data-background and data-poster regressions.
**Tests:** T36 regression passes. `./bin/build` passes. `git diff --check` passes. The focused shared runner also reports unrelated loopback bind failures.
**Next:** No further T36 work in this environment.
**NEEDS:** nothing

### 2026-08-29 21:50 PDT - t36-images (Codex GPT-5)
**Task:** T36 CI portability follow-up
**Did:** Replaced the host-specific Omarchy background path in `buildsTheDeckDocumentTheRendererExpects` with a deterministic temporary missing background and asserted the empty result. The real temporary extensionless PNG T36 coverage remains unchanged.
**Tests:** Not run yet.
**Next:** Run the desktop-safe full gate and `git diff --check`.
**NEEDS:** nothing

### 2026-08-29 21:55 PDT - t36-images (Codex GPT-5)
**Task:** T36 CI portability follow-up
**Did:** Completed the test portability fix without changing production code. The deck-composition test now uses a temporary missing background and expects an empty result.
**Tests:** `QT_QPA_PLATFORMTHEME= QT_STYLE_OVERRIDE=Fusion ./bin/build` passes. The exact full gate returns 1 from known loopback bind failures and QtWebEngine sandbox crashes. `git diff --check` passes.
**Next:** No further T36 work in this environment.
**NEEDS:** Host-capable loopback and QtWebEngine sandbox environment for a green full gate.

### 2026-08-30 00:10 PDT - t37-bento (Codex GPT-5)
**Task:** T37 seven-image bento layout
**Did:** Started after reproducing the sample deck's seven consecutive image block as an intentional `stacked` layout fallback.
**Tests:** Not run yet.
**Next:** Add the seven-image 4+3 grid, additive spec text, and parser/layout/browser regressions.
**NEEDS:** nothing

### 2026-08-30 00:30 PDT - t37-bento (Codex GPT-5)
**Task:** T37 seven-image bento layout
**Did:** Added the additive seven-image 4+3 two-row layout. Existing 2-6 arrangements remain unchanged and eight or more images still stack. Added layout and browser DOM coverage based on seven adjacent embeds.
**Tests:** `node --test tests/renderer/layout.test.mjs` passes 1/1. The seven-image browser test reaches Chromium but the sandbox exits with `SIGTRAP` before DOM output. `git diff --check` passes.
**Next:** Host-capable Chromium must run `node tests/renderer/suite.mjs` to prove the seven-image DOM geometry.
**NEEDS:** Host-capable Chromium sandbox for browser DOM verification.

### 2026-08-30 00:45 PDT - t37-bento (Codex GPT-5)
**Task:** T37 seven-image bento review follow-up
**Did:** Added deterministic seven-image `|main` geometry with a prominent hero tile and six surrounding tiles. Replaced the browser test's raw escaped JSON parsing with DOM-derived row counts.
**Tests:** `node --test tests/renderer/layout.test.mjs` passes. `node tests/renderer/suite.mjs` remains blocked by Chromium `SIGTRAP` sandbox exits and loopback `EPERM`. `git diff --check` passes.
**Next:** Host-capable Chromium must run the browser suite.
**NEEDS:** Host-capable Chromium sandbox for DOM proof.

### 2026-08-30 07:37 PDT - t38-video-race (Codex GPT-5)
**Task:** T38 presentation video bridge race
**Did:** Started after confirming the presentation bridge is already a valid QML property. The scope is delayed WebChannel availability during an explicit YouTube activation.
**Tests:** Not run yet.
**Next:** Add a bounded bridge wait and deterministic renderer tests.
**NEEDS:** nothing

### 2026-08-30 07:41 PDT - t38-video-race (Codex GPT-5)
**Task:** T38 presentation video bridge race
**Did:** Added a bounded qrc-only WebChannel bridge wait for explicitly activated YouTube loaders. A late host uses the existing tokenized loopback shim. A missing host uses the existing QR/open fallback. Slide and recall teardown cancel pending waits. Static files and web bundles remain immediate.
**Tests:** `node --test tests/renderer/embed.test.mjs` — 14/14 pass. `node tests/renderer/suite.mjs` — 48/48 pass. `git diff --check` passes.
**Next:** No commit or push requested.
**NEEDS:** nothing

### 2026-08-30 07:56 PDT - t37-bento (Codex GPT-5)
**Task:** T37 interaction recall timing follow-up
**Did:** Made the interaction test wait for the non-empty `data-recall-after-goto-slide` DOM state before applying the unchanged recall assertions. Product and fixture behavior are unchanged.
**Tests:** `node tests/renderer/suite.mjs` ran 3 times; all 3 were blocked by Chromium `SIGTRAP` with Crashpad `setsockopt: Operation not permitted`. The desktop-safe full gate built successfully. `./bin/test` reached 44 passed and 2 failed in `OmapresentTest` from loopback `server.listen()` failures, plus existing renderer Chromium `SIGTRAP`, PDF Chromium sandbox shutdown, EmbedServer loopback, and WebBundle loopback failures.
**Next:** Host-capable Chromium and loopback environment are required for a green full gate.
**NEEDS:** nothing

### 2026-08-30 07:57 PDT - t38-video-race (Codex GPT-5)
**Task:** T38 browser regression follow-up
**Did:** Added a Chromium DOM regression that clicks before a delayed bridge, proves one tokenized loopback activation after repeated clicks, and proves rerender cancellation prevents a stale replacement.
**Tests:** `node --test tests/renderer/embed.test.mjs` — 15/15 pass. `git diff --check` passes.
**Next:** No commit or push requested.
**NEEDS:** nothing

### 2026-08-30 08:05 PT — Codex (GPT-5)
**Task:** T37/T38 final verified checkpoint
**Did:** Confirmed T37 commit `d63bdb9` and T38 commit `201f5a9` are on `origin/master`. Fresh Sol review is APPROVED.
**Tests:** T38 focused suite — 15/15. Renderer suite — 48/48 three times. Full host gate — 541 C++ + 48 renderer = 589/589. `git diff --check` passed. GitHub Actions run 33318379764 for `201f5a9` succeeded; build-and-test succeeded with no failures.
**Next:** User live-check of the sample deck.
**NEEDS:** Physical audience-window confirmation only.
