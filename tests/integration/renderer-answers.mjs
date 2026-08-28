// Asks the renderer modules the questions the C++ side also answers, so
// tst_integration.cpp can compare the two implementations directly. Reads a
// JSON request from argv[2], writes a JSON answer to stdout. No DOM, no
// network — the same constraint the renderer suites run under.
//
// Request:  { "urls": [line], "imageLines": [line], "slides": [markdown] }
// Answer:   { "urls":  [{ "bare": bool, "host": string }],
//             "imageLines": [reference or null],
//             "slides": [{ "screen": int, "notes": int, "images": [reference] }] }
//
// "imageLines" asks the line-level helper in isolation; "slides.images" asks
// what the renderer would actually draw once it has classified the slide, so a
// path-like line inside a fenced code block is code and not an image.

import { readFileSync } from "node:fs";

import { parseImageReference, parseSlide } from "../../src/renderer/deckparse.js";
import { isBareUrlLine, videoHostFor } from "../../src/renderer/media.js";

const request = JSON.parse(readFileSync(process.argv[2], "utf8"));

const answer = {
    urls: (request.urls ?? []).map(line => ({
        bare: isBareUrlLine(line),
        host: videoHostFor(line),
    })),
    imageLines: (request.imageLines ?? []).map(line => parseImageReference(line)?.reference ?? null),
    slides: (request.slides ?? []).map(markdown => {
        const slide = parseSlide(markdown);
        return {
            screen: slide.screenBlocks.length,
            notes: slide.noteBlocks.length,
            images: slide.blocks
                .flatMap(block => block.images ?? [])
                .filter(Boolean)
                .map(image => image.reference),
        };
    }),
};

process.stdout.write(JSON.stringify(answer));
