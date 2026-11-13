# Spec deviations

Places where `omapresent-spec.md` and the shipped code deliberately differ.
Each of these is a decision, not a bug: the spec was written ahead of the
implementation, and where reality disagreed with it, reality won. Anything here
that is *not* a deliberate deviation belongs in `docs/review-findings.md`
instead.

If you change one of these, change this file in the same commit.

---

## D-001 — `account_members` has no frontmatter spelling

**Spec §9** documents the here.now access call as `PATCH
/api/v1/publish/{slug}/access` with
`mode ∈ {anyone_with_link, password, restricted, account_members}`.

**Spec §4.4** documents the frontmatter key that chooses it as
`access: link | public | password | restricted`.

Nothing in the frontmatter maps to `account_members`, so a deck cannot ask for
it. The two lists were written in different sections and never reconciled.

**What the code does.** `Publisher` accepts exactly the four documented
frontmatter values and maps them to three API modes:

| `access:` | API `mode` |
| --- | --- |
| `link` *(and the default)* | `anyone_with_link` |
| `public` | `anyone_with_link` |
| `password` | `password` |
| `restricted` | `restricted` |

`account_members` is not reachable. An unrecognised value is rejected with
"Publish access '…' is invalid. Use link, public, password, or restricted."

**Why this is the right call for now.** Adding a fifth frontmatter value for a
mode nobody asked for would be inventing a feature the spec does not describe
the behaviour of (§6 of the working agreement). `restricted` already covers
"only these people", by email and by domain, and it is the mode the spec
actually specifies `allowedEmails[]` / `allowedDomains[]` for.

**Also worth knowing:** `link` and `public` are two spellings of one thing. A
published deck is a static bundle on a URL; here.now has no "listed in a public
index" concept to distinguish them. `public` is kept because §4.4 documents it
and rejecting a documented value would be worse than honouring it.

---

## D-002 — A publish password is set through `/metadata`, not `/access`

**Spec §9** describes one call for every access mode: `PATCH
/api/v1/publish/{slug}/access` carrying `mode`, `allowedEmails[]` and
`allowedDomains[]`.

**The live API** does not accept a password on that endpoint. Setting a
password is `PATCH /api/v1/publish/{slug}/metadata` with `{"password": "…"}`.
The `/access` endpoint handles `anyone_with_link` and `restricted`.

**What the code does.** `Publisher` branches on the mode: `password` goes to
`/metadata`, everything else goes to `/access` with the body §9 describes. The
tests follow the live API, so they assert `/metadata` for the password case.

**Why.** The spec's §9 was written from `https://here.now/openapi.json` and is
right about the shape of everything else; this is one endpoint it got wrong. The
server is the authority for its own API, and a test that asserted the spec here
would fail against the real service.

---

## D-003 — `qt6-quickcontrols2` is not an Arch package

**Spec §12** lists the runtime requirements as `qt6-base qt6-declarative
qt6-quickcontrols2 qt6-webengine qt6-multimedia`.

There is no `qt6-quickcontrols2` package in the Arch repositories. Quick
Controls 2 — and Quick Dialogs 2, which the app also uses — ship inside
`qt6-declarative`, which is already on the list.

**What the code does.** `pkgbuild/PKGBUILD` drops `qt6-quickcontrols2` and
keeps `qt6-declarative`. It also adds `qt6-webchannel`, which the renderer
bridge needs and §12 does not mention.

**Why.** A `depends` entry naming a package that does not exist makes `makepkg`
fail outright, so the deck could not be packaged at all. The dependency is not
lost — it is satisfied by a package already required.

---

## D-004: Omapresent also builds and runs on macOS

**Spec §1 and §12** describe an Omarchy application: Arch packaging, Hyprland,
the desktop portal, the Omarchy theme.

**What the code does.** The same source builds on macOS against Homebrew's Qt.
Every difference is answered in `src/platform.{h,cpp}`, and on Linux each of
those functions returns what the code it replaced computed inline:

| Question | Linux | macOS |
| --- | --- | --- |
| Config directory | `QStandardPaths` (`XDG_CONFIG_HOME`) | `XDG_CONFIG_HOME`, else `~/.config` |
| Installed data | `/usr/share/omapresent` | `Omapresent.app/Contents/Resources` |
| Interface font | iA Writer Mono S | the same, else the system monospace |
| Presenter's screen | the primary output | the laptop's own panel, when there is one |
| First window geometry | the compositor's | the screen's available area |
| Keeping the display awake | DBus inhibitors, `omarchy-toggle-idle` | an IOKit power assertion |

Do Not Disturb has no public macOS API, so `presentation.do_not_disturb` does
nothing there. There is no Omarchy theme on a Mac; the built-in palette is used
and follows the system appearance. A document handed to a running app arrives
as a `QFileOpenEvent`, which `main.cpp` routes through the editor's own
`requestOpen`.

Packaging is `bin/install-macos`, which `bin/install` hands over to on a Mac.
It assembles `Omapresent.app` around the plain `build/omapresent` binary. The
bundle is not self-contained: like the Arch package it uses the system's Qt.

**Why.** The presenter also works from a MacBook. One codebase is cheaper to
keep honest than a fork.

---

## D-005: A slide can be a live terminal

**Spec §4.9** defines two words in a separator tag, a recall key and `skip`.
**Spec §1.2** says the app does one thing: present Markdown.

**What the code does.** A third word, `terminal`, makes the slide a real
PTY-backed shell in present mode (`src/terminalsession.cpp`, drawn by a
vendored xterm.js in `src/terminal/`). It combines with the other two. The
rules for who has the keyboard are in `Presentation` and are documented in the
README and `docs/renderer-contract.md` §8.

It stays inside the spirit of the spec in three ways. The Markdown is still the
whole deck: outside present mode the slide is its Markdown. There are no new
styling knobs: the terminal takes its colours and face from the theme. And
AGENTS.md rule 8 holds: no network, and the emulator is vendored with its
licence recorded in `src/terminal/vendor/LICENSES.md`. It is vendored outside
`src/renderer/vendor/` deliberately, so that a published bundle never carries
an emulator it has no shell for.

A deck cannot make the terminal run anything. It opens a prompt, when the
presenter navigates to it, and nothing more.

**Why.** Live coding in front of a room without leaving the deck.

