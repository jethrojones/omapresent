#include <QtTest>

#include "testrunner.h"
#include "publisher.h"

// Suite for src/publisher.h. Owned by the agent that owns that file.
// Replace the placeholder below with real coverage of the spec sections named
// in the header. Do not add QTEST_MAIN — see tests/testrunner.h.
class PublisherTest : public QObject {
    Q_OBJECT

private slots:
    void placeholder() {
        QSKIP("publisher tests not written yet");
    }
};

OMAPRESENT_TEST_SUITE(PublisherTest)
#include "tst_publisher.moc"
