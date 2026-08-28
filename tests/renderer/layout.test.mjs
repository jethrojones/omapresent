import assert from "node:assert/strict";
import test from "node:test";

import { parseSlide } from "../../src/renderer/deckparse.js";
import {
    bentoArrangement,
    fitDecision,
    layoutForBlocks,
    scrollTopForFraction,
} from "../../src/renderer/layout.js";

const layoutCases = [
    ["heading alone", "# Title", "title"],
    ["heading and image without a blank", "# Title\n![[photo.png]]", "heading-image-tight"],
    ["heading and image with a blank", "# Title\n\n![[photo.png]]", "heading-image-spaced"],
    ["image alone", "![[photo.png]]", "image-single"],
    ["consecutive images", "![[one.png]]\n![[two.png]]", "images-bento"],
    ["separated images", "![[one.png]]\n\n![[two.png]]", "images-stacked"],
    ["indented outline", "# Plan\n    First\n        Second", "outline"],
    ["list", "- First\n- Second", "list"],
    ["code", "```js\nconst n = 1;\n```", "centered-block"],
    ["table", "A | B\n--- | ---\n1 | 2", "centered-block"],
    ["quote", "> Make it clear.", "centered-block"],
    ["math", "$$\nx^2\n$$", "centered-block"],
    ["prose", "Speaker notes only.", "notes-only"],
];

test("layout grammar covers every specified arrangement", () => {
    for (const [name, markdown, expected] of layoutCases) {
        const layout = layoutForBlocks(parseSlide(markdown).blocks);
        assert.equal(layout.kind, expected, name);
    }
});

test("bento arrangements map 2, 3, 4, 5 and 6 images", () => {
    const cases = [
        [2, "bento-2", 2, 1],
        [3, "bento-3", 3, 1],
        [4, "bento-4", 2, 2],
        [5, "bento-mosaic", 6, 2],
        [6, "bento-mosaic", 6, 3],
    ];

    for (const [count, kind, columns, rows] of cases) {
        const layout = bentoArrangement(count);
        assert.equal(layout.kind, kind);
        assert.equal(layout.columns, columns);
        assert.equal(layout.rows, rows);
        assert.equal(layout.tiles.length, count);
        assert.deepEqual(layout.tiles.map(tile => tile.index), Array.from({ length: count }, (_, index) => index));
    }
});

test("main image becomes the hero tile without changing document order", () => {
    const layout = bentoArrangement(6, 3);
    assert.equal(layout.kind, "bento-hero");
    assert.equal(layout.heroIndex, 3);
    assert.equal(layout.tiles[3].role, "hero");
    assert.ok(layout.tiles[3].columnSpan > layout.tiles[0].columnSpan);
});

test("outside the closed bento set images fall back to a stack", () => {
    assert.equal(bentoArrangement(1).kind, "stacked");
    assert.equal(bentoArrangement(7).kind, "stacked");
});

test("fit decisions never shrink content and clamp scroll state", () => {
    assert.deepEqual(fitDecision(600, 800, 30), {
        fits: true,
        scrollable: false,
        alignment: "center",
        maxScroll: 0,
        scrollTop: 0,
        scrollFraction: 0,
    });

    assert.deepEqual(fitDecision(1400, 800, 300), {
        fits: false,
        scrollable: true,
        alignment: "top",
        maxScroll: 600,
        scrollTop: 300,
        scrollFraction: 0.5,
    });

    assert.equal(fitDecision(1400, 800, 900).scrollTop, 600);
    assert.equal(scrollTopForFraction(1400, 800, 0.25), 150);
    assert.equal(scrollTopForFraction(1400, 800, 4), 600);
    assert.equal(scrollTopForFraction(600, 800, 0.5), 0);
});
