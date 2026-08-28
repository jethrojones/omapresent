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
if (fixtureParams.get("view") === "read")
    document.documentElement.dataset.opView = "read";
if (fixtureParams.get("mode") === "pdf")
    globalThis.omapresentFixture.mode = "pdf";
const fixtureSlide = Number(fixtureParams.get("slide"));
if (Number.isInteger(fixtureSlide) && fixtureSlide > 0)
    globalThis.addEventListener("load", () => setTimeout(() => globalThis.omapresent.goto(fixtureSlide), 0));
