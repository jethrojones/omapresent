import assert from "node:assert/strict";
import { execFile } from "node:child_process";
import { once } from "node:events";
import { access, mkdtemp, rm } from "node:fs/promises";
import { createServer } from "node:http";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import test from "node:test";
import { promisify } from "node:util";
import { fileURLToPath, pathToFileURL } from "node:url";

const execute = promisify(execFile);
const here = dirname(fileURLToPath(import.meta.url));
const chromium = "/usr/bin/chromium";

async function hasChromium() {
    try {
        await access(chromium);
        return true;
    } catch {
        return false;
    }
}

function attribute(html, name) {
    return html.match(new RegExp(`${name}="([^"]*)"`))?.[1] ?? "";
}

async function fixtureDom(search, extraArguments = []) {
    const fixture = pathToFileURL(resolve(here, "fixture.html"));
    fixture.search = search;
    const profile = await mkdtemp(join(tmpdir(), "omapresent-chromium-"));
    try {
        const { stdout } = await execute(chromium, [
            "--headless", "--no-sandbox", "--disable-gpu", "--allow-file-access-from-files",
            `--user-data-dir=${profile}`, "--virtual-time-budget=1500", ...extraArguments,
            "--dump-dom", fixture.href,
        ], { maxBuffer: 2 * 1024 * 1024 });
        return stdout;
    } finally {
        await rm(profile, { recursive: true, force: true });
    }
}

test("fragments reveal in DOM order and report every state through the host callback", {
    skip: !(await hasChromium()),
}, async () => {
    const html = await fixtureDom("?metrics=interaction");
    assert.equal(attribute(html, "data-fragment-steps"), "0000,1000,1100,1110,1111");
    assert.equal(attribute(html, "data-host-state-serialized"), "true");
    assert.ok(Number(attribute(html, "data-host-state-count")) >= 6);
    assert.equal(attribute(html, "data-host-last-slide"), "2");
    assert.equal(attribute(html, "data-host-last-fragment"), "0");
});

test("heading fragments reveal in order after prior bullets", {
    skip: !(await hasChromium()),
}, async () => {
    const html = await fixtureDom("?metrics=heading-fragments");
    const expected = [
        "L1:0|L2:0|H3:0|L4:0|L5:0",
        "L1:1|L2:0|H3:0|L4:0|L5:0",
        "L1:1|L2:1|H3:0|L4:0|L5:0",
        "L1:1|L2:1|H3:1|L4:0|L5:0",
        "L1:1|L2:1|H3:1|L4:1|L5:0",
        "L1:1|L2:1|H3:1|L4:1|L5:1",
    ];
    const steps = attribute(html, "data-heading-fragment-order").split(";");
    assert.equal(steps.length, 6);
    assert.deepEqual(steps, expected);
});

test("recall overlays preserve all fragments and restore moved underlying state after host goto", {
    skip: !(await hasChromium()),
}, async () => {
    const html = await fixtureDom("?metrics=recall");
    assert.equal(attribute(html, "data-recall-overlay-visible"), "true");
    assert.equal(attribute(html, "data-recall-overlay-fragments-revealed"), "true");
    assert.equal(attribute(html, "data-recall-overlay-fragment-count"), "4");
    assert.equal(attribute(html, "data-recall-after-update-overlay-visible"), "true");
    assert.equal(attribute(html, "data-recall-after-update-overlay-fragments-revealed"), "true");
    assert.equal(attribute(html, "data-recall-after-update-overlay-fragment-count"), "4");
    assert.equal(attribute(html, "data-recall-before-slide"), "0");
    assert.equal(attribute(html, "data-recall-after-goto-slide"), "1");
    assert.equal(attribute(html, "data-recall-after-slide"), "1");
    assert.equal(attribute(html, "data-recall-before-fragment"), attribute(html, "data-recall-after-fragment"));
    assert.equal(attribute(html, "data-recall-after-goto-slide"), attribute(html, "data-recall-after-slide"));
    assert.equal(attribute(html, "data-recall-after-goto-fragment"), "2");
    assert.equal(attribute(html, "data-recall-before-scroll-top"), attribute(html, "data-recall-after-scroll-top"));
    assert.equal(attribute(html, "data-recall-after-scroll-top"), attribute(html, "data-recall-after-goto-scroll-top"));
    assert.ok(Number(attribute(html, "data-recall-after-state-count")) >= 4);
});

