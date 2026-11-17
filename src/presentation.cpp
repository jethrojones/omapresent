#include "presentation.h"

#include <QDebug>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QGuiApplication>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>
#include <QProcess>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlError>
#include <QQuickWindow>
#include <QScreen>
#include <QSet>
#include <QWindow>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>

#include <memory>
#include <utility>

#include "omarchytheme.h"
#include "platform.h"
#include "renderhost.h"
#include "terminalsession.h"

namespace {

const QString audienceRole = QStringLiteral("audience");
const QString presenterRole = QStringLiteral("presenter");
// The next-slide preview in the presenter window. It draws with the renderer
// like everything else, so what the speaker sees ahead is what will appear.
const QString previewRole = QStringLiteral("preview");

// One arrow press of scrolling, and one page. Renderer pixels (spec §4.7).
constexpr qreal arrowScrollPixels = 120.0;
constexpr qreal pageScrollPixels = 640.0;

// The terminal's type size moves in steps, a tenth at a time, between a size
// that fits a wide TUI and one a back row can read.
constexpr int minTerminalZoom = -6;
constexpr int maxTerminalZoom = 12;

QString call(const QString &function) {
    return QStringLiteral("window.omapresent && window.omapresent.%1();").arg(function);
}

QString call(const QString &function, int argument) {
    return QStringLiteral("window.omapresent && window.omapresent.%1(%2);")
        .arg(function)
        .arg(argument);
}

QString call(const QString &function, qreal argument) {
    return QStringLiteral("window.omapresent && window.omapresent.%1(%2);")
        .arg(function)
        .arg(argument, 0, 'f', 6);
}

QString call(const QString &function, const QString &argument) {
    QString quoted = QJsonDocument(QJsonArray{argument}).toJson(QJsonDocument::Compact);
    quoted = quoted.mid(1, quoted.size() - 2);  // the array brackets
    return QStringLiteral("window.omapresent && window.omapresent.%1(%2);")
        .arg(function, quoted);
}

int commonSubsequenceLength(const QVector<QString> &oldFlowMarkdown,
                            const QVector<QString> &newFlowMarkdown) {
    QVector<int> previous(newFlowMarkdown.size() + 1, 0);
    for (const QString &oldMarkdown : oldFlowMarkdown) {
        QVector<int> current(newFlowMarkdown.size() + 1, 0);
        for (int newIndex = 1; newIndex <= newFlowMarkdown.size(); ++newIndex) {
            if (oldMarkdown == newFlowMarkdown.at(newIndex - 1))
                current[newIndex] = previous.at(newIndex - 1) + 1;
            else
                current[newIndex] = qMax(previous.at(newIndex), current.at(newIndex - 1));
        }
        previous.swap(current);
    }
    return previous.last();
}

// Returns one stable, left-to-right longest-common-subsequence alignment.
QVector<int> commonSubsequenceMatches(const QVector<QString> &oldFlowMarkdown,
                                      const QVector<QString> &newFlowMarkdown) {
    const int oldCount = oldFlowMarkdown.size();
    const int newCount = newFlowMarkdown.size();
    const int width = newCount + 1;
    QVector<int> scores((oldCount + 1) * width, 0);
    const auto score = [&scores, width](int oldIndex, int newIndex) {
        return scores.at(oldIndex * width + newIndex);
    };
    const auto setScore = [&scores, width](int oldIndex, int newIndex, int value) {
        scores[oldIndex * width + newIndex] = value;
    };

    for (int oldIndex = oldCount - 1; oldIndex >= 0; --oldIndex) {
        for (int newIndex = newCount - 1; newIndex >= 0; --newIndex) {
            if (oldFlowMarkdown.at(oldIndex) == newFlowMarkdown.at(newIndex))
                setScore(oldIndex, newIndex, score(oldIndex + 1, newIndex + 1) + 1);
            else
                setScore(oldIndex, newIndex,
                         qMax(score(oldIndex + 1, newIndex), score(oldIndex, newIndex + 1)));
        }
    }

    QVector<int> matches(oldCount, -1);
    int oldIndex = 0;
    int newIndex = 0;
    while (oldIndex < oldCount && newIndex < newCount) {
        if (oldFlowMarkdown.at(oldIndex) == newFlowMarkdown.at(newIndex) &&
            score(oldIndex, newIndex) == score(oldIndex + 1, newIndex + 1) + 1) {
            matches[oldIndex] = newIndex;
            ++oldIndex;
            ++newIndex;
        } else if (score(oldIndex + 1, newIndex) >= score(oldIndex, newIndex + 1)) {
            ++oldIndex;
        } else {
            ++newIndex;
        }
    }
    return matches;
}

// Source line spans describe the resulting document, not the edit that made
// it. Inserting before or after equal adjacent slides gives the same raw text
// and spans. Preserve the active slide only when every longest sequence
// alignment contains it at one new index; otherwise setDeck() must clamp.
int unambiguousCurrentSlide(const QVector<QString> &oldFlowMarkdown,
                            const QVector<QString> &newFlowMarkdown,
                            int oldCurrentIndex) {
    if (oldCurrentIndex < 0 || oldCurrentIndex >= oldFlowMarkdown.size())
        return -1;

    const int commonLength = commonSubsequenceLength(oldFlowMarkdown, newFlowMarkdown);
    QVector<QString> withoutCurrent = oldFlowMarkdown;
    withoutCurrent.removeAt(oldCurrentIndex);
    if (commonSubsequenceLength(withoutCurrent, newFlowMarkdown) == commonLength)
        return -1;

    QVector<int> before(newFlowMarkdown.size() + 1, 0);
    for (int oldIndex = 0; oldIndex < oldCurrentIndex; ++oldIndex) {
        QVector<int> next(newFlowMarkdown.size() + 1, 0);
        for (int newIndex = 1; newIndex <= newFlowMarkdown.size(); ++newIndex) {
            if (oldFlowMarkdown.at(oldIndex) == newFlowMarkdown.at(newIndex - 1))
                next[newIndex] = before.at(newIndex - 1) + 1;
            else
                next[newIndex] = qMax(before.at(newIndex), next.at(newIndex - 1));
        }
        before.swap(next);
    }

    QVector<int> after(newFlowMarkdown.size() + 1, 0);
    for (int oldIndex = oldFlowMarkdown.size() - 1; oldIndex > oldCurrentIndex;
         --oldIndex) {
        QVector<int> next(newFlowMarkdown.size() + 1, 0);
        for (int newIndex = newFlowMarkdown.size() - 1; newIndex >= 0; --newIndex) {
            if (oldFlowMarkdown.at(oldIndex) == newFlowMarkdown.at(newIndex))
                next[newIndex] = after.at(newIndex + 1) + 1;
            else
                next[newIndex] = qMax(after.at(newIndex), next.at(newIndex + 1));
        }
        after.swap(next);
    }

    int match = -1;
    const QString &currentMarkdown = oldFlowMarkdown.at(oldCurrentIndex);
    for (int newIndex = 0; newIndex < newFlowMarkdown.size(); ++newIndex) {
        if (newFlowMarkdown.at(newIndex) != currentMarkdown ||
            before.at(newIndex) + 1 + after.at(newIndex + 1) != commonLength)
            continue;
        if (match >= 0)
            return -1;
        match = newIndex;
    }
    return match;
}

// Runs a short command and hands back its stdout. Used only at the two ends of
// a talk, to read and restore the desktop's notification state.
QString runCommand(const QString &program, const QStringList &arguments, bool *ok = nullptr) {
    if (ok)
        *ok = false;
    QProcess process;
    process.start(program, arguments);
    if (!process.waitForFinished(2000)) {
        process.kill();
        process.waitForFinished(200);
        return {};
    }
    if (ok)
        *ok = process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
    return QString::fromUtf8(process.readAllStandardOutput());
}

bool haveCommand(const QString &program) {
    return !QStandardPaths::findExecutable(program).isEmpty();
}

// Keeps the screen awake for the length of a talk (spec §5.3). Taking one
// inhibits, letting it go releases — including down a crash path, which is the
// whole reason this is a holder and not a pair of calls.
class IdleInhibit {
public:
    IdleInhibit() {
        // macOS has its own power assertion and no session bus worth asking.
        // Elsewhere this returns false and the DBus inhibitors below run.
        m_native = Platform::holdDisplayAwake(true);
        if (m_native)
            return;

        // hypridle serves org.freedesktop.ScreenSaver itself, so on Omarchy
        // this one call covers both halves of the spec's "hypridle /
        // org.freedesktop.ScreenSaver". The desktop-neutral PowerManagement
        // interface is the fallback for machines running neither.
        const struct { const char *service; const char *path; const char *interface; } buses[] = {
            {"org.freedesktop.ScreenSaver", "/org/freedesktop/ScreenSaver",
             "org.freedesktop.ScreenSaver"},
            {"org.freedesktop.PowerManagement.Inhibit",
             "/org/freedesktop/PowerManagement/Inhibit",
             "org.freedesktop.PowerManagement.Inhibit"}};

        for (const auto &bus : buses) {
            QDBusInterface interface(QString::fromLatin1(bus.service),
                                     QString::fromLatin1(bus.path),
                                     QString::fromLatin1(bus.interface),
                                     QDBusConnection::sessionBus());
            if (!interface.isValid())
                continue;
            const QDBusReply<uint> reply =
                interface.call(QStringLiteral("Inhibit"), QStringLiteral("omapresent"),
                               QStringLiteral("Presenting"));
            if (!reply.isValid())
                continue;
            m_service = QString::fromLatin1(bus.service);
            m_path = QString::fromLatin1(bus.path);
            m_interface = QString::fromLatin1(bus.interface);
            m_cookie = reply.value();
            break;
        }

        // Omarchy's own switch, which is what hypridle reads on this desktop.
        // Only touched when it was not already held, so we never take away a
        // stay-awake the user set for themselves.
        if (haveCommand(QStringLiteral("omarchy-toggle-idle"))) {
            bool ok = false;
            const QString status =
                runCommand(QStringLiteral("omarchy-toggle-idle"), {QStringLiteral("status")}, &ok);
            m_wasStayingAwake = ok && status.contains(QStringLiteral("\"enabled\":true"));
            if (ok && !m_wasStayingAwake) {
                runCommand(QStringLiteral("omarchy-toggle-idle"), {QStringLiteral("stay-awake")});
                m_heldStayAwake = true;
            }
        }
    }

