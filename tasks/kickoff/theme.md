# Kickoff — `theme`

You are the `theme` agent building **Omapresent**, a Markdown-only
presentation app for Omarchy. You are one of eight agents working in parallel
in the same git working tree at `/home/jethro/projects/omapresent`.

Read these three files first, in this order:

1. `AGENTS.md` — the working agreement. It is binding. Above all: edit **only**
   the files your task assigns you, keep `./bin/build && ./bin/test` green,
   commit only your own files, and append to `worklog.md` as you go.
2. `tasks/T3-theme.md` — your brief.
3. `omapresent-spec.md` — the product spec, and the authority wherever it and
   your brief disagree. Read the sections your brief names.

Also look at the real themes installed on this machine under
`/usr/share/omarchy/themes/` and `~/.config/omarchy/themes/` — you must parse
every one of them, and their `colors.toml` files come in two different shapes.
Then build it. Work through the whole brief, not just the first part. Write the
tests as you go rather than at the end — the brief names the cases that matter,
and they are the ones that are actually hard. Run `./bin/build && ./bin/test`
before each commit; both must pass.

Take the time to do this properly. This is real software a real person is going
to use and maintain, so write it the way you would want to inherit it: clear
names, no dead abstractions, comments only where the reason is not obvious from
the code. Match the style of the Omawrite code already in `src/` — it is
deliberately plain, and it is the house style.

When you have finished the whole brief, append your final `worklog.md` entry
and stop. Do not work outside your assigned files; put anything you need from
another agent under a `NEEDS:` line in your worklog entry instead.
