# T3 — Omarchy theme bridge

**Agent:** `theme` · **Spec:** §6, §15 milestone 1

## Files you own
- `src/omarchytheme.cpp` (implement; `src/omarchytheme.h` is frozen — may add)
- `tests/tst_omarchytheme.cpp`

Nothing else.

## What to build

Omapresent wears the same clothes as the rest of the desktop. This class is
the only thing in the app that knows where Omarchy keeps its colours.

### Sources
- `~/.local/state/omarchy/current/theme/colors.toml` — the live palette
- `~/.local/state/omarchy/current/theme.name` — its name
- `~/.local/state/omarchy/current/background` — the missing-image placeholder

### Both `colors.toml` shapes must work
1. **Rich named form** (catppuccin, gruvbox): `mode`, `accent`, `background`,
   `foreground`, `muted`, `selection`, `*_background`, `*_foreground`,
   `red|orange|yellow|green|cyan|blue|magenta|brown`, `bright_*`.
2. **Terminal form** (gold-rush): `accent`, `foreground`, `background`,
   `cursor`, `selection_*`, `color0`–`color15`. **Derive** the semantic roles
   from the 16 colours when the named keys are absent — `red` from `color1`,
   `bright_red` from `color9`, and so on; `muted` from `color8`.

`parseColorsToml` always returns the full canonical key set documented in the
header — every key present, every colour `#rrggbb`. Handle `#rgb`, `0x` and
bare-hex values, quoted and unquoted, with or without TOML tables. Never fail:
an unreadable file yields a sane dark default.

Look at the themes actually installed on this machine under
`/usr/share/omarchy/themes/` and `~/.config/omarchy/themes/` and make sure you
parse every one of them. Use several as test fixtures — copy the *content* into
the test file, do not read the live system from a test.

### Live reload (§6)
Watch the `current/theme` symlink **and** the `colors.toml` it points at, and
re-read on change so every open window repaints without losing position. A
symlink swap does not fire a file watcher on the target — watch the parent
directory too. Emit `themeChanged()` exactly once per actual change; debounce
the burst of events a theme switch produces.

### Per-deck override (§6)
`setOverrideTheme("gruvbox")` loads `~/.config/omarchy/themes/gruvbox/colors.toml`
then `/usr/share/omarchy/themes/gruvbox/colors.toml`. It applies **only inside
Omapresent's windows** — never write anything that would re-theme the desktop.
An unknown name falls back to the live theme and warns.

### Projector legibility floor (§6)
`contrastRatio` is the real WCAG formula (sRGB → linear → relative luminance,
`(L1+0.05)/(L2+0.05)`). `ensureContrast` returns the foreground unchanged when
it already clears the ratio, otherwise nudges its **lightness only** (convert to
HSL, walk L toward the far end) until it does, preserving hue and saturation.
This is applied to the audience window only — presenter, preview and PDF keep
the exact theme.

### `installedThemes()`
Both theme directories, sorted, deduplicated by name, user config winning.

## Tests
`tests/tst_omarchytheme.cpp`, registered with `OMAPRESENT_TEST_SUITE` — no
`QTEST_MAIN`. Cover: both file shapes, ANSI derivation, malformed input, `#rgb`
expansion, known contrast ratios (black-on-white is 21.0, and pick two mid-tone
pairs you compute by hand), `ensureContrast` raising a failing pair over the
floor while keeping its hue, and override resolution order.

## Done when
`./bin/build && ./bin/test` pass, your suite has real cases, and your worklog
entry is appended.
