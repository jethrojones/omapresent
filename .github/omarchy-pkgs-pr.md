# Omarchy package submission

Target repository: `omacom-io/omarchy-pkgs`

Copy `pkgbuild/omarchy-pkgs/omapresent/` to
`pkgbuilds/omapresent/` on a branch. Open a pull request to `master` after the
Omapresent tag checksum is filled by `pkgbuild/release-checksums`.

The package is a local Omarchy package. Its metadata uses
`{ "source": "local" }`. It is not an AUR sync and it is not marked for the
fast ring. The normal path is edge build, test, and explicit promotion to
stable.

PR title:

`Add omapresent Markdown presentation package`

PR body:

`Adds Omapresent as a local package. The PKGBUILD builds the immutable GitHub
tag source with a verified SHA-256 checksum, installs the desktop entry and
hicolor icons, and ships the app, vendor, and skill licences. Local package
validation passed with makepkg and namcap; the release workflow attaches the
package artifact after the GitHub release gate.`

Created by Codex GPT-5 on 2026-08-28 12:34 PT on ombee.
