# T1 — Document model

**Agent:** `doc-model` · **Spec:** §4.1, §4.3, §4.4, §15 milestone 2

## Files you own
- `src/deckmodel.cpp` (implement; `src/deckmodel.h` is frozen — you may add)
- `tests/tst_deckmodel.cpp`

Nothing else. `src/deckmodel.h` may gain members but keep every existing
declaration exactly as it is.

## What to build

`DeckModel` is the single authority for turning a raw `.md` file into
frontmatter plus an ordered list of slides. It does *not* decide audience-vs-
notes — that is the renderer's job (`docs/renderer-contract.md` §3).

### Separators (§4.1) — get this exactly right
A slide break is a line containing **exactly** `---` with a **blank line both
before and after it**. Everything else is not a break, and each of these is a
real case the tests must cover:
- `---` immediately under text — that is a Setext heading, not a break.
- `---` with no blank line after it.
- `***`, `___`, `- - -`, `----` — never breaks.
- `---` inside a fenced code block — never a break. You must track fences
  (``` and ~~~, any length ≥ 3, with info strings) while scanning.
- A file with no separators is one slide.
- Leading/trailing whitespace on the separator line is allowed; anything else
  on the line is only allowed as a `{...}` recall tag.

### Frontmatter (§4.4)
Only when the **very first line** of the file is `---`. Everything up to the
next `---` is YAML for the whole file. That closing `---` is not a slide break
and the first slide starts after it. There is no per-slide frontmatter.

`parseFrontmatter` handles the subset actually used: `key: value` scalars,
single- and double-quoted strings, `true`/`false`, and one level of nesting for
`publish:`. Unknown keys survive as strings. Malformed YAML never throws — it
yields what it could read and drops the rest.

### Comments (§4.3)
- A line whose first non-whitespace is `//` disappears.
- `%%...%%` spans, possibly multi-line, disappear.
- `<!-- ... -->` spans, possibly multi-line, disappear.
- **But not inside fenced code blocks** — a `// comment` in a C++ example on a
  slide is content, not a comment. Same for `<!-- -->` in an HTML example.
- `// ---` on a separator line drops the **entire following slide** from the
  deck. That slide must not appear in `slides()` at all, and must not shift the
  recall bindings of the slides around it.

### Recall tags (§4.9)
`--- {q}` binds the slide that follows to `q`. `--- {q, skip}` also removes it
from the linear flow (`skipInFlow = true`, and its `index` in `toJson()` is
`-1`). Keys are a single letter or digit; whitespace inside the braces is
insignificant. Cap at 8 bindings per deck — a 9th is ignored and logged with
`qWarning`.

### Line numbers
`sourceStartLine` / `sourceEndLine` are 0-based into the **original** file,
before comment stripping. The editor uses them to sync cursor to slide, so they
must survive comment removal accurately. `slideIndexForLine` returns the slide
containing a line, or -1.

### `toJson()`
Exactly the `frontmatter` + `slides` half of the object in
`docs/renderer-contract.md` §1. Do not invent keys.

## Tests
`tests/tst_deckmodel.cpp`, registered with `OMAPRESENT_TEST_SUITE` (see
`tests/testrunner.h`) — no `QTEST_MAIN`. Cover every bullet above, each as its
own named slot or a `_data()` row. Include a realistic multi-slide document
fixture that exercises frontmatter + comments + a dropped slide + recall tags
together, and assert the exact resulting slide count and line ranges.

## Done when
`./bin/build && ./bin/test` pass, your suite has real cases (no `QSKIP`), and
your worklog entry is appended.
