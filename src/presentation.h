#pragma once

// Presentation — spec §5. Owns the two top-level presentation windows, monitor
// assignment, idle inhibition and Do-Not-Disturb.
//
// Owner: the present agent. Contract frozen.
//
// The shape of it: C++ holds the authoritative position of the talk in a
// DeckNavigator and drives every renderer window to match it through the page
// API of docs/renderer-contract.md. No window navigates itself, so the audience
// and the presenter cannot drift apart. The renderer stays the only thing that
// knows how many fragments a slide has, so those counts are learned from its
// state events and remembered.

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QVector>

#include <functional>

#include "renderhost.h"

class QQmlEngine;
class QQuickWindow;
class QScreen;

// One output as present mode sees it: enough to decide which window goes where
// and nothing more, so the decision can be tested without a compositor.
struct PresentationOutput {
    QString name;
    bool primary = false;
};

// Which output each window takes (spec §5.1). Indices into the output list, or
// -1 when there is no output at all. Equal indices mean a single output, where
// the audience fills the screen and `N` overlays the notes on top of it.
struct MonitorAssignment {
    int audience = -1;
    int presenter = -1;

    bool isEmpty() const { return audience < 0; }
    bool sharedOutput() const { return audience >= 0 && audience == presenter; }
    bool operator==(const MonitorAssignment &other) const {
        return audience == other.audience && presenter == other.presenter;
    }
};

// Audience → the external/non-primary output, presenter → the primary one
// (spec §5.1). With one output both windows share it; with none, nothing is
// assigned. When no output claims to be primary the first one is treated as it,
// which is the same answer Qt gives on a single-seat machine.
MonitorAssignment assignOutputs(const QVector<PresentationOutput> &outputs);

// Pure ownership policy for the two desktop holds used during a talk. The
// injected actions keep settings tests away from DBus and desktop commands.
// A repeated start or stop is a no-op. Stop releases DND before idle, which
// matches the real presentation teardown path.
class PresentationEnvironmentControl {
public:
    using Action = std::function<void(bool)>;

    explicit PresentationEnvironmentControl(Action idleAction = {},
                                            Action doNotDisturbAction = {});
    ~PresentationEnvironmentControl();

    void setPreferences(bool inhibitIdle, bool doNotDisturb);
    void start();
    void stop();

    bool active() const { return m_active; }
    bool inhibitIdleEnabled() const { return m_inhibitIdleEnabled; }
    bool doNotDisturbEnabled() const { return m_doNotDisturbEnabled; }

private:
    void setIdleHeld(bool held);
    void setDoNotDisturbHeld(bool held);

    Action m_idleAction;
    Action m_doNotDisturbAction;
    bool m_inhibitIdleEnabled = true;
    bool m_doNotDisturbEnabled = true;
    bool m_idleHeld = false;
    bool m_doNotDisturbHeld = false;
    bool m_active = false;
};

// Where the talk is: which slide, how many of its fragments are revealed, and
// how far down it we have scrolled (spec §4.7).
struct DeckPosition {
    int slideIndex = 0;
    int fragment = 0;
    qreal scrollFraction = 0.0;

    bool operator==(const DeckPosition &other) const {
        return slideIndex == other.slideIndex && fragment == other.fragment
            && qFuzzyCompare(scrollFraction + 1.0, other.scrollFraction + 1.0);
    }
};

// The navigation model: pure, windowless, and the single authority for where
// the talk is. Presentation keeps one and pushes its position into both
// windows after every change.
class DeckNavigator {
public:
    // Reads the "slides" array of the renderer contract §1. Slides tagged
    // `--- {q, skip}` stay reachable by their recall key but leave the linear
    // flow, so every index here counts only the slides that remain in it.
    void setDeck(const QJsonObject &deckJson);
    int slideCount() const { return m_flowCount; }
    QStringList recallKeys() const { return m_recallKeys; }
    bool isRecallKey(const QString &key) const { return m_recallKeys.contains(key); }

    int slideIndex() const { return m_position.slideIndex; }
    int fragment() const { return m_position.fragment; }
    qreal scrollFraction() const { return m_position.scrollFraction; }
    DeckPosition position() const { return m_position; }

