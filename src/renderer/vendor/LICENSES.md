# Vendored third-party assets

Committed here so the renderer works offline: spec §3 allows no network at
runtime, and rebuilding the app must never require the network either. Each
file is the upstream distribution build, unmodified.

A **published web bundle** carries these same bytes embedded as text inside
`assets/render.js`, with these licence files copied verbatim beside it. The
embedding is necessary because Chromium refuses to load an ES module over
`file://` (the origin is `null`), and a published deck has to open by
double-clicking `index.html`. The libraries' own bytes are unchanged — only the
renderer's relative import specifiers are rewritten, and the vendored libraries
have none of their own.

All three are MIT licensed, as is Omapresent.

| Library | Version | File(s) | Licence |
| --- | --- | --- | --- |
| [markdown-it](https://github.com/markdown-it/markdown-it) | 15.0.1 | `markdown-it.mjs` (browser ESM build) | MIT |
| [KaTeX](https://katex.org) | 0.18.4 | `katex.mjs`, `katex.css`, `fonts/*.woff2` | MIT |
| [qrcode-generator](https://github.com/kazuhikoarase/qrcode-generator) | 2.0.4 | `qrcode.mjs` | MIT |

## Notes

- **markdown-it** is the `dist/browser/markdown-it.esm.min.mjs` build, which has
  its dependencies (mdurl, uc.micro, entities, linkify-it, punycode.js) already
  bundled in. The plain `dist/markdown-it.mjs` imports those by bare specifier
  and therefore cannot load in a browser or from this directory.
- **KaTeX fonts** are the `.woff2` files only — 20 of them, every face
  `katex.css` references. The `.ttf` and `.woff` variants are omitted;
  QtWebEngine is Chromium and reads woff2.
- No markdown-it plugins are vendored. Spec §4.2's element list needs none:
  headings, outlines, lists, code, tables, quotes, math, images and media are
  either core CommonMark, the renderer's own layout grammar, or KaTeX.

## Refreshing

```sh
npm install markdown-it katex qrcode-generator
cp node_modules/markdown-it/dist/browser/markdown-it.esm.min.mjs vendor/markdown-it.mjs
cp node_modules/katex/dist/katex.mjs vendor/katex.mjs
cp node_modules/katex/dist/katex.min.css vendor/katex.css
cp node_modules/katex/dist/fonts/*.woff2 vendor/fonts/
cp node_modules/qrcode-generator/qrcode.mjs vendor/qrcode.mjs
```

Then re-run `node --test tests/renderer/` and update the versions above.
