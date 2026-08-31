# T42 — Audience window: physical video click

Created by Claude Opus 5 on 2026-08-30 20:06 PT on ombee.

## Goal

A real mouse in the separate audience presentation window must hover-highlight
and activate the deferred video loader, exactly as the editor preview already
does. T40 proved the QML hit target and the DOM listener; it did not prove that
a native pointer event ever reaches the page, and it does not.

## Owned files

- `src/AudienceWindow.qml`
- `tests/tst_audience_video_pointer.cpp`
- `tasks/t42-audience-physical-video-click.md`
- `worklog.md` (append-only T42 entries)

These first two are T40's files. T40 is finished; T42 succeeds it on the same
acceptance fault and takes them over. Nothing else in the tree is touched.

## Out of scope

`src/presentation.cpp`, `src/presentation.h`, `src/PresenterWindow.qml`,
`src/Main.qml`, `src/PreviewPane.qml`, the renderer bundle, and
`omapresent-spec.md`.

## Must keep working

Wheel scrolling from the audience window, every key in spec §5.2 reaching
`Presentation::handleKey` from either window, the presenter's controls, the
audience window's independent Wayland top-level identity, offline behaviour,
and the editor preview.

## Acceptance checks

Native pointer events synthesized at the loader's real coordinates in the
production `AudienceWindow.qml`, driving the real qrc renderer, must reach the
page's own listeners: hover and click both. A key pressed straight after that
click must still move the presentation, not go into the page.
