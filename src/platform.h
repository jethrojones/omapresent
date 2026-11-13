#pragma once

// Platform: the one place that knows which desktop this build is for.
//
// Omapresent was written for Omarchy (Arch Linux, Hyprland). It also runs on
// macOS. Everything the two disagree about is answered here, so the rest of the
// tree asks a question instead of carrying an #ifdef. On Linux every function
// below returns exactly what the code it replaced computed inline.

#include <QRect>
#include <QString>
#include <QStringList>

class QScreen;

namespace Platform {

// The directory settings.toml and publish.toml live under, without the
// trailing "omapresent". Linux: QStandardPaths' GenericConfigLocation, which
// honours XDG_CONFIG_HOME. macOS: XDG_CONFIG_HOME, else ~/.config, and not
// ~/Library/Preferences, so the README, the agent skill and a person moving
// between an Omarchy box and a Mac all name the same path.
QString configHome();

// Where an installed copy keeps welcome.md, skill/ and hooks/.
// Linux: /usr/share/omapresent (the Arch package). macOS: the Resources
// directory of Omapresent.app, which bin/install-macos fills.
QString sharedDataDir();

// The font the editor and its chrome are set in. "iA Writer Mono S" whenever
// it is installed, which the Arch package guarantees. A Mac without it gets the
// system's own monospace instead of whatever Qt would substitute.
QString interfaceFontFamily();

// The mono stack for QML `font.family` is a single name, so QML asks for this.
// Kept separate from the function above only so tests can pass a family list.
QString firstInstalledFamily(const QStringList &preferred, const QStringList &installed,
                             const QString &fallback);

// True when `screen` is the computer's own panel. Only macOS answers; on Linux
// this is always false and present mode keeps choosing by "primary" alone, as
// it always has.
bool isBuiltInScreen(const QScreen *screen);

// Where a present-mode window sits on `screen` before it is asked to go
// fullscreen. Linux: the whole output, which a tiling compositor then places.
// macOS: the area the menu bar and Dock leave free, so the title bar is not
// pushed under the menu bar.
QRect windowedGeometry(const QScreen *screen);

// Whether a window has to be given that geometry before it is first shown.
// False under a tiling compositor, which sizes every window itself; true on
// macOS, where a window opens at exactly the size it was given.
bool windowsNeedGeometry();

// Keeps the display awake for the length of a talk using the platform's own
// API. Returns false when this platform has none here, in which case the caller
// falls back to the freedesktop DBus inhibitors. macOS: an IOKit power
// assertion. Linux: always false.
bool holdDisplayAwake(bool hold);

// How to start an interactive shell for the in-deck terminal: true where a new
// terminal conventionally runs a login shell (macOS, whose GUI apps start with
// a bare PATH), false where it does not (Linux).
bool terminalUsesLoginShell();

}  // namespace Platform
