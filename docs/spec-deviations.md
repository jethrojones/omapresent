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
