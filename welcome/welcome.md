---
title: "How Omapresent Works"
author: "Jethro Jones"
date: 2026-08-27
aspect: "16:9"
header: "Omapresent"
footer: "{title} — Slide {slide}/{count}"
slide-numbers: true
progress: true
publish:
  slug: how-omapresent-works
  title: "How Omapresent Works — The Manual"
  provider: herenow
  access: link
---

# Omapresent
## Presentations from plain Markdown

Welcome to Omapresent. This presentation is the manual, and the manual is a real presentation deck.

In Omapresent, the Markdown document is the single source of truth. There are no slide masters, no drag handles, no font pickers, and no layout fiddling. The structure of your writing is the presentation.

---

# What's different?

- Dynamic vertical height slides
- Slide Overlays (hotkey enabled)
- Automatic QR Codes
- Themes, of course

---


# Writing Structure = Slide Structure

- Headings, lists, code, tables, quotes, math, and media → **Audience Screen**
- Plain paragraph prose → **Speaker Notes** (Presenter Display only)
- Slide breaks occur only on a `---` line wrapped in blank lines
- Headings never break slides; they create structure within a slide

Every slide you see here is authored in standard Markdown. The text you see on this slide is visible to the audience because it is formatted as a heading and bullet list.

The prose you are reading right now in the presenter view is a standard paragraph in the Markdown source. Paragraph prose is automatically treated as speaker notes. The audience never sees it, and you never have to learn a proprietary notes syntax.

---

# Slide Breaks & Separators

```markdown
# Slide One Content

---

# Slide Two Content
```

- Exactly `---` on a line by itself
- Must have a blank line before and a blank line after
- Setext headings (`Heading\n---`) and mid-paragraph `---` are not separators
- Three consecutive Returns in the editor automatically inserts a slide break

Slide breaks in Omapresent are strictly defined. A separator must be three hyphens on their own line with a blank line immediately above and below.

This rule protects your content: horizontal rules within paragraphs or Setext heading underlines will never accidentally split your slide. In the editor, pressing Return three times at the end of a slide inserts a clean separator for you.

---

# Simplicity Is the Default

A slide containing only a heading is automatically treated as a title slide.

Omapresent centers single headings both horizontally and vertically. You don't need a special "title slide" layout mode or template. When a slide contains a lone heading, it takes center stage.

---

# Heading with Image (Tight Unit)
![[presentation-hero.png]]

Putting an image directly under a heading on the very next line (with no blank line between them) binds them into a tight visual unit.

The image is rendered directly under the heading, both are centered, and the image scales to near-full content width. This is ideal for figures, charts, and diagrams that directly illustrate a heading.

---

# Heading with Generous Spacing

![[presentation-hero.png]]

Adding a blank line between a heading and an image creates two separate, generously spaced blocks.

Both blocks remain horizontally centered on the slide. This layout gives breathing room to conceptual slides where the heading provides context rather than a direct caption.

---

![[presentation-hero.png]]

An image alone on a slide expands to fill the content width up to the display boundaries.

Images are never cropped. The aspect ratio is always preserved, and if an image exceeds the vertical height of the display, the slide scrolls gracefully rather than compressing the image.

---

![[shot1.png|main]]
![[shot2.png]]
![[shot3.png]]
![[shot4.png]]

Consecutive image lines with no blank lines between them create an automatic Bento grid.

Omapresent arranges consecutive images using an intelligent CSS grid layout: two images sit side by side, three form a row, four become a balanced 2x2 grid, and five or six create a mosaic. Adding the `|main` hint designates that image as the large central hero tile.

---

![[shot1.png]]

![[shot2.png]]

Images separated by blank lines are rendered as stacked vertical blocks.

Each image takes full width and the slide becomes scrollable if the combined height exceeds the window. Use this when presenting a sequence of comparative steps or before-and-after screenshots.

---

# Hierarchical Outlines
	First foundational concept
		Concrete operational implementation step
		Secondary supporting detail
	Second foundational concept
		System architecture and data flow

Indented lines under a heading create a static outline.

Lines indented with a tab or four spaces under a heading are displayed all at once on the audience screen, with indentation reflecting hierarchy depth. Unlike bullet lists, static outlines do not require progressive clicks to reveal.

---

# Progressive Disclosure

- Bulleted and numbered lists reveal one item at a time
- Pressing `Space` or `→` reveals the next fragment
	- Nested child items reveal sequentially with their parent
- Pressing `←` steps backward through revealed fragments
- After the last item is revealed, `Space` or `→` advances to the next slide

List items are designed for speaking. When presenting, each top-level bullet point is hidden until you advance to it.

This progressive reveal keeps your audience focused on what you are saying right now rather than reading ahead. In PDF export and web publish modes, lists are rendered fully expanded.

