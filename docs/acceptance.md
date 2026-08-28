# Acceptance checklist

Every requirement in `omapresent-spec.md`, who owns it, and how it gets
verified. The orchestrator checks these against the tree — not against what an
agent reported. A line is only ticked when someone has actually watched it work
or a test covers it.

Status: `—` not started · `~` in progress · `x` done and verified

## §4 Document model — `doc-model` (T1)

| | Requirement | Verified by |
|---|---|---|
| — | `---` is a break only with a blank line either side | `tst_deckmodel` |
| — | Setext `Heading\n---` is not a break | `tst_deckmodel` |
| — | `***`, `___`, `----`, `- - -` are not breaks | `tst_deckmodel` |
| — | `---` inside a fenced code block is not a break | `tst_deckmodel` |
| — | No separators = one slide | `tst_deckmodel` |
| — | Frontmatter only when line 1 is `---`; closing `---` is not a break | `tst_deckmodel` |
| — | All §4.4 keys parse, incl. nested `publish:` | `tst_deckmodel` |
| — | `//`, `%%…%%`, `<!-- … -->` stripped — but not inside code fences | `tst_deckmodel` |
| — | `// ---` drops the whole following slide | `tst_deckmodel` |
| — | `--- {q}` binds; `--- {q, skip}` also leaves the linear flow | `tst_deckmodel` |
| — | `sourceStartLine`/`EndLine` correct against the original file | `tst_deckmodel` |

## §4.2, §4.6, §4.7, §4.8 Renderer — `renderer` (T2)

| | Requirement | Verified by |
|---|---|---|
| — | Prose is a note; headings/lists/code/tables/quotes/math/media are audience | `tests/renderer` |
| — | Every row of the §4.6 layout table, and nothing beyond it | `tests/renderer` |
| — | Bento: 2 side by side, 3 row, 4 as 2×2, 5–6 mosaic, `\|main` hero | `tests/renderer` |
| — | Lists reveal one item at a time; nested reveal with their parent | `tests/renderer` |
| — | Never shrink to fit; overflow becomes a scroll surface | `tests/renderer` |
| — | Audience mirrors presenter scroll (`scrollFraction` in state) | manual + contract |
| — | Recognised video host → player; any other bare URL → QR **with URL beneath** | `tests/renderer` |
| — | ` ```qr ` and `![[qr:…]]` force a QR | `tests/renderer` |
| — | A URL inside a notes paragraph stays a link, not a QR | `tests/renderer` |
| — | Missing image → theme background + "missing:" tag, hidden in present mode | `tests/renderer` |
| — | `deck.css` uses only the contract's custom properties, no literal colours | grep |
| — | Vendored markdown-it/KaTeX/QR, licences recorded, no runtime network | `LICENSES.md` |

## §4.5 Images — `assets` (T4)

| | Requirement | Verified by |
|---|---|---|
| x | All four reference forms resolve identically | `tst_assetindex` |
| x | Resolution order 1–4, shortest match wins, case-insensitive retry | `tst_assetindex` |
| x | Spaces in paths, broken symlinks, missing root do not break it | `tst_assetindex` |
| x | `\|600` and `\|main` size hints parse | `tst_assetindex` |
| x | `shortestUniqueReference` for drag-and-drop | `tst_assetindex` |
| ~ | Index off the UI thread; watcher survives a large vault | review 1 |

## §6 Theme — `theme` (T3)

| | Requirement | Verified by |
|---|---|---|
| — | Both `colors.toml` shapes parse; ANSI derives the named roles | `tst_omarchytheme` |
| — | Every theme installed on this machine parses | `tst_omarchytheme` |
| — | Live reload on theme switch, no lost position, debounced | manual |
| — | `theme:` override, user dir before system, Omapresent windows only | `tst_omarchytheme` |
| — | WCAG contrast floor on the audience window only | `tst_omarchytheme` |
| — | `installedThemes()` enumerates both directories | `tst_omarchytheme` |

## §4.8 Media — `media` (T5)

| | Requirement | Verified by |
|---|---|---|
| — | All eight hosts + direct files recognised, real URL shapes | `tst_videocache` |
| — | Unrecognised host is **not** a video (no yt-dlp fallback) | `tst_videocache` |
| — | `describe()` never blocks on the network | `tst_videocache` |
| — | Offline prefetch to `.omapresent-cache/`, cached → embed → QR degrade | `tst_videocache` |
| — | No test touches the network | review |

## §5 Present mode — `present` (T9, not yet assigned)

| | Requirement | Verified by |
|---|---|---|
| — | Two independent top-level windows | manual |
| — | Monitor assignment; single-monitor `N` notes overlay; hotplug | manual |
| — | Every key in the §5.2 table | manual |
| — | Idle inhibit and DND on, prior state restored on exit | manual |
| — | Instant cuts, no animation | manual |

## §8 PDF, §4.10 editor, §10 app — `app-shell` (T8)

| | Requirement | Verified by |
|---|---|---|
| — | Live preview beside the editor, position held across edits | manual |
| — | Triple-return inserts a slide break | `tst_omapresent` |
| — | Drag-drop inserts `![[shortest-name]]`; Wayland `text/uri-list` decoded | `tst_omapresent` |
| — | PDF paginates tall slides, never scales; fragments fully expanded | manual |
| — | Session restores last slide + scroll per file | manual |
| — | Text scaling followed without restart | manual |
| — | CLI: open / present / export --pdf / publish, publish confirms first | `tst_omapresent` |
| — | First run installs the skill per-user and opens the welcome deck | manual |

## §9 Publish — `publish` (T6)

| | Requirement | Verified by |
|---|---|---|
| — | `publish.toml` parses; missing file = anonymous herenow | `tst_publisher` |
| — | One-key patch preserves comments, order and unknown keys byte-for-byte | `tst_publisher` |
| — | herenow publish → presigned PUTs → finalize; refresh on expiry | `tst_publisher` |
| — | Anonymous 24h link + claimToken; authenticated bearer flow | `tst_publisher` |
| — | `command` and `s3` providers | `tst_publisher` |
| — | Deck view **and** long read, both from the shared renderer | manual |
| — | Nothing uploads without an explicit user-initiated call | review |

## §7, §11, §12, §13 Skill, welcome, packaging — `skill-docs` (T7)

| | Requirement | Verified by |
|---|---|---|
| x | `SKILL.md` + four references; safety line on publishing | read |
| x | PKGBUILD deps correct, no yt-dlp, no bundled fonts | `bash -n`, read |
| x | Welcome deck is a valid deck: 24 slides, frontmatter, clean separators | validator |
| ~ | Welcome deck demonstrates **every** row of §4.2 (math missing) | review 1 |
| ~ | Welcome deck inherits the live theme (`theme: default` must go) | review 1 |
| x | `bin/check-skill-sync` passes in CI | CI |

## Cross-cutting — orchestrator

| | Requirement | Verified by |
|---|---|---|
| x | MIT, DHH's line kept, Jethro's added, NOTICE credits Omawrite | read |
| x | Started from Omawrite's source as the base commit | `git log` |
| x | `bin/build`, `bin/install`, `bin/test` all work | run |
| x | CI builds, tests, and checks skill/spec agreement | `.github/workflows` |
| — | Fonts not bundled; system iA Writer S with a fallback stack | grep |
| — | No network at runtime outside prefetch and publish | review |
| — | Everything green end to end on a real deck | manual |
