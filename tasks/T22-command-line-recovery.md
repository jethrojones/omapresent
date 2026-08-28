# T22 — Make an explicit file win over draft recovery

**Agent:** `recovery` · **Spec:** §3 and §13

## Files you own

- `src/main.cpp`
- new focused test files needed for this launch case
- the smallest build registration needed for those new tests

Do not edit backend, publisher, renderer, package, settings, or acceptance files.
Do not edit `tests/tst_omapresent.cpp` while T20 owns it. You may append to
`worklog.md`.

## Required work

1. Reproduce the confirmed bug: after draft recovery marks the backend modified,
   `omapresent <file>` opens the recovered draft instead of the named file.
2. Make an explicit command-line file take priority over recovery for normal edit
   mode and present mode.
3. Preserve recovery when the user launches without a file.
4. Add an automated regression. Keep the test free of real desktop side effects.
5. Review the relevant specification text before choosing the smallest safe fix.

## Verification

Run the focused regression, then:

```sh
./bin/build && ./bin/test
```

Append start and finish entries to `worklog.md`. Commit only owned files and the
appended worklog lines.

## Done when

`omapresent <file>` always opens that file. A file-free launch still restores a
recoverable draft. The focused and full gates pass.

Created by Codex GPT-5 on 2026-08-28 12:51 PT on ombee.
