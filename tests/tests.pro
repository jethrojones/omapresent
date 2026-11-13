QT += core gui quick testlib widgets printsupport quickcontrols2 quickdialogs2 \
      dbus webenginequick webchannel multimedia network
CONFIG += testcase c++17
TEMPLATE = app
TARGET = tst_omapresent

INCLUDEPATH += ../src

# macOS: a plain build-tests/tst_omapresent, as on Linux. See omapresent.pro.
macx {
    CONFIG -= app_bundle
    LIBS += -framework IOKit -framework CoreFoundation -framework CoreGraphics
    QMAKE_MACOSX_DEPLOYMENT_TARGET = $$system(sw_vers -productVersion | cut -d. -f1).0
}

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
    tst_audience_video_pointer.cpp \
    tst_settings.cpp \
    tst_webbundle.cpp \
    tst_integration.cpp \
    tst_livesync.cpp \
    tst_export.cpp \
    tst_security.cpp \
    tst_terminal.cpp \
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
    ../src/commandlinepolicy.cpp \
    ../src/platform.cpp \
    ../src/terminalsession.cpp

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
    ../src/markdownhighlighter.h \
    ../src/platform.h \
    ../src/terminalsession.h

# The app's resources, so a test can ask whether `qrc:/...` really resolves.
RESOURCES += ../src/resources.qrc ../src/renderer/renderer.qrc ../src/terminal/terminal.qrc
