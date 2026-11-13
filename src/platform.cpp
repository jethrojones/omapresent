#include "platform.h"

#include <QCoreApplication>
#include <QDir>
#include <QFontDatabase>
#include <QScreen>
#include <QStandardPaths>

#ifdef Q_OS_MACOS
#include <CoreGraphics/CoreGraphics.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#endif

namespace Platform {

QString configHome() {
#ifdef Q_OS_MACOS
    const QString xdg = qEnvironmentVariable("XDG_CONFIG_HOME");
    return xdg.isEmpty() ? QDir::homePath() + QStringLiteral("/.config") : xdg;
#else
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
#endif
}

QString sharedDataDir() {
#ifdef Q_OS_MACOS
    // <bundle>/Contents/MacOS/omapresent -> <bundle>/Contents/Resources
    return QDir::cleanPath(QCoreApplication::applicationDirPath()
                           + QStringLiteral("/../Resources"));
#else
    return QStringLiteral("/usr/share/omapresent");
#endif
}

QString firstInstalledFamily(const QStringList &preferred, const QStringList &installed,
                             const QString &fallback) {
    for (const QString &family : preferred) {
        if (installed.contains(family, Qt::CaseInsensitive))
            return family;
    }
    return fallback;
}

QString interfaceFontFamily() {
    const QString preferred = QStringLiteral("iA Writer Mono S");
#ifdef Q_OS_MACOS
    return firstInstalledFamily(
        {preferred, QStringLiteral("SF Mono"), QStringLiteral("Menlo")},
        QFontDatabase::families(),
        QFontDatabase::systemFont(QFontDatabase::FixedFont).family());
#else
    // The Arch package depends on ttf-ia-writer; fontconfig substitutes for a
    // source build that lacks it, exactly as before.
    return preferred;
#endif
}

bool isBuiltInScreen(const QScreen *screen) {
#ifdef Q_OS_MACOS
    if (!screen)
        return false;
    // Qt does not say which screen is the laptop's panel, but CoreGraphics
    // does, and both describe a display by the same top-left point in the same
    // global coordinate space.
    CGDirectDisplayID displays[16];
    uint32_t count = 0;
    if (CGGetActiveDisplayList(16, displays, &count) != kCGErrorSuccess)
        return false;
    const QPoint origin = screen->geometry().topLeft();
    for (uint32_t i = 0; i < count; ++i) {
        if (!CGDisplayIsBuiltin(displays[i]))
            continue;
        const CGRect bounds = CGDisplayBounds(displays[i]);
        if (qRound(bounds.origin.x) == origin.x() && qRound(bounds.origin.y) == origin.y())
            return true;
    }
    return false;
#else
    Q_UNUSED(screen);
    return false;
#endif
}

QRect windowedGeometry(const QScreen *screen) {
    if (!screen)
        return {};
#ifdef Q_OS_MACOS
    return screen->availableGeometry();
#else
    return screen->geometry();
#endif
}

bool windowsNeedGeometry() {
#ifdef Q_OS_MACOS
    return true;
#else
    return false;
#endif
}

bool holdDisplayAwake(bool hold) {
#ifdef Q_OS_MACOS
    static IOPMAssertionID assertion = kIOPMNullAssertionID;
    if (hold) {
        if (assertion != kIOPMNullAssertionID)
            return true;
        const IOReturn result = IOPMAssertionCreateWithName(
            kIOPMAssertionTypePreventUserIdleDisplaySleep, kIOPMAssertionLevelOn,
            CFSTR("Omapresent is presenting"), &assertion);
        if (result != kIOReturnSuccess)
            assertion = kIOPMNullAssertionID;
        return true;
    }
    if (assertion != kIOPMNullAssertionID) {
        IOPMAssertionRelease(assertion);
        assertion = kIOPMNullAssertionID;
    }
    return true;
#else
    Q_UNUSED(hold);
    return false;
#endif
}

bool terminalUsesLoginShell() {
#ifdef Q_OS_MACOS
    return true;
#else
    return false;
#endif
}

}  // namespace Platform
