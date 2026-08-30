# T39 — The audience window opens windowed

**Agent:** `app-shell` · **Spec:** §5.1, §5.2, §13

## Why

`Present` has always opened the audience window fullscreen, because
`Presentation::placeWindow` called `showFullScreen()` unconditionally. On a
single-output machine — which is most of them, and every machine someone writes
a talk on — that means the act of previewing a deck takes over the display and
gives the user no window to move, resize, or drop into a tile beside the editor.

The user has overridden the old default. The audience window opens as a normal
independent window, and fullscreen becomes something you ask for.

Spec §5.1 already promised both things: "audience → the external/non-primary
output fullscreen" and, four lines later, "The audience window is a normal
shareable window". This task settles that contradiction in favour of the second
sentence and rewrites the first.

## Files you own

- `src/presentation.cpp`, `src/presentation.h`
- `src/PresenterWindow.qml`
- `src/Main.qml`
- `tests/tst_presentation.cpp`, `tests/tst_omapresent.cpp`, plus a focused QML
  test file if one is needed
- `omapresent-spec.md`
- `tasks/t39-windowed-audience.md`
- `worklog.md` (append only, at the true end of the file)

**Do not edit `src/AudienceWindow.qml`** — the click investigation may still need
it as it stands.

## Required behaviour

1. Clicking Present opens the audience as a normal, independent, resizable and
   movable window by default, on one output and on several.
2. Monitor assignment is unchanged: with two or more outputs the audience still
   goes to the external/non-primary one. The compositor may tile the window, and
   that is fine — a tiled window is a movable one.
3. `F` and `F11` both toggle audience fullscreen, in both directions.
4. The fullscreen state survives monitor reassignment. A projector plugged in
   mid-talk must not silently drop the presenter back to windowed, and must not
   silently fullscreen a window they had left windowed.
5. The presenter window carries a clear Fullscreen/Windowed control while it
   exists. The audience window gets no new chrome: it is the projected surface.
6. Everything else about these windows is preserved — the separate presenter
   window, `Qt::Window` flags, non-modal, no transient parent, both titles, the
   single application identity, and every existing key.
7. `F11` must reach the window the user is actually looking at. The editor binds
   it too, and an application-scoped shortcut answers for whichever window of
   the application is focused.
8. The presenter's control is usable without a mouse: a button role and name,
   Tab focus, Return / Enter / Space activation, and a focus state you can see.
9. Every conflicting line of spec, header comment and shortcut text is updated.

## Tests

Deterministic, no compositor required. `Presentation::setWindowFactoryForTesting`
already exists as the seam, so these run under `QT_QPA_PLATFORM=offscreen`
without QtWebEngine:

- the audience window is `QWindow::Windowed` after `start()`
- `toggleFullscreen()` in both directions
- `F11` does exactly what `F` does
- a re-assignment preserves fullscreen, and preserves windowed
- with two outputs the audience lands on the non-primary screen and is still
  windowed
- the presenter window is a separate, non-modal, parentless `Qt::Window`
- `shortcutReference()` names both keys
- the presenter control exists and is wired
- the editor's fullscreen shortcut is window-scoped, so it cannot answer for the
  audience window, and still carries both of the editor's own keys
- the presenter control is a button with a name, takes Tab focus, activates on
  Return / Enter / Space, shows its focus, and still passes every other key to
  the talk
- Tab is left to the focus chain on a slide with no players, and still cycles
  them on a slide that has some
- the `start()` comment and the editor's `Ctrl+?` sheet say what the code does

## Done when

The behaviour above holds, `./bin/build && ./bin/test` is green, and the spec no
longer says the audience window opens fullscreen.

Created by Claude Opus 5 on 2026-08-30 09:02 PT on ombee
