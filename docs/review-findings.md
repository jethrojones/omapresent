# Adversarial review findings

Review dates: 2026-08-27 to 2026-08-28.

Scope: publish, file-copy, cache, state, network, packaging, and hostile-input paths from spec sections 4.5, 4.8, 9, 11, and 12.

Outcome: All eight findings are fixed. All expected-failure pins are now normal regression assertions. The owner suites cover the fixes that did not fit the C++ security suite.

## SEC-001 — High — Asset symlink disclosure outside the asset root

**Status: Fixed in `7336d1d`.**

At review time, `AssetIndex` indexed a readable symlink as a normal file. It returned the symlink path for a matching reference. `WebBundle` then opened that path and copied the target bytes into `media/`.

Original reproduction:

1. Make an asset root with `cover.png` as a symlink to a readable file outside that root. The test uses an `id_rsa` fixture.
2. Put `![[cover.png]]` in the deck.
3. Resolve the deck and build its web bundle.
4. Open the generated `media/*` file. It contains the target file bytes.

The path starts in `src/assetindex.cpp` in the recursive scan and `resolve()`. The copy occurs in `src/webbundle.cpp` in `collectMedia()` and `copyFile()`.

This could disclose a private key or another readable file when a user published an untrusted deck tree. The user still had to confirm publish. The confirmation said that embedded files would upload. The problem was that the visible asset name could hide a different target.

`SecurityTest::indexedAssetSymlinkCannotLeaveTheAssetRoot` now guards the fixed behavior with a normal assertion.

## SEC-002 — Medium — Unapproved network request on deck open

**Status: Fixed in `aa4331a`.**

The C++ network paths were explicit. The renderer was not. It created an eager remote `<iframe>` for a hosted video. It also created a `<video preload="metadata">` element for a direct video URL.

Original reproduction:

1. Start an HTTP server that logs requests on `127.0.0.1:8123`.
2. Make the first slide contain only `http://127.0.0.1:8123/clip.mp4`.
3. Open the deck in the editor. Do not use **Prepare for offline**.
4. The preview creates the video element and the server receives a request for `clip.mp4`.

`src/renderer/render.js` assigns the remote URL to `video.src` and sets `preload` to `metadata`. The same file assigns hosted embed URLs to eager iframes. `src/PreviewPane.qml` runs the preview render as soon as the local renderer page loads.

This broke the frozen no-network contract in `docs/renderer-contract.md`. A deck author could cause a request to a chosen host when the deck opened. This leaked normal request metadata. A loopback URL could also probe a local HTTP service. This did not upload the deck contents.

The renderer now waits for a play action before it assigns a remote video or iframe source. A renderer regression test guards both forms. A real Qt open-deck check made zero HTTP requests before play.

## SEC-003 — Medium — Cache-directory symlink redirected writes outside the deck

**Status: Fixed in `83ba985`.**

At review time, `VideoCache::cacheDir()` joined the deck directory with `.omapresent-cache`. Its writers did not reject a symlink at that path.

Original reproduction:

1. Make `deck/.omapresent-cache` a symlink to another writable directory.
2. Put `clip.mp4` in the deck directory.
3. Call `prefetch({"clip.mp4"})`.
4. The target directory receives `index.json` and the hashed video copy.

The path handling is in `src/videocache.cpp` in `cacheDir()`, `writeIndex()`, and the local-file branch of `prefetchNext()`.

This needed an explicit offline-prepare action. It could overwrite an existing `index.json` in the symlink target. Other output names were SHA-256 cache names. The impact was limited but real.

`SecurityTest::videoCacheSymlinkCannotLeaveTheDeckDirectory` now guards the fixed behavior with a normal assertion.

## SEC-004 — Medium — Whole-file bundle copies could exhaust memory

**Status: Fixed in `83eafde`.**

At review time, `WebBundle::copyFile()` called `source.readAll()` and passed the full byte array to `writeFile()`. A large local video or image therefore needed memory for the full source and output buffers during publish.

Original reproduction:

1. Create a large local video fixture: `truncate -s 2G clip.mp4`.
2. Put `clip.mp4` alone on a slide.
3. Run publish under a 512 MiB memory limit.
4. Bundle creation exhausts the limit before the publisher can upload the snapshot.

The issue is in `src/webbundle.cpp` in `copyFile()`. The later publisher snapshot and hash path streams from `QFile`; the early bundle copy does not.

This was an availability issue. A normal large video could trigger it. An untrusted deck could also hide a large local target behind an asset reference. I did not run the 2 GiB reproduction because it is intentionally resource-exhausting.

`WebBundle` now copies through a fixed 1 MiB buffer. `WebBundleTest::streamsLargeMediaWithoutChangingBytes` guards the streaming path with a sparse multi-chunk fixture.

## SEC-005 — Low — Output symlink redirected bundle writes

**Status: Fixed in `83eafde`.**

At review time, `WebBundle::ensureDirectory()` accepted an existing path when `QFileInfo(path).isDir()` was true. That test followed symlinks. `writeFile()` then opened the joined path without a canonical-root check.

Original reproduction:

1. Make an output directory.
2. Make `output/assets` a symlink to another directory.
3. Put a sentinel `render.js` in the target directory.
4. Call `WebBundle::build(output)`.
5. The sentinel is overwritten by the generated renderer.

The issue is in `src/webbundle.cpp` in `ensureDirectory()` and `writeFile()`.

The GUI and CLI publish path used a new `QTemporaryDir`, so an attacker could not normally pre-place this symlink there. This was a defense-in-depth defect in the public build API. It would have become more serious if a future export path built into a user-selected existing directory.

`SecurityTest::bundleOutputSymlinkCannotLeaveTheOutputRoot` now guards the fixed behavior with a normal assertion.

