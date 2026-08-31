# T44 — Separate package install from source build

**Agent:** `root` · **Spec:** §12

## Why

The README mixes a source build with a package install. Readers must be able to
see when to download the released package and when to clone the source.

## Files you own

- `tasks/t44-readme-install-source.md` — this file
- `README.md` — installation wording only
- `worklog.md` — append only

## Done when

The README gives one path for the prebuilt GitHub release package and a separate
path for building the application from source. Each command must match the
project scripts and package format.

Created by Codex GPT-5.6 Sol on 2026-08-31 11:39 PT on ombee.
