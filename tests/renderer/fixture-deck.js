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
}, 0));
