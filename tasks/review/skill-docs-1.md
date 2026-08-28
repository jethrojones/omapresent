# Review 1 — `skill-docs` (T7)

I validated `welcome/welcome.md` against the real §4.1 rules: 24 slides, the
frontmatter parses, no malformed `---` lines, and the comment / bento / recall /
table / outline coverage is genuinely good. The skill passes
`./bin/check-skill-sync`. Two things to fix.

## 1. Remove `theme: default` from the welcome deck's frontmatter

Two problems with it.

First, `default` is not a theme that exists. I checked the installed set under
`/usr/share/omarchy/themes/` and `~/.config/omarchy/themes/` and there is no
`default`; on this machine the live theme is `gold-rush`.

Second, and more important, it defeats the deck's own argument. This is the
first thing a new user sees, and the whole pitch of §1.2 and §6 is that it is
beautiful by default *because it is already wearing their desktop's theme*.
Pinning it to a fixed theme means the manual opens in someone else's colours
while telling the reader it uses theirs.

Drop the key so the deck inherits the live theme. Put the demonstration of
`theme:` on the theming slide instead — show the syntax in a fenced block, and
explain in the notes that it overrides the live theme for that one deck and
never touches the desktop.

## 2. The deck never demonstrates math

Spec §4.2 lists math as audience content and §4.6 gives it its own centered
block, but there is no `$…$` or `$$…$$` anywhere in `welcome.md`. Since this
deck is the manual, a rule it does not demonstrate is a rule that is not
documented. Add it — inline and display — on a slide of its own, with the
rationale in the notes.

## 3. While you are in there

Walk the rest of the §4.2 table the same way I just did and confirm every row
has a slide that shows it. That table is your checklist for whether the manual
is complete.

## Then

Re-run `./bin/check-skill-sync` and `./bin/build && ./bin/test`, commit, append
a worklog entry.
