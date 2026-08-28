# T19 — Live Omarchy acceptance and checklist closeout

**Agent:** `acceptance` · **Spec:** §5.1, §5.3, §6, §15

## Files you own

- `docs/acceptance.md`

You may append to `worklog.md`. Do not edit source, tests, package files, user
configuration, or installed Omarchy files. If you find a product defect, record
it under `NEEDS:` with exact evidence for the owning agent.

## Required work

1. Reconcile every stale `—` row in `docs/acceptance.md` against current tests
   and recorded live evidence. Mark a row complete only when evidence exists.
2. Verify idle inhibit while presentation mode is active, then verify release on
   normal exit. Use read-only compositor or system state checks.
3. Verify current monitor assignment. If two live outputs exist, test projector
   hotplug or output removal and restoration without changing persistent monitor
   configuration. If the hardware cannot provide two outputs, state the exact
   unverified check.
4. Verify the current Omarchy theme-change integration from installed command
   and hook behavior. Do not change the user's theme or configuration. Decide
   whether file watchers satisfy the spec or whether a package hook is required.
5. Recheck the final application after T15 and T16 land. Record only observed
   behavior.

## Verification

Use the built application on the real desktop where safe. Run read-only system
commands only. Append start, checkpoints, and final status to `worklog.md`.
Commit only `docs/acceptance.md` and your appended worklog lines.

## Done when

The checklist matches the tree and real desktop evidence. Every remaining open
row has one exact reason and an owner.

Created by Codex GPT-5 on 2026-08-28 11:31 PT on ombee.
