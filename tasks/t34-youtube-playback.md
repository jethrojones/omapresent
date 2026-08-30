# T34 — YouTube playback, and the prefetch the manual promises

**Agent:** `webbundle` · **Spec:** §4.8 · **Findings:** `docs/review-findings.md` SEC-002

## Why this exists

A YouTube URL alone on a line renders a themed "Play video" affordance. Clicking
it produced **"Error 153: Video player configuration error."** and no video.

Diagnosed by experiment, not by reading. `file://` and `qrc:` are opaque origins
that send no `Referer`, and YouTube's player refuses to configure without one.
The same iframe markup, from the same browser, in the same session:

| embed URL | from `file://` | from `http://127.0.0.1` |
| --- | --- | --- |
| `?enablejsapi=1` | Error 153 | plays |
| `?autoplay=1`, no jsapi | Error 153 | plays |
| `youtube-nocookie.com` | Error 153 | plays |

So `enablejsapi`, the referrer policy, the cookie domain and CSP are all ruled
out — the origin is the whole cause. The server answers every variant with 200;
153 is decided inside the player at runtime.

The fix follows from one further measurement: a `file://` page hosting a
**nested** frame that is itself served over loopback HTTP gets a working player.
So the deck page never has to leave `qrc:`. Only a small embed shim moves.

That matters. Moving the whole renderer page to an HTTP origin would have broken
every local image and cached video in the app, because an HTTP page cannot load
`file:///` subresources — and it would have forced the loopback server to serve
user files. With a nested shim the server serves exactly one compiled-in
resource and never touches the filesystem at all.

Two further defects came out of the same investigation:

- `presentation.auto_prefetch_video` defaulted to `false`, so a save never
  prefetched, while `welcome/welcome.md` told the reader that it did.
- The demo video `dQw4w9WgXcQ` is not embeddable **anywhere** — it shows "This
  video is unavailable" even from a working HTTP origin.

## What to build

1. `src/embedserver.{h,cpp}` — a `QTcpServer` on `QHostAddress::LocalHost` and
   an ephemeral port, started on first use and serving exactly one GET route,
   `/<token>/embed.html`, out of `:/renderer/embed.html`. Everything else is
   refused: another path, a missing or wrong token, a non-GET method, a `Host`
   header that is not the loopback authority, traversal in any encoding, an
   oversized request. It reads through `QFile(":/renderer/…")` only, so there is
   no filesystem path for traversal to reach even in principle.
2. `src/renderer/embed.html` — the shim. Builds the YouTube iframe from the
   `v` parameter, relays play/pause down to it, and relays player errors up.
3. `RenderHost::embedBase()` — `Q_INVOKABLE`, not a property: the WebChannel
   serialises properties at handshake, and a method is only read when the
   renderer asks. So the socket opens on the first click, not on deck open.
4. `render.js` — at activation, a YouTube embed goes through the shim when the
   bridge offers one, stays a direct embed when the page is already on an HTTP
   origin, and falls back to QR + open-in-browser when neither holds. Player
   errors 101, 150 and 153 fall back the same way. The deferred affordance
   shows the cached poster when the video cache has one.
5. `settings.cpp` — `auto_prefetch_video` defaults to `true`. The gate stays
   where it is, inside the explicit save path, so recovery never prefetches.
6. `welcome/welcome.md` — the verified CC video `aqz-KE-bpKQ` (Big Buck Bunny,
   Blender Foundation), and the two claims that were not true.

## What must not regress

**SEC-002 — zero external requests when a deck opens.** The whole deferred-media
design exists to keep them out, and this task adds a network path. The existing
renderer guard stays, and the loopback server gets its own: no listening socket
until something asks for the base.

The deck JSON shape is frozen. The shim base travels over the existing bridge,
not as a new key.

## Tests

`tests/tst_embedserver.cpp` — loopback binding, ephemeral port, token, method,
`Host`, traversal in three encodings, oversized request, every other resource
refused, and that no socket listens until asked.
Renderer tests — origin construction and the fallback events, deterministic and
offline. Real 153 verification is opt-in behind an environment variable, because
only a real request to YouTube can prove it.

## Done when

`./bin/build && ./bin/test` pass, a YouTube video plays in the app after a
click, a deck open still makes zero external requests, and the worklog entry is
appended.
