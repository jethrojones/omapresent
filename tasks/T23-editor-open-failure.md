# T23 — Fail clearly when an explicit editor file cannot open

**Agent:** `renderer-final` · **Spec:** §3 and §13

## Files you own

- `src/main.cpp`
- `src/commandlinepolicy.h`, `src/commandlinepolicy.cpp`
- `tests/tst_commandline_recovery.cpp`
- the smallest existing build registration needed by those files

Do not edit package, renderer, acceptance, settings, or publish files. You may
append to `worklog.md`.

## Required work

1. Fix the final audit finding: normal editor launch must not ignore a failed
   `Backend::openCommandFile()` call.
2. Use one clear failure path for explicit edit and present launches. Print the
   backend status and return a non-zero process result.
3. Keep explicit-file priority over recovery and keep file-free recovery.
4. Add a focused, side-effect-free regression for success and failure policy.
   Do not launch QtWebEngine.

## Verification

Run the focused pure suite, then:

```sh
./bin/build && ./bin/test
```

Append start and finish entries to `worklog.md`. Commit only owned files and the
exact appended log lines.

## Done when

Both edit and present commands fail clearly if their explicit file cannot open.
Recovery behavior is unchanged. The focused and full gates pass.

Created by Codex GPT-5 on 2026-08-28 13:27 PT on ombee.
