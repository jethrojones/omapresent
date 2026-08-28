# T12 — End-to-end integration tests

**Agent:** `doc-model` (continuing) · **Spec:** all of §4, §7

## Files you own (in addition to your T1 files)
- `tests/tst_integration.cpp` (new)
- `tests/integration/**` (new — fixture decks)
- You may add **only** your own two lines to `tests/tests.pro`

Do not modify any other agent's source. If an integration test fails because of
someone else's code, that is the test doing its job: record it under `NEEDS:`
in your worklog with the exact failing assertion, and leave their file alone.

## Why this exists

Nine agents each have a green suite for their own piece. Nothing yet checks that
the pieces agree with each other, and that is where this project will actually
break: `DeckModel` decides what a slide is, `AssetIndex` decides what an image
reference resolves to, and the renderer decides what is audience content versus
a speaker note. Each can be individually correct and still disagree at the
seams.

## What to build

### 1. The welcome deck is the fixture
`welcome/welcome.md` is a real deck that deliberately exercises every rule in
the spec, and it ships to users. Treat it as the primary fixture — if the app
cannot handle its own manual, nothing else matters.

Assert against it:
- It parses without warnings, and the slide count is stable (assert the actual
  number, so a careless edit to the manual has to be deliberate).
- Its frontmatter parses, including the nested `publish:` map.
- Every recall key it binds is a single letter or digit, and no key is bound
  twice — a duplicate binding is silently unreachable.
- Every image reference in it resolves through `AssetIndex`, **or** is a
  deliberate demonstration of the missing-image placeholder. Enumerate the
  intended-missing ones explicitly rather than allowing any failure.
- Every bare URL line classifies as either a recognised video host or a QR,
  consistent with `VideoCache::hostFor`.
- No slide is empty of both audience content and notes.

### 2. Fixture decks for the seams
Small decks under `tests/integration/` that pin down behaviour a single-unit
test cannot:
- A deck where the same filename exists twice under `root:` at different
  depths, asserting the shortest path wins end to end.
- A deck whose `root:` does not exist, asserting graceful degradation rather
  than a crash.
- A deck that is nothing but frontmatter — no slides at all.
- A deck with no frontmatter and no separators — one long scrolling slide.
- A deck using CRLF line endings throughout.
- A deck whose final line is a separator with no trailing newline.
- A deck with a UTF-8 BOM.
- A deck containing an unclosed code fence, asserting the parser terminates and
  does not treat the rest of the file as code forever.

Each of these is a real file someone will eventually open. The last three in
particular are the ones that turn into bug reports.

### 3. C++ and renderer must agree
Where both sides classify the same thing — what is a bare URL line, what counts
as an image reference — write the assertion on both sides against the same
fixture and make sure they give the same answer. If they disagree, that is a
genuine finding: log it under `NEEDS:` with both answers.

## Tests
`tests/tst_integration.cpp`, registered with `OMAPRESENT_TEST_SUITE` — no
`QTEST_MAIN`. No network. Keep it fast; these run on every commit by every
agent.

## Done when
`./bin/build && ./bin/test` pass, the suite genuinely exercises the seams, and
your worklog entry is appended — including any disagreement you found between
two agents' components, which is the most valuable thing this task can produce.
