import { parseSlide, headingText, slidesForRender } from "./deckparse.js";
import { fitDecision, layoutForBlocks, scrollTopForFraction } from "./layout.js";
import { mediaDecision } from "./media.js";
import ir from "./vendor/markdown-it.mjs";
import katex from "./vendor/katex.mjs";
import qrcode from "./vendor/qrcode.mjs";

const root = document.getElementById("deck");
const roleValues = new Set(["audience", "presenter", "editor", "export", "web"]);
const scrollPositions = new Map();
const deferredPlayers = new WeakMap();

let deck = { mode: "preview", slides: [] };
let slides = [];
let currentSlide = 0;
let fragment = 0;
let recall = "";
let blank = "";
let overview = false;
let role = "editor";
let roleWasAssigned = false;
let activeMediaIndex = -1;
let renderGeneration = 0;
let renderToken = 0;
let recallSnapshot = null;

const fallbackPalette = {
    background: "#1d2021", foreground: "#ebdbb2", accent: "#d79921",
    muted: "#a89984", selection: "#504945", dark_background: "#1d2021",
    dark_foreground: "#d5c4a1", red: "#cc241d", orange: "#d65d0e",
    yellow: "#d79921", green: "#98971a", cyan: "#689d6a", blue: "#458588",
    magenta: "#b16286", brown: "#928374", bright_red: "#fb4934",
    bright_orange: "#fe8019", bright_yellow: "#fabd2f", bright_green: "#b8bb26",
    bright_cyan: "#8ec07c", bright_blue: "#83a598", bright_magenta: "#d3869b",
    bright_brown: "#a89984",
    ansi: ["#1d2021", "#cc241d", "#98971a", "#d79921", "#458588", "#b16286", "#689d6a", "#a89984", "#928374", "#fb4934", "#b8bb26", "#fabd2f", "#83a598", "#d3869b", "#8ec07c", "#ebdbb2"],
};

function escapeHtml(value) {
    return String(value ?? "").replaceAll("&", "&amp;").replaceAll("<", "&lt;")
        .replaceAll(">", "&gt;").replaceAll('"', "&quot;").replaceAll("'", "&#39;");
}

