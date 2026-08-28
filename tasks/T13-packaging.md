# T13 — Make the package actually build and install

**Agent:** `media` (continuing) · **Spec:** §12, §14.5, §15 milestone 9

## Files you own (in addition to your T5 files)
- `pkgbuild/PKGBUILD`, `pkgbuild/omapresent.install`,
  `pkgbuild/omapresent.desktop`, `pkgbuild/omapresent.svg`

You are taking these over from the `skill-docs` agent, whose provider hit its
quota. Its work is committed and is a reasonable start; it was never verified by
actually running `makepkg`, which is the whole point of this task.

Do not touch `skill/`, `welcome/` or `README.md` — those are finished.

## Why this exists

`bash -n PKGBUILD` proves the file is valid shell. It does not prove the package
builds, that its dependencies exist, or that the installed tree is right. Nobody
has run `makepkg` yet. Until someone does, "packaged" is an assumption.

## What to do

### 1. Actually build it
Run `makepkg` (not `-i`) in `pkgbuild/` and make it succeed. Use
`makepkg --printsrcinfo`, `namcap` on the resulting package and on the PKGBUILD
if they are available, and fix what they report. Build in a scratch directory,
and do not install the package system-wide.

### 2. Things I can already see are wrong

- **`qt6-quickcontrols2` is not a package on Arch.** Quick Controls ships inside
  `qt6-declarative`. As written, this dependency cannot be satisfied and the
  package will not install. Check every entry in `depends` and `makedepends`
  against what actually exists (`pacman -Si <name>`), and drop or correct the
  ones that do not.
- **`install -Dm644 skill/reference/*.md "$pkgdir/.../reference/"`** passes
  several sources to `install -D`, which expects a single source and a single
  destination. Use `-t` for a directory target, or loop.
- The `build()` and `package()` functions `cd "$startdir/.."`, which only works
  when makepkg is invoked from `pkgbuild/` inside a full checkout. That is fine
  for a local build but it is not how the Omarchy repo will consume this. Decide
  deliberately: either keep the local-checkout form and say so in a comment, or
  add a real `source=()` pointing at a git tag with `sha256sums`. Whichever you
  choose, write down why in the worklog.

### 3. Verify the installed tree, not just the exit code
After `makepkg`, inspect the built package (`tar tf` the `.pkg.tar.zst`, or use
`makepkg` output dir) and confirm every path the spec promises is present and in
the right place: the binary, `LICENSE`, `NOTICE`, `/usr/share/omapresent/welcome.md`,
the whole `skill/` tree, the `.desktop` file with `MimeType=text/markdown;`, and
the icon. A file silently missing from the package is the failure mode here.

### 4. The `.install` hook
Confirm it is valid, that it does not fail when the icon cache or desktop
database tools are absent, and that `post_remove` undoes what `post_install`
did. Per spec §14.5 the hook ships the skill to `/usr/share/omapresent/skill/`
and the **app** does the per-user symlink on first run — that half belongs to
the `app-shell` agent, so do not add it here.

### 5. Do not
Bundle fonts (§12/§14.2 — depend on the iA Writer S family Omarchy already
ships). Add `yt-dlp` (§12, explicitly excluded).

## Done when
`makepkg` succeeds from a clean tree, the built package contains everything
listed above, `namcap` is clean or its remaining warnings are explained in your
worklog, and your worklog entry is appended.
