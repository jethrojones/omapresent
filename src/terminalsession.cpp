#include "terminalsession.h"

#include <QDir>
#include <QFileInfo>
#include <QLocale>
#include <QProcess>
#include <QSocketNotifier>

#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>

#include <fcntl.h>
#include <pwd.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include "platform.h"

namespace {

// Typing never comes near this. A paste can, and a PTY takes input only as fast
// as the program on the other side reads it.
constexpr qsizetype maxPendingInput = 4 * 1024 * 1024;
constexpr qsizetype maxReadPerWake = 256 * 1024;

QString loginShellFromPasswd() {
    const struct passwd *entry = ::getpwuid(::getuid());
    return entry && entry->pw_shell ? QString::fromLocal8Bit(entry->pw_shell) : QString();
}

QString variableName(const QString &assignment) {
    const int equals = assignment.indexOf(QLatin1Char('='));
    return equals < 0 ? assignment : assignment.left(equals);
}

}  // namespace

TerminalSession::TerminalSession(QObject *parent) : QObject(parent) {}

TerminalSession::~TerminalSession() {
    stop();
}

TerminalSession::Shell TerminalSession::shellFor(const QString &shellFromEnvironment,
                                                 bool loginShell) {
    Shell shell;
    const QFileInfo candidate(shellFromEnvironment);
    shell.program = !shellFromEnvironment.isEmpty() && candidate.isAbsolute()
            && candidate.isFile() && candidate.isExecutable()
        ? shellFromEnvironment
        : QStringLiteral("/bin/sh");
    if (loginShell)
        shell.arguments.append(QStringLiteral("-l"));
    return shell;
}

QStringList TerminalSession::environmentFor(const QStringList &inherited,
                                            const QString &localeName) {
    static const QStringList replaced{
        QStringLiteral("TERM"), QStringLiteral("COLORTERM"), QStringLiteral("TERM_PROGRAM"),
        QStringLiteral("TERM_PROGRAM_VERSION"), QStringLiteral("COLUMNS"),
        QStringLiteral("LINES")};

    QStringList environment;
    bool hasLocale = false;
    for (const QString &assignment : inherited) {
        const QString name = variableName(assignment);
        if (replaced.contains(name))
            continue;
        if ((name == QStringLiteral("LANG") || name == QStringLiteral("LC_ALL")
             || name == QStringLiteral("LC_CTYPE"))
            && assignment.size() > name.size() + 1) {
            hasLocale = true;
        }
        environment.append(assignment);
    }

    environment.append(QStringLiteral("TERM=xterm-256color"));
    environment.append(QStringLiteral("COLORTERM=truecolor"));
    environment.append(QStringLiteral("TERM_PROGRAM=omapresent"));
    if (!hasLocale) {
        // Without one the shell runs in the C locale and every non-ASCII
        // character the speaker types comes back as an escape code.
        const bool usable = localeName.contains(QLatin1Char('_'));
        environment.append(QStringLiteral("LANG=%1.UTF-8")
                               .arg(usable ? localeName : QStringLiteral("en_US")));
    }
    return environment;
}

void TerminalSession::appendScrollback(QByteArray *buffer, const QByteArray &chunk, int limit,
                                       bool *truncated) {
    if (!buffer)
        return;
    buffer->append(chunk);
    if (limit <= 0 || buffer->size() <= limit)
        return;

    qsizetype cut = buffer->size() - limit;
    // Prefer to start the kept part on a fresh line; give up after a screenful
    // rather than throw away much more than was asked.
    const qsizetype lineBreak = buffer->indexOf('\n', cut);
    if (lineBreak >= 0 && lineBreak - cut < 4096)
        cut = lineBreak + 1;
    buffer->remove(0, cut);
    if (truncated)
        *truncated = true;
}

bool TerminalSession::running() const {
    return m_process && m_process->state() != QProcess::NotRunning;
}