    // How many *reveals* the current slide has, which is how many fragment
    // elements the renderer found in it — not how many positions there are.
    // Position 0 shows none of them and position `fragmentCount()` shows them
    // all, so a five-item list has six positions. This is render.js's own rule
    // (`if (fragment < count) fragment += 1`), and the renderer owns it.
    // 0 until the renderer has said otherwise, so a slide we have never
    // displayed behaves like one with nothing to reveal.
    int fragmentCount() const { return fragmentCountAt(m_position.slideIndex); }
    int fragmentCountAt(int slideIndex) const;
    // The renderer reports the count for the slide it is showing; remember it,
    // and if we were waiting to land on that slide's last fragment (a step
    // backwards onto a slide we had not seen yet), land there now.
    void noteFragmentCount(int slideIndex, int count);

    // Next fragment, else next slide. False at the very end of the deck.
    bool next();
    // Previous fragment, else the previous slide with everything revealed —
    // the undo of next(). False at the very start.
    bool previous();
    // Clamps to the deck. Resets fragments (contract §2) and restores the
    // scroll offset this slide was left at (spec §4.7).
    bool gotoSlide(int slideIndex);
    // Remembered per slide for the rest of the session.
    void setScrollFraction(qreal fraction);

    // Digits then Enter (spec §5.2). A digit that is also a recall binding is
    // taken as the binding while the buffer is empty, so `0` first is the way
    // to type a slide number that starts with a bound digit.
    bool appendJumpDigit(const QString &text);
    void clearJump();
    QString jumpBuffer() const { return m_jump; }
    bool jumpPending() const { return !m_jump.isEmpty(); }
    // Jumps to the slide *number* typed, clamped to the deck. False when
    // nothing was typed.
    bool commitJump();

    // Recall overlays (spec §4.9). Showing one remembers the exact position it
    // covered; hiding it puts that position back — slide, fragment and scroll.
    bool showRecall(const QString &key);
    bool hideRecall();
    QString recall() const { return m_recall; }

private:
    DeckPosition m_position;
    // The last known content of each flow slide. Live edits may insert or
    // delete before the active slide, so this lets setDeck() rebase our
    // per-slide state onto the same content rather than following its number.
    QVector<QString> m_flowMarkdown;
    // Scroll offset and fragment count per flow slide, both learned as we go.
    QVector<qreal> m_scroll;
    QVector<int> m_fragmentCounts;
    QStringList m_recallKeys;
    int m_flowCount = 0;
    QString m_jump;
    QString m_recall;
    DeckPosition m_recallReturn;
    // Set when previous() stepped onto a slide whose fragment count we did not
    // know yet, so the renderer's next report can finish the move.
    bool m_revealAllOnArrival = false;
};

class Presentation : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
    Q_PROPERTY(int slideIndex READ slideIndex NOTIFY positionChanged)
    Q_PROPERTY(int slideCount READ slideCount NOTIFY positionChanged)
    Q_PROPERTY(int elapsedSeconds READ elapsedSeconds NOTIFY elapsedChanged)
    // Everything below here exists for the two windows' own chrome.
    Q_PROPERTY(QUrl rendererUrl READ rendererUrl CONSTANT)
    Q_PROPERTY(QString bridgeScript READ bridgeScript CONSTANT)
    Q_PROPERTY(RenderHost *audienceHost READ audienceHost CONSTANT)
    Q_PROPERTY(RenderHost *presenterHost READ presenterHost CONSTANT)
    Q_PROPERTY(QVariantMap palette READ palette NOTIFY deckChanged)
    Q_PROPERTY(QVariantMap audiencePalette READ audiencePalette NOTIFY deckChanged)
    Q_PROPERTY(QString deckTitle READ deckTitle NOTIFY deckChanged)
    Q_PROPERTY(QString heading READ heading NOTIFY positionChanged)
    Q_PROPERTY(QString notesHtml READ notesHtml NOTIFY positionChanged)
    Q_PROPERTY(QStringList recallKeys READ recallKeys NOTIFY deckChanged)
    Q_PROPERTY(QString recall READ recall NOTIFY positionChanged)
    Q_PROPERTY(QString jumpBuffer READ jumpBuffer NOTIFY positionChanged)
    Q_PROPERTY(QString blank READ blank NOTIFY positionChanged)
    Q_PROPERTY(bool overview READ overview NOTIFY positionChanged)
    Q_PROPERTY(bool notesOverlay READ notesOverlay NOTIFY positionChanged)
    Q_PROPERTY(bool shortcutsVisible READ shortcutsVisible NOTIFY positionChanged)
    Q_PROPERTY(bool singleOutput READ singleOutput NOTIFY activeChanged)