---

# Code, Tables & Block Quotes

```python
def present(markdown: str) -> Presentation:
    deck = parse_deck(markdown)
    return Presentation(deck)
```

| Syntax | Audience Screen | Notes View |
| :--- | :--- | :--- |
| `# Heading` | Large title | Title context |
| `Prose paragraph` | *(Hidden)* | Rendered notes |
| `![[figure.png]]` | Sized media | Media preview |

> "Perfection is achieved not when there is nothing more to add, but when there is nothing left to take away."

Code blocks, tables, and block quotes are first-class audience elements.

Each renders centered at its natural size with syntax highlighting powered by your desktop theme. Notice how everything on the slide is crisp and legible without manual formatting.

---

# Mathematical Notation

$$e^{i\pi} + 1 = 0$$

- KaTeX renders LaTeX math formulas into crisp vector typography
- Display math `$$...$$` renders as an isolated, centered equation block
- Inline math like $\sigma = \sqrt{\frac{1}{N}\sum_{i=1}^N (x_i - \mu)^2}$ embeds seamlessly within text

Mathematical expressions are rendered locally using bundled KaTeX without requiring an internet connection.

Display equations form their own centered blocks on the audience screen, while inline formulas scale harmoniously with surrounding typography.

---

# Comments & Draft Slides

// This line comment is stripped and visible nowhere
%% Obsidian comment syntax is also supported %%
<!-- Standard HTML comments work as well -->

```markdown
// ---
# This entire slide is a draft
It will not appear in the presentation or export.
```

Omapresent supports line comments with `//`, Obsidian comments with `%%...%%`, and HTML comments with `<!--...-->`.

Prefixing a slide separator with `//` (writing `// ---`) comments out the entire following slide. This makes it effortless to draft new slides or temporarily hide material without deleting it.

---

# Image Resolution: It Just Finds It

- Obsidian embeds: `![[diagram.png]]` or `![[diagram.png|600]]`
- Standard Markdown: `![Architecture](diagram.png)`
- Bare paths: `~/Pictures/diagram.png` or `./assets/diagram.png`
- Resolution order:
	1. Relative to deck directory
	2. Home folder and environment expansion (`~`, `$HOME`)
	3. Asset index search (recursive through deck folder or `root:`)
	4. Case-insensitive retry
	5. Missing fallback: desktop theme background + `missing:` badge

You never have to fight relative path errors when moving presentations.

Omapresent indexes your deck's folder (or the directory specified by `root:` in frontmatter) and resolves images by filename. If an image cannot be found, Omapresent displays your current desktop wallpaper with an unobtrusive missing-asset indicator so your presentation never crashes.

---

# Video & Interactive Embeds

https://www.youtube.com/watch?v=aqz-KE-bpKQ

A bare URL from a recognized video host on its own line becomes a player. Nothing is fetched until you press Play, so opening a deck never contacts anyone.

Omapresent natively recognizes YouTube, Vimeo, Loom, Descript, TikTok, X/Twitter, Instagram, Facebook, and direct video files (`.mp4`, `.webm`, `.mov`).

Pressing `Space` plays and pauses the video. When playback finishes, the next `Space` advances the slide. Saving pre-fetches what each host allows into `.omapresent-cache/` for reliable offline presentation; a video that cannot be cached or embedded falls back to a QR code of its URL.

--- {r}

# Automatic QR Codes

https://omapresent.com

A standalone URL that is not a video host automatically renders as a scannable QR code.

