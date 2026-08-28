# Bug — Do-Not-Disturb is never actually held during a presentation

**Owner:** `theme` agent, inheriting `src/presentation.cpp` from the `present`
agent, whose provider hit its session limit. You found the missing `.qrc`
registration that was hiding this; I fixed that, and present mode now opens, so
this became visible.

## Symptom

I ran `omapresent present welcome/welcome.md` on the real display and
screenshotted the audience window. It works — fullscreen, title slide, header,
footer tokens, slide counter. **And nine desktop notifications were stacked over
the top right of it.** That is precisely what spec §5.3's Do-Not-Disturb
requirement exists to prevent, and it is the kind of thing that is merely
embarrassing in a demo and genuinely bad in front of an audience.

## Cause

`DoNotDisturbHold` falls through mako, swaync and dunst — none of which is
installed on Omarchy 4.x — to `omarchy-toggle-notification-silencing`. That
branch is written to toggle once and read the state the tool prints, which is a
reasonable design for a toggle-only tool. But the tool prints nothing:

```bash
#!/bin/bash
# omarchy:summary=Toggle notification do-not-disturb mode
state=$(omarchy-shell notifications toggleDnd 2>/dev/null || echo "")
omarchy-shell -q omarchy.indicators refresh
```

It captures the state into `$state` and never echoes it. So stdout is empty,
`readable` is false, and the code takes its own safety path: it undoes the
toggle, sets `m_tool = None`, and returns. The result is that every
presentation toggles the user's DND on and straight back off, and then presents
with notifications enabled.

The safety path itself is good judgement — not knowing what you did and backing
out is the right instinct. The problem is only that the premise, "it prints the
state it toggled to", is false.

## The fix

`omarchy-shell` has a real query, which is much better than toggle-and-guess:

```
omarchy-shell notifications isDnd     # prints "on" / "off"
omarchy-shell notifications setDnd    # see Service.qml for the argument
```

Both are in `/usr/share/omarchy/shell/plugins/notifications/Service.qml`, which
also has `dndState`. Read that file for the exact signatures rather than
guessing.

So: read `isDnd` first, and only enable if it is off, recording that you did.
Restore on exit exactly as the other branches already do — the RAII structure is
right, only this branch's state detection is wrong. Keep `omarchy-toggle-…` as a
last resort if `omarchy-shell` is missing, but prefer the query.

Please also check whether `setDnd` needs an argument and what it returns, and
handle an `omarchy-shell` that exists but whose `notifications` plugin does not,
since the existing "back out and stay out of the way" behaviour is the right
fallback there.

## Verify by looking

Re-run present mode, take a screenshot, and confirm a notification sent during
the presentation (`notify-send test` from another pane) does not appear over the
audience window — and that DND returns to its prior state after `Esc`. Check the
prior-state restore in both directions: DND already on, and DND off.

## Also worth checking while you are in there

`isDnd` reported `on` before, during and after my test, so I could not tell from
the outside whether anything leaked. Once you are reading real state, assert
that a presentation started with DND already on leaves it on afterwards.