bool TerminalSession::start(const QString &workingDirectory, int cols, int rows) {
    if (running())
        return true;
    stop();
    m_error.clear();

    m_cols = qBound(2, cols, 500);
    m_rows = qBound(1, rows, 300);

    m_master = ::posix_openpt(O_RDWR | O_NOCTTY);
    if (m_master < 0 || ::grantpt(m_master) != 0 || ::unlockpt(m_master) != 0) {
        m_error = QStringLiteral("Could not open a pseudo-terminal: %1")
                      .arg(QString::fromLocal8Bit(std::strerror(errno)));
        closeMaster();
        return false;
    }
    const char *name = ::ptsname(m_master);
    if (!name) {
        m_error = QStringLiteral("Could not name the pseudo-terminal.");
        closeMaster();
        return false;
    }
    const QByteArray slaveName(name);

    ::fcntl(m_master, F_SETFD, ::fcntl(m_master, F_GETFD) | FD_CLOEXEC);
    ::fcntl(m_master, F_SETFL, ::fcntl(m_master, F_GETFL) | O_NONBLOCK);

    // Our own slave descriptor, close-on-exec so neither the shell nor a web
    // engine helper inherits it. See m_parentSlave.
    m_parentSlave = ::open(slaveName.constData(), O_RDWR | O_NOCTTY | O_CLOEXEC);
    if (m_parentSlave < 0) {
        m_error = QStringLiteral("Could not open the pseudo-terminal: %1")
                      .arg(QString::fromLocal8Bit(std::strerror(errno)));
        closeMaster();
        return false;
    }
    applyWindowSize();

    QString shellPath = qEnvironmentVariable("SHELL");
    if (shellPath.isEmpty())
        shellPath = loginShellFromPasswd();
    const Shell shell = shellFor(shellPath, Platform::terminalUsesLoginShell());

    const QFileInfo directory(workingDirectory);
    const QString startIn = !workingDirectory.isEmpty() && directory.isDir()
            && directory.isWritable()
        ? directory.absoluteFilePath()
        : QDir::homePath();

    m_process = new QProcess(this);
    m_process->setProgram(shell.program);
    m_process->setArguments(shell.arguments);
    m_process->setWorkingDirectory(startIn);
    m_process->setEnvironment(
        environmentFor(QProcess::systemEnvironment(), QLocale::system().name()));
    // No pipes: the child's stdio becomes the PTY in the modifier below.
    m_process->setStandardInputFile(QProcess::nullDevice());
    m_process->setStandardOutputFile(QProcess::nullDevice());
    m_process->setStandardErrorFile(QProcess::nullDevice());

    // The host process ignores SIGPIPE and holds a great many descriptors; a
    // shell must start with neither.
    QProcess::UnixProcessParameters parameters;
    parameters.flags = QProcess::UnixProcessFlag::ResetSignalHandlers
        | QProcess::UnixProcessFlag::CloseFileDescriptors;
    m_process->setUnixProcessParameters(parameters);

    // Runs in the child between fork and exec, so only async-signal-safe calls:
    // become a session leader, take the PTY as the controlling terminal, and
    // make it stdin, stdout and stderr.
    m_process->setChildProcessModifier([slaveName]() {
        ::setsid();
        const int slave = ::open(slaveName.constData(), O_RDWR);
        if (slave < 0)
            ::_exit(127);
#ifdef TIOCSCTTY
        ::ioctl(slave, TIOCSCTTY, 0);
#endif
#ifdef IUTF8
        struct termios settings;
        if (::tcgetattr(slave, &settings) == 0) {
            // So the line discipline erases a whole character on Backspace.
            settings.c_iflag |= IUTF8;
            ::tcsetattr(slave, TCSANOW, &settings);
        }
#endif
        ::dup2(slave, STDIN_FILENO);
        ::dup2(slave, STDOUT_FILENO);
        ::dup2(slave, STDERR_FILENO);
        if (slave > STDERR_FILENO)
            ::close(slave);
    });

    connect(m_process, &QProcess::started, this, [this]() { closeParentSlave(); });
    connect(m_process, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus) {
        // Whatever the shell printed last is still in the PTY.
        readMaster();
        closeMaster();
        emit exited(exitCode);
    });
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        m_error = QStringLiteral("Could not start %1: %2")
                      .arg(m_process->program(), m_process->errorString());
        closeMaster();
        emit exited(-1);
    });

    m_readNotifier = new QSocketNotifier(m_master, QSocketNotifier::Read, this);
    connect(m_readNotifier, &QSocketNotifier::activated, this, [this]() { readMaster(); });
    m_writeNotifier = new QSocketNotifier(m_master, QSocketNotifier::Write, this);
    m_writeNotifier->setEnabled(false);
    connect(m_writeNotifier, &QSocketNotifier::activated, this, [this]() { flushPending(); });

    m_process->start();
    return true;
}

