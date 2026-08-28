# T5 — Video, embeds and the offline cache

**Agent:** `media` · **Spec:** §4.8, §14.3, §14.4, §15 milestone 5

## Files you own
- `src/videocache.cpp` (implement; `src/videocache.h` is frozen — may add)
- `tests/tst_videocache.cpp`

Nothing else. The renderer draws the players and QR codes; you only resolve and
cache. Keep your host list identical to the renderer's `src/renderer/media.js`
— if they drift, log it under `NEEDS:`.

## What to build

### Host recognition
Recognised at launch: **YouTube, Vimeo, Loom, Descript, TikTok, X/Twitter,
Instagram, Facebook**, plus direct `.mp4`/`.webm`/`.mov` URLs and local video
files. **There is no generic `yt-dlp` fallback** — an unrecognised host is not a
video, it is a QR code (`Host::NotAVideo`).

`hostFor` must handle the real URL shapes: `youtu.be/ID`,
`youtube.com/watch?v=ID`, `/shorts/ID`, `/embed/ID`, with extra query params;
`vimeo.com/ID` and `player.vimeo.com/video/ID`; `loom.com/share/ID`;
`x.com` and `twitter.com`; `instagram.com/p/` and `/reel/`; trailing slashes,
`www.`, `http` vs `https`, and tracking parameters. Anything that is not a URL
at all is `NotAVideo`.

`isBareUrlLine` is true only when the **whole line**, trimmed, is a single URL —
a URL inside a sentence is prose (a speaker note), not media.

### `describe(url)`
Returns the object in `docs/renderer-contract.md` §1 under `media`. It must
**never block on the network**: answer from the cache on disk if it is there,
otherwise return `status: "embed"` immediately. `status` is `"cached"` when a
playable local file exists, `"embed"` when we have an embed URL but no file, and
`"qr"` when neither works.

### `prefetch(urls)` — "Prepare for offline" (§4.8)
Resolve each URL through the host's oEmbed endpoint, then download the
underlying media into `<deck-dir>/.omapresent-cache/` **where the host allows
it**. Async, with `prefetchProgress(done, total)` and
`prefetchFinished(failed)`. Private, age-restricted, DRM'd and geo-blocked
videos simply cannot be fetched: degrade to the live embed, then to a QR code,
and report them once in `failed` so the app can warn at save.

Treat TikTok / X / Instagram / Facebook as **best effort** (§14.3) — they fight
embedding and change often. Embed when it works, QR when it does not. No
scraping, no `yt-dlp`.

Cache files are named by a stable hash of the URL so a re-prefetch is a no-op.
Write a small `index.json` in the cache dir mapping URL → file, poster, title,
dimensions and fetch time, so `describe()` is a cheap local lookup. Never delete
anything outside the cache directory.

## Tests
`tests/tst_videocache.cpp`, registered with `OMAPRESENT_TEST_SUITE` — no
`QTEST_MAIN`. **No test may touch the network.** Cover `hostFor` against a
table of real URL shapes per host plus negatives (a bare word, a mailto:, an
unrecognised host, a URL with `youtube` in the path of another domain),
`isBareUrlLine`, `extractUrls` over a slide mixing a bare URL line with a URL
inside a prose paragraph and one inside a code fence, `embedUrlFor` per host,
and `describe()` against a `QTemporaryDir` cache you populate by hand —
asserting the `cached`, `embed` and `qr` branches.

## Done when
`./bin/build && ./bin/test` pass, your suite has real cases and no network
access, and your worklog entry is appended.
