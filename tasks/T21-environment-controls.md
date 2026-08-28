# T21 — Honor presentation settings and Omarchy theme changes

**Agent:** `publish` · **Spec:** §5.3, §11, and §12

## Files you own

- `src/presentation.h`, `src/presentation.cpp`
- `src/backend.cpp`
- `tests/tst_presentation.cpp`
- `tests/tst_omapresent.cpp`
- `pkgbuild/PKGBUILD`
- `pkgbuild/omarchy-pkgs/omapresent/PKGBUILD`
- one new package-owned Omarchy `theme-set` hook source and its focused tests

Do not edit renderer, command-line recovery, release workflow, README, or
acceptance files. You may append to `worklog.md`.

## Required work

1. Honor `presentation.inhibit_idle` and `presentation.do_not_disturb`.
   Disabled settings must not acquire their system holds. Enabled settings must
   keep the present-mode behavior.
2. Add a side-effect-free test seam. Test all four setting combinations,
   repeated starts, and cleanup on stop.
3. Package an Omarchy `theme-set` hook so a running Omapresent process reloads
   the changed theme when Omarchy swaps its `current` path. Use the installed
   convention at `$HOME/.config/omarchy/hooks/theme-set.d/` and the normal
   `omarchy hook install theme-set <script>` semantics.
4. The hook must be safe when Omapresent is not running. It must not start the
   app, alter user files, or depend on network access.
5. Keep the source PKGBUILD and staged Omarchy PKGBUILD identical where their
   install behavior overlaps.

## Verification

Add focused tests for settings behavior and hook packaging. Use a temporary
HOME for hook checks. Verify the installed path, executable mode, theme argument
handling, and no-process behavior. Then run:

```sh
./bin/build && ./bin/test
```

Append start and finish entries to `worklog.md`. Commit only owned files and the
exact appended log lines.

## Done when

The two presentation settings control their holds. A normal Omarchy theme
change reaches a running app through the packaged hook. Focused and full gates
pass.

Created by Codex GPT-5 on 2026-08-28 12:58 PT on ombee.
