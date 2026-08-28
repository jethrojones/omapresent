#pragma once

// Presentation — spec §5. Owns the two top-level presentation windows, monitor
// assignment, idle inhibition and Do-Not-Disturb.
//
// Owner: the present agent. Contract frozen.

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>

class Presentation : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
    Q_PROPERTY(int slideIndex READ slideIndex NOTIFY positionChanged)
    Q_PROPERTY(int slideCount READ slideCount NOTIFY positionChanged)
    Q_PROPERTY(int elapsedSeconds READ elapsedSeconds NOTIFY elapsedChanged)

public:
    explicit Presentation(QObject *parent = nullptr);
    ~Presentation() override;

    bool active() const;
    int slideIndex() const;
    int slideCount() const;
    int elapsedSeconds() const;

    // Opens the audience and presenter windows (spec §5.1). With two or more
    // outputs the audience goes fullscreen on the external/non-primary one.
    // With one output the audience fills it and `N` toggles a notes overlay.
    Q_INVOKABLE void start(int fromSlideIndex);
    Q_INVOKABLE void stop();
    Q_INVOKABLE void resetTimer();

    // The deck JSON of docs/renderer-contract.md, pushed to both windows.
    // Called on every edit: both windows keep slide and scroll position.
    Q_INVOKABLE void setDeck(const QJsonObject &deckJson);

    // Navigation. next()/previous() step fragments first, then slides.
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();
    Q_INVOKABLE void gotoSlide(int index);
    Q_INVOKABLE void scrollBy(qreal deltaPixels);      // audience mirrors
    Q_INVOKABLE void showRecall(const QString &key);   // spec §4.9 overlay
    Q_INVOKABLE void hideRecall();
    Q_INVOKABLE void setBlank(const QString &mode);    // "black" | "white" | ""
    Q_INVOKABLE void setOverview(bool on);
    Q_INVOKABLE void toggleNotesOverlay();

    // Outputs currently available, re-evaluated on hotplug (spec §5.1).
    Q_INVOKABLE QStringList outputs() const;

signals:
    void activeChanged();
    void positionChanged();
    void elapsedChanged();
    void requestExit();

private:
    void assignMonitors();
    void inhibitIdle(bool on);       // hypridle / org.freedesktop.ScreenSaver
    void setDoNotDisturb(bool on);   // omarchy / makoctl / notification portal

    struct Private;
    Private *d = nullptr;
};
