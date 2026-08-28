# T14 — Adversarial review of the risky paths

**Agent:** `reviewer` · **Spec:** §9, §11, §4.5, §4.8, §12

## Files you own
- `tests/tst_security.cpp` (new — register your two lines in `tests/tests.pro`)
- `docs/review-findings.md` (new)

**Do not fix other agents' code.** Every other file in this repo belongs to
someone who is actively working in it. Your job is to find problems, prove them
with a failing test or a concrete reproduction, and write them up. The
orchestrator routes them to owners.

## Why this exists

Ten agents built this in parallel, each verifying its own piece. Nobody has
looked at the tree as a whole with an adversarial eye, and the places where that
matters are the ones where the app runs commands, writes files, or sends the
user's work to someone else's server.

## What to look at, roughly in order of how much it would hurt

### 1. The `command` publish provider (`src/publisher.cpp`)
It runs a command string from `~/.config/omapresent/publish.toml`. That file is
the user's own, so running it is the feature, not the bug. What matters is
everything around it: is the deck slug or bundle path ever interpolated into a
shell string rather than passed through the environment? A deck titled
`; rm -rf ~` must not be able to do anything, and `slugify` is the only thing
standing between a filename and a command line. Check `QProcess` usage — is it
`start(program, args)` or a single shell string?

### 2. Anything that writes or copies files
`WebBundle` copies referenced media into a bundle directory, and `AssetIndex`
resolves references that came out of a Markdown file. Can a crafted reference —
`../../../../etc/passwd`, an absolute path, a symlink pointing outside the root,
a filename containing `/` or `..` after resolution — cause a write outside the
output directory, or copy something the author did not intend to publish? A
deck that publishes the author's SSH key because it was symlinked under the
asset root is the failure to hunt for.

### 3. The publish safety rule (§9, §11)
Nothing may upload without an explicit, user-initiated `publish()` call. Verify
that by reading, not by trusting: does anything on the save path, the file-watch
path, the prefetch path or first run reach the network? `VideoCache::prefetch`
is allowed to, on an explicit action — is it ever called implicitly? The CLI's
`publish` must confirm before uploading unless given `--yes`.

### 4. Cache and state paths
`<deck-dir>/.omapresent-cache/` is derived from the deck's own directory, and
`~/.local/state/omapresent/` holds per-file state keyed somehow. Can a deck
path with `..`, a newline, or a very long name escape those directories or
collide two decks' state? Does anything delete outside its own cache?

### 5. Robustness on hostile input
Feed the parsers things a real user will eventually produce: a 50 MB Markdown
file, a line 10 MB long with no newline, deeply nested lists, a file that is
valid UTF-16, invalid UTF-8 bytes, 10,000 slides, a frontmatter block that never
closes, `$$` that never closes, a fence that never closes. Nothing may hang,
crash, or consume unbounded memory. `DeckModel`, `AssetIndex` and `VideoCache`
are all reachable from file contents.

## How to report

`docs/review-findings.md`, worst first. For each: what it is, the concrete input
or steps that demonstrate it, which file it lives in, and how bad it actually is
— be honest when something is theoretical. A finding nobody can reproduce is
noise, and overstating severity wastes the owner's time as surely as missing it.

Where you can pin a finding with a test, add it to `tests/tst_security.cpp` and
mark it `QEXPECT_FAIL` with a comment pointing at the finding, so the suite goes
green now and starts failing honestly the moment someone fixes it wrong.

## Done when
`./bin/build && ./bin/test` pass, `docs/review-findings.md` is written, and your
worklog entry is appended. Finding nothing serious is a legitimate result — say
so plainly and describe what you checked, rather than padding the list.
