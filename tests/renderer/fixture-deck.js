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
}, 0));
