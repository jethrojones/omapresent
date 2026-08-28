import assert from "node:assert/strict";
import { access, readFile, readdir } from "node:fs/promises";
import { dirname, resolve } from "node:path";
import test from "node:test";
import { fileURLToPath } from "node:url";

const here = dirname(fileURLToPath(import.meta.url));
const renderer = resolve(here, "../../src/renderer");

async function source(name) {
    return readFile(resolve(renderer, name), "utf8");
}

test("render page has one module entry and no remote resources", async () => {
    const html = await source("render.html");
    assert.match(html, /<main id="deck"/);
    assert.equal((html.match(/<script/g) ?? []).length, 1);
    assert.equal(/(?:src|href)="https?:\/\//.test(html), false);
});

test("DOM shell exposes every frozen API member", async () => {
    const js = await source("render.js");
    for (const member of ["render", "update", "goto", "next", "previous", "scrollBy", "setScroll", "showRecall", "hideRecall", "setBlank", "setOverview", "playPause", "focusNextMedia", "onState"])
        assert.match(js, new RegExp(`\\b${member}\\b`), member);
    assert.match(js, /Object\.defineProperty\(api, "role"/);
    assert.match(js, /window\.omapresent = api/);
    assert.match(js, /omapresentHost\.state\(JSON\.stringify\(state\)\)/);
    assert.match(js, /flowAllBlocks/);
    assert.match(js, /deck\.mode === "web" \? "web"/);
    assert.match(js, /const player = players\[0\]/);
    assert.match(js, /activeMediaIndex = fromIndex >= 0 \? \(fromIndex \+ 1\) % players\.length : 0/);
});

test("deck CSS has no literal color values", async () => {
    const css = await source("deck.css");
    assert.equal(/#[0-9a-f]{3,8}\b/i.test(css), false);
    assert.equal(/\b(?:rgb|rgba|hsl|hsla|oklch|lab|lch)\s*\(/i.test(css), false);
    assert.equal(/:\s*(?:black|white|red|blue|green|yellow|orange|magenta|cyan|brown)\b/i.test(css), false);
});

test("read mode has an article layout branch", async () => {
    const css = await source("deck.css");
    assert.match(css, /html\[data-op-view="read"\] #deck\.op-all-slides \{[\s\S]*?max-width: 38rem;[\s\S]*?line-height: 1\.65;/);
    assert.match(css, /html\[data-op-view="read"\] #deck\.op-all-slides > \.op-slide \{[\s\S]*?min-height: 0;[\s\S]*?overflow: visible;/);
    assert.match(css, /html\[data-op-view="read"\] #deck h1,[\s\S]*?font-size: 2\.75rem;[\s\S]*?text-align: left;/);
    assert.match(css, /html\[data-op-view="read"\] #deck \.op-notes\.is-flow-note \{[\s\S]*?display: block;[\s\S]*?background: transparent;/);
    assert.doesNotMatch(css, /html\[data-op-view="read"\] (?:h[1-6]|p|ul|ol|table|blockquote)\b/);
});

test("print mode lets long slides paginate without clipping", async () => {
    const css = await source("deck.css");
    const js = await source("render.js");
    const print = css.slice(css.indexOf("@media print"));
    assert.match(print, /\.op-all-slides > \.op-slide \{[\s\S]*height: auto;[\s\S]*overflow: visible;/);
    assert.match(print, /\.op-all-slides > \.op-slide > \.op-scroll \{[\s\S]*overflow: visible;/);
    assert.match(print, /min-height: var\(--op-print-slide-height, 100vh\)/);
    assert.match(js, /addEventListener\("beforeprint", preparePrintPageHeights\)/);
});

test("overview owns a viewport scroll surface", async () => {
    const css = await source("deck.css");
    assert.match(css, /\.op-overview-root \{[\s\S]*?height: 100vh;[\s\S]*?overflow: auto;[\s\S]*?\}/);
});

test("normal images keep full scaled height so tall images scroll", async () => {
    const css = await source("deck.css");
    const normalImageRule = css.match(/\.op-image img \{([\s\S]*?)\}/)?.[1] ?? "";
    assert.match(normalImageRule, /width: 100%/);
    assert.match(normalImageRule, /height: auto/);
    assert.doesNotMatch(normalImageRule, /max-height/);
});

test("resource manifest includes every renderer dependency", async () => {
    const qrc = await source("renderer.qrc");
    for (const name of [
        "render.html", "render.js", "deck.css", "deckparse.js", "layout.js", "media.js",
        "vendor/markdown-it.mjs", "vendor/katex.mjs", "vendor/katex.css", "vendor/qrcode.mjs",
        "vendor/LICENSES.md",
    ])
        assert.match(qrc, new RegExp(`<file>${name.replaceAll(".", "\\.")}</file>`), name);

    const entries = [...qrc.matchAll(/<file>([^<]+)<\/file>/g)].map(match => match[1]);
    await Promise.all(entries.map(entry => access(resolve(renderer, entry))));
    const fonts = await readdir(resolve(renderer, "vendor/fonts"));
    for (const font of fonts)
        assert.ok(entries.includes(`vendor/fonts/${font}`), font);
});

test("standalone fixture loads the production shell", async () => {
    const html = await readFile(resolve(here, "fixture.html"), "utf8");
    const data = await readFile(resolve(here, "fixture-deck.js"), "utf8");
    assert.match(html, /src="\.\.\/\.\.\/src\/renderer\/render\.js"/);
    assert.match(data, /globalThis\.omapresentFixture/);
});
