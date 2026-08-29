import assert from "node:assert/strict";
import test from "node:test";

import {
    classifyBlock,
    countFragments,
    headingText,
    parseImageReference,
    parseSlide,
    slidesForRender,
    splitBlocks,
} from "../../src/renderer/deckparse.js";

test("splitBlocks keeps blank lines inside fenced code and display math", () => {
    const markdown = "# Demo\n\n```js\nconst a = 1;\n\nconst b = 2;\n```\n\n$$\na + b\n\nc + d\n$$";
    const blocks = splitBlocks(markdown);

    assert.equal(blocks.length, 3);
    assert.match(blocks[1].raw, /const a[\s\S]+const b/);
    assert.match(blocks[2].raw, /a \+ b[\s\S]+c \+ d/);
});

test("classification follows the screen and notes table", () => {
    const cases = [
        ["# Heading", "heading", true],
        ["Setext heading\n===", "heading", true],
        ["# Heading\n    Child\n        Grandchild", "outline", true],
        ["- First\n  - Child\n1. Second", "list", true],
        ["```js\nconst answer = 42;\n```", "code", true],
        ["    indented code", "code", true],
        ["A | B\n--- | ---\n1 | 2", "table", true],
        ["> A useful quote", "quote", true],
        ["$$\nx^2\n$$", "math", true],
        ["![[photo.png]]", "image", true],
        ["https://youtu.be/abc", "video", true],
        ["./media/keynote clip.mov", "video", true],
        ["https://example.com/info", "qr", true],
        ["A plain paragraph with *formatting*.", "note", false],
    ];

    for (const [source, type, audience] of cases) {
        const block = classifyBlock(source);
        assert.equal(block.type, type, source);
        assert.equal(block.audience, audience, source);
    }
});

test("a URL inside prose stays a speaker note", () => {
    const block = classifyBlock("Read the details at https://example.com/info before the talk.");
    assert.equal(block.type, "note");
    assert.equal(block.media, null);
});

test("image references preserve spaces and parse Obsidian hints", () => {
    assert.deepEqual(parseImageReference("![[photos/team photo.png|main]]"), {
        reference: "photos/team photo.png",
        alt: "team photo.png",
        main: true,
        maxWidth: null,
        source: "![[photos/team photo.png|main]]",
    });
    assert.equal(parseImageReference("![[photo.png|600]]").maxWidth, 600);
    assert.equal(parseImageReference("![Team](photos/team photo.png)").reference, "photos/team photo.png");
    assert.equal(parseImageReference("./photos/team photo.png").reference, "./photos/team photo.png");
    assert.equal(parseImageReference("./photos/cover").reference, "./photos/cover");
    assert.equal(parseImageReference("/tmp/rendered-slide").reference, "/tmp/rendered-slide");
    assert.equal(parseImageReference("not an image"), null);
});

test("parseSlide separates notes and screen blocks", () => {
    const parsed = parseSlide("# Result\n\nThis is a speaker note with **context**.\n\n- One\n- Two");
    assert.deepEqual(parsed.screenBlocks.map(block => block.type), ["heading", "list"]);
    assert.deepEqual(parsed.noteBlocks.map(block => block.type), ["note"]);
    assert.equal(parsed.fragmentCount, 2);
});

test("parseSlide does not expose prose attached directly to a heading", () => {
    const parsed = parseSlide("# Private context\nThis sentence is only for the speaker.");

    assert.deepEqual(parsed.screenBlocks.map(block => block.raw), ["# Private context"]);
    assert.deepEqual(parsed.noteBlocks.map(block => block.raw), ["This sentence is only for the speaker."]);
});

test("parseSlide keeps every audience form beside prose", () => {
    const parsed = parseSlide([
        "Private introduction.",
        "https://example.com/survey",
        "Another private note.",
        "$$",
        "x^2",
        "$$",
        "Closing note.",
        "```js",
        "const answer = 42;",
        "```",
    ].join("\n"));

    assert.deepEqual(parsed.screenBlocks.map(block => block.type), ["qr", "math", "code"]);
    assert.deepEqual(parsed.noteBlocks.map(block => block.raw), [
        "Private introduction.",
        "Another private note.",
        "Closing note.",
    ]);
});

test("parseSlide keeps an outline before an attached speaker note", () => {
    const parsed = parseSlide("# Plan\n    First\n        Second\nPrivate explanation.");

    assert.deepEqual(parsed.screenBlocks.map(block => block.type), ["outline"]);
    assert.deepEqual(parsed.noteBlocks.map(block => block.raw), ["Private explanation."]);
});

test("fragment counting includes nested list items in document order", () => {
    const markdown = [
        "- Parent",
        "  - Child one",
        "    1. Grandchild",
        "  - Child two",
        "- Final",
    ].join("\n");

    assert.equal(countFragments(markdown), 5);
});

test("fragment counting keeps a list attached directly to a heading", () => {
    const markdown = "# Agenda\n- First\n- Second";
    const parsed = parseSlide(markdown);

    assert.deepEqual(parsed.screenBlocks.map(block => block.type), ["heading", "list"]);
    assert.equal(parsed.fragmentCount, 2);
    assert.equal(countFragments(markdown), 2);
});

test("fragment counting counts later headings in reveal order", () => {
    const markdown = "# First section\n- One\n- Two\n\n## Second section\n- Three\n- Four";
    const parsed = parseSlide(markdown);

    assert.deepEqual(parsed.screenBlocks.map(block => block.type), ["heading", "list", "heading", "list"]);
    assert.equal(parsed.fragmentCount, 5);
    assert.equal(countFragments(markdown), 5);
});

test("fragment counting ignores list-like lines inside code and notes", () => {
    const markdown = "```text\n- not a fragment\n```\n\nThis note has - punctuation.\n\n1. Real fragment";
    assert.equal(countFragments(markdown), 1);
    assert.equal(classifyBlock("    - not a fragment").type, "code");
    assert.equal(countFragments("    1. still code"), 0);
});

test("headingText returns the first ATX or Setext heading", () => {
    assert.equal(headingText("A note\n\n## Where we are\n\n# Later"), "Where we are");
    assert.equal(headingText("A note without a blank\n## Attached heading"), "Attached heading");
    assert.equal(headingText("Quarterly review\n===\n\n# Later"), "Quarterly review");
    assert.equal(headingText("```markdown\n# Example only\n```\n\n## Real heading"), "Real heading");
    assert.equal(headingText("Only prose"), "");
});

test("export slide selection restores skipped recall slides only", () => {
    const slides = [
        { index: 0, skip: false, recallKey: "" },
        { index: -1, skip: true, recallKey: "q" },
        { index: -1, skip: true, recallKey: "" },
    ];
    assert.deepEqual(slidesForRender(slides), [slides[0]]);
    assert.deepEqual(slidesForRender(slides, true), [slides[0], slides[1]]);
});
