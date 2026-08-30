import test from "node:test";
import assert from "node:assert/strict";
import { createServer } from "node:http";
import { once } from "node:events";
import { execFile } from "node:child_process";
import { mkdtemp, rm, readFile, writeFile } from "node:fs/promises";
import { promisify } from "node:util";
import { join, resolve, dirname } from "node:path";
import { tmpdir } from "node:os";
import { fileURLToPath } from "node:url";

// The one claim in T34 that no offline test can settle: that the loopback shim
// really does load a hosted player. Proving it means asking YouTube, so this
// file is opt-in and never runs in the ordinary gate.
//
//     OMAPRESENT_NETWORK_TESTS=1 node --test tests/renderer/embed-network.test.mjs
//
// What it deliberately does NOT assert is "Error 153" itself. That error is
// rendered inside the player's own cross-origin document, which --dump-dom
// cannot read, and the player reports nothing through its API when it happens —
// measured: from a folder and from loopback, and for an embeddable video and a
// non-embeddable one, the only message that ever arrives is "ready". So 153 is
// verifiable by looking and not by asserting; the screenshots that established
// it are recorded in tasks/t34-youtube-playback.md.
//
// Which origin gets chosen, what the shim URL looks like and which error codes
// fall back are decided in src/renderer/embed.js and covered offline in
// embed.test.mjs.

const execute = promisify(execFile);
const here = dirname(fileURLToPath(import.meta.url));
const chromium = process.env.CHROMIUM ?? "chromium";
const enabled = process.env.OMAPRESENT_NETWORK_TESTS === "1";

// Big Buck Bunny, Blender Foundation, CC BY 3.0 — embeddable, and the video
// welcome.md now demonstrates.
const VIDEO = "aqz-KE-bpKQ";

async function domOf(url, extraArguments = []) {
    const profile = await mkdtemp(join(tmpdir(), "omapresent-network-"));
    try {
        const { stdout } = await execute(chromium, [
            "--headless", "--no-sandbox", "--disable-gpu",
            `--user-data-dir=${profile}`, "--virtual-time-budget=12000",
            ...extraArguments, "--dump-dom", url,
        ], { maxBuffer: 8 * 1024 * 1024 });
        return stdout;
    } finally {
        await rm(profile, { recursive: true, force: true });
    }
}

test("the loopback shim loads a hosted player", {
    skip: !enabled && "set OMAPRESENT_NETWORK_TESTS=1 to run (contacts youtube.com)",
}, async () => {
    const shim = await readFile(resolve(here, "../../src/renderer/embed.html"), "utf8");
    const directory = await mkdtemp(join(tmpdir(), "omapresent-shim-"));

    try {
        // A page that records what the shim reports, which is the only channel
        // the deck page itself has.
        await writeFile(join(directory, "embed.html"), shim);
        await writeFile(join(directory, "wrapper.html"), `<!doctype html><body>
<script>
window.addEventListener("message", event => {
    const data = event.data;
    if (data && typeof data === "object" && data.op)
        document.body.dataset[data.op] = String(data.code ?? data.state ?? "yes");
});
</script>
<iframe src="embed.html?v=${VIDEO}" style="width:640px;height:360px;border:0"></iframe>
</body>`);

        const server = createServer(async (request, response) => {
            const name = request.url.split("?")[0] === "/embed.html" ? "embed.html" : "wrapper.html";
            response.writeHead(200, { "content-type": "text/html; charset=utf-8" });
            response.end(await readFile(join(directory, name), "utf8"));
        });
        server.listen(0, "127.0.0.1");
        await once(server, "listening");
        const { port } = server.address();

        try {
            const html = await domOf(`http://127.0.0.1:${port}/wrapper.html`);
            // The shim was served over loopback, loaded the player and sent
            // its handshake. This does not prove the player configured — see
            // the note above about why nothing can — but it does catch the
            // shim being served broken, unreachable, or with the wrong id.
            assert.match(html, /data-ready="yes"/);
            // And nothing came back that would send the deck to the fallback.
            assert.doesNotMatch(html, /data-error=/);
        } finally {
            server.close();
        }
    } finally {
        await rm(directory, { recursive: true, force: true });
    }
});