function highlightCode(source) {
    const pattern = /(\/\/[^\n]*|#[^\n]*|\/\*[\s\S]*?\*\/)|("(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'|`(?:\\.|[^`\\])*`)|(\b\d+(?:\.\d+)?\b)|(\b(?:break|case|catch|class|const|continue|def|do|else|enum|export|false|finally|for|from|function|if|import|in|interface|let|new|null|return|struct|switch|throw|true|try|var|while)\b)/g;
    let html = "";
    let offset = 0;
    for (const match of source.matchAll(pattern)) {
        html += escapeHtml(source.slice(offset, match.index));
        const className = match[1] ? "tok-comment" : match[2] ? "tok-string" : match[3] ? "tok-number" : "tok-keyword";
        html += `<span class="${className}">${escapeHtml(match[0])}</span>`;
        offset = match.index + match[0].length;
    }
    return html + escapeHtml(source.slice(offset));
}

const markdown = ir({
    html: false,
    linkify: true,
    typographer: true,
    highlight(source, language) {
        const languageClass = language ? ` language-${escapeHtml(language)}` : "";
        return `<pre class="op-code${languageClass}"><code>${highlightCode(source)}</code></pre>`;
    },
});

markdown.inline.ruler.after("escape", "omapresent_math", (state, silent) => {
    if (state.src[state.pos] !== "$" || state.src[state.pos + 1] === "$")
        return false;
    const end = state.src.indexOf("$", state.pos + 1);
    if (end <= state.pos + 1)
        return false;
    if (!silent) {
        const token = state.push("omapresent_math", "math", 0);
        token.content = state.src.slice(state.pos + 1, end);
    }
    state.pos = end + 1;
    return true;
});

markdown.renderer.rules.omapresent_math = (tokens, index) => renderMath(tokens[index].content, false);

function resolvedAsset(reference) {
    const assets = deck.assets ?? {};
    let decoded = reference;
    try {
        decoded = decodeURIComponent(reference);
    } catch {
        decoded = reference;
    }
    return assets[reference] ?? assets[decoded] ?? "";
}

function isRemoteSource(source) {
    return /^https?:\/\//i.test(String(source ?? "").trim());
}

function localBackgroundSource() {
    return isRemoteSource(deck.backgroundImage) ? "" : String(deck.backgroundImage ?? "");
}

function remoteImageLoaderMarkup(source, alt) {
    return `<button type="button" class="op-inline-image-loader" data-op-remote-image="true" data-source="${escapeHtml(source)}" data-alt="${escapeHtml(alt)}">Load remote image</button>`;
}

markdown.renderer.rules.image = (tokens, index) => {
    const token = tokens[index];
    const reference = token.attrGet("src") ?? "";
    const resolved = resolvedAsset(reference);
    const source = resolved || localBackgroundSource();
    if (isRemoteSource(source))
        return remoteImageLoaderMarkup(source, token.content);
    const sourceAttribute = source ? ` src="${escapeHtml(source)}"` : "";
    return `<img class="op-inline-image${resolved ? "" : " is-missing"}"${sourceAttribute} alt="${escapeHtml(token.content)}"${source ? "" : " hidden"}>`;
};

function renderMath(source, displayMode = true) {
    try {
        return katex.renderToString(source, { displayMode, throwOnError: false, strict: "ignore" });
    } catch {
        return `<code class="op-math-error">${escapeHtml(source)}</code>`;
    }
}

function applyTheme() {
    const palette = { ...fallbackPalette, ...(deck.palette ?? {}) };
    const style = document.documentElement.style;
    const named = ["background", "foreground", "accent", "muted", "selection", "dark_background", "dark_foreground", "red", "orange", "yellow", "green", "cyan", "blue", "magenta", "brown", "bright_red", "bright_orange", "bright_yellow", "bright_green", "bright_cyan", "bright_blue", "bright_magenta", "bright_brown"];
    for (const name of named)
        style.setProperty(`--op-${name.replaceAll("_", "-")}`, palette[name] ?? fallbackPalette[name]);
    const ansi = Array.isArray(palette.ansi) && palette.ansi.length >= 16 ? palette.ansi : fallbackPalette.ansi;
    ansi.slice(0, 16).forEach((value, index) => style.setProperty(`--op-ansi-${index}`, value));
    const requestedFont = String(deck.frontmatter?.font ?? "").trim().replaceAll('"', "");
    style.setProperty("--op-font-body", requestedFont ? `"${requestedFont}", "iA Writer Quattro S", system-ui, sans-serif` : 'system-ui, "iA Writer Quattro S", sans-serif');
    style.setProperty("--op-font-mono", '"iA Writer Mono S", ui-monospace, monospace');
    style.setProperty("--op-text-scale", String(Number(deck.textScale) > 0 ? Number(deck.textScale) : 1));
    document.documentElement.dataset.mode = deck.mode ?? "preview";
    document.documentElement.dataset.role = role;
}

function linearSlides() {
    const includeRecall = deck.mode === "pdf" || role === "export";
    return slidesForRender(deck.slides, includeRecall);
}

function recallSlides() {
    const result = new Map();
    for (const slide of Array.isArray(deck.slides) ? deck.slides : []) {
        const key = String(slide.recallKey ?? "").trim().toLowerCase();
        if (key && !result.has(key))
            result.set(key, slide);
    }
    return result;
}

function captureCurrentRevealState() {
    const scroller = currentScroller();
    return {
        slide: currentSlide,
        fragment,
        scrollTop: scroller?.scrollTop ?? 0,
    };
}

function restoreRevealState(snapshot) {
    if (!snapshot)
        return;
    currentSlide = Math.max(0, Math.min(slides.length - 1, snapshot.slide));
    fragment = Math.max(0, Math.min(parseSlide(slides[currentSlide]?.markdown ?? "").fragmentCount, snapshot.fragment));
    const scroller = currentScroller();
    if (scroller) {
        scroller.scrollTop = snapshot.scrollTop;
        scrollPositions.set(currentSlide, snapshot.scrollTop);
    }
    updateFragments();
}

function missingTag(reference) {
    const tag = document.createElement("span");
    tag.className = "op-missing-tag";
    tag.textContent = `missing: ${reference.split("/").at(-1) || reference}`;
    return tag;
}

function remoteImageLoader(source, alt) {
    const loader = document.createElement("button");
    loader.type = "button";
    loader.className = "op-image-loader";
    loader.dataset.opRemoteImage = "true";
    loader.dataset.source = source;
    loader.dataset.alt = alt;
    loader.setAttribute("aria-label", `Load remote image: ${alt}`);
    const mark = document.createElement("span");
    mark.className = "op-media-loader-mark";
    mark.textContent = "↗";
    mark.setAttribute("aria-hidden", "true");
    const label = document.createElement("span");
    label.className = "op-media-loader-label";
    label.textContent = "Load remote image";
    const detail = document.createElement("span");
    detail.className = "op-media-loader-detail";
    detail.textContent = "Connects to its host";
    loader.append(mark, label, detail);
    return loader;
}

function applyImageFallback(element, container, reference) {
    if (element.dataset.placeholder === "true")
        return;
    element.dataset.placeholder = "true";
    const background = localBackgroundSource();
    if (background)
        element.src = background;
    else
        element.hidden = true;
    element.classList.add("is-missing");
    container?.classList.add("is-missing");
    if (container && !container.querySelector(".op-missing-tag"))
        container.append(missingTag(reference));
}

function activateDeferredImage(loader) {
    const source = loader.dataset.source ?? "";
    if (!isRemoteSource(source))
        return null;
    const element = document.createElement("img");
    element.alt = loader.dataset.alt ?? "";
    element.decoding = "async";
    if (loader.classList.contains("op-inline-image-loader"))
        element.className = "op-inline-image";
    const container = loader.closest(".op-image");
    element.addEventListener("error", () => applyImageFallback(element, container, element.alt));
    // A subtitle track is outside #deck. Emitting slide state when its image
    // loads makes the published chrome replace the track with its original
    // notesHtml, which recreates the load button. Only slide images need a fit
    // pass and state event.
    if (loader.closest("#deck"))
        element.addEventListener("load", scheduleFitUpdate);
    loader.replaceWith(element);
    element.src = source;
    return element;
}

function imageFigure(image, tile = null) {
    const figure = document.createElement("figure");
    figure.className = `op-image${tile?.role === "hero" ? " is-hero" : ""}`;
    if (tile) {
        figure.style.gridColumn = `${tile.column} / span ${tile.columnSpan}`;
        figure.style.gridRow = `${tile.row} / span ${tile.rowSpan}`;
    }
    if (image.maxWidth)
        figure.style.maxWidth = `${image.maxWidth}px`;
    const resolved = resolvedAsset(image.reference);
    const source = resolved || localBackgroundSource();
    const alt = image.alt || image.reference;
    if (isRemoteSource(source)) {
        figure.append(remoteImageLoader(source, alt));
        return figure;
    }
    const element = document.createElement("img");
    element.alt = alt;
    element.decoding = "async";
    if (source)
        element.src = source;
    else
        element.hidden = true;
    if (!resolved) {
        figure.classList.add("is-missing");
        figure.append(missingTag(image.reference));
    }
    element.addEventListener("error", () => applyImageFallback(element, figure, image.reference));
    element.addEventListener("load", scheduleFitUpdate);
    figure.prepend(element);
    return figure;
}

function imagesElement(block, layout) {
    const container = document.createElement("div");
    container.className = "op-images";
    const bento = layout?.bento;
    if (block.images.length > 1 && bento && bento.kind !== "stacked") {
        container.classList.add("is-bento", layout.bento.kind);
        container.style.setProperty("--op-grid-columns", String(layout.bento.columns));
        container.style.setProperty("--op-grid-rows", String(layout.bento.rows));
    } else {
        container.classList.add(block.images.length > 1 ? "is-stacked" : "is-single");
    }
    block.images.forEach((image, index) => {
        const tile = layout?.bento?.tiles.find(item => item.index === index) ?? null;
        container.append(imageFigure(image, tile));
    });
    return container;
}

function qrElement(value) {
    const container = document.createElement("figure");
    container.className = "op-qr";
    try {
        const qr = qrcode(0, "M");
        qr.addData(value, "Byte");
        qr.make();
        const image = document.createElement("div");
        image.className = "op-qr-image";
        image.innerHTML = qr.createSvgTag({ cellSize: 4, margin: 4, scalable: true, alt: value, title: value });
        container.append(image);
    } catch {
        const failure = document.createElement("div");
        failure.className = "op-qr-failure";
        failure.textContent = "QR unavailable";
        container.append(failure);
    }
    const caption = document.createElement("figcaption");
    caption.textContent = value;
    container.append(caption);
    return container;
}

function sendEmbedPlayback(player, playing) {
    const message = player.dataset.host === "youtube"
        ? JSON.stringify({ event: "command", func: playing ? "playVideo" : "pauseVideo", args: [] })
        : { method: playing ? "play" : "pause" };
    player.contentWindow?.postMessage(message, "*");
    player.dataset.playing = playing ? "true" : "false";
}

function autoplaySource(source) {
    try {
        const url = new URL(source);
        url.searchParams.set("autoplay", "1");
        return url.toString();
    } catch {
        return source;
    }
}

function playerElement(decision, allowRemote = false, requestPlayback = false) {
    const remoteSource = isRemoteSource(decision.source);
    let player;
    if (decision.player === "file") {
        player = document.createElement("video");
        player.controls = true;
        player.preload = remoteSource ? "none" : "metadata";
        player.playsInline = true;
        if (!remoteSource || allowRemote)
            player.src = decision.source;
        if (decision.poster && (!isRemoteSource(decision.poster) || allowRemote))
            player.poster = decision.poster;
        player.addEventListener("ended", () => {
            player.dataset.ended = "true";
            emitState();
        });
        player.addEventListener("play", () => {
            player.dataset.ended = "false";
            emitState();
        });
    } else {
        player = document.createElement("iframe");
        if (!remoteSource || allowRemote)
            player.src = requestPlayback ? autoplaySource(decision.source) : decision.source;
        player.loading = "eager";
        player.allow = requestPlayback
            ? "autoplay; fullscreen; picture-in-picture"
            : "fullscreen; picture-in-picture";
        player.referrerPolicy = "strict-origin-when-cross-origin";
        player.title = decision.title || `${decision.host} video`;
    }
    player.className = "op-player";
    player.tabIndex = 0;
    player.dataset.host = decision.host;
    return player;
}

function activateDeferredPlayer(loader) {
    const decision = deferredPlayers.get(loader);
    if (!decision)
        return null;
    const player = playerElement(decision, true, true);
    loader.replaceWith(player);
    player.focus({ preventScroll: true });
    if (player.tagName === "VIDEO") {
        player.play().catch(() => {});
    } else {
        player.dataset.playing = "true";
        player.addEventListener("load", () => {
            sendEmbedPlayback(player, player.dataset.playing === "true");
        }, { once: true });
    }
    emitState();
    return player;
}

function deferredPlayerElement(decision) {
    const loader = document.createElement("button");
    loader.type = "button";
    loader.className = "op-player op-media-loader";
    loader.dataset.host = decision.host;
    loader.setAttribute("aria-label", `Play ${decision.host || "remote"} video`);
    const mark = document.createElement("span");
    mark.className = "op-media-loader-mark";
    mark.textContent = "▶";
    mark.setAttribute("aria-hidden", "true");
    const label = document.createElement("span");
    label.className = "op-media-loader-label";
    label.textContent = "Play video";
    const detail = document.createElement("span");
    detail.className = "op-media-loader-detail";
    detail.textContent = "Loads remote media";
    loader.append(mark, label, detail);
    deferredPlayers.set(loader, decision);
    loader.addEventListener("click", () => activateDeferredPlayer(loader));
    return loader;
}

function mediaElement(block) {
    if (block.media?.kind === "qr")
        return qrElement(block.media.value);
    const value = block.media?.value ?? block.raw.trim();
    const description = { ...(deck.media?.[value] ?? {}) };
    if (!description.cachedFile)
        description.cachedFile = resolvedAsset(value);
    const decision = mediaDecision(value, description);
    if (decision.kind === "qr")
        return qrElement(decision.value);
    const figure = document.createElement("figure");
    figure.className = `op-media${decision.vertical ? " is-vertical" : ""}`;
    if (!decision.source) {
        figure.classList.add("is-missing");
        figure.append(missingTag(value));
        return figure;
    }
    const player = isRemoteSource(decision.source)
        ? deferredPlayerElement(decision)
        : playerElement(decision);
    figure.append(player);
    return figure;
}

document.addEventListener("click", event => {
    const loader = event.target.closest?.("[data-op-remote-image]");
    if (loader?.closest("#deck, #op-notes"))
        activateDeferredImage(loader);
});

function outlineElement(block) {
    const container = document.createElement("div");
    container.className = "op-outline";
    const heading = document.createElement("div");
    heading.innerHTML = markdown.render(block.lines[0]);
    container.append(heading);
    const items = document.createElement("div");
    items.className = "op-outline-items";
    block.lines.slice(1).forEach(line => {
        const indent = line.match(/^(?:\t| {4})+/)?.[0] ?? "";
        const tabs = (indent.match(/\t/g) ?? []).length;
        const spaces = (indent.match(/ /g) ?? []).length;
        const item = document.createElement("div");
        item.className = "op-outline-item";
        item.style.setProperty("--op-outline-depth", String(Math.max(1, tabs + Math.floor(spaces / 4))));
        item.innerHTML = markdown.renderInline(line.trim());
        items.append(item);
    });
    container.append(items);
    return container;
}

function blockElement(block, layout) {
    const element = document.createElement("div");
    element.className = `op-block op-block-${block.type}`;
    if (block.type === "image" || block.type === "images")
        return imagesElement(block, layout);
    if (block.type === "heading-image-tight") {
        element.innerHTML = markdown.render(block.lines[0]);
        element.append(imagesElement({ ...block, type: block.images.length > 1 ? "images" : "image" }, layout));
        return element;
    }
    if (block.type === "outline")
        return outlineElement(block);
    if (block.type === "math") {
        const source = block.raw.trim().replace(/^\$\$\s*/, "").replace(/\s*\$\$$/, "").replace(/^\$/, "").replace(/\$$/, "");
        element.innerHTML = renderMath(source, true);
        return element;
    }
    if (block.type === "video" || block.type === "qr")
        return mediaElement(block);
    element.innerHTML = markdown.render(block.raw);
    return element;
}

function tokenValue(template, slide, index) {
    const frontmatter = deck.frontmatter ?? {};
    const values = { title: frontmatter.title ?? "", author: frontmatter.author ?? "", date: frontmatter.date ?? "", slide: index + 1, count: slides.length, heading: headingText(slide.markdown ?? "") };
    return String(template ?? "").replace(/\{(title|author|date|slide|count|heading)\}/g, (_, key) => String(values[key]));
}

function decorateSlide(section, slide, index) {
    const frontmatter = deck.frontmatter ?? {};
    if (frontmatter.header) {
        const header = document.createElement("header");
        header.className = "op-slide-header";
        header.textContent = tokenValue(frontmatter.header, slide, index);
        section.append(header);
    }
    if (frontmatter.footer) {
        const footer = document.createElement("footer");
        footer.className = "op-slide-footer";
        footer.textContent = tokenValue(frontmatter.footer, slide, index);
        section.append(footer);
    }
    if (frontmatter["slide-numbers"] === true) {
        const number = document.createElement("span");
        number.className = "op-slide-number";
        number.textContent = `${index + 1}/${slides.length}`;
        section.append(number);
    }
    if (frontmatter.progress === true) {
        const progress = document.createElement("div");
        progress.className = "op-progress";
        const bar = document.createElement("span");
        bar.style.width = `${slides.length ? ((index + 1) / slides.length) * 100 : 0}%`;
        progress.append(bar);
        section.append(progress);
    }
}

function notesHtmlFor(parsed) {
    const source = parsed.noteBlocks.map(block => block.raw).join("\n\n");
    return source ? markdown.render(source) : "";
}

function slideElement(slide, index, options = {}) {
    const parsed = parseSlide(slide.markdown ?? "");
    const layout = layoutForBlocks(parsed.blocks);
    const section = document.createElement("section");
    section.className = `op-slide layout-${layout.kind}`;
    section.dataset.slideIndex = String(index);
    const scroller = document.createElement("div");
    scroller.className = "op-scroll";
    const stack = document.createElement("div");
    stack.className = "op-stack";
    const content = document.createElement("div");
    content.className = "op-content";
    const contentBlocks = options.flowAllBlocks ? parsed.blocks : parsed.screenBlocks;
    const blockElements = [];
    contentBlocks.forEach(block => {
        if (block.type === "note") {
            const note = document.createElement("aside");
            note.className = "op-notes is-flow-note";
            note.innerHTML = markdown.render(block.raw);
            content.append(note);
            blockElements.push({ block, element: note });
        } else {
            const element = blockElement(block, layout);
            content.append(element);
            blockElements.push({ block, element });
        }
    });
    if (!parsed.screenBlocks.length)
        content.classList.add("is-blank");
    if (options.showNotes) {
        const notesHtml = notesHtmlFor(parsed);
        if (notesHtml) {
            const notes = document.createElement("aside");
            notes.className = "op-notes";
            notes.innerHTML = notesHtml;
            content.append(notes);
        }
    }
    stack.append(content);
    scroller.append(stack);
    section.append(scroller);
    decorateSlide(section, slide, index);
    let fragmentIndex = 0;
    let headingCount = 0;
    blockElements.forEach(({ block, element }) => {
        if (block.type === "heading") {
            headingCount += 1;
            if (headingCount > 1) {
                fragmentIndex += 1;
                element.classList.add("op-fragment");
                element.dataset.fragment = String(fragmentIndex);
            }
        }
        if (block.type === "note")
            return;
        element.querySelectorAll("li").forEach(item => {
            fragmentIndex += 1;
            item.classList.add("op-fragment");
            item.dataset.fragment = String(fragmentIndex);
        });
    });
    if (options.showAllFragments)
        section.querySelectorAll(".op-fragment").forEach(item => item.dataset.revealed = "true");
    return { section, parsed, fragmentCount: fragmentIndex };
}

function updateFragments() {
    root.querySelector(`.op-slide[data-slide-index="${currentSlide}"]`)?.querySelectorAll(".op-fragment").forEach(item => {
        item.dataset.revealed = Number(item.dataset.fragment) <= fragment ? "true" : "false";
    });
}

function readView() {
    return document.documentElement.dataset.opView === "read";
}

function renderAll() {
    const showNotes = role === "web" || role === "presenter";
    const flowAllBlocks = readView();
    slides.forEach((slide, index) => root.append(slideElement(slide, index, {
        showNotes: showNotes && !flowAllBlocks,
        showAllFragments: true,
        flowAllBlocks,
    }).section));
    root.className = "op-all-slides";
}

function clearPrintPageHeights() {
    root.querySelectorAll(".op-slide").forEach(section => {
        section.style.removeProperty("--op-print-slide-height");
    });
}

function preparePrintPageHeights() {
    if (role !== "export" && deck.mode !== "pdf")
        return;
    clearPrintPageHeights();
    const ruler = document.createElement("div");
    ruler.className = "op-print-page-ruler";
    document.body.append(ruler);
    const pageHeight = ruler.getBoundingClientRect().height;
    ruler.remove();
    if (!(pageHeight > 0))
        return;
    root.querySelectorAll(".op-all-slides > .op-slide").forEach(section => {
        const naturalHeight = Math.max(section.scrollHeight, section.getBoundingClientRect().height);
        const pageCount = Math.max(1, Math.ceil((naturalHeight - 0.5) / pageHeight));
        section.style.setProperty("--op-print-slide-height", `${pageCount * pageHeight}px`);
    });
}

function renderOverview() {
    const grid = document.createElement("div");
    grid.className = "op-overview";
    slides.forEach((slide, index) => {
        const button = document.createElement("button");
        button.type = "button";
        button.className = `op-overview-item${index === currentSlide ? " is-current" : ""}`;
        button.append(slideElement(slide, index, { showAllFragments: true }).section);
        button.addEventListener("click", () => {
            overview = false;
            gotoSlide(index, true);
        });
        grid.append(button);
    });
    root.append(grid);
    root.className = "op-overview-root";
}

function renderCurrent() {
    const token = ++renderToken;
    document.querySelectorAll(".op-blank-overlay, .op-recall-overlay").forEach(element => element.remove());
    root.replaceChildren();
    if (!slides.length) {
        const empty = document.createElement("section");
        empty.className = "op-empty";
        empty.textContent = "Open a Markdown file to start presenting.";
        root.append(empty);
        root.className = "op-current-slide";
        return;
    }
    if (overview) {
        renderOverview();
        return;
    }
    if (role === "export" || deck.mode === "pdf" || readView()) {
        renderAll();
        return;
    }
    root.append(slideElement(slides[currentSlide], currentSlide, {
        showNotes: role === "presenter" || (role === "web" && readView()),
    }).section);
    root.className = "op-current-slide";
    updateFragments();
    requestAnimationFrame(() => {
        if (token !== renderToken)
            return;
        const scroller = currentScroller();
        if (scroller)
            scroller.scrollTop = scrollPositions.get(currentSlide) ?? 0;
        scheduleFitUpdate();
    });
    renderOverlays();
}

function renderOverlays() {
    document.querySelectorAll(".op-blank-overlay, .op-recall-overlay").forEach(element => element.remove());
    if (recall) {
        const recalled = recallSlides().get(recall);
        if (recalled) {
            const overlay = document.createElement("div");
            overlay.className = "op-recall-overlay";
            const section = slideElement(recalled, currentSlide, { showNotes: role === "presenter", showAllFragments: true }).section;
            section.querySelectorAll(".op-fragment").forEach(item => {
                item.dataset.revealed = "true";
            });
            overlay.append(section);
            document.body.append(overlay);
            root.classList.add("has-recall");
        }
    } else {
        root.classList.remove("has-recall");
    }
    if (blank) {
        const overlay = document.createElement("div");
        overlay.className = `op-blank-overlay is-${blank}`;
        document.body.append(overlay);
    }
}

function currentScroller() {
    return root.querySelector(`.op-slide[data-slide-index="${currentSlide}"] > .op-scroll`);
}

function currentFragmentCount() {
    return root.querySelectorAll(`.op-slide[data-slide-index="${currentSlide}"] .op-fragment`).length || parseSlide(slides[currentSlide]?.markdown ?? "").fragmentCount;
}

function saveScroll() {
    const scroller = currentScroller();
    if (scroller)
        scrollPositions.set(currentSlide, scroller.scrollTop);
}

function scheduleFitUpdate() {
    const generation = ++renderGeneration;
    requestAnimationFrame(() => {
        if (generation !== renderGeneration)
            return;
        const scroller = currentScroller();
        if (scroller) {
            const fit = fitDecision(scroller.scrollHeight, scroller.clientHeight, scroller.scrollTop);
            scroller.closest(".op-slide")?.classList.toggle("is-scrollable", fit.scrollable);
        }
        emitState();
    });
}

function stateObject() {
    const slide = slides[currentSlide] ?? { markdown: "" };
    const parsed = parseSlide(slide.markdown ?? "");
    const scroller = currentScroller();
    const fit = scroller ? fitDecision(scroller.scrollHeight, scroller.clientHeight, scroller.scrollTop) : fitDecision(0, 0, 0);
    const nextIndex = Math.min(currentSlide + 1, Math.max(0, slides.length - 1));
    const next = slides[nextIndex];
    const nextHtml = next && nextIndex !== currentSlide ? slideElement(next, nextIndex, { showAllFragments: true }).section.outerHTML : "";
    const players = root.querySelectorAll(`.op-slide[data-slide-index="${currentSlide}"] .op-player`);
    return {
        slideIndex: currentSlide, slideCount: slides.length, fragment,
        fragmentCount: currentFragmentCount(), scrollFraction: fit.scrollFraction,
        scrollable: fit.scrollable, recall, blank, overview,
        heading: headingText(slide.markdown ?? ""), notesHtml: notesHtmlFor(parsed),
        nextSlideHtml: nextHtml, recallKeys: [...recallSlides().keys()],
        mediaCount: players.length,
        mediaActive: players.length > 0,
    };
}

function emitState() {
    const state = stateObject();
    if (typeof api.onState === "function") {
        try {
            api.onState(state);
        } catch {
            // A host callback failure must not stop the renderer.
        }
    } else if (globalThis.omapresentHost && typeof globalThis.omapresentHost.state === "function") {
        globalThis.omapresentHost.state(JSON.stringify(state));
    }
}

function gotoSlide(index, resetFragments) {
    if (!slides.length)
        return;
    saveScroll();
    currentSlide = Math.max(0, Math.min(slides.length - 1, Math.trunc(Number(index) || 0)));
    if (recall) {
        if (recallSnapshot) {
            recallSnapshot.slide = currentSlide;
            const fragmentCount = parseSlide(slides[currentSlide]?.markdown ?? "").fragmentCount;
            fragment = Math.max(0, Math.min(fragmentCount, recallSnapshot.fragment));
            const rebasedScrollTop = recallSnapshot.scrollTop;
            scrollPositions.set(currentSlide, rebasedScrollTop);
            renderCurrent();
            const scroller = currentScroller();
            if (scroller)
                scroller.scrollTop = rebasedScrollTop;
            recallSnapshot.scrollTop = rebasedScrollTop;
        } else {
            recallSnapshot = captureCurrentRevealState();
        }
        emitState();
        return;
    }
    if (resetFragments)
        fragment = 0;
    activeMediaIndex = -1;
    renderCurrent();
    emitState();
}

function replaceDeck(nextDeck) {
    saveScroll();
    const oldSlide = currentSlide;
    const oldFragment = fragment;
    const activeRecall = recall;
    deck = nextDeck && typeof nextDeck === "object" ? nextDeck : { mode: "preview", slides: [] };
    if (activeRecall && !recallSlides().has(activeRecall)) {
        recall = "";
        recallSnapshot = null;
    }
    if (!recall)
        recallSnapshot = null;
    if (!roleWasAssigned) {
        role = deck.mode === "pdf" ? "export" : deck.mode === "web" ? "web" : deck.mode === "present" ? "audience" : "editor";
    }
    slides = linearSlides();
    currentSlide = Math.max(0, Math.min(slides.length - 1, oldSlide));
    fragment = Math.max(0, Math.min(parseSlide(slides[currentSlide]?.markdown ?? "").fragmentCount, oldFragment));
    applyTheme();
    const firstVisibleHeading = slides.map(slide => headingText(slide.markdown ?? ""))
        .find(Boolean) ?? "";
    document.title = deck.frontmatter?.publish?.title
        || deck.frontmatter?.title
        || firstVisibleHeading
        || (deck.mode === "web" ? document.title : "Omapresent");
    renderCurrent();
    emitState();
}

const api = {
    render: replaceDeck,
    update: replaceDeck,
    goto(index) { gotoSlide(index, true); },
    next() {
        if (recall) {
            api.hideRecall();
            return;
        }
        const count = currentFragmentCount();
        if (fragment < count) {
            fragment += 1;
            updateFragments();
            emitState();
        } else if (currentSlide < slides.length - 1) {
            gotoSlide(currentSlide + 1, true);
        } else {
            emitState();
        }
    },
    previous() {
        if (recall) {
            api.hideRecall();
            return;
        }
        if (fragment > 0) {
            fragment -= 1;
            updateFragments();
            emitState();
        } else if (currentSlide > 0) {
            gotoSlide(currentSlide - 1, true);
            fragment = currentFragmentCount();
            updateFragments();
            emitState();
        }
    },
    scrollBy(deltaPixels) {
        const scroller = currentScroller();
        if (!scroller)
            return;
        scroller.scrollTop += Number(deltaPixels) || 0;
        scrollPositions.set(currentSlide, scroller.scrollTop);
        emitState();
    },
    setScroll(fractionValue) {
        const scroller = currentScroller();
        if (!scroller)
            return;
        scroller.scrollTop = scrollTopForFraction(scroller.scrollHeight, scroller.clientHeight, fractionValue);
        scrollPositions.set(currentSlide, scroller.scrollTop);
        emitState();
    },
    showRecall(key) {
        const candidate = String(key ?? "").trim().toLowerCase();
        if (!recallSlides().has(candidate))
            return;
        if (!recall)
            recallSnapshot = captureCurrentRevealState();
        recall = candidate;
        renderOverlays();
        emitState();
    },
    hideRecall() {
        const snapshot = recallSnapshot;
        if (!recall) {
            recallSnapshot = null;
            emitState();
            return;
        }
        recall = "";
        renderOverlays();
        restoreRevealState(snapshot);
        recallSnapshot = null;
        emitState();
    },
    setBlank(mode) {
        blank = ["black", "white"].includes(mode) ? mode : "";
        renderOverlays();
        emitState();
    },
    setOverview(on) {
        overview = Boolean(on);
        renderCurrent();
        emitState();
    },
    playPause() {
        const players = [...root.querySelectorAll(`.op-slide[data-slide-index="${currentSlide}"] .op-player`)];
        if (!players.length) {
            api.next();
            return;
        }
        const player = players[0];
        if (player.classList.contains("op-media-loader")) {
            activateDeferredPlayer(player);
            return;
        }
        if (player.dataset.ended === "true") {
            player.dataset.ended = "false";
            api.next();
            return;
        }
        if (player.tagName === "VIDEO") {
            if (player.paused)
                player.play().catch(() => {});
            else
                player.pause();
        } else {
            const playing = player.dataset.playing === "true";
            sendEmbedPlayback(player, !playing);
        }
        emitState();
    },
    focusNextMedia() {
        const players = [...root.querySelectorAll(`.op-slide[data-slide-index="${currentSlide}"] .op-player`)];
        if (!players.length)
            return;
        const focusedIndex = players.indexOf(document.activeElement);
        const fromIndex = focusedIndex >= 0 ? focusedIndex : activeMediaIndex;
        activeMediaIndex = fromIndex >= 0 ? (fromIndex + 1) % players.length : 0;
        players[activeMediaIndex].focus();
        emitState();
    },
    onState: null,
};

Object.defineProperty(api, "role", {
    enumerable: true,
    get() { return role; },
    set(value) {
        if (!roleValues.has(value))
            return;
        roleWasAssigned = true;
        role = value;
        slides = linearSlides();
        currentSlide = Math.max(0, Math.min(slides.length - 1, currentSlide));
        applyTheme();
        renderCurrent();
        emitState();
    },
});

root.addEventListener("scroll", event => {
    if (!event.target.classList?.contains("op-scroll"))
        return;
    scrollPositions.set(currentSlide, event.target.scrollTop);
    emitState();
}, true);
window.addEventListener("resize", scheduleFitUpdate);
window.addEventListener("beforeprint", preparePrintPageHeights);
window.addEventListener("afterprint", clearPrintPageHeights);
document.addEventListener("keydown", event => {
    if (document.documentElement.dataset.opView || globalThis.omapresentHost || event.metaKey || event.ctrlKey || event.altKey)
        return;
    if ((event.key === " " || event.key === "Enter")
        && event.target instanceof Element
        && event.target.closest(".op-media-loader, [data-op-remote-image]"))
        return;
    if (event.key === "ArrowRight") api.next();
    else if (event.key === "ArrowLeft") api.previous();
    else if (event.key === "ArrowDown" || event.key === "PageDown") api.scrollBy(event.key === "PageDown" ? 640 : 120);
    else if (event.key === "ArrowUp" || event.key === "PageUp") api.scrollBy(event.key === "PageUp" ? -640 : -120);
    else if (event.key === " ") root.querySelector(".op-player") ? api.playPause() : api.next();
    else if (event.key === "Tab" && root.querySelectorAll(".op-player").length > 1) api.focusNextMedia();
    else return;
    event.preventDefault();
});

window.omapresent = api;
if (globalThis.omapresentFixture)
    api.render(globalThis.omapresentFixture);
