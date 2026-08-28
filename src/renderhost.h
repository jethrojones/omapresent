#pragma once

// The C++ half of docs/renderer-contract.md. Two pieces, both host-side:
//
//   RenderHost — the object registered on the QWebChannel as `omapresentHost`,
//                which the renderer calls back into on every state change, plus
//                the pure helpers that build the deck document and the little
//                bits of JavaScript the host injects and runs.
//   PdfExport  — an offscreen page that draws the deck in `pdf` mode and prints
//                it (spec §8). Nothing else in the app owns a QWebEnginePage.
//
// Owner: the app-shell agent.

#include <QJsonObject>
#include <QObject>
#include <QPageLayout>
#include <QString>
#include <QTimer>

class QWebEnginePage;
class QWebEngineProfile;

class RenderHost : public QObject {
    Q_OBJECT
    Q_PROPERTY(int slideIndex READ slideIndex NOTIFY stateChanged)
    Q_PROPERTY(int slideCount READ slideCount NOTIFY stateChanged)
    Q_PROPERTY(qreal scrollFraction READ scrollFraction NOTIFY stateChanged)

public:
    explicit RenderHost(QObject *parent = nullptr);

    int slideIndex() const { return m_slideIndex; }
    int slideCount() const { return m_slideCount; }
    qreal scrollFraction() const { return m_scrollFraction; }
    QJsonObject lastState() const { return m_state; }

    // --- Pure helpers, directly unit-tested -------------------------------
    // The deck document of docs/renderer-contract.md §1. Everything it needs is
    // passed in, so the assembly can be tested without a web engine.
    static QJsonObject composeDeck(const QString &mode, const QJsonObject &deck,
                                   const QJsonObject &assets, const QJsonObject &media,
                                   const QJsonObject &palette,
                                   const QString &backgroundImagePath, qreal textScale);
    // A call to one of the window.omapresent entry points of the contract §2,
    // ready for runJavaScript(): callScript("update", deck).
    static QString callScript(const QString &function, const QJsonObject &deckJson);
    // The landscape page for an `aspect:` value like "16:9" (spec §8).
    // Unparsable values fall back to 16:9.
    static QPageLayout pageLayoutFor(const QString &aspect);

    // The script the host injects into every renderer page: the Qt WebChannel
    // client, plus the `onState` hook the contract §2 says the host assigns.
    // Empty when the WebChannel client cannot be found, which only costs us
    // state reporting — the deck still renders.
    static QString bridgeScript();

public slots:
    // Called by the renderer: omapresentHost.state(JSON.stringify(state)).
    void state(const QString &json);

signals:
    void stateChanged();

private:
    QJsonObject m_state;
    int m_slideIndex = 0;
    int m_slideCount = 0;
    qreal m_scrollFraction = 0.0;
};

class PdfExport : public QObject {
    Q_OBJECT

public:
    explicit PdfExport(QObject *parent = nullptr);
    ~PdfExport() override;

    bool busy() const { return m_busy; }

    // Renders `deckJson` offscreen and writes the PDF to `outputPath`. The deck
    // is used exactly as given, so the caller sets mode "pdf" and expands
    // fragments; nothing here scales a slide down to fit (spec §8).
    void run(const QJsonObject &deckJson, const QString &outputPath);

signals:
    void finished(bool ok, const QString &outputPath, const QString &message);

private:
    void renderDeck();
    void printWhenSettled();
    void fail(const QString &message);

    QWebEngineProfile *m_profile = nullptr;
    QWebEnginePage *m_page = nullptr;
    QTimer m_settleTimer;
    QJsonObject m_deck;
    QString m_outputPath;
    int m_settleAttempts = 0;
    bool m_busy = false;
};
