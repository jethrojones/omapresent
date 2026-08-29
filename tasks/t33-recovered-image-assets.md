# T33 — Recovered image assets

## Scope

Restore the deck directory for recovered local Markdown files so relative image
references resolve in the editor preview and the running audience presentation.

## Requirements

- Preserve file-free recovery behavior.
- Restore the matching media base with the asset base.
- Cover a relative image path containing spaces.
- Prove the preview and audience deck asset maps use the expected file URL.

## Owned files

- `src/backend.cpp`
- `tests/tst_omapresent.cpp`
- `tasks/t33-recovered-image-assets.md`
