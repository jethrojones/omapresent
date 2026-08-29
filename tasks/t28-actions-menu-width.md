# T28 Actions menu width

## Owned files

- `PRODUCT.md`
- `tasks/t28-actions-menu-width.md`
- `src/Main.qml`
- `tests/tst_omapresent.cpp`
- `worklog.md` (append-only)

## Scope

Fix the Actions menu so every current English label is visible at normal text
scale. The width must follow the widest menu item, use a useful minimum, and
stay inside the available screen width. Preserve the existing menu actions,
keyboard handling, focus, accessible names, and theme behavior. Add focused
QML coverage for the width rule.

## Design context

`PRODUCT.md` records only facts supported by `README.md` and
`omapresent-spec.md`. The register is product. `DESIGN.md` remains optional and
was not needed for this narrow component fix because the existing QML control
and Omarchy theme define the visual treatment.

## Verification

Run the focused Omapresent QML test, `qmllint` for `src/Main.qml`,
`git diff --check`, `./bin/build`, and `./bin/test`.

Created by Codex GPT-5 on 2026-08-29 14:30 PT on ombee.
