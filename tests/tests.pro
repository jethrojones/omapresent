QT += core gui quick testlib widgets printsupport quickcontrols2 quickdialogs2 \
      dbus webenginequick webchannel multimedia network
CONFIG += testcase c++17
TEMPLATE = app
TARGET = tst_omapresent

INCLUDEPATH += ../src

SOURCES += \
    main.cpp \
    tst_commandline_recovery.cpp \
    tst_omapresent.cpp \
    tst_deckmodel.cpp \
    tst_assetindex.cpp \
    tst_omarchytheme.cpp \
    tst_videocache.cpp \
    tst_publisher.cpp \
    tst_embedserver.cpp \
    tst_presentation.cpp \
    tst_settings.cpp \
    tst_webbundle.cpp \
    tst_integration.cpp \
    tst_livesync.cpp \
    tst_export.cpp \
    tst_security.cpp \
    ../src/backend.cpp \
    ../src/deckmodel.cpp \
    ../src/assetindex.cpp \
    ../src/omarchytheme.cpp \
    ../src/videocache.cpp \
    ../src/publisher.cpp \
    ../src/embedserver.cpp \
    ../src/presentation.cpp \
    ../src/renderhost.cpp \
    ../src/settings.cpp \
    ../src/webbundle.cpp \
    ../src/markdownhighlighter.cpp \
    ../src/commandlinepolicy.cpp

HEADERS += \
    testrunner.h \
    ../src/backend.h \
    ../src/deckmodel.h \
    ../src/assetindex.h \
    ../src/omarchytheme.h \
    ../src/videocache.h \
    ../src/publisher.h \
    ../src/embedserver.h \
    ../src/presentation.h \
    ../src/renderhost.h \
    ../src/settings.h \
    ../src/webbundle.h \
    ../src/markdownhighlighter.h

# The app's resources, so a test can ask whether `qrc:/...` really resolves.
RESOURCES += ../src/resources.qrc ../src/renderer/renderer.qrc
