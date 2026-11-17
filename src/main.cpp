#include <QLoggingCategory>
#include <QFileOpenEvent>
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
#include "platform.h"
#include "systemtheme.h"

namespace {

// macOS hands a document to a running application as an event, not as an
// argument: that is what `open -a Omapresent talk.md`, a double-click in Finder
// and a drop on the Dock icon all do. Nothing sends this event on Linux.
class FileOpenFilter : public QObject {
public:
    explicit FileOpenFilter(QObject *window, QObject *parent)
        : QObject(parent), m_window(window) {}

protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (event->type() != QEvent::FileOpen)
            return QObject::eventFilter(watched, event);
        const QUrl url = static_cast<QFileOpenEvent *>(event)->url();
        // The window's own path, so unsaved work gets the same prompt it gets
        // from Ctrl+O.
        if (url.isLocalFile() && m_window)
            QMetaObject::invokeMethod(m_window, "requestOpen", Q_ARG(QVariant, url));
        return true;
    }

private:
    QObject *m_window = nullptr;
};

}  // namespace

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
    // The interface is written against one family name. Where that family is
    // not installed (a Mac before bin/install-macos has run), point the name at
    // the platform's own monospace so every `font.family` in the QML still
    // lands on a fixed-width face.
    const QString interfaceFamily = Platform::interfaceFontFamily();
    if (interfaceFamily != QStringLiteral("iA Writer Mono S"))
        QFont::insertSubstitution(QStringLiteral("iA Writer Mono S"), interfaceFamily);
    const QFont interfaceFont(interfaceFamily);
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

    // The presenter window imports the vendored QR generator to draw the phone
    // remote's code. That file is written for browsers and declares its helpers
    // after the functions that use them, which is legal and which the QML
    // compiler reports once per use: 49 lines on every talk. Nothing in them is
    // actionable, and they bury the remote's address in the terminal.
    QLoggingCategory::setFilterRules(QStringLiteral("qt.qml.usedbeforedeclared=false"));

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
    app.installEventFilter(new FileOpenFilter(engine.rootObjects().constFirst(), &app));

    const auto launchMode = command.command == Backend::CommandLine::Present
        ? CommandLinePolicy::LaunchMode::Present
        : CommandLinePolicy::LaunchMode::Edit;
    if (CommandLinePolicy::chooseStartupSource(
            launchMode, !command.file.isEmpty(), backend.modified())
        == CommandLinePolicy::StartupSource::ExplicitFile) {
        const bool opened = backend.openCommandFile(command.file);
        if (CommandLinePolicy::explicitFileOpenFailed(
                CommandLinePolicy::StartupSource::ExplicitFile, opened)) {
            QTextStream(stderr) << backend.status() << '\n';
            return 1;
        }
    }
    backend.completeFirstRun();

    if (command.command == Backend::CommandLine::Present)
        backend.presentFrom(0);

    return app.exec();
}
