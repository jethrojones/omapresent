import assert from "node:assert/strict";
import { execFile } from "node:child_process";
import { access, mkdtemp, rm } from "node:fs/promises";
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

test("fragments reveal in DOM order and report every state through the host callback", {
    skip: !(await hasChromium()),
}, async () => {
    const fixture = pathToFileURL(resolve(here, "fixture.html"));
    fixture.search = "?metrics=interaction";
    const profile = await mkdtemp(join(tmpdir(), "omapresent-chromium-"));
    try {
        const { stdout } = await execute(chromium, [
            "--headless", "--no-sandbox", "--disable-gpu", "--allow-file-access-from-files",
            `--user-data-dir=${profile}`, "--virtual-time-budget=1500", "--dump-dom", fixture.href,
        ], { maxBuffer: 2 * 1024 * 1024 });

        assert.equal(attribute(stdout, "data-fragment-steps"), "0000,1000,1100,1110,1111");
        assert.equal(attribute(stdout, "data-host-state-serialized"), "true");
        assert.ok(Number(attribute(stdout, "data-host-state-count")) >= 6);
        assert.equal(attribute(stdout, "data-host-last-slide"), "2");
        assert.equal(attribute(stdout, "data-host-last-fragment"), "0");
    } finally {
        await rm(profile, { recursive: true, force: true });
    }
});
