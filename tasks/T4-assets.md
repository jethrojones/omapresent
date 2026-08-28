# T4 — Asset index and image resolution

**Agent:** `assets` · **Spec:** §4.5, §15 milestone 3

## Files you own
- `src/assetindex.cpp` (implement; `src/assetindex.h` is frozen — may add)
- `tests/tst_assetindex.cpp`

Nothing else.

## What to build

"It just finds it." A reference written any of these ways resolves to the same
file:
- `![[budget.png]]` — Obsidian embed
- `![alt](budget.png)` — standard Markdown
- `![[~/Pictures/budget.png]]`, `![[/abs/path/x.png]]`, `![[../img/x.png]]`
- a **bare path alone on a line**: `~/Pictures/budget.png` or `./img/x.png`

A bare line only counts as an image if it contains a `/` **or** ends in a known
image extension — a lone word must not be misread as a picture.

### Resolution order (§4.5) — implement exactly
1. Exact path relative to the deck file's directory.
2. `~` / `$HOME` / env-var expansion, then absolute path.
3. **Filename search** against the index of `root` (recursive, default = the
   deck's folder). Shortest / closest match wins, like Obsidian.
4. Case-insensitive retry of step 3 — this matters on Linux.
5. Not found → return empty, so the renderer draws the theme background with a
   "missing: budget.png" tag (the renderer owns that part, not you).

### Things that must not break it
Spaces in paths (no `%20` needed), broken symlinks, permission-denied
directories, an unmounted drive under `root`, a `root` that does not exist, a
huge tree (index lazily/incrementally; do not block the UI thread — do the walk
off-thread and emit `indexChanged()` when it lands), and `http(s)://` URLs,
which `resolve` returns unchanged for the cache layer to fetch.

### Size hints
`![[photo.png|600]]` is a max-width in px; `![[photo.png|main]]` marks the bento
hero. `parseSizeHint` splits them off. `extractReferences` returns the bare
reference without the hint.

### `shortestUniqueReference` (drag-and-drop, §4.5)
Given an absolute path, return the shortest form that still resolves uniquely
against the current index — usually just the filename, a parent directory
prefix when that is ambiguous, the full path when nothing else works. The
editor inserts `![[<that>]]` at the cursor.

### Watching
Watch `root` recursively and emit `indexChanged()` when files appear or vanish.
Debounce — a `git checkout` in the root should produce one signal, not hundreds.

## Tests
`tests/tst_assetindex.cpp`, registered with `OMAPRESENT_TEST_SUITE` — no
`QTEST_MAIN`. Build fixture trees with `QTemporaryDir`: nested folders, two
files with the same name at different depths (assert the shortest wins), a
case-mismatched name, a name with spaces, a broken symlink, and a reference
that resolves nowhere. Cover `extractReferences` against a slide containing all
four reference forms plus a lone word that must **not** be picked up, and a
reference inside a fenced code block that must not be picked up either.

## Done when
`./bin/build && ./bin/test` pass, your suite has real cases, and your worklog
entry is appended.
