# Review 1 — `assets` (T4)

The resolution chain is correct and the 21 tests are real. Two things in
`src/assetindex.cpp` need fixing before T4 is done, both about what happens when
`root:` is a big folder — which is the normal case. The spec's own example is
`root: ~/Documents/aibrain`, an entire Obsidian vault.

## 1. The walk blocks the calling thread

`rebuild()` runs `QDirIterator` synchronously, and it runs again on every
debounced filesystem change. On a vault with tens of thousands of files that is
a visible freeze in the editor every time someone saves a file anywhere under
the root. Your brief asked for the walk off-thread.

Move it: build the new `m_byName` in a worker (`QtConcurrent::run` or a
`QThread`), then swap it in on the main thread and emit `indexChanged()`.
`resolve()` must keep answering from the old index until the new one lands,
rather than returning nothing mid-rebuild.

## 2. `QFileSystemWatcher` will silently stop watching

You add every subdirectory to the watcher. `QFileSystemWatcher` is backed by
inotify, which has a per-user watch limit — commonly 8192, and it is shared with
every other application on the desktop. A large vault exhausts it, `addPaths()`
starts failing, and the index goes stale with nothing to indicate anything is
wrong.

Cap the number of watched directories at something defensible, prefer
directories nearer the root when you hit the cap, and `qWarning()` once when you
do, so the behaviour is at least discoverable. Check `addPaths()`' return value —
it tells you exactly which paths it could not watch.

## Then

Add a test for the mid-rebuild case in 1 if you can do it without making the
suite slow or flaky. If you cannot, say so in the worklog rather than writing a
sleep-based test. Re-run `./bin/build && ./bin/test`, commit, append a worklog
entry.