    ~IdleInhibit() {
        if (m_native) {
            Platform::holdDisplayAwake(false);
            return;
        }
        if (!m_service.isEmpty()) {
            QDBusInterface interface(m_service, m_path, m_interface,
                                     QDBusConnection::sessionBus());
            if (interface.isValid())
                interface.call(QStringLiteral("UnInhibit"), m_cookie);
        }
        if (m_heldStayAwake)
            runCommand(QStringLiteral("omarchy-toggle-idle"), {QStringLiteral("allow-idle")});
    }

    IdleInhibit(const IdleInhibit &) = delete;
    IdleInhibit &operator=(const IdleInhibit &) = delete;

private:
    QString m_service;
    QString m_path;
    QString m_interface;
    uint m_cookie = 0;
    bool m_native = false;
    bool m_wasStayingAwake = false;
    bool m_heldStayAwake = false;
};

// Turns Do-Not-Disturb on for the length of a talk and puts it back exactly as
// it was (spec §5.3). The prior state is read, never assumed — a presenter who
// already lives in DND must not come out of a talk with notifications back on.
class DoNotDisturbHold {
public:
    DoNotDisturbHold() {
        if (haveCommand(QStringLiteral("makoctl"))) {
            m_tool = Mako;
            bool ok = false;
            const QString modes = runCommand(QStringLiteral("makoctl"), {QStringLiteral("mode")}, &ok);
            m_wasEnabled = ok && modes.split(u'\n').contains(QStringLiteral("do-not-disturb"));
            if (ok && !m_wasEnabled) {
                runCommand(QStringLiteral("makoctl"),
                           {QStringLiteral("mode"), QStringLiteral("-a"),
                            QStringLiteral("do-not-disturb")});
                m_held = true;
            }
            return;
        }

        if (haveCommand(QStringLiteral("swaync-client"))) {
            m_tool = Swaync;
            bool ok = false;
            const QString state = runCommand(QStringLiteral("swaync-client"), {QStringLiteral("-D")}, &ok);
            m_wasEnabled = ok && state.trimmed() == QStringLiteral("true");
            if (ok && !m_wasEnabled) {
                runCommand(QStringLiteral("swaync-client"), {QStringLiteral("-df")});
                m_held = true;
            }
            return;
        }

        if (haveCommand(QStringLiteral("dunstctl"))) {
            m_tool = Dunst;
            bool ok = false;
            const QString paused =
                runCommand(QStringLiteral("dunstctl"), {QStringLiteral("is-paused")}, &ok);
            m_wasEnabled = ok && paused.trimmed() == QStringLiteral("true");
            if (ok && !m_wasEnabled) {
                runCommand(QStringLiteral("dunstctl"),
                           {QStringLiteral("set-paused"), QStringLiteral("true")});
                m_held = true;
            }
            return;
        }

        if (haveCommand(QStringLiteral("omarchy-shell"))) {
            // Omarchy 4.x: mako/swaync/dunst are not installed. The shell
            // plugin exposes a real query (`isDnd` prints on/off) and
            // `setDnd("on"|"off"|true|false|1|0|yes)` — see Service.qml.
            // `omarchy-toggle-notification-silencing` only toggles and prints
            // nothing, so using it here used to enable DND and immediately
            // undo it.
            bool ok = false;
            const QString state =
                runCommand(QStringLiteral("omarchy-shell"),
                           {QStringLiteral("notifications"), QStringLiteral("isDnd")},
                           &ok);
            const QString trimmed = state.trimmed().toLower();
            if (!ok || (trimmed != QStringLiteral("on") && trimmed != QStringLiteral("off"))) {
                // Shell is present but the notifications plugin is not
                // answering. Stay out of the way rather than toggling blindly.
                return;
            }
            m_tool = OmarchyShell;
            m_wasEnabled = trimmed == QStringLiteral("on");
            if (!m_wasEnabled) {
                bool setOk = false;
                const QString after =
                    runCommand(QStringLiteral("omarchy-shell"),
                               {QStringLiteral("notifications"), QStringLiteral("setDnd"),
                                QStringLiteral("on")},
                               &setOk);
                if (setOk && after.trimmed().toLower() == QStringLiteral("on")) {
                    runCommand(QStringLiteral("omarchy-shell"),
                               {QStringLiteral("-q"), QStringLiteral("omarchy.indicators"),
                                QStringLiteral("refresh")});
                    m_held = true;
                } else {
                    m_tool = None;
                }
            }
            return;
        }

        if (haveCommand(QStringLiteral("omarchy-toggle-notification-silencing"))) {
            // Last resort when omarchy-shell itself is missing. The wrapper
            // still prints nothing, so if stdout is unreadable we undo.
            m_tool = OmarchyToggle;
            bool ok = false;
            const QString state =
                runCommand(QStringLiteral("omarchy-toggle-notification-silencing"), {}, &ok);
            const QString lowered = state.trimmed().toLower();
            const bool nowEnabled = lowered.contains(QStringLiteral("true"))
                || lowered.contains(QStringLiteral("enabled"))
                || lowered.contains(QStringLiteral("on"))
                || lowered.contains(QStringLiteral("\"dnd\":1"));
            const bool readable = ok
                && (nowEnabled || lowered.contains(QStringLiteral("false"))
                    || lowered.contains(QStringLiteral("disabled"))
                    || lowered.contains(QStringLiteral("off")));
            if (!readable) {
                if (ok)
                    runCommand(QStringLiteral("omarchy-toggle-notification-silencing"), {});
                m_tool = None;
                return;
            }
            m_wasEnabled = !nowEnabled;
            if (m_wasEnabled)
                runCommand(QStringLiteral("omarchy-toggle-notification-silencing"), {});
            else
                m_held = true;
        }
    }

    ~DoNotDisturbHold() {
        if (!m_held)
            return;
        switch (m_tool) {
        case Mako:
            runCommand(QStringLiteral("makoctl"),
                       {QStringLiteral("mode"), QStringLiteral("-r"),
                        QStringLiteral("do-not-disturb")});
            break;
        case Swaync:
            runCommand(QStringLiteral("swaync-client"), {QStringLiteral("-dn")});
            break;
        case Dunst:
            runCommand(QStringLiteral("dunstctl"),
                       {QStringLiteral("set-paused"), QStringLiteral("false")});
            break;
        case OmarchyShell:
            runCommand(QStringLiteral("omarchy-shell"),
                       {QStringLiteral("notifications"), QStringLiteral("setDnd"),
                        QStringLiteral("off")});
            runCommand(QStringLiteral("omarchy-shell"),
                       {QStringLiteral("-q"), QStringLiteral("omarchy.indicators"),
                        QStringLiteral("refresh")});
            break;
        case OmarchyToggle:
            runCommand(QStringLiteral("omarchy-toggle-notification-silencing"), {});
            break;
        case None:
            break;
        }
    }

    DoNotDisturbHold(const DoNotDisturbHold &) = delete;
    DoNotDisturbHold &operator=(const DoNotDisturbHold &) = delete;

private:
    enum Tool { None, Mako, Swaync, Dunst, OmarchyShell, OmarchyToggle };

