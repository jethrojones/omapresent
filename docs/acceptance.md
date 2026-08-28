# Acceptance checklist

Every requirement in `omapresent-spec.md`, who owns it, and how it gets
verified. The orchestrator checks these against the tree — not against what an
agent reported. A line is only ticked when someone has actually watched it work
or a test covers it.

Status: `—` not started · `~` in progress · `x` done and verified

## §4 Document model — `doc-model` (T1)

| | Requirement | Verified by |
|---|---|---|
| x | `---` is a break only with a blank line either side | `tst_deckmodel` |
| x | Setext `Heading\n---` is not a break | `tst_deckmodel` |
| x | `***`, `___`, `----`, `- - -` are not breaks | `tst_deckmodel` |
| x | `---` inside a fenced code block is not a break | `tst_deckmodel` |
| — | No separators = one slide | `tst_deckmodel` |
| x | Frontmatter only when line 1 is `---`; closing `---` is not a break | `tst_deckmodel` |
| x | All §4.4 keys parse, incl. nested `publish:` | `tst_deckmodel` |
| x | `//`, `%%…%%`, `<!-- … -->` stripped — but not inside code fences | `tst_deckmodel` |
| x | `// ---` drops the whole following slide | `tst_deckmodel` |
| x | `--- {q}` binds; `--- {q, skip}` also leaves the linear flow | `tst_deckmodel` |
| x | `sourceStartLine`/`EndLine` correct against the original file | `tst_deckmodel` |

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
| x | Index off the UI thread; watcher survives a large vault | review 1, verified |

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
| x | Welcome deck demonstrates **every** row of §4.2 | review 1, verified |
| x | Welcome deck inherits the live theme | review 1, verified |
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

## Verification log

Things the orchestrator checked directly, rather than taking an agent's word:

- **2026-08-27 — document model.** Built `DeckModel` standalone against a
  hand-written edge-case file and read the output: 5 slides from a file
  containing a Setext heading, `***`, `___`, `- - -`, `----`, and `---` inside
  both ``` and `~~~` fences — none of which became a break. Frontmatter parsed
  with the nested `publish:` map. `--- {q}` and `--- {z, skip}` bound correctly,
  `// ---` dropped its slide without shifting the others, and line ranges
  pointed into the original file. Comment stripping removed `//`, `%%…%%` and
  `<!-- -->` outside fences while leaving `//` inside a ```cpp fence intact —
  the case most implementations get wrong.
- **2026-08-27 — welcome deck.** Parsed `welcome/welcome.md` with the §4.1 rules
  independently: 25 slides, valid frontmatter, no malformed `---`. Confirmed
  `theme: default` was removed (no such theme exists on this machine; the live
  one is `gold-rush`) and that math is now demonstrated.
- **2026-08-27 — asset index.** 23 cases green, including two same-named files
  at different depths, a case-mismatched name, spaces in paths, and a broken
  symlink. Follow-up review confirmed the walk moved to a `QThreadPool` worker
  and the watcher now caps directories and checks `addPaths()` for failures.
- **2026-08-27 — theme.** Ran `parseColorsToml` over **all 29 themes installed
  on this machine**, both `colors.toml` shapes, asserting the full canonical key
  set, `#rrggbb` for every colour, `mode` ∈ {dark,light} and 16 ANSI entries.
  All 29 clean. `contrastRatio` exact (black/white 21.0, self 1.0). Found one
  real bug: `ensureContrast("#767676","#808080")` returned `#ffffff` at ratio
  3.95, below the floor, because the direction is chosen from relative
  luminance — and against a mid-grey background, darkening reaches 5.32 while
  lightening tops out at 3.95. Reported as `tasks/review/theme-1.md`.
- **2026-08-27 — publisher.** Verified `patchToml` against a config with
  comments, blank lines and two provider tables: patching an existing key, a
  top-level key, adding a key to an existing table, and adding a whole new
  table all worked, and in every case the comments, the sibling keys and the
  unrelated `command` provider's line came back byte-identical. `slugify`
  correct on unicode, punctuation runs, whitespace, empty input, and idempotent.