## Second-pass findings — 2026-08-28

### SEC-006 — Medium — Failed CLI input could publish an empty deck

**Status: Fixed in `3e54048`.**

At review time, `Backend::runCommand()` checked only whether the input path existed. It called `open()`, but it did not check whether `open()` loaded the file. A directory or an unreadable file therefore reached `publishDeck()` with the new backend's empty document.

Original reproduction:

1. Configure a command provider that prints a valid URL.
2. Run `omapresent publish <directory> --provider test --yes`.
3. Repeat with an existing file that the current user cannot read.
4. In both cases, the provider runs, the CLI prints its URL, and the process exits 0.

The path is in `src/backend.cpp` in `open()` and `runCommand()`. The failed `open()` sets a status message and returns. `runCommand()` ignores that result and continues.

The user still had to confirm or pass `--yes`. The command could publish an empty deck instead of the named file and report success. It did not disclose the unreadable file because that file was never read.

`Backend::openCommandFile()` now reports whether the complete file read succeeded. Export, present, and publish stop with a nonzero exit before they use a failed load. The error names the requested path and says whether it is missing, a directory, not a regular file, unreadable, or failed during the read.

`SecurityTest::cliRejectsADirectoryBeforePublishing` and `SecurityTest::cliRejectsAnUnreadableFileBeforePublishing` now guard both fixed paths with normal assertions.

### SEC-007 — Low — First run followed a symlinked agent skills directory

**Status: Fixed in `3e54048`.**

At review time, `Backend::agentSkillDirectories()` accepted a path when `QFileInfo(path).isDir()` was true. That check followed a directory symlink. `installAgentSkill()` then created the `omapresent` link through that path.

Original reproduction:

1. Make `~/.claude/skills` a symlink to another writable directory.
2. Start Omapresent.
3. The target directory receives an `omapresent` symlink.

The issue is in `src/backend.cpp` in `agentSkillDirectories()` and `installAgentSkill()`.

This created one known symlink in a directory that the user could already write. It did not replace an existing file, directory, or different symlink. The main risk was a surprising write outside the expected agent tree.

First-run discovery now rejects a symlinked skills directory. It also checks that the canonical skills directory or its creation parent stays inside the canonical home directory. The installer checks the directory again before it creates the link.

`SecurityTest::firstRunRefusesASymlinkedSkillDirectory` now guards the fixed behavior with a normal assertion.

### SEC-008 — Low — A duplicate settings key made a patch report false success

**Status: Fixed in `3e54048`.**

At review time, `Publisher::patchToml()` replaced the first matching key in the first matching table. `Publisher::parseToml()` kept the later value when a malformed file repeated the same table and key. `Settings::setValue()` could therefore return true, but a reload still read the old later value.

Original reproduction:

1. Put `theme = "first"` in one `[editor]` table.
2. Repeat `[editor]` and put `theme = "second"` in it.
3. Call `Settings::setValue("editor.theme", "patched")`.
4. The call returns true, but `Settings::stringValue("editor.theme")` is still `second`.

The patch preserved both unknown keys in the test. It did not make the malformed file less readable. The failure was that the requested change did not take effect even though the API reported success.

`Publisher::patchToml()` now scans all matching assignments and changes the last one. This matches the value that `parseToml()` uses. It still leaves unknown keys and the shadowed earlier assignment unchanged.

`SecurityTest::settingsPatchAppliesWithADuplicateKnownKey` now guards the fixed behavior with a normal assertion.

## Checks that passed

- The command provider runs the configured command through `/bin/sh -c`. This is the feature. Deck-controlled values do not enter that command string. `Publisher` slugifies the slug and passes both the slug and bundle path through environment variables. The security test uses shell metacharacters and confirms that they stay data.
- The publisher upload snapshot skips symlink files. It uploads only the stable snapshot, not changing source files.
- No save, file-watch, cache-read, or first-run C++ path calls `VideoCache::prefetch()` or `Publisher::publish()`.
- The GUI opens a confirmation dialog before `publishDeck()`. The CLI asks on standard input unless `--yes` or `-y` is present. The shipped agent skill also requires explicit user confirmation.
- Session state has one fixed file path. Deck paths are JSON object keys, not path segments. JSON escapes newlines. The file keeps at most 200 entries and does not delete deck files.
- Cache filenames derived from URLs are SHA-256 values. Cached index filenames that contain `/`, `\\`, `.` or `..` are rejected.
- The new parser test completed with one 8 MiB line, 10,000 slides, an unclosed frontmatter opener, and an unclosed code fence. Existing integration tests cover BOM, CRLF, a frontmatter-only file, and an unclosed fence. This is not proof that every 50 MiB input has an acceptable memory peak.
- The package hook only refreshes desktop caches. First run links the packaged skill into existing user agent skill directories. It leaves an existing file or a different symlink unchanged.

## Second-pass checks that passed

- The CLI rejects a missing file with its full path and a nonzero exit code.
- The CLI rejects an unknown `--provider` with the provider name and a clear `is not configured` error.
- A deck with no frontmatter publishes through a local command provider and exits 0.
- Without `--yes` or `-y`, a negative answer stops the CLI before the provider runs. The test uses a marker file to prove that the command did not start.
- First run leaves a real `omapresent` skill directory and its content unchanged.
- A one-key settings patch preserved a 256 KiB unknown key, an unterminated unknown string, two `[editor]` tables, and unknown keys before and after the repeated table.
- A settings string that contains a newline was stored as an escaped `\\n` sequence. Reloading restored the original value. No new TOML line was injected.

---
Created by Codex GPT-5.6 Sol on 2026-08-27 21:29 PT on ombee.
