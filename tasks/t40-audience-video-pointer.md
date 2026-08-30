# T40 — Audience video pointer acceptance

Created by Codex on 2026-08-30 13:39 PT on ombee.

## Goal

Prove the production audience QML window forwards hover and click input to a
deferred YouTube loader. Keep presentation keyboard navigation and wheel
navigation intact.

## Owned files

- `src/AudienceWindow.qml`
- `tests/main.cpp` (QtWebEngine test initialization)
- `tests/tests.pro` (focused test registration)
- `tests/tst_audience_video_pointer.cpp`
- `tasks/t40-audience-video-pointer.md`
- `worklog.md` (append-only T40 entries)

## Out of scope

`presentation.cpp`, `presentation.h`, `PresenterWindow.qml`,
`tests/tst_presentation.cpp`, and `omapresent-spec.md` are T39-owned.

## Acceptance checks

The test loads the real `AudienceWindow.qml` and renderer with a minimal
YouTube deck. It moves the native pointer, reads the real DOM, and proves
preparing state and tokenized loopback replacement. It also proves the
production key/wheel overlay does not own the loader coordinates.

## Verification note

The regression is split into two deterministic proofs. The native proof uses
`QQuickWindow::contentItem()->childAt()` at the loader coordinates and requires
the production `QQuickWebEngineView`. The DOM proof invokes the real loader
button listener through the public `runJavaScript` slot and requires the
preparing mutation and tokenized loopback iframe. QtWebEngine did not accept
`QTest` synthetic pointer delivery on this host. Visual hover and hardware
click remain final user acceptance checks.

The suite filter self-check is:
`OMAPRESENT_TEST_SUITE=DefinitelyNotASuite ./build-tests/tst_omapresent`
It must exit nonzero and report the unknown suite.