    Tool m_tool = None;
    bool m_wasEnabled = false;
    bool m_held = false;
};

// What one renderer window is currently showing, so we only ever send it the
// difference between that and where the talk actually is.
struct ViewState {
    bool ready = false;
    bool deckSent = false;
    // -1 until the first goto, so the first sync always states a position.
    DeckPosition applied{-1, 0, 0.0};
    QString blank;
    QString recall;
    bool overview = false;
    bool terminalFocused = false;
    int terminalZoom = 0;
};

// `window.omapresent.terminalWrite('<base64>')`. PTY output is bytes, and base64
// is the one encoding that survives becoming a JavaScript string literal with
// no escaping to get wrong.
QString terminalWriteScript(const QByteArray &bytes) {
    return QStringLiteral("window.omapresent && window.omapresent.terminalWrite('%1');")
        .arg(QString::fromLatin1(bytes.toBase64()));
}

}  // namespace

// --- Presentation environment controls ------------------------------------

PresentationEnvironmentControl::PresentationEnvironmentControl(
    Action idleAction, Action doNotDisturbAction)
    : m_idleAction(std::move(idleAction))
    , m_doNotDisturbAction(std::move(doNotDisturbAction)) {}

PresentationEnvironmentControl::~PresentationEnvironmentControl() {
    stop();
}

void PresentationEnvironmentControl::setPreferences(bool inhibitIdle,
                                                     bool doNotDisturb) {
    m_inhibitIdleEnabled = inhibitIdle;
    m_doNotDisturbEnabled = doNotDisturb;
    if (!m_active)
        return;

    setIdleHeld(m_inhibitIdleEnabled);
    setDoNotDisturbHeld(m_doNotDisturbEnabled);
}

void PresentationEnvironmentControl::start() {
    if (m_active)
        return;
    m_active = true;
    setIdleHeld(m_inhibitIdleEnabled);
    setDoNotDisturbHeld(m_doNotDisturbEnabled);
}

void PresentationEnvironmentControl::stop() {
    if (!m_active)
        return;
    // Notifications first. A last-window close can start application teardown.
    setDoNotDisturbHeld(false);
    setIdleHeld(false);
    m_active = false;
}

void PresentationEnvironmentControl::setIdleHeld(bool held) {
    if (m_idleHeld == held)
        return;
    if (m_idleAction)
        m_idleAction(held);
    m_idleHeld = held;
}

void PresentationEnvironmentControl::setDoNotDisturbHeld(bool held) {
    if (m_doNotDisturbHeld == held)
        return;
    if (m_doNotDisturbAction)
        m_doNotDisturbAction(held);
    m_doNotDisturbHeld = held;
}

// --- Monitors ---------------------------------------------------------------

MonitorAssignment assignOutputs(const QVector<PresentationOutput> &outputs) {
    MonitorAssignment assignment;
    if (outputs.isEmpty())
        return assignment;
    if (outputs.size() == 1) {
        assignment.audience = 0;
        assignment.presenter = 0;
        return assignment;
    }

    int primary = 0;
    for (int i = 0; i < outputs.size(); ++i) {
        if (outputs.at(i).primary) {
            primary = i;
            break;
        }
    }

    // A known laptop panel takes the presenter view whatever is primary, and
    // the audience takes an external display: the one that is not primary
    // when there is a choice, since a desk monitor is more often the main
    // display than a projector is.
    int builtIn = -1;
    for (int i = 0; i < outputs.size(); ++i) {
        if (outputs.at(i).builtIn) {
            builtIn = i;
            break;
        }
    }
    if (builtIn >= 0) {
        int external = -1;
        for (int i = 0; i < outputs.size(); ++i) {
            if (i == builtIn || outputs.at(i).builtIn)
                continue;
            if (external < 0 || (outputs.at(external).primary && !outputs.at(i).primary))
                external = i;
        }
        if (external >= 0) {
            assignment.presenter = builtIn;
            assignment.audience = external;
            return assignment;
        }
    }

    assignment.presenter = primary;
    assignment.audience = primary == 0 ? 1 : 0;
    return assignment;
}

// --- DeckNavigator ----------------------------------------------------------

void DeckNavigator::setDeck(const QJsonObject &deckJson) {
    const QJsonArray slides = deckJson.value(QStringLiteral("slides")).toArray();

    QStringList keys;
    QStringList terminalKeys;
    QVector<QString> flowMarkdown;
    QVector<bool> flowTerminal;
    for (const QJsonValue &value : slides) {
        const QJsonObject slide = value.toObject();
        const QString key = slide.value(QStringLiteral("recallKey")).toString();
        const bool terminal = slide.value(QStringLiteral("terminal")).toBool();
        if (!key.isEmpty() && !keys.contains(key)) {
            keys.append(key);
            // The first slide bound to a key is the one it pops, here as in
            // the renderer, so that slide alone decides.
            if (terminal)
                terminalKeys.append(key);
        }
        if (!slide.value(QStringLiteral("skip")).toBool()) {
            flowMarkdown.append(slide.value(QStringLiteral("markdown")).toString());
            flowTerminal.append(terminal);
        }
    }

    // A live edit is a full deck replacement. Keep state with the matching
    // slide content, not the old numeric position: inserting or deleting
    // above the speaker must not make the audience jump to another slide.
    const QVector<QString> oldFlowMarkdown = m_flowMarkdown;
    const QVector<qreal> oldScroll = m_scroll;
    const QVector<int> oldFragmentCounts = m_fragmentCounts;
    QVector<bool> newUsed(flowMarkdown.size(), false);
    QVector<int> oldToNew(oldFlowMarkdown.size(), -1);
    QVector<qreal> scroll(flowMarkdown.size(), 0.0);
    QVector<int> fragmentCounts(flowMarkdown.size(), 0);
    const auto match = [&](int oldIndex, int newIndex) {
        newUsed[newIndex] = true;
        oldToNew[oldIndex] = newIndex;
        scroll[newIndex] = oldScroll.value(oldIndex);
        fragmentCounts[newIndex] = oldFragmentCounts.value(oldIndex);
    };

    const int oldCurrentIndex = m_position.slideIndex;
    const int newCurrentIndex =
        unambiguousCurrentSlide(oldFlowMarkdown, flowMarkdown, oldCurrentIndex);
    const bool currentSlideSurvived = newCurrentIndex >= 0;
    const bool currentContentStillExists =
        oldCurrentIndex >= 0 && oldCurrentIndex < oldFlowMarkdown.size() &&
        flowMarkdown.contains(oldFlowMarkdown.at(oldCurrentIndex));

    // Do not give an ambiguous duplicate the active slide's saved state. A
    // deterministic LCS still preserves the other slides in document order.
    QVector<QString> matchingMarkdown;
    QVector<int> matchingOldIndexes;
    matchingMarkdown.reserve(oldFlowMarkdown.size());
    matchingOldIndexes.reserve(oldFlowMarkdown.size());
    for (int oldIndex = 0; oldIndex < oldFlowMarkdown.size(); ++oldIndex) {
        if (!currentSlideSurvived && oldIndex == oldCurrentIndex)
            continue;
        matchingMarkdown.append(oldFlowMarkdown.at(oldIndex));
        matchingOldIndexes.append(oldIndex);
    }
    const QVector<int> matches = commonSubsequenceMatches(matchingMarkdown, flowMarkdown);
    for (int index = 0; index < matches.size(); ++index) {
        if (matches.at(index) >= 0)
            match(matchingOldIndexes.at(index), matches.at(index));
    }

    const auto rebasedIndex = [&oldToNew, flowMarkdown](int oldIndex) {
        const int matched = oldToNew.value(oldIndex, -1);
        return matched >= 0 ? matched : qBound(0, oldIndex, qMax(0, flowMarkdown.size() - 1));
    };

    m_recallKeys = keys;
    m_terminalRecallKeys = terminalKeys;
    m_flowTerminal = flowTerminal;
    m_flowCount = flowMarkdown.size();
    m_flowMarkdown = flowMarkdown;
    m_scroll = scroll;
    m_fragmentCounts = fragmentCounts;
    if (!m_recall.isEmpty() && !m_recallKeys.contains(m_recall))
        m_recall.clear();
    m_position.slideIndex = rebasedIndex(m_position.slideIndex);
    // A changed, deleted, or ambiguous active slide has no identity to carry
    // to its fallback. Keep scroll only for clearly changed new content.
    if (!currentSlideSurvived) {
        m_revealAllOnArrival = false;
        m_position.fragment = 0;
        if (!currentContentStillExists && !newUsed.value(m_position.slideIndex) &&
            !m_scroll.isEmpty())
            m_scroll[m_position.slideIndex] = m_position.scrollFraction;
    } else {
        m_position.fragment = qBound(0, m_position.fragment, fragmentCount());
    }
    m_position.scrollFraction = m_scroll.value(m_position.slideIndex);
    m_recallReturn.slideIndex = rebasedIndex(m_recallReturn.slideIndex);
}

