# T18 — Production application icon

**Agent:** `icon` · **Spec:** §12, §15 milestone 9

## Files you own

- `pkgbuild/omapresent.png`
- optional source and prompt records under `artwork/`

Do not edit `pkgbuild/PKGBUILD`, the desktop file, application code, or the
existing SVG. The release agent will connect the selected PNG to packaging.

## Required work

1. Inspect the current placeholder icon and the application visual language.
2. Use the built-in ChatGPT image-generation tool to create a production square
   icon. It must remain clear at small sizes, use the dark charcoal and warm gold
   Omapresent language, contain no text, and avoid a generic slide-template look.
3. Use a transparent background. Save the final 1024×1024 PNG as
   `pkgbuild/omapresent.png`.
4. Inspect the saved file. Confirm dimensions, alpha, and small-size clarity.

## Verification

Append start and finish entries to `worklog.md`. Commit only owned files and the
appended worklog lines.

## Done when

The project contains a reviewed production PNG and the final prompt is recorded.

Created by Codex GPT-5 on 2026-08-28 11:12 PT on ombee.
