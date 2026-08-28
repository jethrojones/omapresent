# T6 — Web publish and the provider layer

**Agent:** `publish` · **Spec:** §9, §15 milestone 7

## Files you own
- `src/publisher.cpp` (implement; `src/publisher.h` is frozen — may add)
- `tests/tst_publisher.cpp`

Nothing else.

## SAFETY — read this first
Publishing sends the user's deck to an external host. Nothing in this class may
upload without an explicit, user-initiated call to `publish()`. No "helpfully
pre-warm the connection", no upload on save, no telemetry. **Your tests must
never make a network call.**

## What to build

### The config file
`~/.config/omapresent/publish.toml`, shaped as in spec §9. A missing file means
one provider: anonymous `herenow`. `providers()` returns
`{ name -> { "type": ..., ...keys } }`.

`setProviderKey` patches **exactly one key** and writes the file back with every
comment, blank line, key order and unknown key byte-identical (spec §11: read
the file, patch one key, keep the rest — never rewrite wholesale). `patchToml`
is the pure function that does it, and it must insert the key, and its `[table]`
header, when they are absent.

### Provider types for v1
- **`herenow`** — the built-in default. The flow, verified against
  `https://here.now/openapi.json`:
  `POST /api/v1/publish` → `{slug, siteUrl, versionId, claimToken,
  presignedUploads[{path,url}]}` → `PUT` each bundle file to its presigned URL →
  `POST /api/v1/publish/{slug}/finalize` with `{versionId}` (idempotent by
  `versionId`). `POST /api/v1/publish/{slug}/uploads/refresh` re-presigns when
  URLs expire — handle that, uploads of a big deck do time out.
  Updates: `PUT /api/v1/publish/{slug}` stages a new version;
  `GET .../versions` and `POST .../versions/{versionId}/restore` give history
  and instant rollback — expose them as "republish" and "revert".
  No `Authorization` header → a live `{slug}.here.now` URL that expires in 24h,
  plus a `claimToken` for `POST /api/v1/publish/{slug}/claim` later. That is the
  zero-config quick-share default.
  Authenticated: `Authorization: Bearer <api_key>`, obtained through
  `POST /api/auth/agent/request-code` then `POST /api/auth/agent/verify-code` —
  that is `requestSignInCode` / `verifySignInCode`.
  Custom domain: `POST /api/v1/domains` once, then `POST /api/v1/mounts` with
  `{domain, mount_path: "/presentations/<slug>", slug}` per deck.
  Access: `PATCH /api/v1/publish/{slug}/access` with
  `mode ∈ {anyone_with_link, password, restricted, account_members}`.
  Map the frontmatter's `link|public|password|restricted` onto those.
- **`command`** — the escape hatch. Run the configured command with
  `$OMAPRESENT_BUNDLE` (the bundle dir) and `$OMAPRESENT_SLUG` in the
  environment; the **last line of stdout** is the live URL. Never run it through
  a shell you built by string concatenation of user data beyond what the config
  itself specifies; the command string is the user's own config, the slug and
  bundle path are passed as environment variables, not interpolated.
- **`s3`** — any S3-compatible endpoint: `endpoint`, `bucket`, `prefix`,
  `base_url`. Signature v4, content types set per extension.

### `slugify`
Explicit `publish.slug`, else the title, else the filename. Lowercase,
non-alphanumerics collapsed to single hyphens, leading/trailing hyphens
trimmed, non-ASCII transliterated or dropped, never empty (fall back to
`"deck"`), and idempotent.

### Progress and failure
Emit `progress(done, total, what)` per file. Every failure path ends in
`failed(message)` with something a human can act on — not "error 400".

## Tests
`tests/tst_publisher.cpp`, registered with `OMAPRESENT_TEST_SUITE` — no
`QTEST_MAIN`, **no network**. Cover `slugify` (unicode, punctuation runs, empty,
already-a-slug), `parseToml` against the full example in spec §9, and
`patchToml` hard: patching an existing key, adding a key to an existing table,
adding a whole new table, and — the important one — asserting that comments,
spacing and unrelated keys come back **byte-identical**. Drive `providers()` off
a `QTemporaryDir` config file, including the missing-file default.

## Done when
`./bin/build && ./bin/test` pass, your suite has real cases and no network, and
your worklog entry is appended.
