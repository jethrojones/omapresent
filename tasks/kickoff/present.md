# Kickoff — `present`

You are the `present` agent building **Omapresent**, a Markdown-only
presentation app for Omarchy. You are one of nine agents working in parallel in
the same git working tree at `/home/jethro/projects/omapresent`.

Read these four files first, in this order:

1. `AGENTS.md` — the working agreement. It is binding. Above all: edit **only**
   the files your task assigns you, keep `./bin/build && ./bin/test` green,
   commit only your own files, and append to `worklog.md` as you go.
2. `tasks/T9-present-mode.md` — your brief.
3. `docs/renderer-contract.md` — the frozen interface to the renderer. You drive
   it; you do not render Markdown yourself.
4. `omapresent-spec.md` §5, §4.7, §4.9 — the product spec, and the authority
   wherever it and your brief disagree.

Then build it. Work through the whole brief, not just the first part. Write the
tests as you go rather than at the end. Run `./bin/build && ./bin/test` before
each commit; both must pass.

Note that `src/presentation.cpp` and the renderer are being implemented by other
agents right now and currently exist as stubs. Build against the headers and the
contract; the real behaviour will appear underneath you.

Take the time to do this properly. This is real software a real person is going
to use and maintain, so write it the way you would want to inherit it: clear
names, no dead abstractions, comments only where the reason is not obvious from
the code. Match the style of the Omawrite code already in `src/` — it is
deliberately plain, and it is the house style.

When you have finished the whole brief, append your final `worklog.md` entry and
stop. Put anything you need from another agent under a `NEEDS:` line rather than
editing their files.
