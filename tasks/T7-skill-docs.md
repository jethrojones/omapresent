# T7 — Agent skill, welcome deck, packaging and docs

**Agent:** `skill-docs` · **Spec:** §7, §11, §12, §13, §15 milestones 8–9

## Files you own
- `skill/SKILL.md` and `skill/reference/*.md`
- `welcome/welcome.md`
- `README.md`
- `pkgbuild/PKGBUILD`, `pkgbuild/omapresent.desktop`,
  `pkgbuild/omapresent.install`, `pkgbuild/omapresent.svg`

Nothing else. `NOTICE` and `LICENSE` are already done — leave them.

## 1. The welcome deck (§7) — this is the manual

`welcome/welcome.md` is a **real Omapresent deck that explains the whole app by
being it**. Every rule gets its own slide that demonstrates itself, with the
design rationale in the speaker notes (which, per §4.2, is just the plain prose
paragraphs — so write the rationale as prose and the demonstration as headings,
lists, code, tables and media).

It must cover, each demonstrated live: the document model, the separator rule,
every row of the §4.6 layout grammar, comment syntax, image resolution,
video/QR behaviour, recall slides, all present-mode shortcuts, the frontmatter
keys, theming, PDF export and publish.

Write it as something a person actually enjoys reading. It ships read-only from
`/usr/share/omapresent/welcome.md`; "Edit a copy" drops it in `~/`.

Verify it parses the way you intend against the real rules in
`omapresent-spec.md` §4 — this file is also the closest thing the project has to
an end-to-end fixture, so getting it wrong is a real bug.

## 2. The agent skill (§11)

`skill/SKILL.md` with YAML frontmatter (`name: omapresent`, and a `description:`
covering "create / edit / publish Omapresent decks and config"), following the
Agent Skills spec. Plus `skill/reference/document-model.md`,
`publish-toml.md`, `settings-toml.md`, `recipes.md`.

Contents:
- The full document model condensed to something an agent can **act on**:
  separators, comments, screen-vs-notes, the layout grammar, image resolution,
  recall slides, video/QR, frontmatter keys.
- How to edit the two TOML config files safely —
  `~/.config/omapresent/publish.toml` and
  `~/.config/omapresent/settings.toml` — with the schema, the valid enum
  values, and explicit "read the file, patch one key, keep the rest" guidance.
  **Never rewrite a config file wholesale; preserve comments and unknown keys.**
- Recipes: turn this note into a deck; add a recall slide; make a bento image
  slide; wire publishing to my own S3 bucket; prepare this deck for an offline
  venue.
- Invoking the app: `omapresent <file>`, `omapresent present <file>`,
  `omapresent export --pdf <file>`, `omapresent publish <file> [--provider X]`.
- **The safety line, stated plainly:** editing a user's deck content and config
  is fine; `omapresent publish` sends the deck to an external host and must be
  confirmed by the user first.

Derive the schemas from the actual headers — `src/publisher.h` for publish.toml,
spec §4.4 for frontmatter — so the skill and the code agree.

## 3. Packaging (§12)

- `PKGBUILD` for the Omarchy package repo. Depends: `qt6-base qt6-declarative
  qt6-webengine qt6-multimedia`, plus `gst-plugins-base gst-plugins-good
  gst-plugins-bad gst-plugins-ugly` for local video codecs. **No `yt-dlp`.**
  **Do not bundle fonts** — depend on the iA Writer S family already on Omarchy
  (§14.2). Install `welcome/welcome.md` to `/usr/share/omapresent/welcome.md`
  and `skill/` to `/usr/share/omapresent/skill/`.
- `omapresent.desktop`: Name=Omapresent, `MimeType=text/markdown;`.
- `omapresent.install`: on `post_install`/`post_upgrade` put the skill where an
  agent will find it; remove it on `post_remove`. **The open question (§14.5):**
  the hook runs as root and cannot know which user to install for. Ship the
  files to `/usr/share/omapresent/skill/` from the hook, and have the *app*
  symlink them into the invoking user's skills directory on first run —
  `${XDG_DATA_HOME:-~/.local/share}/omarchy/skills/omapresent/` if Omarchy
  defines a skills path, otherwise `~/.claude/skills/omapresent/`. Check what
  Omarchy on this machine actually does (look under `/usr/share/omarchy/` and
  `~/.local/share/omarchy/`) and follow that precedent; write down what you
  found in your worklog entry, and put the app-side first-run step under
  `NEEDS:` rather than writing it yourself.
- `omapresent.svg`: placeholder pineapple-on-projector icon. Jethro replaces it
  later — keep it simple and legible at 48px.

## 4. README

Rewrite `README.md` for Omapresent, not Omawrite. What it is, the one-sentence
pitch, a screenshot placeholder, install, the keyboard reference (§13), and
credit to Omawrite. Badges/topics: `open-source`, `mit`, `omarchy`.

## Done when
`./bin/build && ./bin/test` still pass (you should not have touched code, but
check), `bash -n pkgbuild/PKGBUILD` and `namcap` if available are clean, the
welcome deck is genuinely good, and your worklog entry is appended.