int DeckNavigator::fragmentCountAt(int slideIndex) const {
    return qMax(0, m_fragmentCounts.value(slideIndex));
}

void DeckNavigator::noteFragmentCount(int slideIndex, int count) {
    if (slideIndex < 0 || slideIndex >= m_flowCount || count < 0)
        return;

    m_fragmentCounts[slideIndex] = count;
    if (slideIndex != m_position.slideIndex)
        return;

    if (m_revealAllOnArrival) {
        m_revealAllOnArrival = false;
        m_position.fragment = count;
    }
    m_position.fragment = qBound(0, m_position.fragment, count);
}

bool DeckNavigator::next() {
    if (m_flowCount == 0)
        return false;
    if (m_position.fragment < fragmentCount()) {
        ++m_position.fragment;
        return true;
    }
    return gotoSlide(m_position.slideIndex + 1);
}

bool DeckNavigator::previous() {
    if (m_flowCount == 0)
        return false;
    if (m_position.fragment > 0) {
        --m_position.fragment;
        return true;
    }
    if (m_position.slideIndex == 0)
        return false;

    const int target = m_position.slideIndex - 1;
    gotoSlide(target);
    // Stepping back should undo the last step forward, which means landing on
    // the previous slide fully revealed. If we have never displayed it we do
    // not know how many fragments that is, so ask to be told on arrival.
    const int known = m_fragmentCounts.value(target);
    if (known > 0)
        m_position.fragment = known;
    else
        m_revealAllOnArrival = true;
    return true;
}

bool DeckNavigator::gotoSlide(int slideIndex) {
    if (m_flowCount == 0)
        return false;

    const DeckPosition before = m_position;
    m_revealAllOnArrival = false;
    m_position.slideIndex = qBound(0, slideIndex, m_flowCount - 1);
    m_position.fragment = 0;
    m_position.scrollFraction = m_scroll.value(m_position.slideIndex);
    return !(m_position == before);
}

void DeckNavigator::setScrollFraction(qreal fraction) {
    if (m_flowCount == 0)
        return;
    const qreal clamped = qBound(qreal(0), fraction, qreal(1));
    m_position.scrollFraction = clamped;
    m_scroll[m_position.slideIndex] = clamped;
}

bool DeckNavigator::appendJumpDigit(const QString &text) {
    if (text.size() != 1 || !text.at(0).isDigit())
        return false;
    // A digit bound as a recall key is that binding when nothing has been typed
    // yet; leading with `0` is how you then type a slide number starting with
    // the same digit.
    if (m_jump.isEmpty() && isRecallKey(text))
        return false;
    if (m_jump.size() >= 4)
        return false;
    m_jump += text;
    return true;
}

void DeckNavigator::clearJump() {
    m_jump.clear();
}

bool DeckNavigator::commitJump() {
    if (m_jump.isEmpty())
        return false;
    const int number = m_jump.toInt();
    m_jump.clear();
    gotoSlide(number - 1);
    return true;
}

bool DeckNavigator::showRecall(const QString &key) {
    if (!isRecallKey(key))
        return false;
    // Switching straight from one overlay to another still returns to the slide
    // the first one covered.
    if (m_recall.isEmpty())
        m_recallReturn = m_position;
    m_recall = key;
    return true;
}

bool DeckNavigator::hideRecall() {
    if (m_recall.isEmpty())
        return false;
    m_recall.clear();
    m_position = m_recallReturn;
    return true;
}

// --- Presentation -----------------------------------------------------------

struct Presentation::Private {
    DeckNavigator nav;
    QJsonObject deck;
    // The same deck with the spec §6 projector floor applied to its palette.
    // Only the audience window gets this one.
    QJsonObject audienceDeck;
    QVariantMap palette;
    QVariantMap audiencePalette;
    QString deckTitle;
    QString heading;
    QString notesHtml;
    QString blank;
    bool overview = false;
    bool notesOverlay = false;
    bool shortcuts = false;
    QString remoteUrl;
    bool remoteInUse = false;
    bool active = false;
    bool mediaActive = false;
    int mediaCount = 0;
    // Spec §5.1: the audience window opens windowed. This is the presenter's
    // own answer to `F` / `F11`, kept so a re-assignment restores it rather
    // than resetting it.
    bool audienceFullScreen = false;
    int elapsed = 0;
    QTimer clock;

    QQmlEngine *engine = nullptr;
    std::unique_ptr<QQmlEngine> ownEngine;
    Presentation::WindowFactory windowFactory;
    QPointer<QQuickWindow> audienceWindow;
    QPointer<QQuickWindow> presenterWindow;
    QHash<QString, ViewState> views;
    RenderHost *audienceHost = nullptr;
    RenderHost *presenterHost = nullptr;
    MonitorAssignment assignment;

    std::unique_ptr<IdleInhibit> idle;
    std::unique_ptr<DoNotDisturbHold> doNotDisturb;
    std::unique_ptr<PresentationEnvironmentControl> environment;

    // The live terminal. One session for the whole talk, created the first
    // time a terminal slide is shown.
    std::unique_ptr<TerminalSession> terminal;
    QString terminalDirectory;
    // Roles whose page has built its terminal and is sent live output.
    QSet<QString> terminalAttached;
    // The role whose keystrokes are believed: the page that last took focus.
    // Both pages run a terminal emulator and both answer the shell's queries,
    // so only one of them may talk to it.
    QString terminalInputRole;
    bool terminalFocused = false;
    // Which terminal the keyboard was last handed to: "slide:3", "recall:t",
    // or empty when none is showing. A change of identity is an arrival.
    QString terminalShowing;
    int terminalCols = 0;
    int terminalRows = 0;
    int terminalZoom = 0;

    // The window whose renderer we ask to navigate and scroll, and whose state
    // events we believe. The presenter when there is one, because that is where
    // the speaker's hands are; the audience when presenting on one screen.
    QString masterRole() const {
        return views.value(presenterRole).ready ? presenterRole : audienceRole;
    }
};

Presentation::Presentation(QObject *parent) : QObject(parent), d(new Private) {
    d->audienceHost = new RenderHost(this);
    d->presenterHost = new RenderHost(this);
    d->environment = std::make_unique<PresentationEnvironmentControl>(
        [this](bool on) { inhibitIdle(on); },
        [this](bool on) { setDoNotDisturb(on); });

    d->clock.setInterval(1000);
    connect(&d->clock, &QTimer::timeout, this, [this]() {
        ++d->elapsed;
        emit elapsedChanged();
    });

    connect(d->audienceHost, &RenderHost::stateChanged, this,
            [this]() { adoptState(audienceRole, d->audienceHost->lastState()); });
    connect(d->presenterHost, &RenderHost::stateChanged, this,
            [this]() { adoptState(presenterRole, d->presenterHost->lastState()); });

    // The live terminal. The hosts only relay; everything is decided here,
    // where the role is known and the state of the talk can be checked.
    const struct { RenderHost *host; QString role; } terminalHosts[] = {
        {d->audienceHost, audienceRole}, {d->presenterHost, presenterRole}};
    for (const auto &entry : terminalHosts) {
        const QString role = entry.role;
        connect(entry.host, &RenderHost::terminalAttached, this,
                [this, role]() { terminalAttached(role); });
        connect(entry.host, &RenderHost::terminalInputReceived, this,
                [this, role](const QByteArray &bytes) { terminalInput(role, bytes); });
        connect(entry.host, &RenderHost::terminalResized, this,
                [this, role](int cols, int rows) { terminalResized(role, cols, rows); });
        connect(entry.host, &RenderHost::terminalFocusedIn, this,
                [this, role]() { terminalFocusedIn(role); });
        connect(entry.host, &RenderHost::terminalLeaveRequested, this,
                [this]() { leaveTerminal(); });
        connect(entry.host, &RenderHost::terminalFocusRequested, this,
                [this]() { focusTerminal(); });
        connect(entry.host, &RenderHost::terminalZoomRequested, this,
                [this](int direction) { terminalZoomRequested(direction); });
    }

    // Someone will plug a projector in mid-talk; that is the normal case
    // (spec §5.1), so both windows follow the outputs wherever they go.
    if (QGuiApplication *application = qGuiApp) {
        connect(application, &QGuiApplication::screenAdded, this,
                [this](QScreen *) { assignMonitors(); });
        connect(application, &QGuiApplication::screenRemoved, this,
                [this](QScreen *) { assignMonitors(); });
        connect(application, &QGuiApplication::primaryScreenChanged, this,
                [this](QScreen *) { assignMonitors(); });
    }
}

