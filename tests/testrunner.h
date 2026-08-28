#pragma once

// One test binary, many suites. Each tests/tst_*.cpp defines its own QObject
// suite and registers it at the bottom of the file with:
//
//     OMAPRESENT_TEST_SUITE(MySuite)
//     #include "tst_myfile.moc"
//
// Do not use QTEST_MAIN in a suite file — tests/main.cpp owns main().

#include <QList>
#include <QObject>

using OmapresentSuiteFactory = QObject *(*)();

QList<OmapresentSuiteFactory> &omapresentTestSuites();

struct OmapresentSuiteRegistrar {
    explicit OmapresentSuiteRegistrar(OmapresentSuiteFactory factory) {
        omapresentTestSuites().append(factory);
    }
};

#define OMAPRESENT_TEST_SUITE(Class)                                           \
    static QObject *omapresentMake##Class() { return new Class; }              \
    static const OmapresentSuiteRegistrar omapresentRegistrar##Class(          \
        &omapresentMake##Class);
