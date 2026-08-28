# T20 — Complete the custom-domain publish flow

**Agent:** `publish` · **Spec:** §9

## Files you own

- `src/publisher.h`, `src/publisher.cpp`
- `src/backend.h`, `src/backend.cpp`
- `src/Main.qml`
- `tests/tst_publisher.cpp`
- `tests/tst_omapresent.cpp`

Do not edit renderer, package, CI, README, settings, or acceptance files. You may
append to `worklog.md`.

## Required work

1. Add a public, explicit, user-started domain setup API for the selected
   publish provider. Do not require a publish operation first.
2. Return the provider's DNS records and status through a stable result signal.
3. Add the publish-preferences control that starts domain setup and displays the
   returned records or a clear provider error.
4. Keep all network work behind this explicit user action. Opening, editing, and
   saving a deck must remain offline.
5. Use the existing here.now domain transport. Do not duplicate provider logic.

## Verification

Use the in-process HTTP server tests. Do not contact the real provider. Add
focused backend and UI contract tests, then run:

```sh
./bin/build && ./bin/test
```

Append start and finish entries to `worklog.md`. Commit only owned files and the
appended worklog lines.

## Done when

The user can request domain setup from publish preferences and copy the returned
DNS records. The focused and full gates pass.

Created by Codex GPT-5 on 2026-08-28 12:38 PT on ombee.
