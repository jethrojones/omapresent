globalThis.omapresentFixture = {
    mode: "preview",
    frontmatter: {
        title: "Renderer fixture",
        author: "Omapresent",
        footer: "{title} — {slide}/{count}",
        "slide-numbers": true,
        progress: true,
    },
    palette: {
        background: "#1d2021",
        foreground: "#ebdbb2",
        accent: "#d79921",
        muted: "#a89984",
        selection: "#504945",
        dark_background: "#1d2021",
        dark_foreground: "#d5c4a1",
    },
    textScale: 1,
    assets: {
        "missing-photo.png": "",
    },
    slides: [
        {
            index: 0,
            markdown: "# One renderer\n\nThis paragraph is a speaker note.",
            recallKey: "q",
            skip: false,
        },
        {
            index: 1,
            markdown: "## Fragments\n\n- First point\n- Second point\n  - Nested point\n- Final point",
            recallKey: "",
            skip: false,
        },
        {
            index: 2,
            markdown: "## Offline media\n\n![[missing-photo.png]]\n\nhttps://example.com/omapresent",
            recallKey: "",
            skip: false,
        },
        {
            index: 3,
            markdown: "## Math and code\n\n$$E = mc^2$$\n\n```js\nconst ready = true;\n```",
            recallKey: "",
            skip: false,
        },
        {
            index: 4,
            markdown: "## Forced QR\n\n![[qr:https://youtu.be/abc]]",
            recallKey: "",
            skip: false,
        },
        {
            index: -1,
            markdown: "## Recall appendix\n\nThis skipped recall slide appears in PDF output.",
            recallKey: "z",
            skip: true,
        },
    ],
};

const fixtureParams = new URLSearchParams(globalThis.location.search);
const fixtureHostPayloads = [];
if (fixtureParams.get("metrics") === "remote-media" || fixtureParams.get("deck") === "remote-media") {
    globalThis.omapresentFixture.slides = [
        { index: 0, markdown: "http://127.0.0.1:9/clip.mp4", recallKey: "", skip: false },
        { index: 1, markdown: "https://youtu.be/remote-test", recallKey: "", skip: false },
        { index: 2, markdown: "https://youtu.be/cached-test", recallKey: "", skip: false },
    ];
    globalThis.omapresentFixture.media = {
        "https://youtu.be/cached-test": {
            host: "youtube",
            cachedFile: "file:///tmp/cached-test.mp4",
            status: "cached",
        },
    };
}
if (fixtureParams.get("metrics") === "remote-image") {
    const imagePort = fixtureParams.get("imagePort");
    const imageUrl = `http://127.0.0.1:${imagePort}/remote.png`;
    globalThis.omapresentFixture.slides = [
        { index: 0, markdown: `![Remote diagram](${imageUrl})`, recallKey: "", skip: false },
    ];
    globalThis.omapresentFixture.assets = { [imageUrl]: imageUrl };
}
if (fixtureParams.get("metrics") === "recall") {
    const baseBullets = Array.from({ length: 48 }, (_, index) => `- Base point ${index + 1}`).join("\n");
    const recallDeck = [
        {
            index: 0,
            markdown: `## Scroll and fragments\n\n${baseBullets}`,
            recallKey: "",
            skip: false,
        },
        {
            index: 1,
            markdown: "## Recall appendix\n\n- Headline\n- Proof\n  - Nested proof\n- Close",
            recallKey: "q",
            skip: false,
        },
    ];
    globalThis.omapresentFixture.slides = recallDeck;
}
if (fixtureParams.get("metrics") === "heading-fragments") {
    globalThis.omapresentFixture.slides = [
        {
            index: 0,
            markdown: "# First section\n\n- One\n- Two\n\n## Second section\n- Three\n- Four",
            recallKey: "",
            skip: false,
        },
    ];
}
if (fixtureParams.get("metrics") === "cached-youtube-poster") {
    const poster = fixtureParams.get("poster") ?? "";
    let embedBaseCalls = 0;
    globalThis.omapresentFixture.slides = [
        { index: 0, markdown: "https://youtu.be/aqz-KE-bpKQ", recallKey: "", skip: false },
    ];
    // The cached thumbnail is useful even when no video file was downloaded.
    // Do not add cachedFile here: this must take the deferred hosted path.
    globalThis.omapresentFixture.media = {
        "https://youtu.be/aqz-KE-bpKQ": {
            host: "youtube",
            poster,
            status: "cached",
        },
    };
    globalThis.omapresentHost = {
        embedBase() {
            embedBaseCalls += 1;
            return "http://127.0.0.1:45123/0123456789abcdef/";
        },
    };
    globalThis.omapresentFixture.cachedPosterEmbedBaseCalls = () => embedBaseCalls;
}
if (fixtureParams.get("metrics") === "interaction") {
    globalThis.omapresentHost = {
        state(payload) {
            fixtureHostPayloads.push(JSON.parse(payload));
            document.body.dataset.hostStateSerialized = String(typeof payload === "string");
        },
    };
}
if (fixtureParams.get("view") === "read")
    document.documentElement.dataset.opView = "read";
