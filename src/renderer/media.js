const VIDEO_HOSTS = Object.freeze([
    ["youtube", ["youtube.com", "youtube-nocookie.com", "youtu.be"]],
    ["vimeo", ["vimeo.com"]],
    ["loom", ["loom.com"]],
    ["descript", ["descript.com"]],
    ["tiktok", ["tiktok.com"]],
    ["x", ["x.com", "twitter.com"]],
    ["instagram", ["instagram.com"]],
    ["facebook", ["facebook.com", "fb.com", "fb.watch"]],
]);

const DIRECT_VIDEO_EXTENSIONS = /\.(?:mp4|webm|mov)$/i;
const MEDIA_ID = /^[A-Za-z0-9_-]+$/;

function normaliseHost(hostname) {
    return hostname.toLowerCase().replace(/^www\./, "").replace(/^m\./, "").replace(/^mobile\./, "");
}

function hostMatches(hostname, candidate) {
    return hostname === candidate || hostname.endsWith(`.${candidate}`);
}

function urlFromLine(value) {
    const source = String(value ?? "").trim();
    if (!source || /\s/.test(source))
        return null;
    // A bare video filename is a local path. Do this before schemeless-domain
    // detection because .webm and .mov also happen to look like valid TLDs.
    if (!/^[A-Za-z][A-Za-z0-9+.-]*:/.test(source) && DIRECT_VIDEO_EXTENSIONS.test(source))
        return null;
    try {
        if (/^[A-Za-z][A-Za-z0-9+.-]*:/.test(source))
            return new URL(source);
        if (/^(?:www\.)?(?:[A-Za-z0-9-]+\.)+[A-Za-z]{2,}(?:\/|$)/.test(source))
            return new URL(`https://${source}`);
    } catch {
        return null;
    }
    return null;
}

function pathSegments(url) {
    return url.pathname.split("/").filter(Boolean).map(segment => decodeURIComponent(segment));
}

function youtubeId(url) {
    const host = normaliseHost(url.hostname);
    if (host === "youtu.be") {
        const id = pathSegments(url)[0] ?? "";
        return MEDIA_ID.test(id) ? id : "";
    }
    if (!hostMatches(host, "youtube.com") && !hostMatches(host, "youtube-nocookie.com"))
        return "";
    const queryId = url.searchParams.get("v") ?? "";
    if (MEDIA_ID.test(queryId))
        return queryId;
    const segments = pathSegments(url);
    if (["embed", "shorts", "live", "v", "e"].includes((segments[0] ?? "").toLowerCase())
        && MEDIA_ID.test(segments[1] ?? "")) {
        return segments[1];
    }
    return "";
}

function vimeoParts(url) {
    if (!hostMatches(normaliseHost(url.hostname), "vimeo.com"))
        return { id: "", hash: "" };
    const segments = pathSegments(url);
    let idIndex = -1;
    segments.forEach((segment, index) => {
        if (/^\d+$/.test(segment))
            idIndex = index;
    });
    if (idIndex < 0)
        return { id: "", hash: "" };
    const reserved = new Set(["channels", "groups", "videos", "ondemand", "album", "showcase", "staffpicks"]);
    const candidate = segments[idIndex + 1] ?? "";
    const hash = MEDIA_ID.test(candidate) && !reserved.has(candidate.toLowerCase()) ? candidate : "";
    return { id: segments[idIndex], hash };
}

function firstKindId(url, domain, kinds) {
    if (!hostMatches(normaliseHost(url.hostname), domain))
        return "";
    const segments = pathSegments(url);
    return kinds.includes((segments[0] ?? "").toLowerCase()) && MEDIA_ID.test(segments[1] ?? "")
        ? segments[1]
        : "";
}

function tiktokId(url) {
    const host = normaliseHost(url.hostname);
    if (!hostMatches(host, "tiktok.com"))
        return "";
    const segments = pathSegments(url);
    if (["vm.tiktok.com", "vt.tiktok.com"].includes(host))
        return MEDIA_ID.test(segments[0] ?? "") ? segments[0] : "";
    for (let index = 0; index + 1 < segments.length; index += 1) {
        if (!["video", "v", "t", "embed"].includes(segments[index].toLowerCase()))
            continue;
        const offset = segments[index + 1].toLowerCase() === "v2" ? 2 : 1;
        const id = segments[index + offset] ?? "";
        if (MEDIA_ID.test(id))
            return id;
    }
    return "";
}

function xStatusId(url) {
    const host = normaliseHost(url.hostname);
    if (!hostMatches(host, "x.com") && !hostMatches(host, "twitter.com"))
        return "";
    const segments = pathSegments(url);
    const statusIndex = segments.findIndex(segment => segment.toLowerCase() === "status");
    const id = statusIndex >= 0 ? segments[statusIndex + 1] ?? "" : "";
    return /^\d+$/.test(id) ? id : "";
}

function instagramParts(url) {
    if (!hostMatches(normaliseHost(url.hostname), "instagram.com"))
        return { kind: "", id: "" };
    const segments = pathSegments(url);
    let kind = (segments[0] ?? "").toLowerCase();
    if (!new Set(["p", "reel", "reels", "tv"]).has(kind) || !MEDIA_ID.test(segments[1] ?? ""))
        return { kind: "", id: "" };
    if (kind === "reels")
        kind = "reel";
    return { kind, id: segments[1] };
}

