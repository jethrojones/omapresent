# Bug — the long read uses projector typography in an article column

**Owners:** `publish` (inherited `src/webbundle.cpp`, which generates
`assets/bundle.css`) and `renderer` (`src/renderer/deck.css`, which carries the
`html[data-op-view="read"]` rules). The fix probably needs both, so agree who
does which half before editing.

## What I did

Built a bundle from `welcome/welcome.md`, copied the whole directory to an
unrelated path, and rendered `read/index.html` from `file://` in headless
Chromium — no server.

**The good news first, and it is substantial:** the bundle is genuinely
self-contained. 33 files, zero `file:///`, zero `qrc:`, zero `/home/jethro`
anywhere in the HTML, CSS or JS. It renders correctly from a directory it was
never built in. The deck view (`index.html`) is excellent — theme baked in,
header, footer with `{title}`/`{slide}`/`{count}` resolved, `1 / 25` counter,
prev/next chrome, a Notes toggle and a "Read as article →" cross-link. That is
spec §9.1 delivered properly.

## The defect

`read/index.html` renders the article's own header well — the title, the
`Jethro Jones · 2026-08-27` byline and a rule, all correctly sized. Then the
body content below it keeps **slide** typography: the word "Omapresent" is set
at display size inside a ~600px article column, so it overflows the column and
is visibly clipped mid-word at the right edge, and "Presentations from plain
Markdown" wraps to one or two words per line.

Spec §9.2 asks for "a single scrolling page: headings, media, lists and notes
flowed together as a **well-set article** in the deck's theme." What is there is
slide-sized type poured into an article measure, which is neither a slide nor an
article.

## What it needs

The read view should re-set the type for reading rather than for a projector:
heading scale appropriate to a ~65-character measure, body text at a readable
size, and — the part that makes it worth having — **speaker-note prose promoted
to body text**, flowed inline with the headings and media rather than hidden.
That is the whole point of the long read as against the deck view, and it is
what §9.2 means by "flowed together".

Note that the slide typography is presumably driven by viewport units or a slide
scale factor, which is correct for `deck`/`present`/`pdf` and wrong for `read`.
Whatever sets it needs a read-mode branch rather than a size override piled on
top.

## Reproduce

```sh
# build a bundle (or reuse the one the webbundle agent left), then:
cp -r <bundle> /tmp/moved-deck
chromium --headless --disable-gpu --no-sandbox --virtual-time-budget=8000 \
  --screenshot=/tmp/read.png --window-size=1280,900 \
  "file:///tmp/moved-deck/read/index.html"
```

Then look at `/tmp/read.png`. Check the deck view the same way while you are
there, so a fix to one does not regress the other.
