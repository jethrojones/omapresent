#include <QtTest>

#include "testrunner.h"
#include "deckmodel.h"

// Suite for src/deckmodel.h. Owned by the agent that owns that file.
// Replace the placeholder below with real coverage of the spec sections named
// in the header. Do not add QTEST_MAIN — see tests/testrunner.h.
class DeckModelTest : public QObject {
    Q_OBJECT

private slots:
    void placeholder() {
        QSKIP("deckmodel tests not written yet");
    }
};

OMAPRESENT_TEST_SUITE(DeckModelTest)
#include "tst_deckmodel.moc"
