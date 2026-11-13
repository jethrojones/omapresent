import assert from "node:assert/strict";
import { access, readFile, readdir } from "node:fs/promises";
import { dirname, resolve } from "node:path";
import test from "node:test";
import { fileURLToPath } from "node:url";

// The live terminal (`--- {terminal}`) is drawn by xterm.js, which must never
// reach a published deck: a bundle carries every file under src/renderer/, and
// a published deck has no shell to attach a terminal to. These check that the
// emulator stays where it was put, and that what the app compiles in is there.

const here = dirname(fileURLToPath(import.meta.url));
const renderer = resolve(here, "../../src/renderer");
const terminal = resolve(here, "../../src/terminal");

async function filesUnder(directory, prefix = "") {
    const found = [];
    for (const entry of await readdir(directory, { withFileTypes: true })) {
        const relative = prefix ? `${prefix}/${entry.name}` : entry.name;
        if (entry.isDirectory())
            found.push(...await filesUnder(resolve(directory, entry.name), relative));
        else
            found.push(relative);
    }
    return found;
}

test("the terminal emulator is not part of the renderer bundle", async () => {
    const files = await filesUnder(renderer);
    assert.deepEqual(files.filter(name => /xterm|addon-fit|terminal/i.test(name)), []);

    const qrc = await readFile(resolve(renderer, "renderer.qrc"), "utf8");
    assert.doesNotMatch(qrc, /xterm|terminal/i);

    // Fetched on demand from the app's own resources, never imported: an
    // import would be followed into the published bundle's module graph.
    const js = await readFile(resolve(renderer, "render.js"), "utf8");
    assert.doesNotMatch(js, /^\s*import\b[^\n]*(?:xterm|terminal)/m);
    assert.doesNotMatch(js, /\bimport\s*\(/);
    assert.match(js, /const terminalBase = "qrc:\/terminal\/"/);
});

test("the terminal resource manifest names files that exist, with their licences", async () => {
    const qrc = await readFile(resolve(terminal, "terminal.qrc"), "utf8");
    assert.match(qrc, /<qresource prefix="\/terminal">/);
    const entries = [...qrc.matchAll(/<file>([^<]+)<\/file>/g)].map(match => match[1]);
    await Promise.all(entries.map(entry => access(resolve(terminal, entry))));
    for (const name of ["terminal.js", "terminal.css", "vendor/xterm.js", "vendor/xterm.css",
        "vendor/addon-fit.js", "vendor/LICENSES.md", "vendor/LICENSE.xterm",
        "vendor/LICENSE.addon-fit"])
        assert.ok(entries.includes(name), name);

    // Everything vendored is in the manifest, so nothing ships unlicensed and
    // nothing is licensed that does not ship.
    for (const file of await readdir(resolve(terminal, "vendor")))
        assert.ok(entries.includes(`vendor/${file}`), file);
});

test("the renderer treats a terminal slide as a slide with no reveals", async () => {
    const js = await readFile(resolve(renderer, "render.js"), "utf8");
    assert.match(js, /if \(isLiveTerminal\(slides\[currentSlide\]\)\)\s*return 0;/);
    // Decided by the host and the role the host assigned, not by deck.mode,
    // which a running presentation shares with the editor preview.
    assert.match(js, /function showsLiveTerminals\(\) \{\s*return Boolean\(terminalHost\(\)\) && \(role === "audience" \|\| role === "presenter"\);/);
    for (const member of ["terminalWrite", "terminalSetSize", "terminalSetFocus", "terminalSetZoom", "hostReady"])
        assert.match(js, new RegExp(`\\b${member}\\(`), member);
});

test("the page asks to leave the terminal on Shift+Escape and nothing else", async () => {
    const js = await readFile(resolve(terminal, "terminal.js"), "utf8");
    assert.match(js, /event\.key === "Escape" && event\.shiftKey\s*&& !event\.ctrlKey && !event\.altKey && !event\.metaKey/);
    assert.equal((js.match(/terminalLeave\(\)/g) ?? []).length, 1);
});
