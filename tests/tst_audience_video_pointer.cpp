#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJSValue>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QScopeGuard>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QUrlQuery>
#include <QtTest>

#include "presentation.h"
#include "testrunner.h"

// This test drives the real QML AudienceWindow and its qrc renderer. It does
// not call renderer functions from C++, because the acceptance fault happens
// in the native window's pointer delivery path.
class AudienceVideoPointerTest : public QObject {
    Q_OBJECT

    static QJsonObject youtubeDeck() {
        return QJsonObject{
            {QStringLiteral("mode"), QStringLiteral("present")},
            {QStringLiteral("frontmatter"),
             QJsonObject{{QStringLiteral("title"), QStringLiteral("Pointer test")}}},
            {QStringLiteral("slides"), QJsonArray{
                 QJsonObject{{QStringLiteral("index"), 0},
                             {QStringLiteral("markdown"),
                              QStringLiteral("# Play video\n\n"
                                             "https://www.youtube.com/watch?v=Tp2tq1cmwyA")},
                             {QStringLiteral("recallKey"), QString()},
                             {QStringLiteral("skip"), false}}}}};
    }

    static bool runJavaScript(QObject *view, const QString &script) {
        // WebEngineView exposes this QML slot as public meta-object API. This
        // keeps the production-window test on the supported Qt surface.
        return QMetaObject::invokeMethod(view, "runJavaScript", Qt::DirectConnection,
                                         Q_ARG(QString, script),
                                         Q_ARG(QJSValue, QJSValue()));
    }

    static QJsonObject domState(QObject *view) {
        static int sequence = 0;
        const QString marker = QStringLiteral("omapresent-t40-%1:").arg(++sequence);
        const QString script = QStringLiteral(
            "(() => {"
            " const loader = document.querySelector('button.op-media-loader');"
            " const frame = document.querySelector('iframe.op-player-shim');"
            " return JSON.stringify({"
            " loader: !!loader,"
            " pending: !!loader && loader.dataset.pending === 'true',"
            " preparingSeen: window.__omapresentT40Preparing === true,"
            " disabled: !!loader && loader.disabled,"
            " frame: !!frame, src: frame ? frame.src : '',"
            " keys: !!document.querySelector('#deck')"
            " });"
            "})()")
            .prepend(QStringLiteral("document.title = %1 + encodeURIComponent(")
                         .arg(QStringLiteral("\"") + marker + QStringLiteral("\"")))
            .append(QLatin1Char(')'));
        if (!runJavaScript(view, script))
            return {};

        QElapsedTimer timeout;
        timeout.start();
        while (timeout.elapsed() < 5000) {
            const QString title = view->property("title").toString();
            if (title.startsWith(marker)) {
                const QByteArray json = QUrl::fromPercentEncoding(
                    title.mid(marker.size()).toLatin1()).toUtf8();
                return QJsonDocument::fromJson(json).object();
            }
            QTest::qWait(10);
        }
        return {};
    }

    static QRectF loaderRect(QObject *view) {
        static int sequence = 0;
        const QString marker = QStringLiteral("omapresent-t40-rect-%1:").arg(++sequence);
        const QString script = QStringLiteral(
            "(() => { const element = document.querySelector('button.op-media-loader');"
            " if (!element) return ''; const rect = element.getBoundingClientRect();"
            " return JSON.stringify({x: rect.x, y: rect.y, width: rect.width,"
            " height: rect.height}); })()")
            .prepend(QStringLiteral("document.title = %1 + encodeURIComponent(")
                         .arg(QStringLiteral("\"") + marker + QStringLiteral("\"")))
            .append(QLatin1Char(')'));
        if (!runJavaScript(view, script))
            return {};

        QElapsedTimer timeout;
        timeout.start();
        QJsonObject object;
        while (timeout.elapsed() < 5000) {
            const QString title = view->property("title").toString();
            if (title.startsWith(marker)) {
                object = QJsonDocument::fromJson(QUrl::fromPercentEncoding(
                    title.mid(marker.size()).toLatin1()).toUtf8()).object();
                break;
            }
            QTest::qWait(10);
        }
        return QRectF(object.value(QStringLiteral("x")).toDouble(),
                      object.value(QStringLiteral("y")).toDouble(),
                      object.value(QStringLiteral("width")).toDouble(),
                      object.value(QStringLiteral("height")).toDouble());
    }

