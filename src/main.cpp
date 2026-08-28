#include <QFont>
#include <QFontDatabase>
#include <QApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlError>
#include <QQuickStyle>
#include <QTextStream>
#include <QTimer>
#include <QUrl>
#include <QWindow>
#include <QFile>
#include <QtWebEngineQuick>

#include "backend.h"
#include "commandlinepolicy.h"
#include "systemtheme.h"

int main(int argc, char *argv[]) {
    // The renderer is a web page, and Chromium has to be told before the
    // application object exists.
    QtWebEngineQuick::initialize();

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("omapresent"));
    app.setDesktopFileName(QStringLiteral("omapresent"));
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("omapresent")));

    app.setOrganizationName(QStringLiteral("Omacom"));
    app.setOrganizationDomain(QStringLiteral("omacom.io"));

    QQuickStyle::setStyle(QStringLiteral("Material"));

    const Backend::CommandLine command = Backend::parseCommandLine(app.arguments().mid(1));
    if (!command.error.isEmpty()) {
        QTextStream(stderr) << command.error << '\n' << Backend::usage();
        return 2;
    }

    Backend backend(&app);
    backend.setWebEngineReady(true);

    // `export` and `publish` never open a window: they render, write and exit.
    if (!command.needsWindow()) {
        int exitCode = 1;
        QObject::connect(&backend, &Backend::commandFinished, &app, [&](int code) {
            exitCode = code;
            app.quit();
        });
        QTimer::singleShot(0, &backend, [&backend, command]() {
            backend.runCommand(command);
        });
        app.exec();
        return exitCode;
    }

    SystemTheme systemTheme(&app);
    backend.setDarkMode(systemTheme.darkMode());
    QObject::connect(&systemTheme, &SystemTheme::darkModeChanged, &backend,
                     &Backend::setDarkMode);

    // Carry the desktop's text scale into the default font, so the chrome that
    // inherits it (dialog titles, buttons) grows along with the writing area.
    const QFont interfaceFont(QStringLiteral("iA Writer Mono S"));
    const qreal basePointSize = interfaceFont.pointSizeF() > 0
        ? interfaceFont.pointSizeF()
        : app.font().pointSizeF();
    const auto applyInterfaceFont = [&app, interfaceFont, basePointSize](qreal textScale) {
        QFont scaled = interfaceFont;
        scaled.setPointSizeF(basePointSize * textScale);
        app.setFont(scaled);
    };
    applyInterfaceFont(systemTheme.textScale());

    backend.setTextScale(systemTheme.textScale());
    QObject::connect(&systemTheme, &SystemTheme::textScaleChanged, &backend,
                     [&backend, applyInterfaceFont](qreal textScale) {
        applyInterfaceFont(textScale);
        backend.setTextScale(textScale);
    });

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::warnings, &app,
                     [](const QList<QQmlError> &warnings) {
        for (const QQmlError &warning : warnings)
            qWarning().noquote() << warning.toString();
    });
    engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);

    engine.load(QUrl(QStringLiteral("qrc:/Main.qml")));
    if (engine.rootObjects().isEmpty()) {
        qCritical() << "Could not load the Omapresent interface; resource available:"
                    << QFile::exists(QStringLiteral(":/Main.qml"));
        return -1;
    }

    backend.setParentWindow(qobject_cast<QWindow *>(engine.rootObjects().constFirst()));

    const auto launchMode = command.command == Backend::CommandLine::Present
        ? CommandLinePolicy::LaunchMode::Present
        : CommandLinePolicy::LaunchMode::Edit;
    if (CommandLinePolicy::chooseStartupSource(
            launchMode, !command.file.isEmpty(), backend.modified())
        == CommandLinePolicy::StartupSource::ExplicitFile) {
        if (command.command == Backend::CommandLine::Present) {
            if (!backend.openCommandFile(command.file)) {
                QTextStream(stderr) << backend.status() << '\n';
                return 1;
            }
        } else {
            backend.openCommandFile(command.file);
        }
    }
    backend.completeFirstRun();

    if (command.command == Backend::CommandLine::Present)
        backend.presentFrom(0);

    return app.exec();
}
