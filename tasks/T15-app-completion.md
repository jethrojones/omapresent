# T15 — Finish application wiring and user controls

**Agent:** `app-shell` · **Spec:** §4.8, §4.10, §7, §9, §10, §14

## Files you own

- `src/Main.qml`
- `src/EditorPane.qml`
- `src/PreviewPane.qml`
- `src/backend.h`, `src/backend.cpp`
- `src/settings.h`, `src/settings.cpp`
- `src/videocache.h`, `src/videocache.cpp`
- `tests/tst_omapresent.cpp`
- `tests/tst_settings.cpp`
- `tests/tst_videocache.cpp`
- `skill/reference/recipes.md`

Do not edit renderer, web-bundle, package, CI, README, or icon files.

## Required work

1. Connect `settings.toml` to the running application. Every setting promised by
   the spec must have a real consumer or must be removed from the documented
   contract with a clear spec-compatible reason.
2. Add an explicit, user-started **Prepare for offline** action that calls the
   existing video prefetch path. Opening and saving a deck must not start
   network activity.
3. Add the complete publish preferences and controls required by §9. Reuse the
   existing provider APIs for provider selection, sign-in, domain setup,
   republish, version history, and revert. Keep confirmation before uploads.
4. Add **Help → How Omapresent works** and **Edit a copy** for the welcome deck.
5. Use the frontmatter title for the application window.
6. Correct the skill recipe so it describes the behavior that the application
   actually provides.

## Verification

Add focused tests for every new pure or backend behavior. Run:

```sh
./bin/build && ./bin/test
```

Append start and finish entries to `worklog.md`. Commit only owned files and the
appended worklog lines.

## Done when

All six requirements work, the full gate is green, and no documented setting or
button is inert.

Created by Codex GPT-5 on 2026-08-28 11:12 PT on ombee.
