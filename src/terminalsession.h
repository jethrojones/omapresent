#pragma once

// TerminalSession: the shell behind a `--- {terminal}` slide.
//
// One real pseudo-terminal with the user's own shell on the far end, so
// anything that runs in a terminal runs here: a REPL, vim, a coding agent's
// TUI. This class owns the PTY and the process and nothing else. It does not
// draw: the bytes it emits are fed to xterm.js in the renderer page
// (src/terminal/terminal.js), and what the speaker types comes back through
// write(). Presentation decides when a session exists and who may type into it.
//
// POSIX only, which covers both platforms Omapresent builds for.

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QStringList>

class QProcess;
class QSocketNotifier;

class TerminalSession : public QObject {
    Q_OBJECT

public:
    // What a view that attaches late is replayed, so it shows the same screen
    // as the one that was there from the start.
    static constexpr int scrollbackLimit = 512 * 1024;

    struct Shell {
        QString program;
        QStringList arguments;
    };

    explicit TerminalSession(QObject *parent = nullptr);
    ~TerminalSession() override;

    // Starts the user's shell in `workingDirectory` on a PTY of the given
    // size. A running session is left alone. False when no PTY could be
    // opened; lastError() then says why.
    bool start(const QString &workingDirectory, int cols, int rows);
    // Hangs the session up and reaps the shell. Safe to call twice.
    void stop();
    bool running() const;

    // Bytes for the shell's stdin, exactly as a terminal would send them.
    void write(const QByteArray &bytes);
    void resize(int cols, int rows);
    // Makes full-screen programs repaint, by changing the size and putting it
    // back. Used after a replay that had to drop the start of the scrollback.
    void nudge();

    int cols() const { return m_cols; }
    int rows() const { return m_rows; }
    QByteArray scrollback() const { return m_scrollback; }
    bool scrollbackTruncated() const { return m_scrollbackTruncated; }
    QString lastError() const { return m_error; }

    // --- Pure helpers, directly unit-tested -------------------------------
    // The shell to run. `shellFromEnvironment` is $SHELL; anything that is not
    // an executable file falls back to /bin/sh. A login shell gets `-l`.
    static Shell shellFor(const QString &shellFromEnvironment, bool loginShell);
    // The child's environment: the inherited one with the terminal's own
    // identity (TERM, COLORTERM, TERM_PROGRAM) replaced, the stale size
    // variables dropped, and a UTF-8 LANG supplied when no locale is set at
    // all, which is how a Mac app launched from Finder starts.
    static QStringList environmentFor(const QStringList &inherited, const QString &localeName);
    // Appends `chunk` and keeps at most `limit` bytes, cutting at a line break
    // when one is near so a replay does not begin mid-line.
    static void appendScrollback(QByteArray *buffer, const QByteArray &chunk, int limit,
                                 bool *truncated);

signals:
    // Raw bytes from the PTY. Not text: a multi-byte character may be split
    // across two emissions, so hand them on as bytes.
    void output(const QByteArray &bytes);
    // The shell ended, by itself or because it could not be started (-1).
    void exited(int exitCode);

private:
    void readMaster();
    void flushPending();
    void closeMaster();
    void closeParentSlave();
    void applyWindowSize();

    QProcess *m_process = nullptr;
    QSocketNotifier *m_readNotifier = nullptr;
    QSocketNotifier *m_writeNotifier = nullptr;
    int m_master = -1;
    // Held from start() until the child has opened its own, so the PTY always
    // has a slave open and the master never reports a hangup before the shell
    // exists.
    int m_parentSlave = -1;
    int m_cols = 80;
    int m_rows = 24;
    QByteArray m_pending;
    QByteArray m_scrollback;
    bool m_scrollbackTruncated = false;
    QString m_error;
};
