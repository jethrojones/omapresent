# T35 — Slide heading fragment order

## Task
Fix renderer reveal sequencing so heading-only blocks can be progressive when they follow a prior heading on the same slide.

## Scope
- `src/renderer/deckparse.js`
- `src/renderer/render.js`
- `tests/renderer/deckparse.test.mjs`
- `tests/renderer/fixture-deck.js`
- `tests/renderer/interaction.test.mjs`
- `tasks/t35-heading-fragment-order.md` (owner notes)
- `worklog.md`

## Current finding
On welcome slide 19 (`# Presenter Mode & Multi-Monitor`), `## Share the audience window` is rendered from `recall` start while the first list bullets are hidden by fragment count.

The expected sequence is:
1. initial: first heading visible, list fragment `1` hidden, second heading hidden
2. reveal first list item
3. reveal second list item
4. ... continue list in order ...
5. after last first-list item, reveal second heading
6. reveal second-list items in order

## Notes
This must work with existing recall and slide-overflow logic in `slideElement()` and use existing fragment state APIs.
