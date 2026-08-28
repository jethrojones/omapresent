# Omapresent Document Model & Layout Grammar

This document provides a comprehensive reference for authoring Omapresent presentations.

---

## 1. Slides & Separators

- **Separator Syntax:** A slide break is marked by a line containing strictly `---` preceded by a blank line and followed by a blank line (`\n\n---\n\n`).
- **Edge Rules:**
  - `---` mid-paragraph or without surrounding blank lines is parsed as regular text or thematic break, **not** a slide boundary.
  - Setext headings (`Heading\n---`) are headings, **not** slide separators.
  - Alternate Markdown horizontal rules (`***`, `___`) do **not** split slides.
- **Headings Do Not Break Slides:** One slide can contain multiple `#`, `##`, etc. headings. They create visual structure within the slide.
- **Single-Slide Documents:** A Markdown file containing no valid separators renders as a single, vertically scrollable slide.
- **Editor Shortcut:** Pressing `Return` three consecutive times at the end of a slide automatically inserts `\n\n---\n\n`.

---

## 2. Audience Content vs. Speaker Notes

Omapresent categorizes Markdown elements deterministically:

| Element | Audience Display | Presenter View & Notes |
| :--- | :--- | :--- |
| Headings (`#`–`######`) | ✅ Centered titles | Title context |
| Indented lines (tab / 4-space) | ✅ Static outline hierarchy | Outline context |
| Bulleted / numbered lists | ✅ Progressive reveal (item-by-item) | Full list shown |
| Fenced & indented code blocks | ✅ Syntax-highlighted block | Code context |
| Tables | ✅ Clean formatted table | Table context |
| Block quotes (`>`) | ✅ Centered quote block | Quote context |
| Math formulas (`$...$`, `$$...$$`) | ✅ KaTeX rendered formulas | Math context |
| Images (`![[...]]`, `![...](...)`, bare paths) | ✅ Sized / Bento grid | Image preview |
| Recognized video URLs | ✅ Embedded interactive player | Video player |
| Standalone non-video URLs | ✅ Scannable QR code + URL caption | QR code + URL |
| **Plain paragraph prose** | ❌ *(Hidden from audience)* | ✅ **Rendered speaker notes** |

> **Blank Audience Slides:** If a slide consists solely of plain paragraph prose, the audience display shows a clean blank screen while the presenter view displays the notes.

---

## 3. Layout Grammar

Slides are vertical stacks of blocks (lines separated by blank lines). Blocks are centered horizontally. The entire stack is centered vertically if it fits the display height; otherwise, the slide becomes a scrollable surface.

| Markdown Arrangement | Visual Rendering |
| :--- | :--- |
| **Heading alone on slide** | Large title, centered horizontally and vertically (ideal title slide). |
| **Heading, image on next line (no blank line)** | **Tight unit:** Image directly under the heading, both centered, image near-full content width. |
| **Heading, blank line, image** | Two distinct centered blocks with generous spacing between them. |
| **Image alone on slide** | Expands to full content width up to display bounds; height scales proportionally; never cropped. |
| **Consecutive image lines (no blank lines)** | **Bento Grid:** CSS grid layout (2 side-by-side, 3 in a row, 4 as 2x2, 5–6 as mosaic). Adding `\|main` makes that image the large hero tile. |
| **Images separated by blank lines** | Vertically stacked full-width images; slide scrolls if height exceeds display. |
| **Indented lines under heading** | **Static outline:** Indentation equals hierarchy depth; all items displayed at once. |
| **Bulleted / numbered list** | **Progressive reveal:** Each item appears sequentially on `Space` or `→`. Nested children reveal with parent. |
| **Code / Table / Quote / Math block** | Rendered as a centered, full-width block at native size. |
| **Paragraph prose** | Extracted as speaker notes; omitted from audience display. |

---

## 4. Comments & Draft Slides

- **Line Comments:** Any line starting with `//` (ignoring leading whitespace) is stripped from output:
  ```markdown
  // This heading will not be rendered
  # Internal Note
  ```
- **Obsidian Comments:** Text wrapped in `%%...%%` is removed before parsing.
- **HTML Comments:** Text wrapped in `<!--...-->` is removed before parsing.
- **Draft Slides (`// ---`):** Prefixing a slide separator with `//` excludes the entire subsequent slide from the presentation and exports:
  ```markdown
  // ---
  # Draft Slide
  This slide is completely skipped in presentation, PDF, and web publishing.
  ```