- **2026-08-27 — media.** `hostFor` correct on 26 real URL shapes across all
  eight hosts plus direct and local files, including the negatives that matter:
  `notyoutube.example.com` and `example.com/youtube/article` are not videos,
  and neither is an unrecognised host (spec §4.8 has no yt-dlp fallback).
  `isBareUrlLine` and `extractUrls` correctly ignore a URL inside prose and one
  inside a code fence.
- **2026-08-27 — end to end, for the first time.** Ran
  `omapresent export --pdf welcome/welcome.md`, which drives DeckModel →
  AssetIndex → OmarchyTheme → the renderer in `pdf` mode →
  `QWebEnginePage::printToPdf`. It produced a 499 KB PDF of the real deck in the
  live `gold-rush` theme, with header, footer tokens resolved
  (`How Omapresent Works — Slide 1/25`), slide numbers and the progress bar.
  Rendered pages to PNG and looked at them. Title slide, tables and block quotes
  are genuinely good. Two defects found that no unit test could see:
  **lists lay out in a ~150px column** and wrap after two words (which also
  inflated 25 slides into 61 PDF pages), and **continuation pages strand the
  footer mid-page**. Reported as `tasks/review/renderer-1.md`.
  - A third suspicion — that markdown-it's `typographer` was turning `---` into
    an em dash and corrupting the manual's own explanation of the separator —
    was **wrong**. Checked it directly against the vendored build: `---` inside
    code spans and fences is preserved, and only bare prose `---` becomes an em
    dash, which is correct. The `--` I saw was a `pdftotext` extraction artifact.
- **2026-08-27 — integration suite earns its keep.** Its first run found three
  real cross-component problems: `AssetIndex::looksLikeImageReference` accepts
  any line containing a `/`, so prose like `and/or`, `X/Twitter` and a LaTeX
  formula are treated as bare image paths (confirmed with a direct probe — it
  fires on our own welcome deck); C++ `VideoCache` and the renderer's
  `media.js` disagree about a bare `clip.webm` line, on both whether it is media
  and what host it is; and BOM handling. Reported and reassigned — the `assets`
  agent's provider had run out of quota, so `src/assetindex.cpp` moved to the
  `present` agent.
- **2026-08-27 — theme contrast fix verified.** Re-ran the probe:
  `ensureContrast("#767676","#808080")` now returns `#171717` at ratio 4.54,
  clearing the floor by darkening. All 29 themes still parse.
- **2026-08-27 — present mode, code review.** Read `src/presentation.cpp`'s
  environment handling (§5.3), which is the part that damages the user's desktop
  if it is wrong. `IdleInhibit` and `DoNotDisturbHold` are RAII holders with
  copying deleted, so the inhibit cannot leak on an exception path. DND is
  handled correctly for the case that actually matters: prior state is **read,
  not assumed**, `m_held` is set only when the class actually changed something,
  and the destructor undoes only that — so a presenter who already lives in DND
  does not come out of a talk with notifications switched back on. Covers
  mako, swaync, dunst and Omarchy's toggle-only helper, the last by reading the
  state it prints when flipped. Visual verification of the two windows is still
  outstanding and is assigned to `app-shell`.
- **2026-08-27 — packaging, actually built.** The `media` agent ran `makepkg`
  rather than trusting `bash -n`, and I unpacked the result myself:
  `omapresent-0.1.0-1-x86_64.pkg.tar.zst` contains `/usr/bin/omapresent`,
  `LICENSE`, `NOTICE`, `/usr/share/omapresent/welcome.md`, the whole
  `skill/` tree (SKILL.md + four references), the `.desktop` file and the
  scalable icon. No bundled fonts, no `yt-dlp`.
  - **Spec deviation, deliberate:** §12 lists `qt6-quickcontrols2` as a
    dependency. No such Arch package exists — Quick Controls 2 ships inside
    `qt6-declarative` — so as written the package could never have installed.
    Dropped, and `qt6-webchannel`, `hicolor-icon-theme` and `ttf-ia-writer`
    added. The font is a dependency, not a bundle, which still satisfies §14.2.
