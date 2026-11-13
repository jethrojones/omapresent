# Vendored third-party assets: the live terminal

The terminal emulator behind `--- {terminal}` slides. Committed here so it
works offline, like everything under `src/renderer/vendor/` (spec §3: no
network at runtime). Each file is the upstream distribution build, unmodified.

These live outside `src/renderer/` on purpose. A published web bundle carries
every file in that directory, and a published deck has no shell to attach a
terminal to. Keeping the emulator here means it is compiled into the app
(`src/terminal/terminal.qrc`, served as `qrc:/terminal/`) and loaded only when
a terminal slide is first shown in present mode. It is never in a bundle, a PDF
or the editor preview.

| Library | Version | File(s) | Licence |
| --- | --- | --- | --- |
| [xterm.js](https://github.com/xtermjs/xterm.js) | 6.0.0 | `xterm.js`, `xterm.css` | MIT (`LICENSE.xterm`) |
| [@xterm/addon-fit](https://github.com/xtermjs/xterm.js/tree/master/addons/addon-fit) | 0.11.0 | `addon-fit.js` | MIT (`LICENSE.addon-fit`) |

## Notes

- Both are the UMD builds (`lib/*.js`), loaded as classic scripts that define
  the `Terminal` and `FitAddon` globals. The ESM builds would have to be
  imported from `render.js`, which would pull them into the published bundle.
- xterm.js draws with its DOM renderer. No WebGL or canvas addon is vendored:
  the DOM renderer needs nothing from the GPU and behaves the same in
  QtWebEngine on Linux and macOS.
- The `sourceMappingURL` comments are upstream's. The maps are not vendored and
  nothing requests them outside developer tools.

## Refreshing

```sh
npm pack @xterm/xterm @xterm/addon-fit
tar -xzf xterm-xterm-*.tgz && cp package/lib/xterm.js package/css/xterm.css vendor/
cp package/LICENSE vendor/LICENSE.xterm
tar -xzf xterm-addon-fit-*.tgz && cp package/lib/addon-fit.js vendor/
cp package/LICENSE vendor/LICENSE.addon-fit
```

Then run `./bin/test`, present a deck with a `--- {terminal}` slide on both
platforms, and update the versions above.
