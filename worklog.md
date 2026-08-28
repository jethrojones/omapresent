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
