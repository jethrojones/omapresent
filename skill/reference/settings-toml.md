# Settings Configuration (`settings.toml`)

Omapresent stores application preferences in `~/.config/omapresent/settings.toml`.

---

## 1. Schema & Default Values

```toml
# ----------------------------------------------------------------------
# Editor Preferences
# ----------------------------------------------------------------------
[editor]
# Text scaling multiplier (follows Omarchy display scale by default)
text_scale = 1.0

# Color appearance: "auto" (follows system portal), "dark", or "light"
dark_mode = "auto"

# Global font family override (defaults to iA Writer Quattro S / system UI font)
# font = "IBM Plex Sans"

# Global theme override (defaults to current Omarchy desktop theme)
# theme = "gruvbox"

# Automatically insert a slide break (\n\n---\n\n) on three consecutive Returns
auto_break_triple_return = true

# Restore window geometry on restart
remember_geometry = true

# ----------------------------------------------------------------------
# Presentation Mode Preferences
# ----------------------------------------------------------------------
[presentation]
# Inhibit system idle and screensaver while presentation is active
inhibit_idle = true

# Enable Do-Not-Disturb (silence system notifications) while presenting
do_not_disturb = true

# Default aspect ratio for slide canvas and exports ("16:9", "4:3", "16:10")
default_aspect = "16:9"

# Toggle speaker notes overlay by default when running on a single display
single_monitor_notes = false

# Pre-fetch and cache online video embeds into .omapresent-cache/ upon file save
auto_prefetch_video = true

# ----------------------------------------------------------------------
# Export Preferences
# ----------------------------------------------------------------------
[export]
# Default canvas aspect ratio for PDF export
pdf_aspect = "16:9"

# Automatically paginate tall slides across multiple PDF pages
pdf_paginated = true
```

---

## 2. Safe Editing Guidance for Agents

When inspecting or updating `settings.toml`:

1. **Read Existing File First:** Never assume default content or overwrite the entire file.
2. **Patch Dotted Keys:** Target the specific key that needs to change (for example, `presentation.do_not_disturb = false`).
3. **Preserve Comments:** Keep all explanatory comments and commented-out templates intact.
4. **Enum Validation:**
   - `dark_mode`: `"auto"`, `"dark"`, or `"light"`
   - `default_aspect` / `pdf_aspect`: `"16:9"`, `"4:3"`, `"16:10"`, `"1:1"`
