---
name: omapresent
description: Create, edit, format, and publish Omapresent presentations, decks, and configuration files.
---

# Omapresent Skill

Omapresent is a dead-simple, Markdown-only presentation app for Omarchy. In Omapresent, any Markdown file is already a presentation. There are no slide masters, font pickers, or layout dials: the structure of your writing *is* the presentation.

This skill equips agents to create new presentations, convert notes into decks, safely update configuration files, and invoke Omapresent CLI tools.

## The Safety Boundary

- **Local Deck & Config Edits:** You may freely create, format, and edit Markdown deck files and local configuration files (`~/.config/omapresent/settings.toml`, `~/.config/omapresent/publish.toml`).
- **Publishing (`omapresent publish`):** Publishing uploads the presentation and its assets to an external host (such as `here.now`, an S3 bucket, or a custom remote server). **You must always ask for and receive explicit user confirmation before running `omapresent publish`.**

---

## Start a presentation

Use the editor or the command line:

- Press `F5` to start from the beginning.
- Press `Ctrl+Return` to start from the current editor slide.
- Click the footer `Present` button to start from the current editor slide.
- From a terminal, run `omapresent present FILE` to start the deck.

---

## Command Line Usage

| Command | Action |
| :--- | :--- |
| `omapresent <file.md>` | Open the deck in the Omapresent editor and live preview |
| `omapresent present <file.md>` | Start presentation mode for the file (dual presenter/audience windows) |
| `omapresent export --pdf <file.md>` | Export pixel-identical, paginated PDF slides |
| `omapresent publish <file.md> [--provider <name>]` | Build static bundle and upload to hosting provider (*requires user confirmation*) |

---

## Core Document Rules Summary

1. **Slide Breaks:** A slide separator is exactly `---` with a **blank line both before and after it** (`\n\n---\n\n`). Headings never break slides.
2. **Screen vs. Speaker Notes:**
   - **Audience Screen:** Headings (`#`), indented outline lines (tab/4-space), bulleted/numbered lists, fenced code blocks, tables, block quotes, math (`$...$`, `$$...$$`), images, video URLs, and standalone non-video URLs (QR codes).
   - **Speaker Notes:** Plain paragraph prose is hidden from the audience and displayed in the presenter view and web subtitles.
3. **Comments:** `// line comment`, `%% Obsidian comment %%`, `<!-- HTML comment -->`. A `// ---` separator excludes the subsequent slide entirely (draft slide).
4. **Recall Overlays:** `--- {q}` binds the next slide to key `q` as an instant toggleable overlay. `--- {q, skip}` binds the key but removes the slide from the linear left/right flow.
5. **Images & Bento Grids:** `![[name.png]]`, `![alt](name.png)`, or bare path `~/Pictures/photo.png`. Consecutive image lines with no blank lines between them automatically form a Bento CSS grid. Append `|main` to designate a hero tile (`![[photo.png|main]]`).
6. **No Shrink to Fit:** Content keeps its natural size. If a slide overflows vertically, it becomes a smooth scroll surface mirrored between presenter and audience screens.
7. **Theming:** Inherits the desktop theme from `~/.local/state/omarchy/current/theme/colors.toml`. Override per deck with `theme: <name>` in frontmatter for Omapresent windows only.

---

## Reference Guides

Detailed reference documentation is available in `skill/reference/`:

- [Document Model & Layout Grammar](reference/document-model.md) — Comprehensive guide to separators, elements, layout grammar, image resolution, media embeds, and YAML frontmatter.
- [Publish Configuration (`publish.toml`)](reference/publish-toml.md) — Schema, providers (`herenow`, `command`, `s3`), domain mounting, and safe patching guidance.
- [Settings Configuration (`settings.toml`)](reference/settings-toml.md) — Schema for editor, presentation, and desktop integration preferences.
- [Authoring Recipes](reference/recipes.md) — Step-by-step guides for converting notes, building bento grids, adding recall overlays, preparing offline decks, and configuring custom S3 targets.