test("remote video sources do not enter the DOM until play", {
    skip: !(await hasChromium()),
}, async () => {
    const html = await fixtureDom("?metrics=remote-media", ["--host-resolver-rules=MAP * 127.0.0.1"]);
    assert.equal(attribute(html, "data-loader-media-active"), "true");
    assert.equal(attribute(html, "data-direct-before-play"), "false");
    assert.equal(attribute(html, "data-direct-placeholder"), "true");
    assert.equal(attribute(html, "data-direct-space-prevented"), "false");
    assert.equal(attribute(html, "data-direct-enter-prevented"), "false");
    assert.equal(attribute(html, "data-direct-after-play"), "true");
    assert.equal(attribute(html, "data-direct-preload"), "none");
    assert.equal(attribute(html, "data-embed-before-play"), "false");
    assert.equal(attribute(html, "data-embed-placeholder"), "true");
    assert.equal(attribute(html, "data-embed-after-play"), "true");
    assert.equal(attribute(html, "data-embed-autoplay"), "1");
    assert.equal(attribute(html, "data-embed-allows-autoplay"), "true");
    assert.equal(attribute(html, "data-cached-placeholder"), "false");
    assert.equal(attribute(html, "data-cached-source"), "file:///tmp/cached-test.mp4");
    assert.equal(attribute(html, "data-cached-preload"), "metadata");
});

test("remote images make zero requests until the reader loads them", {
    skip: !(await hasChromium()),
}, async () => {
    let imageRequests = 0;
    const pixel = Buffer.from("iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNk+A8AAQUBAScY42YAAAAASUVORK5CYII=", "base64");
    const server = createServer((request, response) => {
        if (request.url === "/remote.png") {
            imageRequests += 1;
            response.writeHead(200, { "content-type": "image/png", "content-length": pixel.length });
            response.end(pixel);
            return;
        }
        response.writeHead(404).end();
    });
    server.listen(0, "127.0.0.1");
    await once(server, "listening");
    const port = server.address().port;
    try {
        const closed = await fixtureDom(`?metrics=remote-image&imagePort=${port}`);
        assert.equal(attribute(closed, "data-remote-image-before-load"), "false");
        assert.equal(attribute(closed, "data-remote-image-placeholder"), "true");
        assert.equal(attribute(closed, "data-remote-image-after-load"), "false");
        assert.equal(attribute(closed, "data-remote-image-space-prevented"), "false");
        assert.equal(attribute(closed, "data-remote-image-enter-prevented"), "false");
        assert.equal(imageRequests, 0);

        const opened = await fixtureDom(`?metrics=remote-image&imagePort=${port}&load=1`);
        assert.equal(attribute(opened, "data-remote-image-before-load"), "false");
        assert.equal(attribute(opened, "data-remote-image-after-load"), "true");
        assert.equal(imageRequests, 1);
    } finally {
        server.close();
        await once(server, "close");
    }
});

test("read view uses article type and flows speaker notes into the body", {
    skip: !(await hasChromium()),
}, async () => {
    const html = await fixtureDom("?view=read&metrics=read");
    assert.equal(attribute(html, "data-read-heading-size"), "33.6px");
    assert.equal(attribute(html, "data-read-second-heading-size"), "26.4px");
    assert.equal(attribute(html, "data-read-heading-align"), "left");
    assert.equal(attribute(html, "data-read-slide-min-height"), "0px");
    assert.equal(attribute(html, "data-read-section-margin"), "32px");
    assert.equal(attribute(html, "data-read-section-padding"), "32px");
    assert.equal(attribute(html, "data-read-stack-display"), "block");
    assert.equal(attribute(html, "data-read-note-display"), "block");
    assert.equal(attribute(html, "data-read-note-text"), "This paragraph is a speaker note.");
    assert.ok(Number(attribute(html, "data-read-measure")) <= 608);
    assert.equal(attribute(html, "data-read-scroll-y"), "0");
});