Presentation::~Presentation() {
    // The holders release here too, so an exception or a crash on the way out
    // still gives the desktop its idle timer and its notifications back.
    d->environment->stop();
    endTerminalSession();
    closeWindows();
    delete d;
}

bool Presentation::active() const { return d->active; }
int Presentation::slideIndex() const { return d->nav.slideIndex(); }
int Presentation::slideCount() const { return d->nav.slideCount(); }
int Presentation::elapsedSeconds() const { return d->elapsed; }

const DeckNavigator &Presentation::navigator() const { return d->nav; }

QUrl Presentation::rendererUrl() const {
    return QUrl(QStringLiteral("qrc:/renderer/render.html"));
}

QString Presentation::bridgeScript() const { return RenderHost::bridgeScript(); }
RenderHost *Presentation::audienceHost() const { return d->audienceHost; }
RenderHost *Presentation::presenterHost() const { return d->presenterHost; }
QVariantMap Presentation::palette() const { return d->palette; }
QVariantMap Presentation::audiencePalette() const { return d->audiencePalette; }

QJsonObject Presentation::deckForRole(const QString &role) const {
    // The next-slide preview lives in the presenter window, on the presenter's
    // screen, so it keeps the exact theme even though its page role is
    // "audience" — that role only decides whether notes are drawn.
    return role == audienceRole ? d->audienceDeck : d->deck;
}
QString Presentation::deckTitle() const { return d->deckTitle; }
QString Presentation::heading() const { return d->heading; }
QString Presentation::notesHtml() const { return d->notesHtml; }
QStringList Presentation::recallKeys() const { return d->nav.recallKeys(); }
QString Presentation::recall() const { return d->nav.recall(); }
QString Presentation::jumpBuffer() const { return d->nav.jumpBuffer(); }
QString Presentation::blank() const { return d->blank; }

QString Presentation::remoteUrl() const { return d->remoteUrl; }
bool Presentation::remoteInUse() const { return d->remoteInUse; }

void Presentation::setRemoteUrl(const QString &url) {
    if (d->remoteUrl == url)
        return;
    d->remoteUrl = url;
    emit remoteChanged();
}

void Presentation::setRemoteInUse(bool inUse) {
    if (d->remoteInUse == inUse)
        return;
    d->remoteInUse = inUse;
    emit remoteChanged();
}
bool Presentation::overview() const { return d->overview; }
bool Presentation::notesOverlay() const { return d->notesOverlay; }
bool Presentation::shortcutsVisible() const { return d->shortcuts; }
bool Presentation::singleOutput() const { return d->assignment.sharedOutput(); }

void Presentation::setQmlEngine(QQmlEngine *engine) { d->engine = engine; }

void Presentation::setWindowFactoryForTesting(WindowFactory factory) {
    d->windowFactory = std::move(factory);
}

void Presentation::setEnvironmentPreferences(bool inhibitIdle, bool doNotDisturb) {
    d->environment->setPreferences(inhibitIdle, doNotDisturb);
}

bool Presentation::inhibitIdleEnabled() const {
    return d->environment->inhibitIdleEnabled();
}

bool Presentation::doNotDisturbEnabled() const {
    return d->environment->doNotDisturbEnabled();
}

void Presentation::start(int fromSlideIndex) {
    if (d->active) {
        gotoSlide(fromSlideIndex);
        return;
    }

    d->environment->start();

    d->active = true;
    d->blank.clear();
    d->overview = false;
    d->notesOverlay = false;
    d->shortcuts = false;
    // Every presentation starts as a window. Fullscreen is a thing you ask for
    // during the talk, not a state the last talk leaves behind.
    d->audienceFullScreen = false;
    d->nav.clearJump();
    d->nav.hideRecall();
    d->nav.gotoSlide(fromSlideIndex);

    d->elapsed = 0;
    d->clock.start();

    assignMonitors();
    updateTerminalState();
    emit activeChanged();
    emit elapsedChanged();
    emit positionChanged();
}

void Presentation::stop() {
    if (!d->active)
        return;

    d->active = false;
    d->clock.stop();

    // Give the desktop back BEFORE tearing down the windows. The editor can
    // stay open after Esc, so these have to be dropped here rather than in
    // ~Presentation or they would last until quit — and they have to be
    // dropped first, because closing the last window can start application
    // teardown, and a half-run stop() that has already closed the windows but
    // not yet released these leaves the user's notifications switched off with
    // nothing left running to switch them back on.
    d->environment->stop();

    // The shell belongs to the talk. It ends here, with its jobs, rather than
    // running on behind an editor that has no way to show it.
    endTerminalSession();

    closeWindows();
    emit activeChanged();
}

void Presentation::resetTimer() {
    d->elapsed = 0;
    emit elapsedChanged();
}

void Presentation::setDeck(const QJsonObject &deckJson) {
    d->deck = deckJson;
    d->nav.setDeck(deckJson);

    // Spec §6: the audience is reading a washed-out projector across a room, so
    // its text colours get nudged until they clear the legibility floor. The
    // presenter is a foot from a laptop screen, and nudging their colours would
    // only stop the notes matching the theme they chose — so the presenter, the
    // preview and the PDF all keep the palette exactly as the theme gave it.
    const QJsonObject exact = deckJson.value(QStringLiteral("palette")).toObject();
    d->audienceDeck = deckJson;
    d->audienceDeck.insert(
        QStringLiteral("palette"),
        OmarchyTheme::paletteForRole(exact, audienceRole));

    d->palette = exact.toVariantMap();
    d->audiencePalette = d->audienceDeck.value(QStringLiteral("palette"))
                             .toObject()
                             .toVariantMap();
    d->deckTitle = deckJson.value(QStringLiteral("frontmatter"))
                       .toObject()
                       .value(QStringLiteral("title"))
                       .toString();

    // A live edit keeps both windows where they are (renderer contract §2), so
    // pages that already hold the deck get update(), not render().
    for (auto it = d->views.begin(); it != d->views.end(); ++it) {
        if (it->deckSent && it->ready) {
            emit runInView(it.key(), RenderHost::callScript(QStringLiteral("update"),
                                                           deckForRole(it.key())));
        }
    }

    emit deckChanged();
    updateTerminalState();
    emit positionChanged();
    syncViews();
}

void Presentation::next() {
    d->nav.next();
    afterNavigation();
}

void Presentation::previous() {
    d->nav.previous();
    afterNavigation();
}

void Presentation::gotoSlide(int index) {
    d->nav.gotoSlide(index);
    afterNavigation();
}

void Presentation::scrollBy(qreal deltaPixels) {
    // The renderer turns pixels into a position and reports the fraction back;
    // that report is what the audience window is then mirrored to (spec §4.7).
    emit runInView(d->masterRole(), call(QStringLiteral("scrollBy"), deltaPixels));
}

void Presentation::showRecall(const QString &key) {
    if (!d->nav.showRecall(key))
        return;
    updateTerminalState();
    for (const QString &role : {audienceRole, presenterRole})
        applyOverlays(role);
    emit positionChanged();
}

void Presentation::hideRecall() {
    if (!d->nav.hideRecall())
        return;
    // Back to exactly where the overlay went up: same slide, same fragment,
    // same scroll offset (spec §4.9).
    afterNavigation();
}

void Presentation::setBlank(const QString &mode) {
    const QString wanted = (mode == QStringLiteral("black") || mode == QStringLiteral("white"))
        ? mode
        : QString();
    if (d->blank == wanted)
        return;
    d->blank = wanted;
    applyOverlays(audienceRole);
    emit positionChanged();
}

void Presentation::setOverview(bool on) {
    if (d->overview == on)
        return;
    d->overview = on;
    updateTerminalState();
    for (const QString &role : {audienceRole, presenterRole})
        applyOverlays(role);
    emit positionChanged();
}

void Presentation::toggleNotesOverlay() {
    // Two windows already put the notes in front of the speaker; an overlay
    // then would only put them on the audience screen (spec §5.1).
    if (!singleOutput())
        return;
    d->notesOverlay = !d->notesOverlay;
    emit positionChanged();
}

QStringList Presentation::outputs() const {
    QStringList names;
    const QList<QScreen *> screens = QGuiApplication::screens();
    names.reserve(screens.size());
    for (const QScreen *screen : screens)
        names.append(screen->name());
    return names;
}

