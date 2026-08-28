# Review 1 — `theme` (T3)

I verified your parser independently: I ran `parseColorsToml` over **all 29
themes installed on this machine** — both shapes, `/usr/share/omarchy/themes/`
and `~/.config/omarchy/themes/` — asserting the full canonical key set is
present, every colour matches `^#[0-9a-f]{6}$`, `mode` is `dark`/`light`, and
`ansi` has 16 entries. **All 29 parsed cleanly, zero problems.** Contrast is
exact too: black on white is 21.0, a colour on itself is 1.0.

There is one real bug.

## `ensureContrast` picks the wrong direction and gives up short of the floor

```
ensureContrast("#767676", "#808080", 4.5)  ->  #ffffff, ratio 3.95
```

It returns a colour that does **not** clear the floor, which is the one thing
this function exists to guarantee.

The cause is the direction choice:

```cpp
const qreal dir = (relativeLuminance(bgHex) < 0.5) ? 1.0 : -1.0;
```

`#808080` has a relative luminance of 0.216 — below 0.5, so you walk lightness
*up*. But against a mid-grey background, up is the losing direction:

- white on `#808080` → (1.0 + 0.05) / (0.216 + 0.05) = **3.95**, never reaches 4.5
- black on `#808080` → (0.216 + 0.05) / (0.0 + 0.05) = **5.32**, clears it easily

So a reachable answer existed and the function returned an unreachable one. Note
that relative luminance is not perceptual lightness — 0.5 luminance is a much
lighter grey than mid-grey — which is why the `< 0.5` test picks wrong for a
whole band of backgrounds around mid-tone.

**Fix:** walk both directions and take the first that clears `minRatio`. If
neither does, return whichever end achieved the higher ratio rather than
whichever you happened to try. Prefer the direction that preserves the original
lightness relationship when both clear it, so a light theme does not suddenly
get dark text.

This matters in the real case, not a synthetic one: it is the audience window on
a projector, where washed-out contrast is exactly the failure the spec added
this floor to prevent.

## Tests to add

- The `#767676` on `#808080` case above, asserting the **returned** colour
  clears 4.5 — assert the postcondition, not a specific hex value.
- A sweep: for a set of backgrounds across the luminance range (include several
  mid-tones near `#808080`), assert `contrastRatio(ensureContrast(fg, bg), bg)
  >= 4.5` whenever any colour could achieve it.
- A background where the floor genuinely cannot be met, asserting you return the
  best available rather than looping or returning the input unchanged.

## Also

Your worklog only has the "Starting" entry; append the real one describing what
you built and the decisions you made.