public:
    using WindowFactory = std::function<QQuickWindow *(const QString &source)>;

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

    // --- Added by the present agent ---------------------------------------

    // Present mode creates its own windows, so it needs an engine to create
    // them with. Optional: without one it builds a private engine on first use.
    void setQmlEngine(QQmlEngine *engine);

    // A narrow test seam for native-window lifecycle coverage. Production
    // leaves this empty and creates the QML windows above.
    void setWindowFactoryForTesting(WindowFactory factory);

    // Settings §11 controls whether present mode takes each desktop hold.
    // Changing a preference during a talk applies it at once.
    void setEnvironmentPreferences(bool inhibitIdle, bool doNotDisturb);
    bool inhibitIdleEnabled() const;
    bool doNotDisturbEnabled() const;

    // Every key from both windows arrives here (spec §5.2), so the two behave
    // identically and the dispatch is testable without a window. Returns true
    // when the key was ours.
    Q_INVOKABLE bool handleKey(int key, int modifiers, const QString &text);
    // The wheel and the presenter's own scrollbar, in renderer pixels.
    Q_INVOKABLE void toggleFullscreen();
    Q_INVOKABLE void toggleShortcuts();
    // The `Ctrl+?` sheet (spec §5.2 and §13), one "key<tab>action" row each, so
    // both windows show the same list and it stays next to the key handling.
    Q_INVOKABLE QStringList shortcutReference() const;

    // What a window binds to. See the Q_PROPERTYs above.
    QUrl rendererUrl() const;
    QString bridgeScript() const;
    RenderHost *audienceHost() const;
    RenderHost *presenterHost() const;
    QVariantMap palette() const;
    QVariantMap audiencePalette() const;
    QString deckTitle() const;
    QString heading() const;
    QString notesHtml() const;
    QStringList recallKeys() const;
    QString recall() const;
    QString jumpBuffer() const;
    QString blank() const;
    bool overview() const;
    bool notesOverlay() const;
    bool shortcutsVisible() const;
    bool singleOutput() const;

    // Called by the windows as their renderer pages come up and report back.
    Q_INVOKABLE void viewReady(const QString &role);
    Q_INVOKABLE void viewGone(const QString &role);

    // The deck as one role's window receives it. Only the audience's differs,
    // and only in its palette: spec §6 puts the projector legibility floor on
    // the audience screen alone.
    Q_INVOKABLE QJsonObject deckForRole(const QString &role) const;

    // The navigation model, for tests and for anything that needs to ask where
    // the talk is without going through the properties.
    const DeckNavigator &navigator() const;

signals:
    void activeChanged();
    void positionChanged();
    void elapsedChanged();
    void requestExit();

    // Added: JavaScript for one renderer window to run. `role` is "audience",
    // "presenter" or "preview"; each window runs only its own.
    void runInView(const QString &role, const QString &script);
    void deckChanged();

private:
    void assignMonitors();
    void inhibitIdle(bool on);       // hypridle / org.freedesktop.ScreenSaver
    void setDoNotDisturb(bool on);   // omarchy / makoctl / notification portal

    // A state event from one window's renderer (contract §2).
    void adoptState(const QString &role, const QJsonObject &state);
    void afterNavigation();
    // Bring the windows to where the talk now is, sending each only the
    // difference between that and what it is already showing.
    void syncViews();
    void applyTo(const QString &role);
    void applyOverlays(const QString &role);
    QString overlayScript(const QString &role);

    QVector<PresentationOutput> currentOutputs() const;
    QQuickWindow *createWindow(const QString &source);
    void placeWindow(QQuickWindow *window, QScreen *screen);
    void closeWindows();

    struct Private;
    Private *d = nullptr;
};
