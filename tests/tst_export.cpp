#include <QtTest>

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPrintDialog>
#include <QProcess>
#include <QProcessEnvironment>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>

#include "backend.h"
#include "testrunner.h"

namespace {

struct ToolResult {
    QString program;
    bool started = false;
    bool finished = false;
    int exitCode = -1;
    QProcess::ExitStatus exitStatus = QProcess::NormalExit;
    QString processError;
    QByteArray standardOutput;
    QByteArray standardError;
};

ToolResult runProcess(const QString &program, const QStringList &arguments,
                      const QString &workingDirectory = QString(),
                      const QProcessEnvironment &environment = QProcessEnvironment())
{
    QProcess process;
    if (!workingDirectory.isEmpty())
        process.setWorkingDirectory(workingDirectory);
    if (!environment.isEmpty())
        process.setProcessEnvironment(environment);
    process.start(program, arguments);

    ToolResult result;
    result.program = program;
    result.started = process.waitForStarted(5000);
    if (!result.started) {
        result.processError = process.errorString();
        return result;
    }
    result.finished = process.waitForFinished(60000);
    if (!result.finished) {
        process.kill();
        process.waitForFinished();
    }
    result.exitCode = process.exitCode();
    result.exitStatus = process.exitStatus();
    result.processError = process.errorString();
    result.standardOutput = process.readAllStandardOutput();
    result.standardError = process.readAllStandardError();
    return result;
}

QString processFailure(const ToolResult &result)
{
    if (!result.started) {
        return QStringLiteral("Could not start %1: %2")
            .arg(result.program, result.processError);
    }
    if (!result.finished) {
        return QStringLiteral("%1 did not finish within 60 seconds: %2")
            .arg(result.program, result.processError);
    }
    return QStringLiteral("%1 exited with code %2 (%3).\nstdout:\n%4\nstderr:\n%5")
        .arg(result.program)
        .arg(result.exitCode)
        .arg(result.exitStatus == QProcess::NormalExit ? QStringLiteral("normal exit")
                                                       : QStringLiteral("crashed"))
        .arg(QString::fromUtf8(result.standardOutput),
             QString::fromUtf8(result.standardError));
}

bool writeUtf8(const QString &path, const QString &text, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        *error = file.errorString();
        return false;
    }
    if (file.write(text.toUtf8()) != text.toUtf8().size()) {
        *error = file.errorString();
        return false;
    }
    return true;
}

int pdfPageCount(const QString &pdfPath, QString *error)
{
    const ToolResult info = runProcess(QStringLiteral("pdfinfo"), {pdfPath});
    if (!info.finished || info.exitCode != 0) {
        *error = processFailure(info);
        return -1;
    }
    const QRegularExpression pages(QStringLiteral(R"(^Pages:\s+(\d+)\s*$)"),
                                   QRegularExpression::MultilineOption);
    const QRegularExpressionMatch match = pages.match(QString::fromUtf8(info.standardOutput));
    if (!match.hasMatch()) {
        *error = QStringLiteral("pdfinfo did not report Pages:\n%1")
                     .arg(QString::fromUtf8(info.standardOutput));
        return -1;
    }
    return match.captured(1).toInt();
}

QSizeF pdfPageSize(const QString &pdfPath, QString *error)
{
    const ToolResult info = runProcess(QStringLiteral("pdfinfo"),
                                      {QStringLiteral("-f"), QStringLiteral("1"),
                                       QStringLiteral("-l"), QStringLiteral("1"), pdfPath});
    if (!info.finished || info.exitCode != 0) {
        *error = processFailure(info);
        return {};
    }
    const QRegularExpression size(
        QStringLiteral(R"(^Page(?:\s+\d+)? size:\s+([0-9.]+) x ([0-9.]+) pts\s*$)"),
        QRegularExpression::MultilineOption);
    const QRegularExpressionMatch match = size.match(QString::fromUtf8(info.standardOutput));
    if (!match.hasMatch()) {
        *error = QStringLiteral(
            "pdfinfo ran successfully, but its output had no Page size line.\n"
            "stdout:\n%1\nstderr:\n%2")
                     .arg(QString::fromUtf8(info.standardOutput),
                          QString::fromUtf8(info.standardError));
        return {};
    }
    return QSizeF(match.captured(1).toDouble(), match.captured(2).toDouble());
}

QByteArray pdfText(const QString &pdfPath, QString *error)
{
    const ToolResult text = runProcess(QStringLiteral("pdftotext"),
                                      {QStringLiteral("-layout"), pdfPath,
                                       QStringLiteral("-")});
    if (!text.finished || text.exitCode != 0) {
        *error = processFailure(text);
        return {};
    }
    return text.standardOutput;
}

