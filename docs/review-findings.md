# Adversarial review findings

Review date: 2026-08-27.

Scope: publish, file-copy, cache, state, network, packaging, and hostile-input paths from spec sections 4.5, 4.8, 9, 11, and 12.

## SEC-001 — High — An asset symlink can disclose a file outside the asset root

`AssetIndex` indexes a readable symlink as a normal file. It returns the symlink path for a matching reference. `WebBundle` then opens that path and copies the target bytes into `media/`.

Concrete reproduction:

1. Make an asset root with `cover.png` as a symlink to a readable file outside that root. The test uses an `id_rsa` fixture.
2. Put `![[cover.png]]` in the deck.
3. Resolve the deck and build its web bundle.
4. Open the generated `media/*` file. It contains the target file bytes.

The path starts in `src/assetindex.cpp` in the recursive scan and `resolve()`. The copy occurs in `src/webbundle.cpp` in `collectMedia()` and `copyFile()`.

This can disclose a private key or another readable file when a user publishes an untrusted deck tree. The user must still confirm publish. The confirmation says that embedded files will upload. The problem is that the visible asset name can hide a different target.

`SecurityTest::indexedAssetSymlinkCannotLeaveTheAssetRoot` pins this as an expected failure.

## SEC-002 — Medium — Opening a deck can make an unapproved network request

The C++ network paths are explicit. The renderer is not. It creates an eager remote `<iframe>` for a hosted video. It also creates a `<video preload="metadata">` element for a direct video URL.

Concrete reproduction:

1. Start an HTTP server that logs requests on `127.0.0.1:8123`.
2. Make the first slide contain only `http://127.0.0.1:8123/clip.mp4`.
3. Open the deck in the editor. Do not use **Prepare for offline**.
4. The preview creates the video element and the server receives a request for `clip.mp4`.

`src/renderer/render.js` assigns the remote URL to `video.src` and sets `preload` to `metadata`. The same file assigns hosted embed URLs to eager iframes. `src/PreviewPane.qml` runs the preview render as soon as the local renderer page loads.

This breaks the frozen no-network contract in `docs/renderer-contract.md`. A deck author can cause a request to a chosen host when the deck opens. This leaks normal request metadata. A loopback URL can also probe a local HTTP service. This does not upload the deck contents.

I did not add a C++ expected-failure test. The reproduction needs a live Qt WebEngine request observer. T14 owns only the C++ security suite.

## SEC-003 — Medium — A cache-directory symlink redirects writes outside the deck

`VideoCache::cacheDir()` joins the deck directory with `.omapresent-cache`. Its writers do not reject a symlink at that path.

Concrete reproduction:

1. Make `deck/.omapresent-cache` a symlink to another writable directory.
2. Put `clip.mp4` in the deck directory.
3. Call `prefetch({"clip.mp4"})`.
4. The target directory receives `index.json` and the hashed video copy.

The path handling is in `src/videocache.cpp` in `cacheDir()`, `writeIndex()`, and the local-file branch of `prefetchNext()`.

This needs an explicit offline-prepare action. It can overwrite an existing `index.json` in the symlink target. Other output names are SHA-256 cache names. The impact is limited but real.

`SecurityTest::videoCacheSymlinkCannotLeaveTheDeckDirectory` pins this as an expected failure.

## SEC-004 — Medium — Bundle media copies use memory equal to the whole file

`WebBundle::copyFile()` calls `source.readAll()` and passes the full byte array to `writeFile()`. A large local video or image therefore needs memory for the full source and output buffers during publish.

Concrete reproduction:

1. Create a large local video fixture: `truncate -s 2G clip.mp4`.
2. Put `clip.mp4` alone on a slide.
3. Run publish under a 512 MiB memory limit.
4. Bundle creation exhausts the limit before the publisher can upload the snapshot.

The issue is in `src/webbundle.cpp` in `copyFile()`. The later publisher snapshot and hash path streams from `QFile`; the early bundle copy does not.

This is an availability issue. A normal large video can trigger it. An untrusted deck can also hide a large local target behind an asset reference. I did not run the 2 GiB reproduction because it is intentionally resource-exhausting.

## SEC-005 — Low — A pre-existing output symlink can redirect bundle writes

`WebBundle::ensureDirectory()` accepts an existing path when `QFileInfo(path).isDir()` is true. That test follows symlinks. `writeFile()` then opens the joined path without a canonical-root check.

Concrete reproduction:

1. Make an output directory.
2. Make `output/assets` a symlink to another directory.
3. Put a sentinel `render.js` in the target directory.
4. Call `WebBundle::build(output)`.
5. The sentinel is overwritten by the generated renderer.

The issue is in `src/webbundle.cpp` in `ensureDirectory()` and `writeFile()`.

The current GUI and CLI publish path uses a new `QTemporaryDir`, so an attacker cannot normally pre-place this symlink there. This is a defense-in-depth defect in the public build API. It becomes more serious if a future export path builds into a user-selected existing directory.

`SecurityTest::bundleOutputSymlinkCannotLeaveTheOutputRoot` pins this as an expected failure.

## Checks that passed

- The command provider runs the configured command through `/bin/sh -c`. This is the feature. Deck-controlled values do not enter that command string. `Publisher` slugifies the slug and passes both the slug and bundle path through environment variables. The security test uses shell metacharacters and confirms that they stay data.
- The publisher upload snapshot skips symlink files. It uploads only the stable snapshot, not changing source files.
- No save, file-watch, cache-read, or first-run C++ path calls `VideoCache::prefetch()` or `Publisher::publish()`.
- The GUI opens a confirmation dialog before `publishDeck()`. The CLI asks on standard input unless `--yes` or `-y` is present. The shipped agent skill also requires explicit user confirmation.
- Session state has one fixed file path. Deck paths are JSON object keys, not path segments. JSON escapes newlines. The file keeps at most 200 entries and does not delete deck files.
- Cache filenames derived from URLs are SHA-256 values. Cached index filenames that contain `/`, `\\`, `.` or `..` are rejected.
- The new parser test completed with one 8 MiB line, 10,000 slides, an unclosed frontmatter opener, and an unclosed code fence. Existing integration tests cover BOM, CRLF, a frontmatter-only file, and an unclosed fence. This is not proof that every 50 MiB input has an acceptable memory peak.
- The package hook only refreshes desktop caches. First run links the packaged skill into existing user agent skill directories. It leaves an existing file or a different symlink unchanged.

---
Created by Codex GPT-5.6 Sol on 2026-08-27 21:29 PT on ombee.