void Presentation::toggleFullscreen() {
    if (!d->audienceWindow)
        return;

    d->audienceFullScreen = d->audienceWindow->visibility() != QWindow::FullScreen;
    if (d->audienceFullScreen)
        d->audienceWindow->showFullScreen();
    else
        d->audienceWindow->showNormal();

    // The presenter's control reads audienceFullScreen to label itself.
    emit positionChanged();
}

bool Presentation::audienceFullScreen() const {
    return d->audienceFullScreen;
}

void Presentation::toggleShortcuts() {
    d->shortcuts = !d->shortcuts;
    emit positionChanged();
}

QStringList Presentation::shortcutReference() const {
    return QStringList{
        QStringLiteral("→ / Space\tNext fragment, then next slide"),
        QStringLiteral("Space\tPlay / pause the slide's first player"),
        QStringLiteral("←\tPrevious fragment, then previous slide"),
        QStringLiteral("↑ ↓ PgUp PgDn\tScroll this slide"),
        QStringLiteral("Home / End\tFirst / last slide"),
        QStringLiteral("digits then Enter\tJump to a slide number"),
        QStringLiteral("F / F11\tFullscreen the audience window"),
        QStringLiteral("B / W\tBlack / white the audience screen"),
        QStringLiteral("O\tOverview grid; arrows and Enter to pick"),
        QStringLiteral("N\tNotes overlay (single screen)"),
        QStringLiteral("Tab\tNext player on the slide"),
        QStringLiteral("bound key\tShow or hide that recall slide"),
        QStringLiteral("Shift+Esc\tLeave the terminal; Enter types in it again"),
        QStringLiteral("Esc\tClose what is open, then exit"),
        QStringLiteral("Ctrl+?\tThis list")};
}

// --- Keys (spec §5.2) -------------------------------------------------------

bool Presentation::handleKey(int key, int modifiers, const QString &text) {
    const Qt::KeyboardModifiers mods(modifiers);

    if (terminalVisible()) {
        // The one key a terminal never gets. Plain Esc belongs to whatever is
        // running in it, so leaving needs a chord nothing in a shell uses.
        if (key == Qt::Key_Escape && mods == Qt::KeyboardModifiers(Qt::ShiftModifier)) {
            leaveTerminal();
            return true;
        }
        if (d->terminalFocused) {
            // The page owns the keyboard while the shell has it, so a key that
            // lands here arrived while focus was somewhere else in the window.
            // Pass on what it typed and have the window take focus back.
            if (!text.isEmpty())
                terminalInput(d->terminalInputRole.isEmpty() ? d->masterRole()
                                                             : d->terminalInputRole,
                              text.toUtf8());
            emit terminalKeyboardWanted();
            return true;
        }
    }

    if (mods.testFlag(Qt::ControlModifier)) {
        // `Ctrl+?` is Ctrl+Shift+/ on most layouts, so accept either spelling.
        if (key == Qt::Key_Question || key == Qt::Key_Slash) {
            toggleShortcuts();
            return true;
        }
        return false;
    }
    if (mods.testFlag(Qt::AltModifier) || mods.testFlag(Qt::MetaModifier))
        return false;

    switch (key) {
    case Qt::Key_Escape:
        // Unwind whatever is on top before leaving; a presenter reaching for
        // Escape to clear an overlay must not end the talk instead.
        if (d->shortcuts) {
            toggleShortcuts();
        } else if (d->nav.jumpPending()) {
            d->nav.clearJump();
            emit positionChanged();
        } else if (!d->nav.recall().isEmpty()) {
            hideRecall();
        } else if (d->overview) {
            setOverview(false);
        } else if (!d->blank.isEmpty()) {
            setBlank(QString());
        } else {
            stop();
            emit requestExit();
        }
        return true;

    case Qt::Key_Space:
        if (!d->nav.recall().isEmpty()) {
            hideRecall();
        } else if (d->mediaActive) {
            // Space plays/pauses the first player first (spec §4.8); the
            // renderer decides which one that is.
            emit runInView(d->masterRole(), call(QStringLiteral("playPause")));
        } else {
            next();
        }
        return true;

    case Qt::Key_Right:
        if (d->overview)
            gotoSlide(d->nav.slideIndex() + 1);
        else
            next();
        return true;

    case Qt::Key_Left:
        if (d->overview)
            gotoSlide(d->nav.slideIndex() - 1);
        else
            previous();
        return true;

    case Qt::Key_Down:
        if (d->overview)
            gotoSlide(d->nav.slideIndex() + 1);
        else
            scrollBy(arrowScrollPixels);
        return true;

    case Qt::Key_Up:
        if (d->overview)
            gotoSlide(d->nav.slideIndex() - 1);
        else
            scrollBy(-arrowScrollPixels);
        return true;

    case Qt::Key_PageDown:
        scrollBy(pageScrollPixels);
        return true;

    case Qt::Key_PageUp:
        scrollBy(-pageScrollPixels);
        return true;

    case Qt::Key_Home:
        gotoSlide(0);
        return true;

    case Qt::Key_End:
        gotoSlide(d->nav.slideCount() - 1);
        return true;

    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (d->nav.jumpPending()) {
            d->nav.commitJump();
            afterNavigation();
        } else if (d->overview) {
            setOverview(false);
        } else if (terminalVisible()) {
            focusTerminal();
        }
        return true;

    case Qt::Key_Tab:
        // Spec §4.8: Tab moves focus between the players on the slide. A slide
        // with no players has nothing to cycle, and claiming the key there
        // would leave the presenter window's own controls unreachable from the
        // keyboard — so it goes back to the window's focus chain instead.
        if (d->mediaCount <= 0)
            return false;
        emit runInView(d->masterRole(), call(QStringLiteral("focusNextMedia")));
        return true;

    case Qt::Key_F:
    case Qt::Key_F11:
        toggleFullscreen();
        return true;

    case Qt::Key_B:
        setBlank(d->blank == QStringLiteral("black") ? QString() : QStringLiteral("black"));
        return true;

    case Qt::Key_W:
        setBlank(d->blank == QStringLiteral("white") ? QString() : QStringLiteral("white"));
        return true;

    case Qt::Key_O:
        setOverview(!d->overview);
        return true;

    case Qt::Key_N:
        toggleNotesOverlay();
        return true;

    default:
        break;
    }

    if (text.size() != 1)
        return false;

    if (d->nav.appendJumpDigit(text)) {
        emit positionChanged();
        return true;
    }

    // Anything left that is bound shows or hides its overlay (spec §4.9). The
    // fixed keys above win a collision: the speaker must always be able to
    // black the screen, whatever the deck bound.
    const QString binding = text.toLower();
    if (d->nav.isRecallKey(binding)) {
        if (d->nav.recall() == binding)
            hideRecall();
        else
            showRecall(binding);
        return true;
    }

    return false;
}

// --- Windows ----------------------------------------------------------------

void Presentation::viewReady(const QString &role) {
    ViewState &view = d->views[role];
    view.ready = true;
    view.deckSent = false;
    view.applied = DeckPosition{-1, 0, 0.0};
    view.blank.clear();
    view.recall.clear();
    view.overview = false;
    view.terminalFocused = false;
    view.terminalZoom = 0;
    // A page that has just loaded has no terminal yet; it attaches again when
    // it has built one, and is replayed the session so far.
    d->terminalAttached.remove(role);
    applyTo(role);
}

void Presentation::viewGone(const QString &role) {
    d->views.remove(role);
    d->terminalAttached.remove(role);
    if (d->terminalInputRole == role)
        d->terminalInputRole.clear();
}

void Presentation::adoptState(const QString &role, const QJsonObject &state) {
    ViewState &view = d->views[role];
    const int slideIndex = state.value(QStringLiteral("slideIndex")).toInt();
    view.applied.slideIndex = slideIndex;
    view.applied.fragment = state.value(QStringLiteral("fragment")).toInt();
    const qreal sentScroll = view.applied.scrollFraction;
    const bool sameSlide = view.applied.slideIndex == d->nav.slideIndex();
    view.applied.scrollFraction = state.value(QStringLiteral("scrollFraction")).toDouble();

    if (role != d->masterRole()) {
        // A follower's page scrolls natively under the pointer too. That is a
        // scroll like any other, so it becomes the talk's position; otherwise
        // the next reveal would send this window back to where the master is.
        // A follower landing a pixel off where it was sent is only an echo.
        constexpr qreal echo = 0.002;
        if (role != previewRole && sameSlide
            && qAbs(view.applied.scrollFraction - sentScroll) > echo) {
            d->nav.setScrollFraction(view.applied.scrollFraction);
            for (const QString &other : {audienceRole, presenterRole}) {
                if (other != role)
                    applyTo(other);
            }
            emit positionChanged();
        }
        return;
    }

    // The renderer is the only thing that knows how many fragments a slide has
    // and where a scroll landed. Everything else here is chrome for the
    // presenter window.
    d->nav.noteFragmentCount(slideIndex, state.value(QStringLiteral("fragmentCount")).toInt());
    d->nav.setScrollFraction(view.applied.scrollFraction);
    d->heading = state.value(QStringLiteral("heading")).toString();
    d->notesHtml = state.value(QStringLiteral("notesHtml")).toString();
    const int mediaCount = state.value(QStringLiteral("mediaCount")).toInt();
    d->mediaCount = mediaCount;
    d->mediaActive = mediaCount > 0
        && state.value(QStringLiteral("mediaActive")).toBool(true);

    // Only the followers are driven from here. Pushing the master back to a
    // position it just reported would be a loop.
    for (const QString &follower : {audienceRole, presenterRole, previewRole}) {
        if (follower != role)
            applyTo(follower);
    }
    emit positionChanged();
}

