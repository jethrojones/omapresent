# Authoring Recipes

Actionable step-by-step patterns for common Omapresent authoring and configuration tasks.

---

## Recipe 1: Convert a Markdown Document into a Presentation Deck

To convert a raw note, article, or outline into a polished Omapresent presentation:

1. **Add Frontmatter:**
   Add minimal frontmatter at the top of the file:
   ```yaml
   ---
   title: "Engineering Review Q3"
   author: "Jane Doe"
   footer: "{title} — {slide}/{count}"
   slide-numbers: true
   ---
   ```
2. **Identify Slide Boundaries:**
   Divide distinct ideas using slide breaks (`\n\n---\n\n`).
3. **Separate Screen Content from Speaker Notes:**
   - Turn key ideas into `#` or `##` headings.
   - Convert sequential points into bulleted lists (`- item`) so they reveal progressively.
   - Leave detailed explanatory paragraphs as plain prose. The audience will see only the clean headings and lists, while the speaker notes will appear in your presenter view.

### Before & After Example
**Original Note:**
```markdown
# Database Migration
We need to migrate our Postgres database to version 16 this weekend.
First, we will take a snapshot backup at 22:00.
Then, we run the schema upgrade migrations.
Finally, we verify data consistency and traffic cutover.
If anything fails, rollback procedure is documented in RUNBOOK.md.
```

**Converted Deck:**
```markdown
---
title: "Database Migration"
footer: "{title} — {slide}/{count}"
---

# Database Migration
## Weekend Plan

- Snapshot backup initiated at 22:00
- Schema upgrade migrations execute
- Verification & traffic cutover
- Rollback plan ready via runbook

We need to migrate our Postgres database to version 16 this weekend. Make sure the operations on-call is paged before we begin the snapshot.
```

---

## Recipe 2: Add a Recall / Overlay Slide

Recall slides allow you to pop reference material over your presentation at any moment by pressing a bound key.

1. **Tag the Separator:** Add `{<key>}` to the slide separator line (e.g. `--- {q}`).
2. **Exclude from Linear Flow (Optional):** Add `skip` to keep the overlay available via key without having it appear in the regular next/previous slide sequence:
   ```markdown
   --- {q, skip}

   # Architecture Reference Map
   ![[architecture-diagram.png]]

   This diagram can be pulled up whenever an audience question touches on system topology.
   ```
3. **During the Talk:** Press `q` to toggle the overlay. Press `Esc` or `Space` to dismiss.

---

## Recipe 3: Build a Bento Image Grid

To create a balanced Bento grid (CSS grid layout):

1. **Place image references on consecutive lines** with no blank lines between them:
   ```markdown
   # Product Features

   ![[hero-dashboard.png|main]]
   ![[mobile-view.png]]
   ![[settings-panel.png]]
   ![[analytics-chart.png]]
   ```
2. **Designate the Hero Tile:** Add `|main` to the most important image. It will expand to occupy the dominant central tile while other images tile cleanly around it.

---

## Recipe 4: Configure Web Publishing to an S3 / Backblaze B2 Bucket

1. **Open or Create `~/.config/omapresent/publish.toml`:**
   ```toml
   default = "s3"

   [providers.s3]
   type = "s3"
   endpoint = "https://s3.us-west-002.backblazeb2.com"
   bucket = "my-presentation-bucket"
   prefix = "decks/"
   base_url = "https://slides.mycompany.com"
   region = "us-west-002"
   ```
2. **Set Credentials in Environment:**
   ```bash
   export AWS_ACCESS_KEY_ID="your_key_id"
   export AWS_SECRET_ACCESS_KEY="your_secret_key"
   ```
3. **Publish (with user confirmation):**
   ```bash
   omapresent publish presentation.md --provider s3
   ```

---

## Recipe 5: Prepare a Deck for an Offline Venue

When presenting in environments with unreliable Wi-Fi:

1. **Verify Local Image References:**
   Ensure image paths use filenames indexed in your `root:` folder or local relative paths.
2. **Pre-fetch Video Embeds:**
   Save the file in Omapresent, or let the C++ backend pre-cache embeds into `.omapresent-cache/`.
3. **Test in Offline Mode:**
   Run `omapresent present presentation.md` with network disabled to verify all cached media and QR code fallbacks render cleanly.

---

## Recipe 6: Export Pixel-Identical PDF Slides

To generate high-resolution PDF slides:

```bash
omapresent export --pdf presentation.md
```

- **Canvas Ratio:** Defaults to `16:9` (or the `aspect:` frontmatter setting).
- **Expanded Fragments:** All bullet points and list items are exported fully expanded.
- **Tall Slide Pagination:** Content that extends beyond a single screen is cleanly paginated across multiple landscape pages.