void TerminalSession::stop() {
    QProcess *process = m_process;
    m_process = nullptr;
    if (process) {
        process->disconnect(this);
        const qint64 pid = process->processId();
        // Closing the master hangs up the session: the kernel sends SIGHUP to
        // whatever is in the foreground, and the shell passes it to its jobs.
        closeMaster();
        if (process->state() != QProcess::NotRunning) {
            if (pid > 0)
                ::kill(pid_t(pid), SIGHUP);
            if (!process->waitForFinished(400)) {
                process->kill();
                process->waitForFinished(400);
            }
        }
        delete process;
    }
    closeMaster();
    m_pending.clear();
}

void TerminalSession::write(const QByteArray &bytes) {
    if (m_master < 0 || bytes.isEmpty())
        return;
    if (m_pending.size() + bytes.size() > maxPendingInput)
        return;
    m_pending.append(bytes);
    flushPending();
}

void TerminalSession::flushPending() {
    while (m_master >= 0 && !m_pending.isEmpty()) {
        const ssize_t written = ::write(m_master, m_pending.constData(),
                                        size_t(m_pending.size()));
        if (written > 0) {
            m_pending.remove(0, written);
            continue;
        }
        if (written < 0 && errno == EINTR)
            continue;
        if (written < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            // The program is not reading yet. Wait for room rather than block
            // the interface on it.
            if (m_writeNotifier)
                m_writeNotifier->setEnabled(true);
            return;
        }
        m_pending.clear();  // nobody is listening any more
    }
    if (m_writeNotifier)
        m_writeNotifier->setEnabled(false);
}

void TerminalSession::readMaster() {
    if (m_master < 0)
        return;

    QByteArray chunk;
    bool hungUp = false;
    char buffer[65536];
    while (chunk.size() < maxReadPerWake) {
        const ssize_t count = ::read(m_master, buffer, sizeof(buffer));
        if (count > 0) {
            chunk.append(buffer, count);
            continue;
        }
        if (count < 0 && errno == EINTR)
            continue;
        if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            break;
        // End of file on macOS, EIO on Linux: the last slave descriptor closed.
        hungUp = true;
        break;
    }

    if (hungUp && m_readNotifier) {
        // QProcess::finished reports the exit; stop the notifier spinning on a
        // descriptor that will never have more to say.
        m_readNotifier->setEnabled(false);
    }
    if (chunk.isEmpty())
        return;
    appendScrollback(&m_scrollback, chunk, scrollbackLimit, &m_scrollbackTruncated);
    emit output(chunk);
}

void TerminalSession::resize(int cols, int rows) {
    const int boundedCols = qBound(2, cols, 500);
    const int boundedRows = qBound(1, rows, 300);
    if (boundedCols == m_cols && boundedRows == m_rows)
        return;
    m_cols = boundedCols;
    m_rows = boundedRows;
    applyWindowSize();
}

void TerminalSession::nudge() {
    if (m_master < 0 || m_rows < 2)
        return;
    const int rows = m_rows;
    m_rows = rows - 1;
    applyWindowSize();
    m_rows = rows;
    applyWindowSize();
}

void TerminalSession::applyWindowSize() {
    if (m_master < 0)
        return;
    struct winsize size {};
    size.ws_col = static_cast<unsigned short>(m_cols);
    size.ws_row = static_cast<unsigned short>(m_rows);
    ::ioctl(m_master, TIOCSWINSZ, &size);
}

void TerminalSession::closeParentSlave() {
    if (m_parentSlave >= 0) {
        ::close(m_parentSlave);
        m_parentSlave = -1;
    }
}

void TerminalSession::closeMaster() {
    // deleteLater: this can be reached from a slot of the notifier itself.
    if (m_readNotifier) {
        m_readNotifier->setEnabled(false);
        m_readNotifier->deleteLater();
        m_readNotifier = nullptr;
    }
    if (m_writeNotifier) {
        m_writeNotifier->setEnabled(false);
        m_writeNotifier->deleteLater();
        m_writeNotifier = nullptr;
    }
    closeParentSlave();
    if (m_master >= 0) {
        ::close(m_master);
        m_master = -1;
    }
}