---

## 5. Image Syntax & Resolution

### Supported Syntax
- Obsidian embed: `![[diagram.png]]` or with width constraint `![[diagram.png|600]]`
- Standard Markdown: `![Architecture Diagram](diagram.png)`
- Absolute / relative paths: `![[../assets/diagram.png]]`, `![[/home/user/images/diagram.png]]`
- Bare paths: `~/Pictures/diagram.png` or `./img/diagram.png` (must include `/` or known image extension `.png`, `.jpg`, `.jpeg`, `.gif`, `.svg`, `.webp`, `.avif`, `.apng`)

### Resolution Order ("It Just Finds It")
When resolving an image name like `budget.png`:
1. Exact path relative to the presentation `.md` file's directory.
2. Home directory expansion (`~`, `$HOME`) or absolute file path.
3. **Asset Index Search:** Recursive search inside the `root:` directory (default: deck folder) for the closest/shortest matching filename.
4. Case-insensitive retry of step 3.
5. **Placeholder Fallback:** If not found, renders the current Omarchy desktop wallpaper (`~/.local/state/omarchy/current/background`) with an unobtrusive `missing: filename` indicator (hidden during present mode).

---

## 6. Media & QR Codes

### Video Embeds
A recognized video URL alone on its own line renders as an embedded player:
- Supported hosts: **YouTube, Vimeo, Loom, Descript, TikTok, X / Twitter, Instagram, Facebook**.
- Direct video files: `.mp4`, `.webm`, `.mov`.
- **Playback Controls:** `Space` toggles play/pause on the active video player. When video finishes, pressing `Space` advances the slide.
- **Offline Cache:** Video embeds are cached locally into `.omapresent-cache/` when saved or prepared for offline.

### Automatic QR Codes
A standalone URL from an unrecognized host automatically renders as a high-resolution QR code with the complete URL displayed legibly underneath:
```markdown
https://example.com/survey-feedback
```
Explicit QR codes can also be generated with:
````markdown
```qr
https://example.com/survey-feedback
```
````
or `![[qr:https://example.com/survey-feedback]]`.

---

## 7. Recall / Overlay Slides

Recall slides allow you to pop reference material (agendas, glossaries, contact cards) over any current slide during a talk:

- **Binding a Key:** Append `{<key>}` to the separator line:
  ```markdown
  --- {q}
  # Quick Reference Agenda
  - Item 1
  - Item 2
  ```
- **Behavior:** Pressing `q` during the presentation immediately opens this slide as an overlay with the background dimmed. Pressing `q`, `Esc`, or `Space` dismisses it.
- **Excluding from Linear Sequence:** Append `, skip`:
  ```markdown
  --- {q, skip}
  # Reference Diagram
  ```
  The slide is reachable via key `q`, but will not appear during linear `→` navigation.

---

## 8. YAML Frontmatter

Optional YAML frontmatter at the very beginning of the document (lines 1 to closing `---`):

```yaml
---
title: "Quarterly Review"           # Deck title (<title>, publish slug, header/footer)
author: "Jethro Jones"              # Presenter name ({author} token)
date: 2026-09-01                    # Presentation date ({date} token)
theme: "gruvbox"                    # Per-deck theme override (Omapresent windows only)
font: "IBM Plex Sans"               # Custom body font override
aspect: "16:9"                      # PDF and export canvas aspect ratio
root: "~/Documents/assets"          # Root folder for asset index search
header: "Company Confidential"      # Persistent header text on all slides
footer: "{title} — {slide}/{count}" # Persistent footer supporting tokens
slide-numbers: true                 # Show slide number badge (default: false)
progress: true                      # Show thin progress bar (default: false)
publish:                            # Web publishing settings (see publish-toml.md)
  slug: "q3-review"                 # URL slug override
  title: "Q3 Review Presentation"   # Web page title
  provider: "herenow"               # Target provider name
  access: "link"                    # link | public | password | restricted
---
```

### Supported Header & Footer Tokens
- `{title}` — Frontmatter title or first heading
- `{author}` — Frontmatter author
- `{date}` — Frontmatter date
- `{slide}` — Current 1-based slide number
- `{count}` — Total slide count
- `{heading}` — Current slide's primary heading text