void Presentation::afterNavigation() {
    updateTerminalState();
    syncViews();
    emit positionChanged();
}

void Presentation::syncViews() {
    for (const QString &role : {audienceRole, presenterRole, previewRole})
        applyTo(role);
}

void Presentation::applyTo(const QString &role) {
    auto it = d->views.find(role);
    if (it == d->views.end() || !it->ready)
        return;
    ViewState &view = *it;

    QStringList script;
    if (!view.deckSent) {
        script << RenderHost::callScript(QStringLiteral("render"), deckForRole(role));
        script << QStringLiteral("window.omapresent && (window.omapresent.role = %1);")
                      .arg(role == previewRole ? QStringLiteral("'audience'")
                                               : QStringLiteral("'%1'").arg(role));
        view.deckSent = true;
        view.applied = DeckPosition{-1, 0, 0.0};
    }

    // The preview is one slide ahead and nothing else: no fragments, no scroll,
    // no overlays (spec §5.1).
    const DeckPosition target = role == previewRole
        ? DeckPosition{qMin(d->nav.slideIndex() + 1, qMax(0, d->nav.slideCount() - 1)), 0, 0.0}
        : d->nav.position();

    if (view.applied.slideIndex != target.slideIndex
        || view.applied.fragment > target.fragment) {
        script << call(QStringLiteral("goto"), target.slideIndex);
        view.applied = DeckPosition{target.slideIndex, 0, 0.0};
    }
    for (int fragment = view.applied.fragment; fragment < target.fragment; ++fragment)
        script << call(QStringLiteral("next"));
    view.applied.fragment = target.fragment;

    if (!qFuzzyCompare(view.applied.scrollFraction + 1.0, target.scrollFraction + 1.0)) {
        script << call(QStringLiteral("setScroll"), target.scrollFraction);
        view.applied.scrollFraction = target.scrollFraction;
    }

    if (role != previewRole)
        script << overlayScript(role);

    script.removeAll(QString());
    if (!script.isEmpty())
        emit runInView(role, script.join(QLatin1Char('\n')));
}

void Presentation::applyOverlays(const QString &role) {
    const QString script = overlayScript(role);
    if (!script.isEmpty())
        emit runInView(role, script);
}

QString Presentation::overlayScript(const QString &role) {
    auto it = d->views.find(role);
    if (it == d->views.end() || !it->ready)
        return {};
    ViewState &view = *it;
    QStringList script;

    // `B` and `W` black or white the *audience* screen; the presenter keeps its
    // slide and shows an indicator instead (spec §5.2).
    const QString blank = role == audienceRole ? d->blank : QString();
    if (view.blank != blank) {
        view.blank = blank;
        script << call(QStringLiteral("setBlank"), blank);
    }

    if (view.overview != d->overview) {
        view.overview = d->overview;
        script << QStringLiteral("window.omapresent && window.omapresent.setOverview(%1);")
                      .arg(d->overview ? QStringLiteral("true") : QStringLiteral("false"));
    }

    const QString recall = d->nav.recall();
    if (view.recall != recall) {
        view.recall = recall;
        script << (recall.isEmpty() ? call(QStringLiteral("hideRecall"))
                                    : call(QStringLiteral("showRecall"), recall));
    }

    // After the recall, so a terminal popped by its key exists in the page
    // before it is told it has the keyboard.
    if (view.terminalZoom != d->terminalZoom) {
        view.terminalZoom = d->terminalZoom;
        script << call(QStringLiteral("terminalSetZoom"), d->terminalZoom);
    }
    if (view.terminalFocused != d->terminalFocused) {
        view.terminalFocused = d->terminalFocused;
        script << QStringLiteral("window.omapresent && window.omapresent.terminalSetFocus(%1);")
                      .arg(d->terminalFocused ? QStringLiteral("true")
                                              : QStringLiteral("false"));
    }

    return script.join(QLatin1Char('\n'));
}

QVector<PresentationOutput> Presentation::currentOutputs() const {
    QVector<PresentationOutput> outputs;
    const QList<QScreen *> screens = QGuiApplication::screens();
    const QScreen *primary = QGuiApplication::primaryScreen();
    outputs.reserve(screens.size());
    for (const QScreen *screen : screens) {
        outputs.append(PresentationOutput{screen->name(), screen == primary,
                                          Platform::isBuiltInScreen(screen)});
    }
    return outputs;
}

void Presentation::assignMonitors() {
    const QList<QScreen *> screens = QGuiApplication::screens();
    const MonitorAssignment assignment = assignOutputs(currentOutputs());
    d->assignment = assignment;

    if (!d->active || assignment.isEmpty())
        return;

    if (!d->audienceWindow)
        d->audienceWindow = createWindow(QStringLiteral("qrc:/AudienceWindow.qml"));
    placeWindow(d->audienceWindow, screens.value(assignment.audience),
                d->audienceFullScreen);

    if (assignment.sharedOutput()) {
        // One screen: the audience window is the whole presentation and `N`
        // brings the notes up over it (spec §5.1).
        if (d->presenterWindow) {
            d->views.remove(presenterRole);
            d->views.remove(previewRole);
            d->presenterWindow->close();
            delete d->presenterWindow;
        }
    } else {
        if (!d->presenterWindow)
            d->presenterWindow = createWindow(QStringLiteral("qrc:/PresenterWindow.qml"));
        // The presenter window is chrome, and chrome is never fullscreen.
        placeWindow(d->presenterWindow, screens.value(assignment.presenter), false);
        d->notesOverlay = false;
    }

    emit activeChanged();
    emit positionChanged();
}

QQuickWindow *Presentation::createWindow(const QString &source) {
    if (d->windowFactory) {
        QQuickWindow *window = d->windowFactory(source);
        if (!window)
            return nullptr;
        window->setParent(static_cast<QWindow *>(nullptr));
        window->setTransientParent(nullptr);
        window->setModality(Qt::NonModal);
        window->setFlags(Qt::Window);
        return window;
    }

    QQmlEngine *engine = d->engine;
    if (!engine) {
        // Present mode can be asked for before anyone has handed us the
        // application's engine; build one rather than refuse to present.
        if (!d->ownEngine)
            d->ownEngine.reset(new QQmlEngine);
        engine = d->ownEngine.get();
    }

    QQmlComponent component(engine, QUrl(source));
    if (component.isError()) {
        for (const QQmlError &error : component.errors())
            qWarning().noquote() << "omapresent: present mode:" << error.toString();
        return nullptr;
    }

    QQmlContext *context = new QQmlContext(engine->rootContext(), this);
    context->setContextProperty(QStringLiteral("presentation"), this);
    QObject *object = component.create(context);
    if (!object) {
        for (const QQmlError &error : component.errors())
            qWarning().noquote() << "omapresent: present mode:" << error.toString();
        delete context;
        return nullptr;
    }

    QQuickWindow *window = qobject_cast<QQuickWindow *>(object);
    if (!window) {
        qWarning() << "omapresent:" << source << "is not a Window";
        delete object;
        delete context;
        return nullptr;
    }
    context->setParent(window);
    // QML Window defaults happen to describe this today. Make the desktop
    // contract explicit before placeWindow creates the native Wayland surface:
    // present windows are peers of the editor, never a dialog or transient.
    window->setParent(static_cast<QWindow *>(nullptr));
    window->setTransientParent(nullptr);
    window->setModality(Qt::NonModal);
    window->setFlags(Qt::Window);
    QQmlEngine::setObjectOwnership(window, QQmlEngine::CppOwnership);
    return window;
}

