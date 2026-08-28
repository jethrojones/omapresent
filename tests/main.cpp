#include <QApplication>
#include <QQuickStyle>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

#include "testrunner.h"

QList<OmapresentSuiteFactory> &omapresentTestSuites() {
    static QList<OmapresentSuiteFactory> suites;
    return suites;
}

int main(int argc, char *argv[]) {
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
    for (OmapresentSuiteFactory factory : omapresentTestSuites()) {
        QObject *suite = factory();
        status |= QTest::qExec(suite, argc, argv);
        delete suite;
    }
    return status;
}
