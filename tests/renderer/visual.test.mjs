import assert from "node:assert/strict";
import { execFile } from "node:child_process";
import { access, mkdtemp, rm } from "node:fs/promises";
import { promisify } from "node:util";
import { dirname, join, resolve } from "node:path";
import { tmpdir } from "node:os";
import test from "node:test";
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

test("rendered lists use the slide content width", { skip: !(await hasChromium()) }, async () => {
    const fixture = pathToFileURL(resolve(here, "fixture.html"));
    fixture.search = "?slide=1&metrics=list-width";
    const profile = await mkdtemp(join(tmpdir(), "omapresent-chromium-"));
    try {
        const { stdout } = await execute(chromium, [
            "--headless", "--no-sandbox", "--disable-gpu", "--allow-file-access-from-files",
            `--user-data-dir=${profile}`, "--virtual-time-budget=1500", "--dump-dom", fixture.href,
        ], { maxBuffer: 2 * 1024 * 1024 });
        const ratio = Number(stdout.match(/data-list-width-ratio="([^"]+)"/)?.[1]);
        assert.ok(Number.isFinite(ratio), "fixture did not report a list width");
        assert.ok(ratio >= 0.9, `list used only ${(ratio * 100).toFixed(1)}% of the content width`);
    } finally {
        await rm(profile, { recursive: true, force: true });
    }
});
