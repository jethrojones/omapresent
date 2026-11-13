// The page half of a `--- {terminal}` slide: one xterm.js terminal in a fixed
// layer over the deck, fed by the host's PTY (src/terminalsession.cpp).
//
// A classic script, loaded by render.js the first time a terminal slide is
// shown in present mode, after vendor/xterm.js and vendor/addon-fit.js have
// defined the `Terminal` and `FitAddon` globals. It defines one global of its
// own, `omapresentTerminal`, and render.js is its only caller.
//
// Two pages run this at once, the audience window's and the presenter
// window's, both drawing the same shell:
//
//   - The audience page owns the grid. It picks a type size a room can read,
//     fits as many columns and rows as its window holds, and reports that to
//     the host, which sizes the PTY to match.
//   - Every other page mirrors it: same columns and rows, at whatever type
//     size makes that grid fit its own pane.
//
// The layer lives outside #deck, so the slide underneath can be redrawn (a
// live edit, a theme change, a recall) without the terminal losing its screen.
(function () {
    "use strict";

    // iA Writer Mono for the text, so the terminal reads like the rest of the
    // deck. It has no box-drawing or prompt glyphs, which CSS takes one at a
    // time from the faces after it.
    const fontFamily = '"iA Writer Mono S", "JetBrainsMono Nerd Font", '
        + '"CaskaydiaMono Nerd Font", "Symbols Nerd Font", ui-monospace, Menlo, '
        + '"DejaVu Sans Mono", monospace';
    const isMac = /Mac/i.test(navigator.platform || "");

    let term = null;
    let fitAddon = null;
    let layer = null;
    let mount = null;
    let host = null;
    let owner = false;
    let visible = false;
    let focused = false;
    let attached = false;
    let zoom = 0;
    let textScale = 1;
    let grid = null;
    let reportedGrid = "";
    let themeKey = "";
    let fitFrame = 0;

    // Sized from the window, like every other piece of type in a deck: about a
    // hundred columns across the audience screen at the default step.
    function ownerFontSize() {
        const base = Math.max(12, Math.min(44, window.innerWidth * 0.015));
        return Math.round(base * textScale * Math.pow(1.1, zoom) * 2) / 2;
    }

    function themeFrom(palette) {
        const ansi = Array.isArray(palette.ansi) ? palette.ansi : [];
        const names = ["black", "red", "green", "yellow", "blue", "magenta", "cyan", "white",
            "brightBlack", "brightRed", "brightGreen", "brightYellow", "brightBlue",
            "brightMagenta", "brightCyan", "brightWhite"];
        const theme = {
            background: palette.background,
            foreground: palette.foreground,
            cursor: palette.accent,
            cursorAccent: palette.background,
            selectionBackground: palette.selection,
        };
        names.forEach((name, index) => {
            if (ansi[index])
                theme[name] = ansi[index];
        });
        return theme;
    }

    function applyPalette(palette) {
        if (!term || !palette)
            return;
        const key = JSON.stringify(palette);
        if (key === themeKey)
            return;
        themeKey = key;
        term.options.theme = themeFrom(palette);
    }

    function create() {
        layer = document.createElement("div");
        layer.className = "op-terminal-layer";
        layer.hidden = true;
        mount = document.createElement("div");
        mount.className = "op-terminal-mount";
        layer.append(mount);
        document.body.append(layer);

        term = new globalThis.Terminal({
            fontFamily,
            fontSize: ownerFontSize(),
            lineHeight: 1,
            cursorBlink: true,
            scrollback: 5000,
            // Option is Meta, as a terminal on a Mac is usually set up: word
            // motions and the Alt chords of a TUI work without configuration.
            macOptionIsMeta: true,
            macOptionClickForcesSelection: true,
            allowProposedApi: false,
        });
        fitAddon = new globalThis.FitAddon.FitAddon();
        term.loadAddon(fitAddon);
        term.open(mount);

        // What the speaker types. Both pages would answer the shell's own
        // queries too, so the host listens to one page only.
        term.onData(data => host?.terminalInput(data));
        term.onBinary(data => host?.terminalBinary(data));
        term.textarea?.addEventListener("focus", () => host?.terminalFocusIn());

        term.attachCustomKeyEventHandler(event => {
            const down = event.type === "keydown";

            // The way out. Plain Esc belongs to whatever is running in the
            // shell; Shift+Esc is sent by no terminal program, so it is ours.
            if (event.key === "Escape" && event.shiftKey
                && !event.ctrlKey && !event.altKey && !event.metaKey) {
                if (down)
                    host?.terminalLeave();
                return false;
            }

            // Type size, on the chord each platform's terminals already use:
            // Cmd on a Mac, Ctrl elsewhere. Ctrl+Shift+- is left alone because
            // that is Ctrl+_, which shells and editors bind to undo.
            const chord = isMac
                ? event.metaKey && !event.ctrlKey && !event.altKey
                : event.ctrlKey && !event.metaKey && !event.altKey;
            if (chord && (event.key === "=" || event.key === "+" || event.key === "0"
                          || (event.key === "-" && (isMac || !event.shiftKey)))) {
                if (down)
                    host?.terminalZoom(event.key === "0" ? 0 : event.key === "-" ? -1 : 1);
                return false;
            }

            // Shift+Enter is a new line in a prompt, not a submit. A terminal
            // cannot tell the two apart unless it sends something different,
            // and Esc-Return is what line editors and coding agents listen for.
            if (event.key === "Enter" && event.shiftKey
                && !event.ctrlKey && !event.altKey && !event.metaKey) {
                if (down)
                    host?.terminalInput("\x1b\r");
                return false;
            }
            return true;
        });

        // A click on a terminal that does not have the keyboard asks for it.
        layer.addEventListener("mousedown", () => {
            if (!focused)
                host?.terminalFocus();
        });
        window.addEventListener("resize", scheduleFit);
    }

    function fitOwner() {
        const size = ownerFontSize();
        if (term.options.fontSize !== size)
            term.options.fontSize = size;
        // The new cell size is measured by the time the next frame runs.
        requestAnimationFrame(() => {
            if (!visible)
                return;
            try {
                fitAddon.fit();
            } catch {
                return;
            }
            if (!attached) {
                attached = true;
                host?.terminalAttach();
            }
            const key = `${term.cols}x${term.rows}`;
            if (key !== reportedGrid) {
                reportedGrid = key;
                host?.terminalResize(term.cols, term.rows);
            }
        });
    }

    // Same grid as the audience, at the largest type size that shows all of it.
    function fitMirror(passes) {
        if (!visible)
            return;
        if (!attached) {
            attached = true;
            host?.terminalAttach();
        }
        if (!grid)
            return;
        if (term.cols !== grid.cols || term.rows !== grid.rows)
            term.resize(grid.cols, grid.rows);

        const screen = mount.querySelector(".xterm-screen");
        const room = mount.getBoundingClientRect();
        const drawn = screen?.getBoundingClientRect();
        if (!drawn || !(drawn.width > 0) || !(drawn.height > 0) || !(room.width > 0))
            return;
        const scale = Math.min(room.width / drawn.width, room.height / drawn.height);
        const next = Math.max(4, Math.floor(term.options.fontSize * scale * 4) / 4);
        if (next === term.options.fontSize)
            return;
        term.options.fontSize = next;
        if (passes > 1)
            requestAnimationFrame(() => fitMirror(passes - 1));
    }

    function scheduleFit() {
        cancelAnimationFrame(fitFrame);
        fitFrame = requestAnimationFrame(() => {
            if (!term || !visible)
                return;
            if (owner)
                fitOwner();
            else
                fitMirror(3);
        });
    }

    function applyFocus() {
        if (!term)
            return;
        const holds = focused && visible;
        layer.classList.toggle("has-keyboard", holds);
        // Only one window can hold the system's keyboard focus, and it is
        // usually the presenter's. The audience must still see a solid cursor
        // while the shell is being typed into, so the cursor shows who has the
        // keyboard in the talk, not which window the desktop has focused.
        term.options.cursorInactiveStyle = holds ? "block" : "outline";
        if (holds)
            term.focus();
        else
            term.blur();
    }

    globalThis.omapresentTerminal = {
        // Puts the terminal on screen, building it on the first call.
        show(options) {
            host = options.host ?? host;
            owner = Boolean(options.owner);
            textScale = Number(options.textScale) > 0 ? Number(options.textScale) : 1;
            zoom = Number(options.zoom) || 0;
            grid = options.grid ?? grid;
            focused = Boolean(options.focused);
            if (!term)
                create();
            layer.classList.toggle("is-mirror", !owner);
            applyPalette(options.palette);
            visible = true;
            layer.hidden = false;
            scheduleFit();
            applyFocus();
        },
        // Off screen, still running: output keeps arriving and is kept.
        hide() {
            visible = false;
            if (layer)
                layer.hidden = true;
            applyFocus();
        },
        // Base64 of the PTY's bytes. Bytes, not text: xterm.js does its own
        // UTF-8 decoding and copes with a character split across two writes.
        write(encoded) {
            if (!term)
                return;
            const binary = atob(encoded);
            const bytes = new Uint8Array(binary.length);
            for (let index = 0; index < binary.length; index += 1)
                bytes[index] = binary.charCodeAt(index);
            term.write(bytes);
        },
        setGrid(cols, rows) {
            grid = { cols: Math.max(2, cols | 0), rows: Math.max(1, rows | 0) };
            if (term && !owner)
                scheduleFit();
        },
        setFocus(on) {
            focused = Boolean(on);
            applyFocus();
        },
        setZoom(step) {
            zoom = Number(step) || 0;
            if (term && owner)
                scheduleFit();
        },
    };
})();
