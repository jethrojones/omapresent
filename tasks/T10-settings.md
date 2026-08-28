# T10 — `settings.toml` support

**Agent:** `skill-docs` (continuing) · **Spec:** §9 settings surface, §11, §10

## Files you own (in addition to your T7 files)
- `src/settings.cpp` (implement; `src/settings.h` is frozen — may add)
- `tests/tst_settings.cpp`

The orchestrator has already added both to `omapresent.pro` and
`tests/tests.pro`, so leave those alone.

## Why this exists

You documented `~/.config/omapresent/settings.toml` in
`skill/reference/settings-toml.md`, and it is a good schema. But nothing in the
app reads it, which means the skill currently describes a file the app ignores —
exactly the failure `bin/check-skill-sync` exists to prevent. You wrote the
schema, so you are the right one to make it real.

`skill/reference/settings-toml.md` is the specification for this task. Where it
and this brief differ, fix whichever is wrong and say which in your worklog.

## What to build

`Settings::defaults()` is the **single source of truth** for every key and its
default value: `editor.text_scale`, `editor.dark_mode`, `editor.font`,
`editor.theme`, `editor.auto_break_triple_return`, `editor.remember_geometry`,
`presentation.inhibit_idle`, `presentation.do_not_disturb`,
`presentation.default_aspect`, `presentation.single_monitor_notes`,
`presentation.auto_prefetch_video`, `export.pdf_aspect`,
`export.pdf_paginated`. Every getter falls back to it, so the app behaves
identically with no config file at all — that is the property to test hardest.

Reading: parse the file into `m_fileValues` and merge defaults underneath.
**Do not write a second TOML parser** — `Publisher::parseToml` already exists
for exactly this shape, and two parsers that disagree is a bug waiting to
happen. Same for writing: `setValue` must go through `Publisher::patchToml` so
comments, blank lines, key order and unknown keys survive byte-identical.
(The publish agent is implementing both right now; build against the
declarations in `src/publisher.h`.)

Validate the enums named in your own reference doc — `dark_mode` is
`auto|dark|light`, the aspect keys are `16:9|4:3|16:10|1:1`. An invalid value in
the file is not a crash and not a silent acceptance: fall back to the default
and `qWarning()` once naming the key and what it contained.

Watch the file and emit `settingsChanged()` when it changes on disk, so editing
the TOML by hand takes effect without a restart. Debounce it.

`setValue` creates `~/.config/omapresent/` when absent, writes atomically
(temp file then rename — never a partial config), and returns false without
touching the file on any error.

## Tests
`tests/tst_settings.cpp`, registered with `OMAPRESENT_TEST_SUITE` — no
`QTEST_MAIN`. Drive everything off a `QTemporaryDir`. Cover: every default
returned with no file present; a partial file where only some keys are set;
an invalid enum falling back with a warning; `setValue` round-tripping; and the
important one — `setValue` on a file full of comments leaves every comment and
unrelated key byte-identical. Add a test asserting `defaults()` and
`skill/reference/settings-toml.md` name the same key set, so the two cannot
drift.

## Done when
`./bin/build && ./bin/test` and `./bin/check-skill-sync` pass, and your worklog
entry is appended.
