# T9 — Present mode

**Agent:** `present` · **Spec:** §5, §4.7, §4.9, §15 milestone 4 & 6

## Files you own
- `src/presentation.cpp` (implement; `src/presentation.h` is frozen — may add)
- `src/PresenterWindow.qml`, `src/AudienceWindow.qml` (new — create them)
- `tests/tst_presentation.cpp` (new — register it in `tests/tests.pro`, which
  is the one shared file you may touch, and only to add your two lines)

Nothing else. `src/Main.qml`, `src/backend.*` and `src/resources.qrc` belong to
the `app-shell` agent — it will register your QML files and call `start()`.
Put anything you need from it under `NEEDS:` in your worklog entry.

## What to build

### Two windows, deliberately separate (§5.1)
Two **separate top-level windows**, not one window with panes — so Hyprland's
tiling and window rules treat them independently, and either can be screen-shared
or captured in OBS on its own. That last part is the point: sharing just the
audience window in a video call gives a clean full-frame capture with no
presenter notes leaking into it.

- **Audience window** — headings, media and lists only. Themed, fills its output.
- **Presenter window** — the current slide scaled down, a next-slide preview,
  the speaker notes rendered as formatted Markdown (not raw), an elapsed timer
  that resets on click, a wall clock, the current slide x/count, and the list of
  recall-key bindings.

Both load the shared renderer (`qrc:/renderer/render.html`) in a
`WebEngineView` with `role` set to `"audience"` / `"presenter"` — you do not
render Markdown yourself. Read `docs/renderer-contract.md`; the page API and the
state event are the whole interface. Wire the state event through a
`QWebChannel` object named `omapresentHost`, exactly as the contract says.

### Monitors (§5.1)
Two or more outputs: audience goes fullscreen on the external/non-primary
output, presenter on the other. One output: audience fills it, and `N` toggles a
notes overlay on the same screen. **Projector hotplug mid-talk must work** —
re-evaluate outputs on `QGuiApplication::screenAdded`/`screenRemoved` and move
the windows. Someone will plug in a projector while presenting; that is the
normal case, not the edge case.

### Keys (§5.2)
`→`/`Space` next fragment then next slide, with Space playing/pausing media
first · `←` back · `↑`/`↓`/PgUp/PgDn/wheel scroll the current slide, and **the
audience mirrors the presenter's scroll live** · Home/End · digits then Enter to
jump to a slide number · `F` fullscreen · `B` black · `W` white · `O` overview
grid, arrows and Enter to pick · `N` notes overlay · a bound letter or digit
shows/hides that recall overlay · `Esc` exits both windows · `Ctrl+?` shortcuts.

Keys must work with either window focused — the presenter is where the speaker's
hands are, but the audience window may have focus after a click.

### Recall overlays (§4.9)
A bound key renders that slide as a full-screen overlay over the current one,
current slide dimmed behind. The same key, `Esc` or `Space` dismisses it and
returns **exactly** where you were — same slide, same scroll offset, same
fragment. That is the whole value of the feature; losing position defeats it.

### Environment (§5.3)
- **Inhibit idle** for the duration: `org.freedesktop.ScreenSaver.Inhibit` over
  DBus, and hypridle where present. Release it on exit, including on a crash
  path — take the inhibit in a RAII holder, not a bare pair of calls.
- **Enable Do-Not-Disturb** while presenting and **restore the prior state** on
  exit — read it first, do not assume it was off. `omarchy` / `makoctl` /
  the notification portal, whichever this machine has.
- Transitions are **instant cuts**. No animation, no fade.

Both of these must be exception-safe and idempotent: presenting twice in one
session must not stack two inhibits or lose the original DND state.

### Scroll position (§4.7)
Remembered per slide within a session, so navigating away and back returns to
where you were.

## Tests
`tests/tst_presentation.cpp`, registered with `OMAPRESENT_TEST_SUITE` — no
`QTEST_MAIN`. Windowing and DBus are not unit-testable here, so test the logic
that is: navigation across fragments and slides including the boundaries, the
digit-then-Enter jump parser (including a jump past the end), recall
show/hide restoring the exact prior position, scroll memory per slide, and the
monitor-assignment decision as a pure function of a list of outputs. Add your
suite to `tests/tests.pro` — only your own two lines.

## Done when
`./bin/build && ./bin/test` pass, and your worklog entry is appended.
