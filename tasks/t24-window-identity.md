# T24 — Audience top-level window identity

**Agent:** `app-final` · **Model:** `Codex GPT-5` · **Spec:** §5.1, §5.3, §15

## Files I own

- `src/presentation.cpp`
- `src/presentation.h`
- `src/AudienceWindow.qml`
- `tests/tst_presentation.cpp`
- `tasks/t24-window-identity.md`

I may append to `worklog.md` only. I will not edit Hyprland or Omarchy user
configuration.

## Required work

1. Keep the audience as an independent native Wayland top-level window.
2. Make its normal top-level, non-modal, no-transient-parent state explicit.
3. Give it the stable title and application identity `Omapresent`.
4. Keep the editor independent. Preserve presenter, lifecycle, and resize
   behavior.
5. Add focused regression coverage and verify the live Hyprland clients.

Created by Codex GPT-5 on 2026-08-28 14:34 PT on ombee.
