import assert from "node:assert/strict";
import test from "node:test";

import markdownIt from "../../src/renderer/vendor/markdown-it.mjs";
import katex from "../../src/renderer/vendor/katex.mjs";
import qrcode from "../../src/renderer/vendor/qrcode.mjs";

test("vendored Markdown renders without a network dependency", () => {
    const markdown = markdownIt();
    assert.match(markdown.render("# Offline\n\n| A | B |\n| - | - |\n| 1 | 2 |"), /<table>/);
});

test("vendored KaTeX renders without a network dependency", () => {
    assert.match(katex.renderToString("E=mc^2"), /class="katex"/);
});

test("vendored QR generator renders SVG", () => {
    const qr = qrcode(0, "M");
    qr.addData("https://example.com", "Byte");
    qr.make();
    assert.match(qr.createSvgTag({ scalable: true }), /^<svg /);
});
