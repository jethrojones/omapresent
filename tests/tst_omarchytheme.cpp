#include <QtTest>

#include "testrunner.h"
#include "omarchytheme.h"

// Suite for src/omarchytheme.h. Owned by the agent that owns that file.
// Replace the placeholder below with real coverage of the spec sections named
// in the header. Do not add QTEST_MAIN — see tests/testrunner.h.
class OmarchyThemeTest : public QObject {
    Q_OBJECT

private slots:
    void placeholder() {
        QSKIP("omarchytheme tests not written yet");
    }
};

OMAPRESENT_TEST_SUITE(OmarchyThemeTest)
#include "tst_omarchytheme.moc"
