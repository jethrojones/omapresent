# Review 1 — `renderer` (T2)

I ran the real end-to-end path for the first time: `omapresent export --pdf`
over `welcome/welcome.md`, which drives `DeckModel` → `AssetIndex` →
`OmarchyTheme` → your renderer in `pdf` mode → `QWebEnginePage::printToPdf`.

**It works.** A 499 KB PDF, 25 slides, real content, the live `gold-rush` theme,
header and footer with `{title}`/`{slide}`/`{count}` tokens resolved, slide
numbers, and the progress bar. The title slide is genuinely handsome. Tables
render full-width with themed borders, and block quotes get the accent bar.
That is the whole architectural bet in §3 paying off.

Two defects, both visible only when you look at the output.

## 1. Lists are laid out in a ~150px column (the important one)

A bulleted list on a 16:9 slide wraps after two or three words, in a narrow
strip in the middle of the canvas, with most of the slide empty on both sides:

```
        •  Headings,
           lists, code,
           tables,
           quotes,
           math, and
           media →
```

Headings, tables and block quotes on the same deck are all correctly
full-width, so this is specific to `<ul>` / `<ol>` — most likely the list or its
fragment-reveal wrapper is being sized to `min-content` / `fit-content`, or is
inside a flex or grid container that is not letting it take the content width.

This is not cosmetic. It is the single most common slide element there is, it
looks broken, and it cascades: the narrow column makes every list slide many
times taller than it should be, which is why the 25-slide deck paginated into
**61 PDF pages**. Fix the width and the page count should fall a long way.

Reproduce it exactly as I did:

```sh
cp welcome/welcome.md /tmp/e2e.md
QT_QPA_PLATFORM=offscreen ./build/omapresent export --pdf /tmp/e2e.md
pdftoppm -png -r 60 -f 2 -l 3 /tmp/e2e.pdf /tmp/page
```

Then look at `/tmp/page-02.png`. Please look at the rendered image rather than
only the DOM — this is a class of bug the unit tests cannot see, and your 23
tests are all green while it is happening.

## 2. Continuation pages leave the footer stranded mid-page

On a slide that paginates, the last page puts the footer partway down with a
large empty band beneath it, instead of at the bottom of the page. See page 7 of
the same PDF. The footer should sit at the page bottom on every page, or appear
only on the slide's final page — either is defensible, but a footer floating in
the middle of empty space is not.

## Then

Re-run `./bin/build && ./bin/test`, re-export the PDF, and check the pages
visually again before you call it done. Adding a test that asserts a list's
rendered width is close to the content width would stop this recurring.
