#include <QtTest>

#include "testrunner.h"
#include "assetindex.h"

// Suite for src/assetindex.h. Owned by the agent that owns that file.
// Replace the placeholder below with real coverage of the spec sections named
// in the header. Do not add QTEST_MAIN — see tests/testrunner.h.
class AssetIndexTest : public QObject {
    Q_OBJECT

private slots:
    void placeholder() {
        QSKIP("assetindex tests not written yet");
    }
};

OMAPRESENT_TEST_SUITE(AssetIndexTest)
#include "tst_assetindex.moc"
