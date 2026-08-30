import test from "node:test";
import assert from "node:assert/strict";
import { execFile } from "node:child_process";
import { access, mkdtemp, rm, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { promisify } from "node:util";
import { fileURLToPath, pathToFileURL } from "node:url";

import {
    EMBED_FALLBACK_ERRORS,
    createShimRegistry,
    embedStrategy,
    isFallbackError,
    isFromShim,
    needsEmbedOrigin,
    originCanEmbed,
    playerMessage,
    resolveEmbedBase,
    shimEmbedUrl,
} from "../../src/renderer/embed.js";

const execute = promisify(execFile);
const here = dirname(fileURLToPath(import.meta.url));
const chromium = "/usr/bin/chromium";

const LOOPBACK = "http://127.0.0.1:45123/0123456789abcdef/";

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

async function fixtureDom(search) {
    const fixture = pathToFileURL(resolve(here, "fixture.html"));
    fixture.search = search;
    const profile = await mkdtemp(join(tmpdir(), "omapresent-chromium-"));
    try {
        const { stdout } = await execute(chromium, [
            "--headless", "--no-sandbox", "--disable-gpu", "--allow-file-access-from-files",
            "--host-resolver-rules=MAP * 127.0.0.1", `--user-data-dir=${profile}`,
            "--virtual-time-budget=1500", "--dump-dom", fixture.href,
        ], { maxBuffer: 2 * 1024 * 1024 });
        return stdout;
    } finally {
        await rm(profile, { recursive: true, force: true });
    }
}

test("only an http(s) page has an origin a hosted player will accept", () => {
    assert.equal(originCanEmbed("http:"), true);
    assert.equal(originCanEmbed("https:"), true);
    // The two the app actually runs on, and the reason this module exists.
    assert.equal(originCanEmbed("file:"), false);
    assert.equal(originCanEmbed("qrc:"), false);
    assert.equal(originCanEmbed("data:"), false);
    assert.equal(originCanEmbed(""), false);
    assert.equal(originCanEmbed(undefined), false);
});

test("a hosted embed prefers the shim, then the page's own origin, then the link", () => {
    const hosted = { host: "youtube", player: "embed" };

    // The app: a host offered a loopback origin, so use it wherever the deck
    // page itself came from.
    assert.equal(embedStrategy({ ...hosted, protocol: "qrc:", embedBase: LOOPBACK }), "shim");
    assert.equal(embedStrategy({ ...hosted, protocol: "file:", embedBase: LOOPBACK }), "shim");

    // A published page already served over http(s) needs no shim.
    assert.equal(embedStrategy({ ...hosted, protocol: "https:", embedBase: "" }), "direct");

    // A published bundle opened from a folder has neither, and a frame there is
    // certain to fail, so the reader gets the link instead (spec §4.8).
    assert.equal(embedStrategy({ ...hosted, protocol: "file:", embedBase: "" }), "fallback");
    // Same when the host could not bind a socket.
    assert.equal(embedStrategy({ ...hosted, protocol: "qrc:", embedBase: "" }), "fallback");
});

test("only a hosted embed needs an origin at all", () => {
    // A cached file plays from disk and a direct URL is fetched by the browser;
    // neither asks who is embedding it, so neither is ever sent to the shim.
    for (const decision of [
        { host: "youtube", player: "file" },
        { host: "local", player: "file" },
        { host: "direct", player: "file" },
        { host: "vimeo", player: "embed" },
        { host: "loom", player: "embed" },
    ]) {
        assert.equal(embedStrategy({ ...decision, protocol: "file:", embedBase: LOOPBACK }),
                     "direct", `${decision.host}/${decision.player}`);
    }
});

test("the shim URL carries the video as an id and points only at loopback", () => {
    const url = new URL(shimEmbedUrl(LOOPBACK, "aqz-KE-bpKQ"));
    assert.equal(url.origin, "http://127.0.0.1:45123");
    assert.equal(url.pathname, "/0123456789abcdef/embed.html");
    assert.equal(url.searchParams.get("v"), "aqz-KE-bpKQ");

    assert.equal(new URL(shimEmbedUrl(LOOPBACK, "aqz-KE-bpKQ", "A talk")).searchParams.get("title"),
                 "A talk");

    // Nothing to build with.
    assert.equal(shimEmbedUrl("", "aqz-KE-bpKQ"), "");
    assert.equal(shimEmbedUrl(LOOPBACK, ""), "");
    assert.equal(shimEmbedUrl(LOOPBACK, undefined), "");

    // An id is a short opaque token, and it arrives from deck text. Anything
    // that could steer the URL is refused rather than escaped.
    for (const hostile of ["../../etc/passwd", "abc/def", "abc?x=1", "abc#frag",
                           "a b", "javascript:alert(1)", "x".repeat(64), "abc&v=other"]) {
        assert.equal(shimEmbedUrl(LOOPBACK, hostile), "", hostile);
    }

    // And a base that is not the loopback origin the host gave us is not a
    // place this renderer will open a frame, however it was arrived at.
    for (const base of ["https://example.com/t/", "http://example.com/t/",
                        "http://127.0.0.1.example.com/t/", "file:///tmp/t/",
                        "javascript:alert(1)//", "not a url"]) {
        assert.equal(shimEmbedUrl(base, "aqz-KE-bpKQ"), "", base);
    }
});

test("the codes that mean the video will not play here", () => {
    // 101 and 150: the owner disallowed embedding. 153: the configuration
    // error this whole path exists to avoid. 2 and 100: bad id, missing video.
    for (const code of [2, 5, 100, 101, 150, 153])
        assert.equal(isFallbackError(code), true, String(code));
    assert.equal(isFallbackError("153"), true);

    for (const code of [0, 1, 3, 152, 154, null, undefined, "", "abc", {}])
        assert.equal(isFallbackError(code), false, JSON.stringify(code));

    assert.deepEqual([...EMBED_FALLBACK_ERRORS].sort((a, b) => a - b), [2, 5, 100, 101, 150, 153]);
});

test("a message is read for shape before it is acted on", () => {
    assert.deepEqual(playerMessage({ op: "error", code: 153 }), { op: "error", code: 153 });
    assert.deepEqual(playerMessage({ op: "error", code: "101" }), { op: "error", code: 101 });
    assert.deepEqual(playerMessage({ op: "state", state: 1 }), { op: "state", state: 1 });
    assert.deepEqual(playerMessage({ op: "state", state: 0 }), { op: "state", state: 0 });
    assert.deepEqual(playerMessage({ op: "ready" }), { op: "ready" });

    // An error code that is not one of ours is not an instruction to give up.
    assert.equal(playerMessage({ op: "error", code: 42 }), null);
    // Anything a page might post that is not one of the three shapes.
    for (const data of [null, undefined, "", "error", 153, [], ["error"],
                        { op: "play" }, { op: "error" }, { op: "state" },
                        { op: "state", state: "soon" }, {}]) {
        assert.equal(playerMessage(data), null, JSON.stringify(data) ?? String(data));
    }
});

test("a hosted player keeps the server asleep while a cached local poster renders", {
    skip: !(await hasChromium()),
}, async () => {
    assert.equal(needsEmbedOrigin({ host: "youtube", player: "embed" }), true);

    // Every other kind of media is built in the same turn as the click and
    // never reaches the bridge — a local file must not open a socket.
    for (const decision of [
        { host: "youtube", player: "file" },
        { host: "vimeo", player: "embed" },
        { host: "loom", player: "embed" },
        { host: "tiktok", player: "embed" },
        { host: "direct", player: "file" },
        { host: "local", player: "file" },
        {},
    ]) {
        assert.equal(needsEmbedOrigin(decision), false, JSON.stringify(decision));
    }
    assert.equal(needsEmbedOrigin(null), false);
    assert.equal(needsEmbedOrigin(undefined), false);

    // The browser guard covers the deferred state itself. The fixture has a
    // YouTube URL and a real local poster, but deliberately no cachedFile.
    // Rendering must show the poster without asking the bridge for a loopback
    // base or creating a remote player that could make a request.
    const directory = await mkdtemp(join(tmpdir(), "omapresent-poster-"));
    const poster = join(directory, "cached-poster.png");
    const pixel = Buffer.from("iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNk+A8AAQUBAScY42YAAAAASUVORK5CYII=", "base64");
    await writeFile(poster, pixel);
    try {
        const posterUrl = pathToFileURL(poster).href;
        const html = await fixtureDom(`?metrics=cached-youtube-poster&poster=${encodeURIComponent(posterUrl)}`);
        assert.equal(attribute(html, "data-cached-poster-rendered"), "true");
        assert.equal(attribute(html, "data-cached-poster-video"), "false");
        assert.equal(attribute(html, "data-cached-poster-remote-source"), "false");
        assert.equal(attribute(html, "data-cached-poster-embed-base-calls"), "0");
        assert.equal(attribute(html, "data-cached-poster-external-resources"), "0");
    } finally {
        await rm(directory, { recursive: true, force: true });
    }
});

test("the bridge answers on a later turn, and a silent one does not hang Play", async () => {
    // This is how a WebChannel method really behaves: it returns undefined and
    // calls back later. Reading the return value loses the shim entirely.
    const deferred = {
        embedBase(callback) {
            setTimeout(() => callback(LOOPBACK), 5);
            return undefined;
        },
    };
    assert.equal(await resolveEmbedBase(deferred), LOOPBACK);

    // A plain object host may answer by returning instead.
    assert.equal(await resolveEmbedBase({ embedBase: () => LOOPBACK }), LOOPBACK);

    // No host at all, and a host that answers with something that is not a URL.
    assert.equal(await resolveEmbedBase(null), "");
    assert.equal(await resolveEmbedBase({}), "");
    assert.equal(await resolveEmbedBase({ embedBase: callback => callback(undefined) }), "");
    assert.equal(await resolveEmbedBase({ embedBase: () => { throw new Error("gone"); } }), "");

    // A host that never answers resolves empty on its own timer rather than
    // leaving the reader looking at a play button that does nothing.
    let armed;
    // Not awaited yet: the injected timer is what resolves this, so it has to
    // fire before there is anything to wait for.
    const silent = resolveEmbedBase({ embedBase() { /* never calls back */ } },
                                    { timeoutMs: 1234, setTimer: fire => { armed = fire; } });
    assert.equal(armed === undefined, false, "a timeout should have been armed");
    armed();
    assert.equal(await silent, "");

    // And a late answer after the timeout does not resolve it twice.
    let callback;
    const raced = resolveEmbedBase({ embedBase(cb) { callback = cb; } },
                                   { timeoutMs: 0, setTimer: fire => fire() });
    callback(LOOPBACK);
    assert.equal(await raced, "");
});

test("a message is trusted only from the frame, at the origin it was created on", () => {
    const frame = { contentWindow: { id: "shim" } };
    const other = { id: "someone else" };

    assert.equal(isFromShim({ source: frame.contentWindow, origin: "http://127.0.0.1:45123" },
                            frame, LOOPBACK), true);

    // The deck page itself, posting a message that looks exactly right.
    assert.equal(isFromShim({ source: other, origin: "http://127.0.0.1:45123" },
                            frame, LOOPBACK), false);
    // The right window, but claiming a different origin.
    assert.equal(isFromShim({ source: frame.contentWindow, origin: "https://youtube.com" },
                            frame, LOOPBACK), false);
    assert.equal(isFromShim({ source: frame.contentWindow, origin: "null" },
                            frame, LOOPBACK), false);
    assert.equal(isFromShim({ source: frame.contentWindow, origin: "http://127.0.0.1:45124" },
                            frame, LOOPBACK), false);
    // Nothing to compare against.
    assert.equal(isFromShim(null, frame, LOOPBACK), false);
    assert.equal(isFromShim({ source: frame.contentWindow, origin: "x" }, null, LOOPBACK), false);
    assert.equal(isFromShim({ source: frame.contentWindow, origin: "x" }, frame, ""), false);
    assert.equal(isFromShim({ source: frame.contentWindow, origin: "x" }, frame, "not a url"),
                 false);
});

test("a shim frame's listener goes when the element carrying it does", () => {
    // A minimal stand-in for the two places a shim frame lives: inside the deck
    // root, and inside a recall overlay, which is removed by its own code path.
    const frameIn = container => {
        const frame = { matches: selector => selector === "iframe.op-player-shim" };
        container.frames.push(frame);
        return frame;
    };
    const container = () => {
        const held = { frames: [] };
        held.querySelectorAll = selector =>
            selector === "iframe.op-player-shim" ? held.frames : [];
        held.matches = () => false;
        return held;
    };

    const registry = createShimRegistry();
    const root = container();
    const overlay = container();
    let stopped = 0;

    const inRoot = frameIn(root);
    const inOverlay = frameIn(overlay);
    registry.watch(inRoot, () => { stopped += 1; });
    registry.watch(inOverlay, () => { stopped += 1; });

    // Removing the overlay releases its frame and leaves the deck's alone —
    // this is the recall path, which drops overlays without re-rendering.
    assert.equal(registry.release(overlay), 1);
    assert.equal(stopped, 1);

    // Releasing twice is not an error and does not stop anything twice.
    assert.equal(registry.release(overlay), 0);
    assert.equal(stopped, 1);

    // And the re-render releases what is left.
    assert.equal(registry.release(root), 1);
    assert.equal(stopped, 2);

    // A frame handed in directly, rather than found by a query.
    const loose = { matches: selector => selector === "iframe.op-player-shim" };
    registry.watch(loose, () => { stopped += 1; });
    assert.equal(registry.release(loose), 1);
    assert.equal(stopped, 3);

    // Nothing to release, and nothing that was ever watched.
    assert.equal(registry.release(null), 0);
    assert.equal(registry.release(container()), 0);
});
