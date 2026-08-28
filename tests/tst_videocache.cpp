#include <QtTest>

#include "testrunner.h"
#include "videocache.h"

// Suite for src/videocache.h. Owned by the agent that owns that file.
// Replace the placeholder below with real coverage of the spec sections named
// in the header. Do not add QTEST_MAIN — see tests/testrunner.h.
class VideoCacheTest : public QObject {
    Q_OBJECT

private slots:
    void placeholder() {
        QSKIP("videocache tests not written yet");
    }
};

OMAPRESENT_TEST_SUITE(VideoCacheTest)
#include "tst_videocache.moc"
