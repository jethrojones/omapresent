# T32 README uninstall directions

## Owned files

- `README.md`
- `tasks/t32-readme-uninstall.md`
- `worklog.md` (append-only T32 entries)

## Verified sources

- `bin/install` runs `makepkg -fsi`.
- Both package files set `pkgname=omapresent`.
- The Omarchy `pkg drop` helper removes named packages with `pacman -Rns`.
- The package has no uninstall script and owns only system paths. Settings live
  at `~/.config/omapresent/settings.toml`.

## Scope

Add a short README `Uninstall` section under Installation. Document the exact
Omarchy command, the package removal command for `./bin/install`, and the
preservation of user decks and settings. Change no other files.
