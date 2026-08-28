import assert from "node:assert/strict";
import test from "node:test";

import {
    embedUrlFor,
    forcedQrValue,
    isBareUrlLine,
    isLocalVideoLine,
    mediaDecision,
    videoHostFor,
} from "../../src/renderer/media.js";

test("bare URL recognition accepts only a whole HTTP or HTTPS line", () => {
    assert.equal(isBareUrlLine(" https://example.com/a?b=1 "), true);
    assert.equal(isBareUrlLine("www.example.com/a"), true);
    assert.equal(isBareUrlLine("mailto:hello@example.com"), true);
    assert.equal(isBareUrlLine("file:///tmp/video.mp4"), true);
    assert.equal(isBareUrlLine("See https://example.com"), false);
    assert.equal(isBareUrlLine("https://example.com/space here"), false);
});

test("video host recognition mirrors the frozen C++ host list", () => {
    const cases = [
        ["https://youtu.be/abc", "youtube"],
        ["https://www.youtube.com/watch?v=abc", "youtube"],
        ["https://player.vimeo.com/video/123", "vimeo"],
        ["https://www.loom.com/share/abc", "loom"],
        ["https://share.descript.com/view/abc", "descript"],
        ["https://www.tiktok.com/@person/video/1", "tiktok"],
        ["https://x.com/person/status/1", "x"],
        ["https://twitter.com/person/status/1", "x"],
        ["https://www.instagram.com/reel/abc", "instagram"],
        ["https://www.facebook.com/watch/?v=1", "facebook"],
        ["https://fb.watch/abc", "facebook"],
        ["https://fb.com/watch/?v=1", "facebook"],
        ["https://cdn.example.com/movie.mp4?token=1", "direct"],
        ["./media/movie.webm", "local"],
    ];

    for (const [url, expected] of cases)
        assert.equal(videoHostFor(url), expected, url);
    assert.equal(videoHostFor("https://example.com/watch/1"), "");
    assert.equal(videoHostFor("https://youtube.com/"), "");
    assert.equal(videoHostFor("https://x.com/person"), "");
    assert.equal(videoHostFor("https://instagram.com/person"), "");
    assert.equal(videoHostFor("https://www.tiktok.com/@person"), "");
});

test("local video paths can contain spaces", () => {
    assert.equal(isLocalVideoLine("./media/keynote clip.mov"), true);
    assert.equal(videoHostFor("./media/keynote clip.mov"), "local");
});

test("bare video filenames are local before schemeless-domain detection", () => {
    for (const filename of ["clip.mp4", "clip.webm", "clip.mov"]) {
        assert.equal(isBareUrlLine(filename), false, filename);
        assert.equal(isLocalVideoLine(filename), true, filename);
        assert.equal(videoHostFor(filename), "local", filename);
        assert.equal(mediaDecision(filename).kind, "video", filename);
    }
});

test("known share URLs get standalone embed fallbacks", () => {
    assert.equal(embedUrlFor("https://youtu.be/abc"), "https://www.youtube.com/embed/abc");
    assert.equal(embedUrlFor("https://www.youtube.com/watch?v=abc"), "https://www.youtube.com/embed/abc");
    assert.equal(embedUrlFor("https://vimeo.com/123456"), "https://player.vimeo.com/video/123456");
    assert.equal(embedUrlFor("https://www.loom.com/share/abc"), "https://www.loom.com/embed/abc");
    assert.equal(embedUrlFor("https://share.descript.com/view/abc"), "https://share.descript.com/embed/abc");
    assert.equal(embedUrlFor("https://www.tiktok.com/@person/video/123"), "https://www.tiktok.com/embed/v2/123");
    assert.equal(embedUrlFor("https://x.com/person/status/20"), "https://platform.twitter.com/embed/Tweet.html?id=20");
    assert.equal(embedUrlFor("https://www.instagram.com/reel/abc"), "https://www.instagram.com/reel/abc/embed/");
    assert.match(embedUrlFor("https://www.facebook.com/watch/?v=1"), /^https:\/\/www\.facebook\.com\/plugins\/video\.php\?href=/);
});

test("media decisions cover cached, embedded, direct and QR branches", () => {
    assert.deepEqual(mediaDecision("https://example.com/info"), {
        kind: "qr",
        value: "https://example.com/info",
        host: "",
        forced: false,
    });

    const cached = mediaDecision("https://youtu.be/abc", {
        cachedFile: "file:///tmp/abc.mp4",
        vertical: true,
        title: "Clip",
    });
    assert.equal(cached.kind, "video");
    assert.equal(cached.player, "file");
    assert.equal(cached.source, "file:///tmp/abc.mp4");
    assert.equal(cached.vertical, true);

    const youtube = mediaDecision("https://youtu.be/abc", {
        embedUrl: "https://www.youtube.com/embed/abc",
        status: "embed",
    });
    assert.equal(youtube.source, "https://www.youtube.com/embed/abc?enablejsapi=1");

    const embedded = mediaDecision("https://vimeo.com/123", {
        embedUrl: "https://player.vimeo.com/video/123",
        status: "embed",
    });
    assert.equal(embedded.player, "embed");
    assert.equal(embedded.source, "https://player.vimeo.com/video/123");

    assert.equal(mediaDecision("https://cdn.example.com/movie.mp4").player, "file");
    assert.equal(mediaDecision("./media/movie.mp4").source, "");
    assert.equal(mediaDecision("./media/movie.mp4", { cachedFile: "file:///tmp/movie.mp4" }).source, "file:///tmp/movie.mp4");
    assert.equal(mediaDecision("file:///tmp/movie.mp4").source, "file:///tmp/movie.mp4");
    assert.equal(mediaDecision("https://youtu.be/abc", { status: "qr" }).kind, "qr");
    assert.equal(mediaDecision("A URL https://example.com in prose").kind, "none");
});

test("explicit QR forms always force a QR", () => {
    assert.equal(forcedQrValue("![[qr:https://example.com/survey]]"), "https://example.com/survey");
    assert.equal(forcedQrValue("```qr\nhttps://example.com/survey\n```"), "https://example.com/survey");
    assert.equal(mediaDecision("![[qr:https://youtu.be/abc]]").kind, "qr");
    assert.equal(mediaDecision("```qr\nhttps://youtu.be/abc\n```").kind, "qr");
});
