# Kickoff — `webbundle`

You are the `webbundle` agent building **Omapresent**, a Markdown-only
presentation app for Omarchy. You are one of several agents working in parallel
in the same git working tree at `/home/jethro/projects/omapresent`.

You are picking up a task whose previous agent ran out of quota before starting
it, so nothing has been written yet — `src/webbundle.cpp` is still the stub.

Read these four files first, in this order:

1. `AGENTS.md` — the working agreement. It is binding. Above all: edit **only**
   the files your task assigns you, keep `./bin/build && ./bin/test` green,
   commit only your own files, and append to `worklog.md` as you go.
2. `tasks/T11-web-bundle.md` — your brief.
3. `docs/renderer-contract.md` — the deck JSON you consume and the renderer you
   drive. You do not render Markdown yourself.
4. `omapresent-spec.md` §9 — the product spec, and the authority wherever it and
   your brief disagree.

Note that the renderer under `src/renderer/` and `src/publisher.cpp` are being
implemented by other agents right now. Build against the headers and the
contract; the real behaviour will appear underneath you. If the tree is red on a
file you do not own, that is someone else's in-flight work — say so in your
worklog and keep going rather than fixing their file.

Then build it. Work through the whole brief. Write the tests as you go. Run
`./bin/build && ./bin/test` before each commit; both must pass.

Take the time to do this properly. This is real software a real person is going
to use and maintain, so write it the way you would want to inherit it: clear
names, no dead abstractions, comments only where the reason is not obvious from
the code. Match the style of the code already in `src/` — it is deliberately
plain, and it is the house style.

When you have finished, append your final `worklog.md` entry and stop.