    static bool installPreparingProbe(QObject *view) {
        return runJavaScript(view, QStringLiteral(
            "window.__omapresentT40Preparing = false;"
            "(() => { const loader = document.querySelector('button.op-media-loader');"
            " if (!loader) return;"
            " new MutationObserver(() => {"
            "   if (loader.dataset.pending === 'true')"
            "     window.__omapresentT40Preparing = true;"
            " }).observe(loader, {attributes: true});"
            "})()"));
    }

private slots:
    void nativeLoaderCoordinatesBelongToWebEngineView() {
        Presentation presentation;
        presentation.setDeck(youtubeDeck());

        QQmlEngine engine;
        QQmlContext context(engine.rootContext());
        context.setContextProperty(QStringLiteral("presentation"), &presentation);
        QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/AudienceWindow.qml")));
        QScopedPointer<QObject> root(component.create(&context));
        QVERIFY2(root, qPrintable(component.errorString()));

        auto *window = qobject_cast<QQuickWindow *>(root.data());
        QVERIFY(window);
        const auto closeWindow = qScopeGuard([window] { window->close(); });
        window->setGeometry(0, 0, 1024, 576);
        window->show();
        QTRY_VERIFY(window->isVisible());
        QVERIFY(QTest::qWaitForWindowExposed(window));
        window->requestActivate();
        QTRY_VERIFY(window->isActive());

        QObject *view = nullptr;
        for (QObject *candidate : root->findChildren<QObject *>()) {
            if (candidate->inherits("QQuickWebEngineView")) {
                view = candidate;
                break;
            }
        }
        QVERIFY(view);
        QTRY_VERIFY(domState(view).value(QStringLiteral("loader")).toBool());

        // The full-window key and WheelHandler layer is part of this production
        // component. Its hit target must still be the WebEngineView.
        const QRectF rect = loaderRect(view);
        QVERIFY(rect.width() > 0.0);
        QVERIFY(rect.height() > 0.0);
        const QPoint point = rect.center().toPoint();
        QQuickItem *hit = window->contentItem()->childAt(point.x(), point.y());
        QVERIFY(hit);
        QCOMPARE(hit, qobject_cast<QQuickItem *>(view));
        QTest::mouseMove(window, QPoint(1, 1));
        QTest::qWait(20);
        QTest::mouseMove(window, point);
    }

    void domButtonClickCreatesTokenizedLoopbackShim() {
        Presentation presentation;
        presentation.setDeck(youtubeDeck());

        QQmlEngine engine;
        QQmlContext context(engine.rootContext());
        context.setContextProperty(QStringLiteral("presentation"), &presentation);
        QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/AudienceWindow.qml")));
        QScopedPointer<QObject> root(component.create(&context));
        QVERIFY2(root, qPrintable(component.errorString()));

        auto *window = qobject_cast<QQuickWindow *>(root.data());
        QVERIFY(window);
        const auto closeWindow = qScopeGuard([window] { window->close(); });
        window->setGeometry(0, 0, 1024, 576);
        window->show();
        QTRY_VERIFY(window->isVisible());
        QVERIFY(QTest::qWaitForWindowExposed(window));

        QObject *view = nullptr;
        for (QObject *candidate : root->findChildren<QObject *>()) {
            if (candidate->inherits("QQuickWebEngineView")) {
                view = candidate;
                break;
            }
        }
        QVERIFY(view);
        QTRY_VERIFY(domState(view).value(QStringLiteral("loader")).toBool());

        // Invoke the real button listener in the production DOM. This avoids
        // a host-specific QTest pointer limitation while still exercising the
        // renderer's preparing state, WebChannel call, and shim replacement.
        QVERIFY(installPreparingProbe(view));
        QVERIFY(runJavaScript(view, QStringLiteral(
            "document.querySelector('button.op-media-loader')?.click();")));
        QTRY_VERIFY(domState(view).value(QStringLiteral("preparingSeen")).toBool());
        QTRY_VERIFY_WITH_TIMEOUT(domState(view).value(QStringLiteral("frame")).toBool(),
                                 12000);

        const QJsonObject state = domState(view);
        QVERIFY(!state.value(QStringLiteral("loader")).toBool());
        const QUrl shimUrl(state.value(QStringLiteral("src")).toString());
        QCOMPARE(shimUrl.scheme(), QStringLiteral("http"));
        QCOMPARE(shimUrl.host(), QStringLiteral("127.0.0.1"));
        const QStringList pathSegments = shimUrl.path().split(QLatin1Char('/'), Qt::SkipEmptyParts);
        QCOMPARE(pathSegments.size(), 2);
        QVERIFY(QRegularExpression(QStringLiteral("^[0-9a-f]{32}$"))
                    .match(pathSegments.at(0)).hasMatch());
        QCOMPARE(pathSegments.at(1), QStringLiteral("embed.html"));
        QCOMPARE(QUrlQuery(shimUrl).queryItemValue(QStringLiteral("v")),
                 QStringLiteral("Tp2tq1cmwyA"));

    }
};

OMAPRESENT_TEST_SUITE(AudienceVideoPointerTest)

#include "tst_audience_video_pointer.moc"