QSizeF wordBox(const QString &pdfPath, const QString &word, QString *error)
{
    const ToolResult bbox = runProcess(QStringLiteral("pdftotext"),
                                      {QStringLiteral("-bbox"), pdfPath,
                                       QStringLiteral("-")});
    if (!bbox.finished || bbox.exitCode != 0) {
        *error = processFailure(bbox);
        return {};
    }

    const QString escaped = QRegularExpression::escape(word);
    const QRegularExpression box(
        QStringLiteral("<word xMin=\"([0-9.]+)\" yMin=\"([0-9.]+)\" "
                       "xMax=\"([0-9.]+)\" yMax=\"([0-9.]+)\">%1</word>")
            .arg(escaped));
    const QRegularExpressionMatch match = box.match(QString::fromUtf8(bbox.standardOutput));
    if (!match.hasMatch()) {
        *error = QStringLiteral("pdftotext -bbox did not find %1:\n%2")
                     .arg(word, QString::fromUtf8(bbox.standardOutput));
        return {};
    }
    return QSizeF(match.captured(3).toDouble() - match.captured(1).toDouble(),
                  match.captured(4).toDouble() - match.captured(2).toDouble());
}

} // namespace

class ExportTest : public QObject {
    Q_OBJECT

private:
    QTemporaryDir *m_sandbox = nullptr;

    QString applicationPath() const
    {
        return QDir(QCoreApplication::applicationDirPath())
            .absoluteFilePath(QStringLiteral("../build/omapresent"));
    }

    bool exportDeck(const QString &stem, const QString &markdown,
                    QString *pdfPath, QString *error) const
    {
        const QString source = m_sandbox->filePath(stem + QStringLiteral(".md"));
        if (!writeUtf8(source, markdown, error))
            return false;

        QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
        environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
        const ToolResult result = runProcess(
            applicationPath(),
            {QStringLiteral("export"), QStringLiteral("--pdf"), source},
            m_sandbox->path(), environment);
        if (!result.finished || result.exitCode != 0) {
            *error = processFailure(result);
            return false;
        }

        *pdfPath = m_sandbox->filePath(stem + QStringLiteral(".pdf"));
        if (!QFileInfo::exists(*pdfPath)) {
            *error = QStringLiteral("The CLI reported success but did not create %1.\n%2")
                         .arg(*pdfPath, QString::fromUtf8(result.standardOutput));
            return false;
        }
        return true;
    }

private slots:
    void initTestCase()
    {
        QVERIFY2(QFileInfo(applicationPath()).isExecutable(),
                 qPrintable(QStringLiteral("Build the CLI first: %1")
                                .arg(applicationPath())));
        for (const QString &tool : {QStringLiteral("pdfinfo"),
                                    QStringLiteral("pdftotext"),
                                    QStringLiteral("pdftoppm")}) {
            QVERIFY2(!QStandardPaths::findExecutable(tool).isEmpty(),
                     qPrintable(QStringLiteral("Missing required Poppler tool: %1").arg(tool)));
        }
    }

    void init()
    {
        m_sandbox = new QTemporaryDir;
        QVERIFY(m_sandbox->isValid());
    }

    void cleanup()
    {
        delete m_sandbox;
        m_sandbox = nullptr;
    }

    void canvasFollowsFrontmatterAspect_data()
    {
        QTest::addColumn<QString>("aspectLine");
        QTest::addColumn<double>("expectedWidth");
        QTest::addColumn<double>("expectedHeight");

        QTest::newRow("default 16:9") << QString() << 960.0 << 540.0;
        QTest::newRow("4:3") << QStringLiteral("aspect: \"4:3\"\n") << 960.0 << 720.0;
        QTest::newRow("16:10") << QStringLiteral("aspect: \"16:10\"\n") << 960.0 << 600.0;
    }

