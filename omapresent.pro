QT += core gui widgets printsupport qml quick quickcontrols2 quickdialogs2 dbus \
      webenginequick webchannel multimedia network

CONFIG += c++17 release
TARGET = omapresent
TEMPLATE = app

HEADERS += \
    src/backend.h \
    src/deckmodel.h \
    src/assetindex.h \
    src/omarchytheme.h \
    src/videocache.h \
    src/publisher.h \
    src/presentation.h \
    src/markdownhighlighter.h \
    src/systemtheme.h

SOURCES += \
    src/main.cpp \
    src/backend.cpp \
    src/deckmodel.cpp \
    src/assetindex.cpp \
    src/omarchytheme.cpp \
    src/videocache.cpp \
    src/publisher.cpp \
    src/presentation.cpp \
    src/markdownhighlighter.cpp \
    src/systemtheme.cpp

RESOURCES += src/resources.qrc src/renderer/renderer.qrc