void Presentation::placeWindow(QQuickWindow *window, QScreen *screen, bool fullScreen) {
    if (!window || !screen)
        return;

    // A fullscreen window will not move between outputs while it is fullscreen,
    // so drop out, move, and go back — which is also what makes a projector
    // plugged in mid-talk land on the right screen.
    const bool changesOutput = window->screen() != screen;
    if (changesOutput) {
        window->setVisibility(QWindow::Windowed);
        window->setScreen(screen);
    }
    // A window that is not moving keeps the place it has. The exception is a
    // desktop that does not tile, the first time a window is shown there: the
    // compositor will not size it, so an audience window that happens to start
    // on its own output would open at Qt's default of a few pixels square.
    if (changesOutput || (!window->isVisible() && Platform::windowsNeedGeometry()))
        window->setGeometry(Platform::windowedGeometry(screen));

    // Spec §5.1: this is an ordinary top-level window that happens to be
    // showing a deck. It opens windowed so it can be tiled, moved, resized and
    // shared like anything else on the desktop; `F` / `F11` is how it fills the
    // output. On Wayland the geometry above is a request — a tiling compositor
    // will place it, which is the point.
    if (fullScreen)
        window->showFullScreen();
    else
        window->showNormal();
    window->requestActivate();
}

void Presentation::closeWindows() {
    d->views.clear();

    // deleteLater, never delete. This is reached from handleKey, which QML
    // invokes on these very windows, so destroying them here would tear down
    // an object the QML engine is still executing a method on — Qt calls that
    // fatal and aborts. Clearing the pointers first means anything that runs
    // between now and the deletion sees no windows rather than dangling ones.
    if (QObject *audience = d->audienceWindow) {
        d->audienceWindow = nullptr;
        QMetaObject::invokeMethod(audience, "close");
        audience->deleteLater();
    }
    if (QObject *presenter = d->presenterWindow) {
        d->presenterWindow = nullptr;
        QMetaObject::invokeMethod(presenter, "close");
        presenter->deleteLater();
    }

}

void Presentation::inhibitIdle(bool on) {
    if (on) {
        if (!d->idle)
            d->idle.reset(new IdleInhibit);
    } else {
        d->idle.reset();
    }
}

void Presentation::setDoNotDisturb(bool on) {
    if (on) {
        if (!d->doNotDisturb)
            d->doNotDisturb.reset(new DoNotDisturbHold);
    } else {
        d->doNotDisturb.reset();
    }
}

// --- The live terminal ------------------------------------------------------

bool Presentation::terminalVisible() const {
    return d->active && !d->overview && d->nav.showingTerminal();
}

bool Presentation::terminalFocused() const { return d->terminalFocused; }

TerminalSession *Presentation::terminalSession() const { return d->terminal.get(); }

void Presentation::setTerminalDirectory(const QString &directory) {
    d->terminalDirectory = directory;
}

void Presentation::updateTerminalState() {
    QString showing;
    if (terminalVisible()) {
        showing = d->nav.recall().isEmpty()
            ? QStringLiteral("slide:%1").arg(d->nav.slideIndex())
            : QStringLiteral("recall:%1").arg(d->nav.recall());
    }

    // Arriving on a terminal is the speaker saying "I am going to type now", so
    // it takes the keyboard. Anything that takes the terminal off screen gives
    // the keyboard back, so no key press is ever swallowed by a shell nobody
    // can see.
    if (showing.isEmpty())
        d->terminalFocused = false;
    else if (showing != d->terminalShowing)
        d->terminalFocused = true;
    d->terminalShowing = showing;
}

void Presentation::focusTerminal() {
    if (!terminalVisible() || d->terminalFocused)
        return;
    d->terminalFocused = true;
    for (const QString &role : {audienceRole, presenterRole})
        applyOverlays(role);
    emit positionChanged();
}

void Presentation::leaveTerminal() {
    if (!terminalVisible())
        return;

    // A terminal popped by its recall key is left the way every recall slide
    // is: it closes, and the talk is back exactly where it was.
    if (!d->nav.recall().isEmpty())
        hideRecall();

    // A terminal slide in the flow stays up, with the arrows driving the talk
    // again. (And if the slide under a closed recall is itself a terminal, it
    // does not grab the keyboard the speaker just asked to have back.)
    d->terminalFocused = false;
    for (const QString &role : {audienceRole, presenterRole})
        applyOverlays(role);
    emit positionChanged();
}

void Presentation::startTerminalSession() {
    if (!d->terminal) {
        d->terminal = std::make_unique<TerminalSession>();
        connect(d->terminal.get(), &TerminalSession::output, this,
                [this](const QByteArray &bytes) { sendToTerminalViews(bytes); });
        connect(d->terminal.get(), &TerminalSession::exited, this, [this](int exitCode) {
            const QString message = exitCode < 0 && d->terminal
                ? d->terminal->lastError()
                : QStringLiteral("[shell exited. Press Enter to start a new one.]");
            sendToTerminalViews(QStringLiteral("\r\n%1\r\n").arg(message).toUtf8());
        });
    }

    if (!d->terminal->start(d->terminalDirectory,
                            d->terminalCols > 0 ? d->terminalCols : 80,
                            d->terminalRows > 0 ? d->terminalRows : 24)) {
        sendToTerminalViews(
            QStringLiteral("\r\n%1\r\n").arg(d->terminal->lastError()).toUtf8());
    }
}

void Presentation::endTerminalSession() {
    d->terminal.reset();
    d->terminalAttached.clear();
    d->terminalInputRole.clear();
    d->terminalShowing.clear();
    d->terminalFocused = false;
    d->terminalCols = 0;
    d->terminalRows = 0;
    d->terminalZoom = 0;
}

void Presentation::sendToTerminalViews(const QByteArray &bytes) {
    if (bytes.isEmpty() || d->terminalAttached.isEmpty())
        return;
    const QString script = terminalWriteScript(bytes);
    for (const QString &role : std::as_const(d->terminalAttached))
        emit runInView(role, script);
}

void Presentation::terminalAttached(const QString &role) {
    const auto view = d->views.constFind(role);
    if (!d->active || view == d->views.constEnd() || !view->ready)
        return;
    d->terminalAttached.insert(role);

    // The audience decides the grid; everyone else draws that grid at whatever
    // type size fits their own pane.
    if (role != audienceRole && d->terminalCols > 0) {
        emit runInView(role,
                       QStringLiteral("window.omapresent && window.omapresent.terminalSetSize(%1, %2);")
                           .arg(d->terminalCols)
                           .arg(d->terminalRows));
    }

    // A page that joins a session already under way is shown what the others
    // have: the output so far, replayed into its own emulator.
    if (!d->terminal)
        return;
    const QByteArray replay = d->terminal->scrollback();
    if (!replay.isEmpty())
        emit runInView(role, terminalWriteScript(replay));
    if (d->terminal->scrollbackTruncated())
        d->terminal->nudge();
}

void Presentation::terminalResized(const QString &role, int cols, int rows) {
    // The audience screen is the one being read, so its grid is the grid.
    if (!d->active || role != audienceRole || cols < 2 || rows < 1)
        return;
    d->terminalCols = cols;
    d->terminalRows = rows;

    if (d->terminal && d->terminal->running())
        d->terminal->resize(cols, rows);
    else if (!d->terminal && terminalVisible())
        startTerminalSession();  // the first time a terminal is shown

    if (d->terminalAttached.contains(presenterRole)) {
        emit runInView(presenterRole,
                       QStringLiteral("window.omapresent && window.omapresent.terminalSetSize(%1, %2);")
                           .arg(cols)
                           .arg(rows));
    }
}

void Presentation::terminalInput(const QString &role, const QByteArray &bytes) {
    // Nothing reaches the shell unless a terminal is on screen and has been
    // given the keyboard. A page cannot type into a shell behind the speaker's
    // back, and a shell that is not showing never receives a stray key.
    if (bytes.isEmpty() || !terminalVisible() || !d->terminalFocused)
        return;
    const QString owner = d->terminalInputRole.isEmpty() ? d->masterRole()
                                                         : d->terminalInputRole;
    if (role != owner)
        return;

    if (!d->terminal || !d->terminal->running()) {
        // The shell ended (`exit`, Ctrl+D) or never started. Enter asks for
        // another; no other key does, so an exit is never undone by accident.
        if (bytes == "\r" || bytes == "\n")
            startTerminalSession();
        return;
    }
    d->terminal->write(bytes);
}

void Presentation::terminalFocusedIn(const QString &role) {
    if (d->views.contains(role))
        d->terminalInputRole = role;
}

void Presentation::terminalZoomRequested(int direction) {
    if (!terminalVisible())
        return;
    const int zoom = direction == 0
        ? 0
        : qBound(minTerminalZoom, d->terminalZoom + (direction > 0 ? 1 : -1), maxTerminalZoom);
    if (zoom == d->terminalZoom)
        return;
    d->terminalZoom = zoom;
    for (const QString &role : {audienceRole, presenterRole})
        applyOverlays(role);
}