    void canvasFollowsFrontmatterAspect()
    {
        QFETCH(QString, aspectLine);
        QFETCH(double, expectedWidth);
        QFETCH(double, expectedHeight);

        const QString markdown = QStringLiteral(
            "---\n"
            "title: Geometry proof\n"
            "%1"
            "---\n"
            "# GEOMETRY_PROOF\n").arg(aspectLine);
        QString pdf;
        QString error;
        QVERIFY2(exportDeck(QStringLiteral("geometry"), markdown, &pdf, &error),
                 qPrintable(error));
        QCOMPARE(pdfPageCount(pdf, &error), 1);
        QVERIFY2(error.isEmpty(), qPrintable(error));

        const QSizeF size = pdfPageSize(pdf, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(qAbs(size.width() - expectedWidth) <= 0.1);
        QVERIFY(qAbs(size.height() - expectedHeight) <= 0.1);
        QVERIFY(size.width() > size.height());
    }

    void tallSlidePaginatesWithoutShrinking()
    {
        const QString frontmatter = QStringLiteral(
            "---\n"
            "title: Pagination proof\n"
            "aspect: \"16:9\"\n"
            "---\n");
        const QString shortDeck = frontmatter + QStringLiteral(
            "# Short slide\n\n"
            "- SIZEPROBE\n"
            "- SHORT_END\n");

        QStringList tallItems{QStringLiteral("- SIZEPROBE")};
        for (int index = 1; index <= 80; ++index) {
            tallItems += QStringLiteral("- Tall item %1 keeps the slide at its natural type size")
                             .arg(index);
        }
        tallItems += QStringLiteral("- LAST_TALL_ITEM");
        const QString tallDeck = frontmatter + QStringLiteral("# Tall slide\n\n")
            + tallItems.join(QLatin1Char('\n')) + QLatin1Char('\n');

        QString shortPdf;
        QString tallPdf;
        QString error;
        QVERIFY2(exportDeck(QStringLiteral("short"), shortDeck, &shortPdf, &error),
                 qPrintable(error));
        QVERIFY2(exportDeck(QStringLiteral("tall"), tallDeck, &tallPdf, &error),
                 qPrintable(error));
        QCOMPARE(pdfPageCount(shortPdf, &error), 1);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY2(pdfPageCount(tallPdf, &error) > 1, qPrintable(error));

        const QByteArray tallText = pdfText(tallPdf, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(tallText.contains("LAST_TALL_ITEM"));

        const QSizeF shortBox = wordBox(shortPdf, QStringLiteral("SIZEPROBE"), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        const QSizeF tallBox = wordBox(tallPdf, QStringLiteral("SIZEPROBE"), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY2(qAbs(shortBox.width() - tallBox.width()) <= 0.2,
                 qPrintable(QStringLiteral("SIZEPROBE width changed from %1 pt to %2 pt")
                                .arg(shortBox.width()).arg(tallBox.width())));
        QVERIFY2(qAbs(shortBox.height() - tallBox.height()) <= 0.2,
                 qPrintable(QStringLiteral("SIZEPROBE height changed from %1 pt to %2 pt")
                                .arg(shortBox.height()).arg(tallBox.height())));
    }

    void fragmentsAreFullyExpanded()
    {
        const QString markdown = QStringLiteral(
            "---\n"
            "title: Fragment proof\n"
            "---\n"
            "# Fragment proof\n\n"
            "- FRAGMENT_ONE\n"
            "- FRAGMENT_TWO\n"
            "- FRAGMENT_THREE\n");
        QString pdf;
        QString error;
        QVERIFY2(exportDeck(QStringLiteral("fragments"), markdown, &pdf, &error),
                 qPrintable(error));
        const QByteArray text = pdfText(pdf, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(text.contains("FRAGMENT_ONE"));
        QVERIFY(text.contains("FRAGMENT_TWO"));
        QVERIFY(text.contains("FRAGMENT_THREE"));
    }

    void recallSlidesExportInDocumentOrderIncludingSkipped()
    {
        const QString markdown = QStringLiteral(
            "---\n"
            "title: Recall proof\n"
            "---\n"
            "# BEFOREPDF\n\n"
            "--- {q, skip}\n\n"
            "# SKIPPEDQ\n\n"
            "--- {w}\n\n"
            "# NORMALW\n\n"
            "---\n\n"
            "# AFTERPDF\n");
        QString pdf;
        QString error;
        QVERIFY2(exportDeck(QStringLiteral("recall"), markdown, &pdf, &error),
                 qPrintable(error));
        QCOMPARE(pdfPageCount(pdf, &error), 4);
        QVERIFY2(error.isEmpty(), qPrintable(error));

        const QByteArray text = pdfText(pdf, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        const qsizetype before = text.indexOf("BEFOREPDF");
        const qsizetype skipped = text.indexOf("SKIPPEDQ");
        const qsizetype normal = text.indexOf("NORMALW");
        const qsizetype after = text.indexOf("AFTERPDF");
        QVERIFY(before >= 0);
        QVERIFY(skipped > before);
        QVERIFY(normal > skipped);
        QVERIFY(after > normal);
    }

    void controlPUsesTheSystemPrintDialog()
    {
        const QString mainQmlPath = QFINDTESTDATA("../src/Main.qml");
        QVERIFY(!mainQmlPath.isEmpty());

        Backend backend;
        QQmlEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
        QQmlComponent component(&engine, QUrl::fromLocalFile(mainQmlPath));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *window = qobject_cast<QQuickWindow *>(object.data());
        QVERIFY(window);
        window->show();
        window->requestActivate();
        QTest::qWait(20);

        bool sawPrintDialog = false;
        QTimer closer;
        closer.setInterval(10);
        connect(&closer, &QTimer::timeout, [&]() {
            for (QWidget *widget : QApplication::topLevelWidgets()) {
                if (auto *dialog = qobject_cast<QPrintDialog *>(widget)) {
                    sawPrintDialog = true;
                    dialog->reject();
                }
            }
        });
        closer.start();
        QTest::keyClick(window, Qt::Key_P, Qt::ControlModifier);
        closer.stop();
        QVERIFY(sawPrintDialog);
    }
};

OMAPRESENT_TEST_SUITE(ExportTest)
#include "tst_export.moc"
