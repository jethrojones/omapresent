#include <QApplication>
#include <QQuickStyle>
#include <QSettings>
#include <QTemporaryDir>
#include <QTextStream>
#include <QtTest>
#include <QtWebEngineQuick>

#include "testrunner.h"

QList<OmapresentSuiteFactory> &omapresentTestSuites() {
    static QList<OmapresentSuiteFactory> suites;
    return suites;
}

int main(int argc, char *argv[]) {
    // The audience pointer suite instantiates the production WebEngine QML
    // window. Chromium must initialize before QApplication, as in src/main.cpp.
    QtWebEngineQuick::initialize();
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("omapresent-tests"));
    QQuickStyle::setStyle(QStringLiteral("Material"));
    QSettings::setDefaultFormat(QSettings::IniFormat);

    QTemporaryDir settingsDirectory;
    if (settingsDirectory.isValid()) {
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                           settingsDirectory.path());
    }

    int status = 0;
    const QByteArray requestedSuite = qgetenv("OMAPRESENT_TEST_SUITE");
    bool matchedSuite = requestedSuite.isEmpty();
    for (OmapresentSuiteFactory factory : omapresentTestSuites()) {
        QObject *suite = factory();
        if (!requestedSuite.isEmpty()
            && requestedSuite != suite->metaObject()->className()) {
            delete suite;
            continue;
        }
        matchedSuite = true;
        status |= QTest::qExec(suite, argc, argv);
        delete suite;
    }
    if (!matchedSuite) {
        QTextStream(stderr) << "Unknown OMAPRESENT_TEST_SUITE: "
                            << requestedSuite << Qt::endl;
        return 2;
    }
    return status;
}
