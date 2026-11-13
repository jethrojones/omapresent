QT += core gui widgets printsupport qml quick quickcontrols2 quickdialogs2 dbus \
      webenginequick webchannel multimedia network

CONFIG += c++17 release
TARGET = omapresent
TEMPLATE = app

# macOS: build the same plain build/omapresent binary Linux gets. bin/install-macos
# wraps it in Omapresent.app; nothing else in the tree has to know about bundles.
macx {
    CONFIG -= app_bundle
    LIBS += -framework IOKit -framework CoreFoundation -framework CoreGraphics
    # Build for the macOS this machine runs: the Qt it links is Homebrew's,
    # built for the same release, and an older target only earns a linker
    # warning per framework.
    QMAKE_MACOSX_DEPLOYMENT_TARGET = $$system(sw_vers -productVersion | cut -d. -f1).0
}

HEADERS += \
    src/backend.h \
    src/deckmodel.h \
    src/assetindex.h \
    src/omarchytheme.h \
    src/videocache.h \
    src/publisher.h \
    src/embedserver.h \
    src/renderhost.h \
    src/settings.h \
    src/webbundle.h \
    src/presentation.h \
    src/markdownhighlighter.h \
    src/commandlinepolicy.h \
    src/systemtheme.h \
    src/platform.h \
    src/terminalsession.h

SOURCES += \
    src/main.cpp \
    src/backend.cpp \
    src/deckmodel.cpp \
    src/assetindex.cpp \
    src/omarchytheme.cpp \
    src/videocache.cpp \
    src/publisher.cpp \
    src/embedserver.cpp \
    src/renderhost.cpp \
    src/settings.cpp \
    src/webbundle.cpp \
    src/presentation.cpp \
    src/markdownhighlighter.cpp \
    src/commandlinepolicy.cpp \
    src/systemtheme.cpp \
    src/platform.cpp \
    src/terminalsession.cpp

RESOURCES += src/resources.qrc src/renderer/renderer.qrc src/terminal/terminal.qrc
