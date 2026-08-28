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

`SecurityTest::indexedAssetSymlinkCannotLeaveTheAssetRoot` now guards the fixed behavior with a normal assertion.

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

Fixed in commit `83ba985`. `SecurityTest::videoCacheSymlinkCannotLeaveTheDeckDirectory` now guards the fixed behavior with a normal assertion.

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

`SecurityTest::bundleOutputSymlinkCannotLeaveTheOutputRoot` now guards the fixed behavior with a normal assertion.

## Second-pass findings — 2026-08-28

### SEC-006 — Medium — The CLI can publish an empty deck after its input fails to open

`Backend::runCommand()` checks only whether the input path exists. It calls `open()`, but it does not check whether `open()` loaded the file. A directory or an unreadable file therefore reaches `publishDeck()` with the new backend's empty document.

Concrete reproduction:

1. Configure a command provider that prints a valid URL.
2. Run `omapresent publish <directory> --provider test --yes`.
3. Repeat with an existing file that the current user cannot read.
4. In both cases, the provider runs, the CLI prints its URL, and the process exits 0.

The path is in `src/backend.cpp` in `open()` and `runCommand()`. The failed `open()` sets a status message and returns. `runCommand()` ignores that result and continues.

The user must still confirm or pass `--yes`. The command can publish an empty deck instead of the named file and report success. It does not disclose the unreadable file because that file was never read.

`SecurityTest::cliRejectsADirectoryBeforePublishing` and `SecurityTest::cliRejectsAnUnreadableFileBeforePublishing` pin both forms as expected failures.

### SEC-007 — Low — First run follows a symlinked agent skills directory

`Backend::agentSkillDirectories()` accepts a path when `QFileInfo(path).isDir()` is true. That check follows a directory symlink. `installAgentSkill()` then creates the `omapresent` link through that path.

Concrete reproduction:

1. Make `~/.claude/skills` a symlink to another writable directory.
2. Start Omapresent.
3. The target directory receives an `omapresent` symlink.

The issue is in `src/backend.cpp` in `agentSkillDirectories()` and `installAgentSkill()`.

This creates one known symlink in a directory that the user can already write. It does not replace an existing file, directory, or different symlink. The main risk is a surprising write outside the expected agent tree.

`SecurityTest::firstRunRefusesASymlinkedSkillDirectory` pins this as an expected failure.

### SEC-008 — Low — A duplicate settings key can make a patch report success without applying

`Publisher::patchToml()` replaces the first matching key in the first matching table. `Publisher::parseToml()` keeps the later value when a malformed file repeats the same table and key. `Settings::setValue()` can therefore return true, but a reload still reads the old later value.

Concrete reproduction:

1. Put `theme = "first"` in one `[editor]` table.
2. Repeat `[editor]` and put `theme = "second"` in it.
3. Call `Settings::setValue("editor.theme", "patched")`.
4. The call returns true, but `Settings::stringValue("editor.theme")` is still `second`.

The patch preserves both unknown keys in the test. It does not make the malformed file less readable. The failure is that the requested change does not take effect even though the API reports success.

`SecurityTest::settingsPatchAppliesWithADuplicateKnownKey` pins this as an expected failure.

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