if (fixtureParams.get("mode") === "pdf")
    globalThis.omapresentFixture.mode = "pdf";
const fixtureSlide = Number(fixtureParams.get("slide"));
globalThis.addEventListener("load", () => setTimeout(() => {
    if (Number.isInteger(fixtureSlide) && fixtureSlide > 0)
        globalThis.omapresent.goto(fixtureSlide);
    if (fixtureParams.get("metrics") === "list-width") {
        setTimeout(() => {
            const content = document.querySelector(".op-content");
            const list = document.querySelector(".op-block-list > ul, .op-block-list > ol");
            if (content && list)
                document.body.dataset.listWidthRatio = String(list.getBoundingClientRect().width / content.getBoundingClientRect().width);
        }, 100);
    }
    if (fixtureParams.get("metrics") === "interaction") {
        globalThis.omapresent.onState = state => globalThis.omapresentHost.state(JSON.stringify(state));
        globalThis.omapresent.goto(1);
        const visibility = () => [...document.querySelectorAll(".op-fragment")]
            .map(item => getComputedStyle(item).visibility === "visible" ? "1" : "0").join("");
        const steps = [visibility()];
        for (let index = 0; index < 4; index += 1) {
            globalThis.omapresent.next();
            steps.push(visibility());
        }
        globalThis.omapresent.next();
        const lastState = fixtureHostPayloads.at(-1) ?? {};
        document.body.dataset.fragmentSteps = steps.join(",");
        document.body.dataset.hostStateCount = String(fixtureHostPayloads.length);
        document.body.dataset.hostLastSlide = String(lastState.slideIndex);
        document.body.dataset.hostLastFragment = String(lastState.fragment);
    }
    if (fixtureParams.get("metrics") === "heading-fragments") {
        const snapshotFragments = () => [...document.querySelectorAll(".op-fragment")]
            .map(item => {
                const kind = item.classList.contains("op-block-heading") ? "H" : "L";
                const fragment = Number(item.dataset.fragment ?? 0);
                const revealed = item.dataset.revealed === "true";
                return { kind, fragment, revealed };
            })
            .sort((left, right) => left.fragment - right.fragment)
            .map(item => `${item.kind}${item.fragment}:${item.revealed ? 1 : 0}`)
            .join("|");

        const steps = [snapshotFragments()];
        for (let index = 0; index < 5; index += 1) {
            globalThis.omapresent.next();
            steps.push(snapshotFragments());
        }
        document.body.dataset.headingFragmentOrder = steps.join(";");
    }
    if (fixtureParams.get("metrics") === "recall") {
        const recallStates = [];
        const deckAfterUpdate = JSON.parse(JSON.stringify(globalThis.omapresentFixture));
        deckAfterUpdate.slides.unshift({
            index: 0,
            markdown: "## Inserted before the hidden slide",
            recallKey: "",
            skip: false,
        });
        for (let i = 0; i < deckAfterUpdate.slides.length; i += 1)
            deckAfterUpdate.slides[i].index = i;
        globalThis.omapresent.onState = state => recallStates.push(state);
        globalThis.omapresent.goto(0);
        globalThis.omapresent.next();
        globalThis.omapresent.next();
        setTimeout(() => {
            const scroller = document.querySelector(".op-scroll");
            if (scroller)
                scroller.scrollTop = 180;
            const beforeState = recallStates.at(-1) ?? {};
            const beforeScrollTop = scroller?.scrollTop ?? 0;
            const beforeScrollFraction = String(beforeState.scrollFraction ?? "0");
            document.body.dataset.recallBeforeFragment = String(beforeState.fragment ?? 0);
            document.body.dataset.recallBeforeSlide = String(beforeState.slideIndex ?? 0);
            document.body.dataset.recallBeforeScrollTop = String(beforeScrollTop);
            document.body.dataset.recallBeforeScrollFraction = beforeScrollFraction;

            globalThis.omapresent.showRecall("q");
            const overlayFragments = [...document.querySelectorAll(".op-recall-overlay .op-fragment")];
            document.body.dataset.recallOverlayVisible = String(Boolean(overlayFragments.length));
            document.body.dataset.recallOverlayFragmentsRevealed = String(overlayFragments.every(item => item.dataset.revealed === "true"));
            document.body.dataset.recallOverlayFragmentCount = String(overlayFragments.length);

            globalThis.omapresent.update(deckAfterUpdate);
            const overlayAfterUpdateFragments = [...document.querySelectorAll(".op-recall-overlay .op-fragment")];
            document.body.dataset.recallAfterUpdateOverlayVisible = String(Boolean(overlayAfterUpdateFragments.length));
            document.body.dataset.recallAfterUpdateOverlayFragmentsRevealed = String(overlayAfterUpdateFragments.every(item => item.dataset.revealed === "true"));
            document.body.dataset.recallAfterUpdateOverlayFragmentCount = String(overlayAfterUpdateFragments.length);

            globalThis.omapresent.goto(1);
            requestAnimationFrame(() => {
                const afterGotoState = recallStates.at(-1) ?? {};
                document.body.dataset.recallAfterGotoSlide = String(afterGotoState.slideIndex ?? 0);
                document.body.dataset.recallAfterGotoFragment = String(afterGotoState.fragment ?? 0);
                document.body.dataset.recallAfterGotoScrollTop = String(document.querySelector(".op-scroll")?.scrollTop ?? 0);

                globalThis.omapresent.hideRecall();
                const afterState = recallStates.at(-1) ?? {};
                const afterScroller = document.querySelector(".op-scroll");
                document.body.dataset.recallAfterFragment = String(afterState.fragment ?? 0);
                document.body.dataset.recallAfterSlide = String(afterState.slideIndex ?? 0);
                document.body.dataset.recallAfterScrollTop = String(afterScroller?.scrollTop ?? 0);
                document.body.dataset.recallAfterStateCount = String(recallStates.length);
            });
        }, 0);
    }
    if (fixtureParams.get("metrics") === "remote-media") {
        const remoteStates = [];
        globalThis.omapresent.onState = state => remoteStates.push(state);
        globalThis.omapresent.goto(0);
        document.body.dataset.loaderMediaActive = String(remoteStates.at(-1)?.mediaActive);
        const hasRemoteSource = () => [...document.querySelectorAll("video[src], iframe[src]")]
            .some(element => /^https?:\/\//i.test(element.getAttribute("src") ?? ""));
        document.body.dataset.directBeforePlay = String(hasRemoteSource());
        document.body.dataset.directPlaceholder = String(Boolean(document.querySelector(".op-media-loader")));
        const directLoader = document.querySelector(".op-media-loader");
        directLoader?.focus();
        for (const key of [" ", "Enter"]) {
            const event = new KeyboardEvent("keydown", { key, bubbles: true, cancelable: true });
            directLoader?.dispatchEvent(event);
            document.body.dataset[key === " " ? "directSpacePrevented" : "directEnterPrevented"] = String(event.defaultPrevented);
        }
        directLoader?.click();
        document.body.dataset.directAfterPlay = String(hasRemoteSource());
        document.body.dataset.directPreload = document.querySelector("video")?.preload ?? "";
        globalThis.omapresent.goto(1);
        document.body.dataset.embedBeforePlay = String(hasRemoteSource());
        document.body.dataset.embedPlaceholder = String(Boolean(document.querySelector(".op-media-loader")));
        document.querySelector(".op-media-loader")?.click();
        document.body.dataset.embedAfterPlay = String(hasRemoteSource());
        const embed = document.querySelector("iframe");
        document.body.dataset.embedAutoplay = new URL(embed?.src ?? "https://invalid.example").searchParams.get("autoplay") ?? "";
        document.body.dataset.embedAllowsAutoplay = String(embed?.allow.split(";").map(value => value.trim()).includes("autoplay"));
        globalThis.omapresent.goto(2);
        const cached = document.querySelector("video");
        document.body.dataset.cachedPlaceholder = String(Boolean(document.querySelector(".op-media-loader")));
        document.body.dataset.cachedSource = cached?.getAttribute("src") ?? "";
        document.body.dataset.cachedPreload = cached?.preload ?? "";
    }
    if (fixtureParams.get("metrics") === "cached-youtube-poster") {
        const loader = document.querySelector("button.op-media-loader.has-poster");
        const poster = fixtureParams.get("poster") ?? "";
        const hasRemoteSource = [...document.querySelectorAll("video[src], iframe[src]")]
            .some(element => /^https?:\/\//i.test(element.getAttribute("src") ?? ""));
        const externalResources = performance.getEntriesByType("resource")
            .filter(entry => /^https?:\/\//i.test(entry.name));
        document.body.dataset.cachedPosterRendered = String(Boolean(loader)
            && loader.style.backgroundImage.includes(poster));
        document.body.dataset.cachedPosterVideo = String(Boolean(document.querySelector("video[src]")));
        document.body.dataset.cachedPosterRemoteSource = String(hasRemoteSource);
        document.body.dataset.cachedPosterEmbedBaseCalls = String(
            globalThis.omapresentFixture.cachedPosterEmbedBaseCalls());
        document.body.dataset.cachedPosterExternalResources = String(externalResources.length);
    }
    if (fixtureParams.get("metrics") === "remote-image") {
        const hasRemoteSource = () => [...document.querySelectorAll("img[src]")]
            .some(element => /^https?:\/\//i.test(element.getAttribute("src") ?? ""));
        document.body.dataset.remoteImageBeforeLoad = String(hasRemoteSource());
        document.body.dataset.remoteImagePlaceholder = String(Boolean(document.querySelector("[data-op-remote-image]")));
        const loader = document.querySelector("[data-op-remote-image]");
        loader?.focus();
        for (const key of [" ", "Enter"]) {
            const event = new KeyboardEvent("keydown", { key, bubbles: true, cancelable: true });
            loader?.dispatchEvent(event);
            document.body.dataset[key === " " ? "remoteImageSpacePrevented" : "remoteImageEnterPrevented"] = String(event.defaultPrevented);
        }
        if (fixtureParams.get("load") === "1")
            loader?.click();
        document.body.dataset.remoteImageAfterLoad = String(hasRemoteSource());
    }
    if (fixtureParams.get("metrics") === "read") {
        const heading = document.querySelector(".op-slide h1");
        const slide = document.querySelector(".op-slide");
        const nextSlide = document.querySelector(".op-slide:nth-child(2)");
        const note = document.querySelector(".op-notes.is-flow-note");
        document.body.dataset.readHeadingSize = getComputedStyle(heading).fontSize;
        document.body.dataset.readSecondHeadingSize = getComputedStyle(nextSlide.querySelector("h2")).fontSize;
        document.body.dataset.readHeadingAlign = getComputedStyle(heading).textAlign;
        document.body.dataset.readSlideMinHeight = getComputedStyle(slide).minHeight;
        document.body.dataset.readSectionMargin = getComputedStyle(slide).marginBottom;
        document.body.dataset.readSectionPadding = getComputedStyle(nextSlide).paddingTop;
        document.body.dataset.readStackDisplay = getComputedStyle(document.querySelector(".op-stack")).display;
        document.body.dataset.readNoteDisplay = getComputedStyle(note).display;
        document.body.dataset.readNoteText = note?.textContent.trim() ?? "";
        document.body.dataset.readMeasure = String(document.querySelector("#deck").getBoundingClientRect().width);
        document.body.dataset.readScrollY = String(globalThis.scrollY);
    }
}, 0));
