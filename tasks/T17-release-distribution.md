# T17 — GitHub release and Omarchy distribution

**Agent:** `release` · **Spec:** §12, §15 milestone 9

## Files you own

- `README.md`
- `.github/`
- `pkgbuild/PKGBUILD`
- `pkgbuild/omapresent.install`
- `pkgbuild/omapresent.desktop`
- release and distribution metadata added under `pkgbuild/` or `.github/`

The icon agent owns `pkgbuild/omapresent.png`. You may consume it after its
handoff, but do not replace or regenerate it. Do not edit application code,
renderer code, skill files, or tests outside release infrastructure.

## Required work

1. Correct the README dependency list and do not claim package availability
   until it is real.
2. Ensure CI installs Chromium and runs the browser suites without skipping.
3. Convert the local-checkout PKGBUILD into a release-source package that uses a
   GitHub tag and verified checksums. Preserve a practical local validation path.
4. Run a clean `makepkg`, inspect package contents, and run `namcap` on both the
   PKGBUILD and package. Explain any accepted warnings.
5. Discover the current Omarchy application/package submission process from the
   installed Omarchy tools and authoritative upstream repositories. Prepare the
   exact package metadata or pull request needed for normal Omarchy installation.
6. Inspect GitHub authentication and repository state. Prepare a tagged release
   with release notes and package artifacts. Do not publish a release before the
   orchestrator confirms the final product gate.

## Verification

Run all release checks that do not require publication. Run:

```sh
./bin/build && ./bin/test
```

Append start and finish entries to `worklog.md`. Commit only owned files and the
appended worklog lines.

## Done when

The repository can produce a release package from a tag, CI cannot skip browser
tests, package lint is complete, and the Omarchy submission path is ready for the
final external publication step.

Created by Codex GPT-5 on 2026-08-28 11:12 PT on ombee.
