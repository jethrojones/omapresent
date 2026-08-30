# T38 - Presentation video bridge race

## Scope

When a reader activates a deferred YouTube player before the qrc page's
WebChannel bridge is ready, keep the loader in a bounded preparing state. Retry
for the bridge, then use the existing tokenized loopback shim. Fall back to the
existing QR/open affordance if the bridge does not arrive.

## Owned files

- `src/renderer/embed.js`
- `src/renderer/render.js`
- `tests/renderer/embed.test.mjs`
- `tasks/t38-presentation-video-race.md`
- `worklog.md` (append-only T38 entries)

## Out of scope

- `src/AudienceWindow.qml` and `src/PresenterWindow.qml`; both correctly read
  `Presentation::bridgeScript` as a QML property.
- T37 files and worklog entries.
