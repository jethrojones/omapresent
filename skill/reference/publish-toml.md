# Publish Configuration (`publish.toml`)

Omapresent supports pluggable web publishing targets configured in `~/.config/omapresent/publish.toml`.

Publishing builds a self-contained, offline-capable static web bundle containing:
1. **Deck View:** An interactive, presentation-ready slide viewer with toggleable speaker note subtitles.
2. **Long Read:** A beautifully formatted, single-page reading article.

---

## 1. Safety & Confirmation

> **CRITICAL AGENT SAFETY RULE:**
> `omapresent publish` uploads slides and media to external network endpoints. **Never execute `omapresent publish` without explicit confirmation from the user.**

---

## 2. Configuration Schema

```toml
# Default provider used when publish.provider is not specified in deck frontmatter
default = "herenow"

# ----------------------------------------------------------------------
# 1. here.now Provider (Built-in Default)
# ----------------------------------------------------------------------
[providers.herenow]
type = "herenow"

# API Key:
# - If omitted or empty: Publishes as an anonymous 24-hour temporary link (e.g. https://slug.here.now)
# - If set: Publishes permanently to your authenticated here.now account
api_key = "hn_xxxxxxxxxxxxxxxx"

# Optional custom domain registered on here.now (e.g. "omapresent.com")
domain = "omapresent.com"

# Optional path prefix on the custom domain (deck lands at https://<domain>/presentations/<slug>)
mount_prefix = "/presentations"

# ----------------------------------------------------------------------
# 2. Command Provider (Generic Shell / Script Escape Hatch)
# ----------------------------------------------------------------------
[providers.mybox]
type = "command"

# Command executed during publish.
# Environment variables provided by Omapresent:
#   $OMAPRESENT_BUNDLE - Absolute path to the generated static bundle directory
#   $OMAPRESENT_SLUG   - Resolved deck slug
# The last line printed to stdout by this command is captured as the live URL.
publish = "rsync -avz --delete $OMAPRESENT_BUNDLE/ me@host:/var/www/decks/$OMAPRESENT_SLUG/ && echo https://decks.example.com/$OMAPRESENT_SLUG"

# ----------------------------------------------------------------------
# 3. S3-Compatible Provider (AWS S3, Cloudflare R2, Backblaze B2, MinIO)
# ----------------------------------------------------------------------
[providers.s3]
type = "s3"
endpoint = "https://s3.us-west-002.backblazeb2.com"
bucket = "my-decks"
prefix = "presentations/"
base_url = "https://decks.example.com"
region = "us-west-002"

# Optional credentials (if omitted, standard AWS_ACCESS_KEY_ID / AWS_SECRET_ACCESS_KEY env vars are used)
access_key_id = "AKIA..."
secret_access_key = "secret..."
```

---

## 3. Provider Details

### `herenow` Provider
- **Zero-Config Anonymous Mode:** If `publish.toml` does not exist or `api_key` is blank, Omapresent publishes anonymously. A claim token is stored locally to allow claiming the site to an account later.
- **Custom Domains & Mounts:** Multiple presentations can share a single custom domain under different paths using `mount_prefix`.
- **Access Modes:** Frontmatter `publish.access` controls access permissions:
  - `link` (default): Anyone with the secret URL link can view.
  - `public`: Listed publicly on your domain index.
  - `password`: Password-protected via here.now access control.
  - `restricted`: Restricts access to specified email domains or users.

### `command` Provider
- Executes any local script or shell pipeline.
- Synchronously passes `$OMAPRESENT_BUNDLE` and `$OMAPRESENT_SLUG`.
- Must return exit code 0 and emit the live URL on its final stdout line.

### `s3` Provider
- Uploads the bundle files recursively with appropriate MIME types and cache headers.
- Formats final URL as `{base_url}/{prefix}{slug}/index.html`.

---

## 4. Safe Configuration Editing Rules

When automating or editing `~/.config/omapresent/publish.toml`:

1. **Read Before Writing:** Always read the existing file content first.
2. **Patch Specific Keys:** Modify only the targeted setting (e.g. `default`, `providers.herenow.api_key`).
3. **Preserve Structure:** Never overwrite the file with a blank or partial template. Retain all user comments, blank lines, and custom providers.
4. **Valid Provider Types:** `type` must be one of `"herenow"`, `"command"`, or `"s3"`.
