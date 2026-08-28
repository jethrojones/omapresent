# Working agreement — Omapresent

Several agents build this repo in parallel, each in its own Herdr pane, all in
the same working tree. These rules keep that from becoming a mess. Read them
before your first edit.

## 1. Stay inside your files

`tasks/<your-task>.md` lists the files you own. **Edit only those files.**
If you need a change in someone else's file, write the request at the bottom of
your worklog entry under `NEEDS:` and keep going — the orchestrator lands it.

The one shared file everyone appends to is `worklog.md`, and only ever by
appending a new entry at the end. Never rewrite or reorder it.

## 2. Contracts are frozen

The public interface of every header in `src/` and the JSON shapes in
`docs/renderer-contract.md` are settled. You may **add** members. You may not
change the name, signature or documented meaning of anything already declared —
other agents are compiling against it right now. If a contract is genuinely
wrong, say so under `NEEDS:` and implement the closest thing that compiles.

## 3. The tree must always build

Before you finish any unit of work:

```sh
./bin/build && ./bin/test
```

Both must pass. A red tree blocks everyone. If you break something you do not
own, fix it or revert your change — do not leave it red and move on.

## 4. Commit as you go

Small commits, one logical change each, present tense, explaining the *why*:

```sh
git add <your files> && git commit -m "Resolve image references against the asset index

..."
```

Only `git add` files you own. Never `git add -A`, never `git commit -a`,
never rebase, reset, or touch another agent's commits, never force anything.
If `git commit` reports a conflict or a file you did not expect, stop and log it.

## 5. Log every step

Append to `worklog.md` when you start a task, when you finish one, and whenever
you hit something the next person needs to know. Format:

```markdown
### 2026-08-27 18:40 PT — <agent-name> (<model>)
**Task:** T3 image resolution
**Did:** Implemented AssetIndex::resolve steps 1-4 and the recursive index.
**Tests:** tests/tst_assetindex.cpp — 14 cases, all green. `./bin/test` passes.
**Next:** Step 5 placeholder rendering, drag-drop shortest-name insert.
**NEEDS:** nothing
```

Keep it short and factual. "Did" describes what is now true in the tree, not
what you intended.

## 6. The spec is the authority

`omapresent-spec.md` is the brief. Where your task file and the spec disagree,
the spec wins — and log the discrepancy. Do not invent features it does not
ask for. Do not add styling knobs; §1.2 and §4.6 are deliberate.

## 7. Tests are part of done

Every pure function named in your header gets real cases, including the ugly
ones the spec calls out (spaces in paths, both `colors.toml` shapes, `---` that
is not a separator, comments inside code fences). A task with no tests is not
finished. C++ suites: `tests/tst_*.cpp`, registered via `tests/testrunner.h` —
no `QTEST_MAIN`. Renderer suites: `tests/renderer/*.test.mjs`, run by
`node --test`.

## 8. No network at runtime

The app makes exactly two kinds of network call, both explicit and
user-initiated: video pre-fetch (spec §4.8) and publish (spec §9). Everything
else — Markdown, math, QR, fonts — is vendored under `src/renderer/vendor/`
with its licence recorded in `src/renderer/vendor/LICENSES.md`.

## 9. When you are done

Append your final worklog entry, make sure `./bin/build && ./bin/test` pass,
commit, and say so plainly. Then stop and wait — the orchestrator will send you
the next task in the same pane.

## 10. Two things that will bite you in a shared tree

**Builds take turns.** `bin/build` and `bin/test` take an exclusive `flock`
before running, because eight agents sharing one `build/` directory would
otherwise produce half-written object files and failures that belong to nobody.
If a build seems to pause before it starts, it is waiting for the lock, not
hanging. Let it wait.

**Git's index is not shared.** If a commit fails with
`Unable to create '.git/index.lock'`, another agent is committing at that exact
moment. Wait a few seconds and run the same command again — it will work. Never
delete `index.lock` to get past it; you would corrupt someone else's commit.
