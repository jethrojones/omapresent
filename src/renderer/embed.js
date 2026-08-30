// Where a hosted video player is allowed to load from, and what to do when it
// says it cannot play. Pure: no DOM, no network, no globals — render.js passes
// in what it knows and acts on the answer, and node --test covers it directly.
//
// The problem this encodes: a hosted player refuses to configure when the page
// embedding it has an opaque origin, because there is no Referer to send. It
// answers "Error 153: Video player configuration error", and no combination of
// embed parameters avoids it — measured across enablejsapi, autoplay and
// youtube-nocookie, all of which work unchanged from an http origin.
//
// So there are three outcomes, in order of preference:
//
//   "shim"     the app: a loopback origin the host offers over its bridge
//   "direct"   a published page already served over http(s)
//   "fallback" neither: a QR code and a link, per spec §4.8's last resort

// A YouTube id, as it appears in a URL. Anything else never reaches a URL.
const VIDEO_ID = /^[A-Za-z0-9_-]{5,32}$/;

// What a player reports when it will not play here. 101 and 150 are "the owner
// disallowed embedding"; 153 is the configuration error above; 2 and 100 are a
// bad id and a missing video, which are equally unplayable.
export const EMBED_FALLBACK_ERRORS = Object.freeze([2, 5, 100, 101, 150, 153]);

export function isFallbackError(code) {
    return EMBED_FALLBACK_ERRORS.includes(Number(code));
}

// True for a page origin a hosted player will accept.
export function originCanEmbed(protocol) {
    return /^https?:$/i.test(String(protocol ?? ""));
}

// True when a decision is one that needs an origin at all — and so the only
// case in which the host is asked for a loopback base, and the only case that
// ever starts the server. Everything else is built in the same turn as the
// click, with no bridge call.
export function needsEmbedOrigin(decision) {
    return Boolean(decision) && decision.host === "youtube" && decision.player === "embed";
}

// The host's answer, as a promise. A WebChannel method answers on a later turn
// through its callback rather than by returning, so a caller that reads the
// return value gets undefined and silently loses the shim. A host that never
// answers resolves empty rather than leaving Play hanging.
export function resolveEmbedBase(host, {
    timeoutMs = 2000,
    setTimer = setTimeout,
    clearTimer = clearTimeout,
} = {}) {
    if (!host || typeof host.embedBase !== "function")
        return Promise.resolve("");
    return new Promise(resolve => {
        let settled = false;
        let timer = null;
        const finish = value => {
            if (settled)
                return;
            settled = true;
            // The timer is only there to rescue a host that never answers.
            // Once one has, it is a handle nothing is waiting on.
            if (timer !== null)
                clearTimer(timer);
            resolve(typeof value === "string" ? value : "");
        };
        try {
            const returned = host.embedBase(finish);
            // A plain object host answers by returning; a WebChannel one
            // answers by calling back, and may do either in a test double.
            if (typeof returned === "string")
                finish(returned);
        } catch {
            finish("");
        }
        if (!settled)
            timer = setTimer(() => finish(""), timeoutMs);
    });
}

// Which of the three outcomes applies. `embedBase` is what the host bridge
// offered, "" when there is no host or it could not bind a socket.
export function embedStrategy({ host, player, protocol, embedBase } = {}) {
    // Only a hosted embed has this problem. A cached or local file plays from
    // disk, and a direct video URL is fetched by the browser, not by a player
    // that wants to know who is asking.
    if (!needsEmbedOrigin({ host, player }))
        return "direct";
    if (embedBase)
        return "shim";
    if (originCanEmbed(protocol))
        return "direct";
    return "fallback";
}

// The shim URL for a video, or "" when either half is missing. The id travels
// as a parameter to a page we serve, so no part of a deck's text is ever
// concatenated into a URL on the host's side.
export function shimEmbedUrl(embedBase, videoId, title = "") {
    if (!embedBase || !VIDEO_ID.test(String(videoId ?? "")))
        return "";
    let url;
    try {
        url = new URL("embed.html", embedBase);
    } catch {
        return "";
    }
    // Only the loopback origin the host gave us. A base that came from anywhere
    // else is not somewhere this renderer will open a frame.
    if (url.protocol !== "http:" || (url.hostname !== "127.0.0.1" && url.hostname !== "[::1]"))
        return "";
    url.searchParams.set("v", videoId);
    if (title)
        url.searchParams.set("title", title);
    return url.toString();
}

// True when a message really came from the frame this page created, at the
// origin it was created on. Any page can post, and a message that merely looks
// right is not one to act on.
export function isFromShim(event, frame, embedBase) {
    if (!event || !frame || event.source !== frame.contentWindow)
        return false;
    let expected;
    try {
        expected = new URL(embedBase).origin;
    } catch {
        return false;
    }
    return event.origin === expected;
}

// What a message from the shim means, or null when it is not one to act on.
// Everything here is untrusted: the shim is a frame, and any page can post.
// isFromShim() checks the sender; this checks the shape.
export function playerMessage(data) {
    if (!data || typeof data !== "object" || Array.isArray(data))
        return null;
    if (data.op === "error")
        return isFallbackError(data.code) ? { op: "error", code: Number(data.code) } : null;
    if (data.op === "state") {
        const state = Number(data.state);
        return Number.isFinite(state) ? { op: "state", state } : null;
    }
    if (data.op === "ready")
        return { op: "ready" };
    return null;
}

// Tracks the "stop listening" for each shim frame, so a frame that is removed —
// by a re-render, or with the recall overlay that carried it — takes its
// listener with it instead of leaving one bound to the page for good.
export function createShimRegistry() {
    const stops = new WeakMap();
    return {
        watch(frame, stop) {
            stops.set(frame, stop);
        },
        // Releases every shim frame inside `within`, including `within` itself.
        release(within) {
            if (!within)
                return 0;
            const frames = [...(within.querySelectorAll?.("iframe.op-player-shim") ?? [])];
            if (within.matches?.("iframe.op-player-shim"))
                frames.push(within);
            let released = 0;
            for (const frame of frames) {
                const stop = stops.get(frame);
                if (!stop)
                    continue;
                stop();
                stops.delete(frame);
                released += 1;
            }
            return released;
        },
    };
}
