# Omapresent release gate

The source package uses the GitHub tag archive. The archive also contains
`pkgbuild/PKGBUILD`, so its checksum cannot be embedded in the same tag.

Use this order for the first release:

1. Run `./bin/build && ./bin/test` and inspect the final owned diff.
2. Commit the release-ready tree. Create `v0.1.0` on that commit.
3. After the orchestrator approves external publication, push the branch and
   tag to GitHub.
4. Run `pkgbuild/release-checksums v0.1.0`. It hashes the immutable GitHub tag
   archive and updates both the local PKGBUILD and the staged Omarchy copy.
5. Commit and push that checksum update. Do not move the release tag.
6. Create the GitHub release from `v0.1.0` with the prepared notes in
   `.github/release-notes/v0.1.0.md`.

The release workflow checks the tag version on tag push. On the published
release event it checks the checksum on the default branch, builds the package
from that PKGBUILD and the immutable tag source, runs `namcap`, and attaches
the package to the release.

## Omarchy package submission

The exact package tree is staged under `pkgbuild/omarchy-pkgs/omapresent/`.
Copy that directory to `pkgbuilds/omapresent/` in a branch of
`omacom-io/omarchy-pkgs`, then open a pull request to its `master` branch.
The metadata declares `{ "source": "local" }`, which means Omarchy owns this
PKGBUILD instead of syncing it from the AUR. The package enters the normal
`edge` build path and reaches `stable` through the repository promotion flow.

Created by Codex GPT-5 on 2026-08-28 12:34 PT on ombee.
