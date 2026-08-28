QT += core gui quick testlib widgets printsupport quickcontrols2 quickdialogs2 \
      dbus webenginequick webchannel multimedia network
CONFIG += testcase c++17
TEMPLATE = app
TARGET = tst_omapresent

INCLUDEPATH += ../src

SOURCES += \
    main.cpp \
    tst_omapresent.cpp \
    tst_deckmodel.cpp \
    tst_assetindex.cpp \
    tst_omarchytheme.cpp \
    tst_videocache.cpp \
    tst_publisher.cpp \
    ../src/backend.cpp \
    ../src/deckmodel.cpp \
    ../src/assetindex.cpp \
    ../src/omarchytheme.cpp \
    ../src/videocache.cpp \
    ../src/publisher.cpp \
    ../src/markdownhighlighter.cpp

HEADERS += \
    testrunner.h \
    ../src/backend.h \
    ../src/deckmodel.h \
    ../src/assetindex.h \
    ../src/omarchytheme.h \
    ../src/videocache.h \
    ../src/publisher.h \
    ../src/markdownhighlighter.h
