function clamp(value, minimum, maximum) {
    return Math.min(maximum, Math.max(minimum, value));
}

function regularTiles(count) {
    if (count === 2)
        return [
            { column: 1, row: 1, columnSpan: 1, rowSpan: 1 },
            { column: 2, row: 1, columnSpan: 1, rowSpan: 1 },
        ];
    if (count === 3)
        return Array.from({ length: 3 }, (_, index) => ({ column: index + 1, row: 1, columnSpan: 1, rowSpan: 1 }));
    if (count === 4)
        return Array.from({ length: 4 }, (_, index) => ({ column: (index % 2) + 1, row: Math.floor(index / 2) + 1, columnSpan: 1, rowSpan: 1 }));
    if (count === 5)
        return [
            { column: 1, row: 1, columnSpan: 3, rowSpan: 1 },
            { column: 4, row: 1, columnSpan: 3, rowSpan: 1 },
            { column: 1, row: 2, columnSpan: 2, rowSpan: 1 },
            { column: 3, row: 2, columnSpan: 2, rowSpan: 1 },
            { column: 5, row: 2, columnSpan: 2, rowSpan: 1 },
        ];
    if (count === 6)
        return [
            { column: 1, row: 1, columnSpan: 2, rowSpan: 1 },
            { column: 3, row: 1, columnSpan: 4, rowSpan: 1 },
            { column: 1, row: 2, columnSpan: 3, rowSpan: 1 },
            { column: 4, row: 2, columnSpan: 3, rowSpan: 1 },
            { column: 1, row: 3, columnSpan: 4, rowSpan: 1 },
            { column: 5, row: 3, columnSpan: 2, rowSpan: 1 },
        ];
    return [];
}

function heroTiles(count, mainIndex) {
    const sideTiles = [];
    if (count === 2) {
        sideTiles.push({ column: 3, row: 1, columnSpan: 1, rowSpan: 2 });
        return { columns: 3, rows: 2, hero: { column: 1, row: 1, columnSpan: 2, rowSpan: 2 }, sideTiles };
    }
    if (count === 3) {
        sideTiles.push(
            { column: 1, row: 1, columnSpan: 1, rowSpan: 2 },
            { column: 4, row: 1, columnSpan: 1, rowSpan: 2 },
        );
        return { columns: 4, rows: 2, hero: { column: 2, row: 1, columnSpan: 2, rowSpan: 2 }, sideTiles };
    }
    if (count === 4) {
        sideTiles.push(
            { column: 1, row: 1, columnSpan: 1, rowSpan: 4 },
            { column: 4, row: 1, columnSpan: 1, rowSpan: 2 },
            { column: 4, row: 3, columnSpan: 1, rowSpan: 2 },
        );
        return { columns: 4, rows: 4, hero: { column: 2, row: 1, columnSpan: 2, rowSpan: 4 }, sideTiles };
    }

    sideTiles.push(
        { column: 1, row: 1, columnSpan: 1, rowSpan: 2 },
        { column: 1, row: 3, columnSpan: 1, rowSpan: 2 },
        { column: 5, row: 1, columnSpan: 1, rowSpan: 2 },
        { column: 5, row: 3, columnSpan: 1, rowSpan: 2 },
    );
    if (count === 6)
        sideTiles.push({ column: 2, row: 4, columnSpan: 3, rowSpan: 1 });
    return {
        columns: 5,
        rows: count === 6 ? 4 : 4,
        hero: { column: 2, row: 1, columnSpan: 3, rowSpan: count === 6 ? 3 : 4 },
        sideTiles,
    };
}

export function bentoArrangement(imageCount, requestedMainIndex = -1) {
    const count = Number(imageCount);
    if (!Number.isInteger(count) || count < 2 || count > 6)
        return { kind: "stacked", columns: 1, rows: Math.max(0, Number.isFinite(count) ? count : 0), heroIndex: -1, tiles: [] };

    const mainIndex = Number.isInteger(requestedMainIndex) && requestedMainIndex >= 0 && requestedMainIndex < count
        ? requestedMainIndex
        : -1;
    if (mainIndex < 0) {
        const columns = count <= 4 ? (count === 4 ? 2 : count) : 6;
        const rows = count <= 3 ? 1 : count === 4 || count === 5 ? 2 : 3;
        return {
            kind: count <= 4 ? `bento-${count}` : "bento-mosaic",
            columns,
            rows,
            heroIndex: -1,
            tiles: regularTiles(count).map((tile, index) => ({ ...tile, index, role: "tile" })),
        };
    }

    const geometry = heroTiles(count, mainIndex);
    let sideIndex = 0;
    const tiles = Array.from({ length: count }, (_, index) => {
        if (index === mainIndex)
            return { ...geometry.hero, index, role: "hero" };
        const tile = geometry.sideTiles[sideIndex++];
        return { ...tile, index, role: "tile" };
    });
    return {
        kind: "bento-hero",
        columns: geometry.columns,
        rows: geometry.rows,
        heroIndex: mainIndex,
        tiles,
    };
}

export function layoutForBlocks(blocks) {
    const screenBlocks = blocks.filter(block => block.audience !== false && block.type !== "note");
    if (!screenBlocks.length)
        return { kind: "notes-only", bento: null };

    if (screenBlocks.length === 1) {
        const block = screenBlocks[0];
        if (block.type === "heading")
            return { kind: "title", bento: null };
        if (block.type === "heading-image-tight")
            return { kind: "heading-image-tight", bento: null };
        if (block.type === "image")
            return { kind: "image-single", bento: null };
        if (block.type === "images") {
            const mainIndex = block.images.findIndex(image => image.main);
            return { kind: "images-bento", bento: bentoArrangement(block.images.length, mainIndex) };
        }
        if (block.type === "outline")
            return { kind: "outline", bento: null };
        if (block.type === "list")
            return { kind: "list", bento: null };
        if (["code", "table", "quote", "math"].includes(block.type))
            return { kind: "centered-block", bento: null };
        return { kind: "stack", bento: null };
    }

    if (screenBlocks.length === 2 && screenBlocks[0].type === "heading" && screenBlocks[1].type === "image")
        return { kind: "heading-image-spaced", bento: null };
    if (screenBlocks.every(block => block.type === "image"))
        return { kind: "images-stacked", bento: null };
    return { kind: "stack", bento: null };
}

export function fitDecision(contentHeight, viewportHeight, scrollTop = 0) {
    const content = Math.max(0, Number(contentHeight) || 0);
    const viewport = Math.max(0, Number(viewportHeight) || 0);
    const maxScroll = Math.max(0, content - viewport);
    const top = clamp(Number(scrollTop) || 0, 0, maxScroll);
    return {
        fits: content <= viewport,
        scrollable: maxScroll > 0,
        alignment: maxScroll > 0 ? "top" : "center",
        maxScroll,
        scrollTop: top,
        scrollFraction: maxScroll > 0 ? top / maxScroll : 0,
    };
}

export function scrollTopForFraction(contentHeight, viewportHeight, fraction) {
    const maxScroll = Math.max(0, (Number(contentHeight) || 0) - (Number(viewportHeight) || 0));
    return maxScroll * clamp(Number(fraction) || 0, 0, 1);
}
