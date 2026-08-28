.pragma library

function normalizePlainText(text) {
    return text.replace(/\r\n/g, "\n").replace(/\r/g, "\n");
}

function replaceRange(editor, rangeStart, rangeEnd, replacement,
                      selectionStartOffset, selectionEndOffset) {
    var start = Math.max(0, Math.min(editor.text.length, rangeStart));
    var end = Math.max(start, Math.min(editor.text.length, rangeEnd));
    var insertedText = normalizePlainText(replacement);

    if (start !== end)
        editor.remove(start, end);

    editor.cursorPosition = start;
    editor.insert(start, insertedText);

    // TextEdit.insert() already leaves the caret after the inserted text. Only
    // move it again when the caller deliberately requests a selection/caret
    // within the replacement.
    if (selectionStartOffset !== undefined && selectionEndOffset !== undefined) {
        var insertedEnd = editor.cursorPosition;
        var selectionStart = Math.max(start,
                                      Math.min(insertedEnd, start + selectionStartOffset));
        var selectionEnd = Math.max(start,
                                    Math.min(insertedEnd, start + selectionEndOffset));
        if (selectionStart === selectionEnd)
            editor.cursorPosition = selectionStart;
        else
            editor.select(selectionStart, selectionEnd);
    }

    return insertedText;
}

// Spec §4.10: pressing Return a third consecutive time at the end of the
// document turns the blank lines it made into a slide break. Returns the edit
// to apply — {start, end, insert} — or null when this is an ordinary Return.
//
// It reads the text rather than counting keystrokes, so it behaves the same
// however the blank lines got there, and it deliberately does nothing when:
//   - there is anything but whitespace after the caret (mid-document Returns),
//   - nothing has been written yet (a leading `---` would be frontmatter),
//   - the last thing written was itself a separator.
function slideBreakForReturn(text, cursorPosition) {
    if (cursorPosition < 0 || cursorPosition > text.length)
        return null;
    if (text.slice(cursorPosition).trim() !== "")
        return null;

    var before = text.slice(0, cursorPosition);
    var contentEnd = before.length;
    while (contentEnd > 0 && /\s/.test(before.charAt(contentEnd - 1)))
        contentEnd--;
    if (contentEnd === 0)
        return null;

    // Two blank lines behind the caret means three newlines behind it.
    if (before.slice(contentEnd).split("\n").length - 1 < 3)
        return null;

    var lastLineStart = before.lastIndexOf("\n", contentEnd - 1) + 1;
    if (before.slice(lastLineStart, contentEnd).trim() === "---")
        return null;

    return { start: contentEnd, end: text.length, insert: "\n\n---\n\n" };
}
