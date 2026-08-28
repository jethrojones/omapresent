# T11 — The static web bundle

**Agent:** `assets` (continuing) · **Spec:** §9, §15 milestone 7

## Files you own (in addition to your T4 files)
- `src/webbundle.cpp` (implement; `src/webbundle.h` is frozen — may add)
- `tests/tst_webbundle.cpp`

Both are already wired into `omapresent.pro` and `tests/tests.pro`, so leave
those alone.

## Why this exists

Spec §9 promises "two artifacts from one command". The `publish` agent is
building the upload half — its `publish()` takes a bundle directory that is
already built. Nothing builds it. That is this task.

## What to build

Two views of the same deck, both produced by the **shared renderer** in `web`
mode (spec §3 — the whole architectural bet is that preview, projector, PDF and
published page are the same code), written as a self-contained static site.

1. **Deck view** (`index.html`) — the slides as presented, navigable by arrow
   key and by swipe on a touch screen, with the **speaker notes shown as
   subtitles beneath each slide**, toggleable.
2. **Long read** (`read/index.html`) — one scrolling page where headings, media,
   lists and notes flow together as a well-set article in the deck's theme.
   This is not the deck with different CSS; prose that was a speaker note
   becomes body text here, which is the whole point of the view.

Cross-link the two so a reader can switch between them.

### Self-contained means self-contained
The output must open from `file://` with no server and no network. Copy every
referenced image and every cached video into `media/` and rewrite the
references to relative paths. Vendored JS and CSS get copied to `assets/`.
Nothing may point at `qrc:`, at an absolute path on the author's machine, or at
a CDN. A deck published from `/home/jethro/...` and unzipped on someone else's
laptop has to render identically — test that by building into a
`QTemporaryDir` and grepping the output for `qrc:`, `file:///home` and `http`.

Use `AssetIndex` for resolution — you wrote it, and the deck JSON's `assets`
map already holds the resolved paths. Filenames in `media/` must be stable
across rebuilds (hash or slug the source path) so republishing does not churn
every file, and must not collide when two different directories hold the same
filename.

### Theme
The palette is baked into the bundle as CSS custom properties from
`deck.palette`, so the published page keeps the deck's theme rather than
following the reader's desktop. Include both light and dark only if the deck's
palette provides them; do not invent a second theme.

### Report accurately
`files()` returns exactly what was written, relative to `outputDir` — the
publisher uploads that list, so a file missing from it silently does not get
published. `totalBytes()` is the real total. Failures set `lastError()` to
something a human can act on and return false without leaving a half-written
bundle behind.

## Tests
`tests/tst_webbundle.cpp`, registered with `OMAPRESENT_TEST_SUITE` — no
`QTEST_MAIN`, no network. Build a small deck into a `QTemporaryDir` and assert:
both HTML files exist; `files()` matches what is actually on disk; images
referenced by the deck are present in `media/` and referenced relatively; two
same-named images from different directories both survive; no output file
contains `qrc:`, an absolute home path, or an `http` asset reference; a rebuild
produces the same filenames; and a failure path (unwritable output directory)
returns false with a useful `lastError()` and no partial bundle.

## Done when
`./bin/build && ./bin/test` pass, a bundle built from `welcome/welcome.md`
opens correctly in a browser from `file://`, and your worklog entry is appended.