function isFacebookVideo(url) {
    const host = normaliseHost(url.hostname);
    const segments = pathSegments(url);
    if (host === "fb.watch")
        return segments.length > 0;
    if (!hostMatches(host, "facebook.com") && !hostMatches(host, "fb.com"))
        return false;
    if (url.searchParams.get("v"))
        return true;
    for (let index = 0; index < segments.length; index += 1) {
        const segment = segments[index].toLowerCase();
        if (["reel", "reels", "videos"].includes(segment) && MEDIA_ID.test(segments[index + 1] ?? ""))
            return true;
        if (segment === "share" && ["v", "r", "reel"].includes((segments[index + 1] ?? "").toLowerCase())
            && MEDIA_ID.test(segments[index + 2] ?? "")) {
            return true;
        }
    }
    return false;
}

export function isBareUrlLine(line) {
    return Boolean(urlFromLine(line));
}

export function isLocalVideoLine(line) {
    const value = String(line ?? "").trim();
    if (!value || /\n/.test(value))
        return false;
    const parsed = urlFromLine(value);
    if (parsed)
        return parsed.protocol === "file:" && DIRECT_VIDEO_EXTENSIONS.test(parsed.pathname);
    return !/^[A-Za-z][A-Za-z0-9+.-]*:/.test(value) && DIRECT_VIDEO_EXTENSIONS.test(value);
}

export function videoHostFor(value) {
    const source = String(value ?? "").trim();
    const parsed = urlFromLine(source);
    if (parsed) {
        if (["mailto:", "javascript:", "data:"].includes(parsed.protocol))
            return "";
        if (youtubeId(parsed))
            return "youtube";
        if (vimeoParts(parsed).id)
            return "vimeo";
        if (firstKindId(parsed, "loom.com", ["share", "embed"]))
            return "loom";
        if (firstKindId(parsed, "descript.com", ["view", "embed"]))
            return "descript";
        if (tiktokId(parsed))
            return "tiktok";
        if (xStatusId(parsed))
            return "x";
        if (instagramParts(parsed).id)
            return "instagram";
        if (isFacebookVideo(parsed))
            return "facebook";
        if (DIRECT_VIDEO_EXTENSIONS.test(parsed.pathname)) {
            if (parsed.protocol === "file:")
                return "local";
            if (["http:", "https:"].includes(parsed.protocol))
                return "direct";
        }
        return "";
    }
    return isLocalVideoLine(source) ? "local" : "";
}

export function forcedQrValue(source) {
    const value = String(source ?? "").trim();
    const embed = value.match(/^!\[\[qr:([\s\S]+?)\]\]$/i);
    if (embed)
        return embed[1].trim();
    const fence = value.match(/^```qr[^\S\r\n]*\r?\n([\s\S]*?)\r?\n```$/i);
    return fence ? fence[1].trim() : "";
}

export function embedUrlFor(value, host = videoHostFor(value)) {
    const parsed = urlFromLine(value);
    if (!parsed)
        return host === "local" ? String(value).trim() : "";
    if (host === "youtube")
        return `https://www.youtube.com/embed/${youtubeId(parsed)}`;
    if (host === "vimeo") {
        const { id, hash } = vimeoParts(parsed);
        return `https://player.vimeo.com/video/${id}${hash ? `?h=${encodeURIComponent(hash)}` : ""}`;
    }
    if (host === "loom")
        return `https://www.loom.com/embed/${firstKindId(parsed, "loom.com", ["share", "embed"])}`;
    if (host === "descript")
        return `https://share.descript.com/embed/${firstKindId(parsed, "descript.com", ["view", "embed"])}`;
    if (host === "tiktok")
        return `https://www.tiktok.com/embed/v2/${tiktokId(parsed)}`;
    if (host === "x")
        return `https://platform.twitter.com/embed/Tweet.html?id=${xStatusId(parsed)}`;
    if (host === "instagram") {
        const { kind, id } = instagramParts(parsed);
        return `https://www.instagram.com/${kind}/${id}/embed/`;
    }
    if (host === "facebook")
        return `https://www.facebook.com/plugins/video.php?href=${encodeURIComponent(parsed.href)}`;
    if (host === "direct" || host === "local")
        return String(value).trim();
    return "";
}

function youtubeApiUrl(value) {
    try {
        const url = new URL(value);
        url.searchParams.set("enablejsapi", "1");
        return url.toString();
    } catch {
        return value;
    }
}

export function mediaDecision(source, description = {}) {
    const value = String(source ?? "").trim();
    const forced = forcedQrValue(value);
    if (forced)
        return { kind: "qr", value: forced, host: "", forced: true };
    const host = videoHostFor(value);
    if (!host) {
        if (isBareUrlLine(value))
            return { kind: "qr", value, host: "", forced: false };
        return { kind: "none", value, host: "", forced: false };
    }
    if (description.status === "qr")
        return { kind: "qr", value, host, forced: false };
    const cachedFile = String(description.cachedFile ?? "").trim();
    const direct = host === "direct";
    const local = host === "local";
    const describedEmbed = String(description.embedUrl ?? "").trim();
    let playerSource = cachedFile || (direct ? value : local
        ? describedEmbed || (/^file:/i.test(value) ? value : "")
        : describedEmbed || embedUrlFor(value, host));
    if (!cachedFile && host === "youtube")
        playerSource = youtubeApiUrl(playerSource);
    return {
        kind: "video",
        value,
        host,
        source: playerSource,
        player: cachedFile || direct || local ? "file" : "embed",
        poster: String(description.poster ?? ""),
        title: String(description.title ?? ""),
        vertical: Boolean(description.vertical),
        forced: false,
    };
}

export { VIDEO_HOSTS };
