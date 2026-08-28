import { forcedQrValue, isBareUrlLine, mediaDecision, videoHostFor } from "./media.js";

const IMAGE_EXTENSIONS = /\.(?:apng|avif|bmp|gif|heic|heif|jpe?g|png|raw|svg|tiff?|webp|pdf)(?:$|[?#])/i;
const LIST_ITEM = /^(\s*)(?:[-+*]|\d+[.)])\s+(.*)$/;
const ATX_HEADING = /^ {0,3}(#{1,6})(?:\s+|$)(.*)$/;
const SETEXT_UNDERLINE = /^ {0,3}(?:=+|-+)\s*$/;

function normaliseNewlines(value) {
    return String(value ?? "").replace(/\r\n?/g, "\n");
}

function fenceMarker(line) {
    const match = line.match(/^\s*(`{3,}|~{3,})(.*)$/);
    return match ? { marker: match[1][0], length: match[1].length } : null;
}

function closesFence(line, fence) {
    const match = line.match(/^\s*(`{3,}|~{3,})\s*$/);
    return Boolean(match && match[1][0] === fence.marker && match[1].length >= fence.length);
}

export function splitBlocks(markdown) {
    const lines = normaliseNewlines(markdown).split("\n");
    const blocks = [];
    let current = [];
    let startLine = 0;
    let fence = null;
    let mathFence = false;

    const flush = endLine => {
        if (!current.length)
            return;
        blocks.push({
            raw: current.join("\n"),
            lines: [...current],
            startLine,
            endLine,
        });
        current = [];
    };

    lines.forEach((line, index) => {
        const trimmed = line.trim();
        if (!current.length && trimmed)
            startLine = index;

        if (fence) {
            current.push(line);
            if (closesFence(line, fence))
                fence = null;
            return;
        }

        if (mathFence) {
            current.push(line);
            if (trimmed === "$$")
                mathFence = false;
            return;
        }

        const openingFence = fenceMarker(line);
        if (openingFence) {
            current.push(line);
            fence = openingFence;
            return;
        }

        if (trimmed === "$$") {
            current.push(line);
            mathFence = true;
            return;
        }

        if (!trimmed) {
            flush(index - 1);
            return;
        }

        current.push(line);
    });

    flush(lines.length - 1);
    return blocks;
}

function parseObsidianImage(line) {
    const match = line.trim().match(/^!\[\[([\s\S]+?)\]\]$/);
    if (!match || /^qr:/i.test(match[1]))
        return null;

    const parts = match[1].split("|");
    const hint = parts.length > 1 ? parts.at(-1).trim() : "";
    const hasHint = hint === "main" || /^\d+$/.test(hint);
    const reference = (hasHint ? parts.slice(0, -1) : parts).join("|").trim();
    if (!reference)
        return null;

    return {
        reference,
        alt: reference.split("/").at(-1) || reference,
        main: hint === "main",
        maxWidth: /^\d+$/.test(hint) ? Number(hint) : null,
        source: line.trim(),
    };
}

function parseMarkdownImage(line) {
    const value = line.trim();
    const match = value.match(/^!\[([^\]]*)\]\(([\s\S]+)\)$/);
    if (!match)
        return null;

    let target = match[2].trim();
    if (target.startsWith("<") && target.endsWith(">"))
        target = target.slice(1, -1);
    else
        target = target.replace(/\s+(?:"[^"]*"|'[^']*'|\([^)]*\))\s*$/, "");
    if (!target)
        return null;

    return {
        reference: target,
        alt: match[1],
        main: false,
        maxWidth: null,
        source: value,
    };
}

function parseBareImage(line) {
    const value = line.trim();
    if (!value || /\n/.test(value) || forcedQrValue(value))
        return null;
    if (videoHostFor(value))
        return null;
    const webUrl = /^https?:\/\//i.test(value);
    const explicitPath = /^(?:\.{0,2}\/|~\/|\$[A-Za-z_][A-Za-z0-9_]*\/|file:\/\/)/.test(value);
    const slashPath = value.includes("/") && (!/\s/.test(value) || explicitPath);
    if (!IMAGE_EXTENSIONS.test(value) && (!slashPath || webUrl || value.includes("://")))
        return null;
    return {
        reference: value,
        alt: value.split("/").at(-1) || value,
        main: false,
        maxWidth: null,
        source: value,
    };
}

export function parseImageReference(line) {
    return parseObsidianImage(line) || parseMarkdownImage(line) || parseBareImage(line);
}

export function isHeadingLine(line) {
    return ATX_HEADING.test(line);
}

function isSetextHeading(lines) {
    return lines.length === 2 && Boolean(lines[0].trim()) && SETEXT_UNDERLINE.test(lines[1]);
}

function isListBlock(lines) {
    if (!LIST_ITEM.test(lines[0] ?? ""))
        return false;
    return lines.every(line => LIST_ITEM.test(line) || /^\s{2,}\S/.test(line));
}

function isTableBlock(lines) {
    if (lines.length < 2 || !lines[0].includes("|"))
        return false;
    return /^\s*\|?\s*:?-{3,}:?(?:\s*\|\s*:?-{3,}:?)+\s*\|?\s*$/.test(lines[1]);
}

function isMathBlock(lines) {
    const value = lines.join("\n").trim();
    return /^\$\$[\s\S]*\$\$$/.test(value)
        || (lines.length === 1 && /^\$(?!\$).+\$$/.test(value));
}

function headingAndIndented(lines) {
    return isHeadingLine(lines[0] ?? "")
        && lines.length > 1
        && lines.slice(1).every(line => /^(?:\t| {4,})\S/.test(line));
}

function headingAndImages(lines) {
    return isHeadingLine(lines[0] ?? "")
        && lines.length > 1
        && lines.slice(1).every(line => Boolean(parseImageReference(line)));
}

function screenMixedBlock(lines) {
    return lines.some((line, index) => startsAudienceAt(lines, index));
}

export function classifyBlock(block) {
    const input = typeof block === "string"
        ? { raw: normaliseNewlines(block), lines: normaliseNewlines(block).split("\n") }
        : block;
    const lines = input.lines;
    const raw = input.raw;
    const trimmed = raw.trim();
    let type = "note";
    let media = null;
    let images = [];

    if (/^```qr(?:\s|$)/i.test(trimmed)) {
        type = "qr";
        media = mediaDecision(trimmed);
    } else if (/^\s*(`{3,}|~{3,})/.test(lines[0] ?? "")) {
        type = "code";
    } else if (isMathBlock(lines)) {
        type = "math";
    } else if (headingAndImages(lines)) {
        type = "heading-image-tight";
        images = lines.slice(1).map(parseImageReference);
    } else if (headingAndIndented(lines)) {
        type = "outline";
    } else if ((lines.length === 1 && isHeadingLine(lines[0])) || isSetextHeading(lines)) {
        type = "heading";
    } else if (lines.every(line => Boolean(parseImageReference(line)))) {
        images = lines.map(parseImageReference);
        type = images.length === 1 ? "image" : "images";
    } else if (lines.every(line => /^(?:\t| {4,})\S/.test(line))) {
        type = "code";
    } else if (isListBlock(lines)) {
        type = "list";
    } else if (isTableBlock(lines)) {
        type = "table";
    } else if (lines.every(line => /^\s*>/.test(line))) {
        type = "quote";
    } else if (lines.length === 1 && forcedQrValue(lines[0])) {
        type = "qr";
        media = mediaDecision(lines[0]);
    } else if (lines.length === 1 && (isBareUrlLine(lines[0]) || videoHostFor(lines[0]))) {
        media = mediaDecision(lines[0]);
        type = media.kind === "video" ? "video" : "qr";
    } else if (screenMixedBlock(lines)) {
        type = "mixed";
    }

    const fragmentCount = type === "list"
        ? lines.filter(line => LIST_ITEM.test(line)).length
        : 0;

    return {
        ...input,
        type,
        audience: type === "mixed" ? null : type !== "note",
        fragmentCount,
        images,
        media,
    };
}

function isIndentedLine(line) {
    return /^(?:\t| {4,})\S/.test(line);
}

function startsAudienceAt(lines, index) {
    const line = lines[index] ?? "";
    return Boolean(fenceMarker(line))
        || line.trim() === "$$"
        || isMathBlock([line])
        || isHeadingLine(line)
        || (Boolean(line.trim()) && SETEXT_UNDERLINE.test(lines[index + 1] ?? ""))
        || isTableBlock(lines.slice(index, index + 2))
        || Boolean(parseImageReference(line))
        || LIST_ITEM.test(line)
        || /^\s*>/.test(line)
        || Boolean(forcedQrValue(line))
        || isBareUrlLine(line)
        || Boolean(videoHostFor(line))
        || isIndentedLine(line);
}

function logicalBlock(block, start, end) {
    const lines = block.lines.slice(start, end + 1);
    return {
        raw: lines.join("\n"),
        lines,
        startLine: (block.startLine ?? 0) + start,
        endLine: (block.startLine ?? 0) + end,
    };
}

function splitLogicalBlock(block) {
    const segments = [];
    const lines = block.lines;
    let index = 0;

    const add = (start, end) => {
        segments.push(classifyBlock(logicalBlock(block, start, end)));
        index = end + 1;
    };

    while (index < lines.length) {
        const line = lines[index];
        const fence = fenceMarker(line);
        if (fence) {
            let end = index + 1;
            while (end < lines.length && !closesFence(lines[end], fence))
                end += 1;
            add(index, Math.min(end, lines.length - 1));
            continue;
        }

        if (line.trim() === "$$") {
            let end = index + 1;
            while (end < lines.length && lines[end].trim() !== "$$")
                end += 1;
            add(index, Math.min(end, lines.length - 1));
            continue;
        }

        if (isHeadingLine(line)) {
            let end = index;
            if (parseImageReference(lines[index + 1] ?? "")) {
                while (parseImageReference(lines[end + 1] ?? ""))
                    end += 1;
            } else if (isIndentedLine(lines[index + 1] ?? "") && !LIST_ITEM.test(lines[index + 1])) {
                while (isIndentedLine(lines[end + 1] ?? "") && !LIST_ITEM.test(lines[end + 1]))
                    end += 1;
            }
            add(index, end);
            continue;
        }

        if (Boolean(line.trim()) && SETEXT_UNDERLINE.test(lines[index + 1] ?? "")) {
            add(index, index + 1);
            continue;
        }

        if (isTableBlock(lines.slice(index, index + 2))) {
            let end = index + 1;
            while (end + 1 < lines.length && lines[end + 1].includes("|"))
                end += 1;
            add(index, end);
            continue;
        }

        if (parseImageReference(line)) {
            let end = index;
            while (parseImageReference(lines[end + 1] ?? ""))
                end += 1;
            add(index, end);
            continue;
        }

        if (isIndentedLine(line)) {
            let end = index;
            while (isIndentedLine(lines[end + 1] ?? ""))
                end += 1;
            add(index, end);
            continue;
        }

        if (LIST_ITEM.test(line)) {
            let end = index;
            while (end + 1 < lines.length && (LIST_ITEM.test(lines[end + 1]) || /^\s{2,}\S/.test(lines[end + 1])))
                end += 1;
            add(index, end);
            continue;
        }

        if (/^\s*>/.test(line)) {
            let end = index;
            while (/^\s*>/.test(lines[end + 1] ?? ""))
                end += 1;
            add(index, end);
            continue;
        }

        if (startsAudienceAt(lines, index)) {
            add(index, index);
            continue;
        }

        let end = index;
        while (end + 1 < lines.length && !startsAudienceAt(lines, end + 1))
            end += 1;
        add(index, end);
    }

    return segments;
}

function classifiedBlocks(markdown) {
    return splitBlocks(markdown).flatMap(splitLogicalBlock);
}

export function countFragments(input) {
    const blocks = Array.isArray(input) ? input : classifiedBlocks(input);
    return blocks.reduce((count, block) => count + (block.fragmentCount ?? classifyBlock(block).fragmentCount), 0);
}

export function parseSlide(markdown) {
    const blocks = classifiedBlocks(markdown);
    return {
        markdown: normaliseNewlines(markdown),
        blocks,
        screenBlocks: blocks.filter(block => block.audience === true),
        noteBlocks: blocks.filter(block => block.audience === false),
        fragmentCount: countFragments(blocks),
    };
}

export function headingText(markdown) {
    for (const block of classifiedBlocks(markdown)) {
        if (block.type === "code")
            continue;
        for (const line of block.lines) {
            const atx = line.match(ATX_HEADING);
            if (atx)
                return atx[2].replace(/\s+#+\s*$/, "").trim();
        }
        if (isSetextHeading(block.lines))
            return block.lines[0].trim();
    }
    return "";
}

export function slidesForRender(allSlides, includeRecall = false) {
    return (Array.isArray(allSlides) ? allSlides : []).filter(slide => {
        if (!slide.skip && slide.index !== -1)
            return true;
        return includeRecall && Boolean(String(slide.recallKey ?? "").trim());
    });
}

export { IMAGE_EXTENSIONS, LIST_ITEM };