The full destination URL is printed legibly beneath the code on both the audience and presenter screens. This makes it trivial to share links, repositories, feedback forms, and contact cards with your audience. You can also force a QR code using a ````qr` fence or `![[qr:https://...]]`.

---

# Never Shrink to Fit

- Slide content preserves its natural typographic scale
- When content exceeds the display height, the slide scrolls smoothly
- Presenter scrolling is mirrored live on the audience screen
- Scroll position is preserved when navigating between slides
- Use `↑` / `↓`, `PgUp` / `PgDn`, or mouse wheel to scroll

Traditional presentation software shrinks text until it fits on a single slide, destroying legibility.

Omapresent refuses to shrink text. If you have a detailed table, a longer code sample, or multiple diagrams, the slide becomes a scrollable surface. The audience display tracks your scrolling position in real time.

---

# Presenter Mode & Multi-Monitor

- **Dual-Window Architecture:** Separate Presenter and Audience windows
- **Multi-Monitor:** Audience window automatically fullscreens on external displays
- **Single-Monitor:** Press `N` to toggle a presenter notes overlay
- **Environment Control:** Automatic idle inhibition and Do-Not-Disturb activation
- Hotplug detection automatically repositions windows when connecting a projector

Present mode opens two independent top-level windows.

The Presenter window shows the current slide, next slide preview, rendered speaker notes, elapsed timer, wall clock, and recall key badges. The Audience window displays clean, themed presentation content with instant slide cuts.

## Share the audience window

- The editor and audience are separate native windows
- Click `Present` in the editor footer to start from the current editor slide
- If presentation is already running, share the audience window titled `<deck title> — Omapresent` or `Omapresent`
- Do not share `aquamarine - WAYLAND-1`; it is the outer compositor/output

The share portal may show its own label. Omapresent does not control that label. Select the direct Omapresent audience window in the picker.

---

# Present Mode Keyboard Controls

## Start a presentation

- In the editor, press `F5` to start from the beginning
- In the editor, press `Ctrl+Return` to start from the current editor slide
- In the editor, click the footer `Present` button to start from the current editor slide
- From a terminal, run `omapresent present FILE` to start the deck

| Key | Action |
| :--- | :--- |
| `Space` / `→` | Next fragment or advance slide |
| `←` | Previous fragment or step back |
| `↑` / `↓` / Wheel | Scroll current slide (mirrored live) |
| `Home` / `End` | Jump to first or last slide |
| `Digits` + `Enter` | Jump directly to slide number |
| `F` | Toggle fullscreen on audience display |
| `B` / `W` | Black out / White out audience display |
| `O` | Toggle slide overview grid |
| `N` | Toggle speaker notes overlay |
| `Esc` | Exit presentation mode |

These shortcuts give you complete control during a live presentation.

Pressing `Ctrl+?` in either the editor or presenter view displays the complete keyboard reference sheet at any time.

--- {q}

# Recall Overlay Slide (Key: Q)

- Bound to the `Q` key via the separator line: `--- {q}`
- Pressing `Q` at any point during your talk pops this slide as an overlay
- Current presentation slide is dimmed behind the overlay
- Press `Q`, `Esc`, or `Space` to dismiss and resume exactly where you were
- Use `--- {q, skip}` to make an overlay that does not appear in the linear flow

Recall slides are perfect for agendas, reference diagrams, glossary definitions, and question prompts that you want to return to on demand.

Tagging a separator with `--- {k}` binds the subsequent slide to key `k`. The presenter display lists all active recall bindings so you never forget your shortcuts.

---

# Theming & Desktop Harmony

```yaml
---
theme: gruvbox
---
```

- Inherits your live Omarchy desktop theme (`colors.toml`) by default
- Supports rich named color palettes and 16-color ANSI terminal themes
- Watches theme files and repaints instantly on system theme changes
- Override theme per-deck via `theme: <name>` frontmatter (Omapresent windows only)
- System typography powered by the open-source iA Writer S family

Omapresent feels right at home on your desktop.

By default, Omapresent leaves the theme key unset so your presentations automatically inherit whatever theme your desktop is currently wearing.

When you want a specific presentation to use a particular palette, setting `theme: <name>` loads that theme from `~/.config/omarchy/themes/` or `/usr/share/omarchy/themes/`. The override applies strictly to Omapresent's windows and never alters the rest of your desktop.

---

# Deck Frontmatter

```yaml
---
title: "Quarterly Strategy"
author: "Jethro Jones"
date: 2026-09-01
theme: gruvbox
aspect: "16:9"
root: ~/Pictures
header: "Organization Name"
footer: "{title} — {slide}/{count}"
slide-numbers: true
progress: true
publish:
  slug: quarterly-strategy
  provider: herenow
  access: link
---
```

Frontmatter at the start of your file configures deck-wide metadata and options.

Header and footer templates support dynamic replacement tokens: `{title}`, `{author}`, `{date}`, `{slide}`, `{count}`, and `{heading}`. All frontmatter fields are optional.

---

# PDF Export & Pluggable Publishing

- `Ctrl+E`: Export pixel-identical PDF slides (tall slides paginate automatically)
- `Ctrl+Shift+P`: Publish online producing both a **Deck View** and a **Long Read** article
- Pluggable publishing targets configured in `~/.config/omapresent/publish.toml`:
	- `herenow`: Built-in zero-config publishing with custom domain support
	- `command`: Custom shell command or rsync script
	- `s3`: Direct upload to any S3 or Backblaze B2 bucket

Your presentations can be shared anywhere.

Web publishing produces a static bundle containing both a slide presentation with subtitle speaker notes and a beautifully formatted long-read document. You own your distribution without vendor lock-in.

---

# Start Creating

- Press `F5` to start an open deck from the beginning
- Press `Ctrl+Return` or click the footer `Present` button to start from the current editor slide
- From a terminal, run `omapresent present FILE`
- Press `Ctrl+N` to start a new deck

Use `F5` for a full run. Use `Ctrl+Return` or the footer button when the editor is already on the slide you want. The CLI command starts presentation mode for the file.
