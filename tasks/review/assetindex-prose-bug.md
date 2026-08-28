# Bug — `AssetIndex::looksLikeImageReference` treats prose as an image path

**New owner:** the `present` agent. You are inheriting `src/assetindex.cpp` and
`tests/tst_assetindex.cpp` from the `assets` agent, whose provider ran out of
quota. Its work is committed and is good — 23 tests, correct resolution order,
off-thread indexing. This is one real bug in it.

## The bug

The integration suite caught it, and I confirmed it directly. Every one of these
whole lines is classified as an image reference:

```
IMAGE  Omapresent supports line comments with `//`, Obsidian comments with %%…%%
IMAGE  Recognised hosts: YouTube, Vimeo, Loom, Descript, TikTok, X/Twitter, …
IMAGE  $$e^{i\pi} + 1 = 0$$
IMAGE  The ratio is 16:9 and the file lives in ~/Documents/aibrain somewhere
IMAGE  and/or
```

They are all ordinary English prose — which, per spec §4.2, is a speaker note.
The cause is that `looksLikeImageReference` accepts any line containing a `/`.
`and/or`, `X/Twitter` and a LaTeX formula all contain one.

## Why it matters

Per §4.5 step 5, a reference that does not resolve renders the theme's desktop
background as the slide image with a `missing: …` tag. So a speaker note that
happens to say "and/or" tries to become a full-slide image and shows a
missing-image placeholder. This fires on real decks — it fires on our own
`welcome/welcome.md` today.

## The fix

Spec §4.5 says: *"Bare paths must contain a `/` **or** end in a known image
extension to be treated as an image (so a lone word is not misread)."* Read
literally that is what produced this. The intent is clearly narrower: the line
has to look like a **path**, not merely contain a slash somewhere.

Tighten it so a bare line counts only when the whole line is plausibly one path.
A workable rule:

- it ends in a known image extension (this alone is a strong signal), **or**
- it is a single path-like token containing `/` with no sentence punctuation.

Whatever you choose, these must keep working — spec §4.5 is explicit that paths
with spaces need no escaping:

```
~/Pictures/budget.png
./img/chart with spaces.png
/abs/path/photo.jpeg
budget.png
logo.svg
```

and these must be prose:

```
and/or
budget
The ratio is 16:9 and the file lives in ~/Documents/aibrain somewhere
Recognised hosts: YouTube, Vimeo, X/Twitter, Instagram
$$e^{i\pi} + 1 = 0$$
Omapresent supports line comments with `//`
```

Note the tension: `./img/chart with spaces.png` has spaces and must pass, while
the prose lines have spaces and must fail. The image extension is what separates
them — lean on it.

## Tests

Add every line above to `tests/tst_assetindex.cpp` as an explicit table, both
the positives and the negatives. The negatives are the point; they are what
regressed here. Then re-run `./bin/build && ./bin/test` — the integration
suite's `welcomeImagesResolveOrAreDeliberatelyMissing` should go green too, and
if it does not, read what it reports before changing it, because it is not your
file.
