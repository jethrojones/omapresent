# T16 — Close renderer and published-output gaps

**Agent:** `renderer` · **Spec:** §1.1, §3, §4.4, §4.5, §9

## Files you own

- `src/assetindex.h`, `src/assetindex.cpp`
- `src/renderer/`
- `src/webbundle.h`, `src/webbundle.cpp`
- `tests/tst_assetindex.cpp`
- `tests/tst_webbundle.cpp`
- `tests/renderer/`
- `docs/renderer-contract.md`

Do not edit application QML/backend, package, CI, README, or icon files.

## Required work

1. Stop remote images from making a network request when a deck opens. Use an
   explicit user action or the existing explicit prefetch contract. Add a real
   zero-request regression for remote images.
2. Make published deck speaker-note subtitles visible and usable. Add a browser
   test that checks visibility, not only markup.
3. Use the first heading as the published title fallback when frontmatter has no
   title.
4. Make the missing-image wallpaper behavior obey both the package security
   boundary and the theme-background requirement. Document the exact policy.
5. Keep preview, presentation, PDF, and published deck rendering aligned.

## Verification

Run the focused C++ and browser suites, then:

```sh
./bin/build && ./bin/test
```

Append start and finish entries to `worklog.md`. Commit only owned files and the
appended worklog lines.

## Done when

The no-network rule holds for remote images, subtitles are visibly reachable,
the title fallback is correct, all tests pass, and the renderer contract matches
the implementation.

Created by Codex GPT-5 on 2026-08-28 11:12 PT on ombee.
