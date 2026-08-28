#include <QtTest>

#include "testrunner.h"
#include "webbundle.h"

// Suite for src/webbundle.h. Owned by the webbundle agent.
class WebBundleTest : public QObject {
    Q_OBJECT

private slots:
    void placeholder() {
        QSKIP("webbundle tests not written yet");
    }
};

OMAPRESENT_TEST_SUITE(WebBundleTest)
#include "tst_webbundle.moc"
