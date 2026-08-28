#include <QtTest>

#include "testrunner.h"
#include "settings.h"

// Suite for src/settings.h. Owned by the settings agent.
class SettingsTest : public QObject {
    Q_OBJECT

private slots:
    void placeholder() {
        QSKIP("settings tests not written yet");
    }
};

OMAPRESENT_TEST_SUITE(SettingsTest)
#include "tst_settings.moc"
