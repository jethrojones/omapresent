# T30 — Final GitHub listing and Actions menu acceptance

**Agent:** `acceptance` · **Spec:** §5.1, §4.7, §7, §15

## Scope

Verify the T27 screenshot, T28 Actions menu sizing, T29 README feature text,
the final local gate, and the exact master CI run for commit `34306d8`.

## Evidence

- `master` and `origin/master` match at `34306d8`.
- The README states no text shrinking, vertical scrolling for tall slides, and
  verified QWERTY `Q`, `Space`, and `Esc` recall behavior.
- `welcome/screenshot.png` is tracked and its raw GitHub URL returns HTTP 200.
- The Actions menu uses the owning window's current screen, follows the widest
  full label when possible, caps at narrow screen width, and keeps font sizes.
- The focused regression passes. The desktop-safe gate passes with 521 C++ and
  43 renderer tests.
- CI run `33277564262` for `34306d8` passed.
- v0.1.1 remains published. PR #235 remains present; its current GitHub
  mergeability result was `UNKNOWN` during this read-only check.

Created by Codex GPT-5 on 2026-08-29 15:07 PT on ombee.
