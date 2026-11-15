#pragma once
// Native ViPER lifecycle controller. No GUI widgets or PipeWire daemon control.
// The only process eligible for replacement is this user's marked filter-chain
// client. The Audio Output monitor remains a separately supervised Python worker.
#include "viper-backend-state.h"
#include "gui-backend-exit.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QStringList>
#include <QThread>
#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <fcntl.h>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace viper_lifecycle {
struct Failure : std::runtime_error {
    using std::runtime_error::runtime_error;
};
struct Paths {
    bool system = false;
    QString root, ctl, dsp, routing, gui, backendStart, monitor, retention;
    static Paths forExecutable(const QString& argv0) {
        QString absolute = argv0;
        if (!QFileInfo(absolute).isAbsolute()) {
            const QString found = QStandardPaths::findExecutable(absolute);
            if (!found.isEmpty()) absolute = found;
        }
        const QFileInfo executable(QFileInfo(absolute).canonicalFilePath());
        Paths p;
        p.system = executable.absoluteFilePath() == "/usr/bin/viper4linux-ctl";
        if (p.system) {
            p.root = "/usr";
            p.ctl = "/usr/bin/viper4linux-ctl";
            p.dsp = "/usr/lib/ladspa/viper_dsp_ladspa.so";
            p.routing = "/usr/share/viper4linux/routing-config/filter-chain.conf.d";
            p.gui = "/usr/bin/viper4linux-gui";
            p.backendStart = "/usr/lib/viper4linux/start-backend";
            p.monitor = "/usr/lib/viper4linux/viper_output_monitor.py";
            p.retention = "/usr/lib/viper4linux/viper_log_retention.py";
        } else {
#ifdef VIPER4LINUX_SOURCE_ROOT
            p.root = QString::fromUtf8(VIPER4LINUX_SOURCE_ROOT);
            const QString build = QString::fromUtf8(VIPER4LINUX_SOURCE_BUILD_DIR);
#else
            p.root = QDir(executable.absolutePath()).absoluteFilePath("..");
            p.root = QDir(p.root).canonicalPath();
            const QString build = QDir(p.root).filePath("build");
#endif
            p.ctl = QDir(build).filePath("viper-ctl");
            p.dsp = QDir(build).filePath("viper_dsp_ladspa.so");
            p.routing = QDir(p.root).filePath("routing-config");
            if (!QFileInfo::exists(QDir(p.routing).filePath("filter-chain.conf")))
                p.routing = QDir(build).filePath("routing-config");
            p.gui = QDir(build).filePath("viper-gui");
            p.backendStart = QDir(p.root).filePath("scripts/start.sh");
            p.monitor = QDir(p.root).filePath("src/viper_output_monitor.py");
            p.retention = QDir(p.root).filePath("src/viper_log_retention.py");
        }
        return p;
    }
};
inline QString runtime() {
    const QByteArray configured = qgetenv("XDG_RUNTIME_DIR");
    return configured.isEmpty() ? "/run/user/" + QString::number(getuid())
                                : QString::fromLocal8Bit(configured);
}
inline QString stateDir() {
    const QByteArray configured = qgetenv("XDG_STATE_HOME");
    return (configured.isEmpty() ? QDir::homePath() + "/.local/state"
                                 : QString::fromLocal8Bit(configured)) + "/viper4linux";
}
inline QByteArray readFile(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    return file.readAll();
}
inline QString shaFile(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) throw Failure(("Cannot read: " + path).toStdString());
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd()) {
        const QByteArray block = file.read(1024 * 1024);
        if (block.isEmpty() && !file.atEnd()) throw Failure(("Read failed: " + path).toStdString());
        hash.addData(block);
    }
    return QString::fromLatin1(hash.result().toHex());
}
inline QString fingerprint(const Paths& p) {
    QStringList paths{p.dsp, p.ctl};
    if (!p.system && !QFileInfo(QDir(p.routing).filePath("filter-chain.conf")).isFile())
        throw Failure("Source routing configuration is missing; rerun CMake.");
    QDirIterator iterator(p.routing, {"*.conf"}, QDir::Files,
                          QDirIterator::Subdirectories);
    QStringList configs;
    while (iterator.hasNext()) configs << iterator.next();
    configs.sort(Qt::CaseSensitive);
    paths.append(configs);
    if (paths.size() < 3) throw Failure("ViPER DSP/control/routing files are incomplete.");
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(QByteArray("viper4linux-backend-fingerprint-v1\0",
                             sizeof("viper4linux-backend-fingerprint-v1")));
    for (const auto& fileName : paths) {
        QFile file(fileName);
        if (!file.open(QIODevice::ReadOnly))
            throw Failure(("Missing backend build artifact: " + fileName).toStdString());
        hash.addData(QDir(p.root).relativeFilePath(fileName).toUtf8());
        hash.addData(QByteArray(1, '\0'));
        while (!file.atEnd()) {
            QByteArray block = file.read(1024 * 1024);
            if (block.isEmpty() && !file.atEnd())
                throw Failure(("Could not read backend build artifact: " + fileName).toStdString());
            hash.addData(block);
        }
    }
    return QString::fromLatin1(hash.result().toHex());
}
inline bool ownedProc(const QString& path) {
    struct stat st{};
    const QByteArray name = QFile::encodeName(path);
    return ::stat(name.constData(), &st) == 0 && st.st_uid == getuid();
}
inline std::vector<pid_t> processIds(const QString& procRoot = "/proc") {
    std::vector<pid_t> result;
    const QDir directory(procRoot);
    for (const QString& name : directory.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        bool ok = false;
        const int number = name.toInt(&ok);
        if (ok && number > 0 && ownedProc(directory.filePath(name)))
            result.push_back(static_cast<pid_t>(number));
    }
    return result;
}
inline QString envValue(const QByteArray& environment, const QByteArray& prefix) {
    for (const QByteArray& field : environment.split('\0'))
        if (field.startsWith(prefix))
            return QString::fromLatin1(field.mid(prefix.size()));
    return {};
}
inline std::map<pid_t, QString> clients(const QString& procRoot = "/proc") {
    std::map<pid_t, QString> result;
    for (pid_t pid : processIds(procRoot)) {
        const QString path = procRoot + "/" + QString::number(pid);
        const QByteArray cmd = readFile(path + "/cmdline");
        const QByteArray env = readFile(path + "/environ");
        if (!viperbass::detail::pipewireCommand(std::string(cmd.constData(), size_t(cmd.size()))) ||
            !viperbass::detail::nulField(std::string(env.constData(), size_t(env.size())),
                         "VIPER_CLEAN_INSTANCE=viperlinux-unified-v1"))
            continue;
        result[pid] = envValue(env, "VIPER_BACKEND_FINGERPRINT=");
    }
    return result;
}
// One documented decision shared by real launch and read-only preflight.
enum class LaunchAction { Start, Reuse, Replace };
inline LaunchAction launchAction(const std::map<pid_t, QString>& found,
                                 const QString& wantedFingerprint) {
    if (found.empty()) return LaunchAction::Start;
    if (found.size() == 1 && found.begin()->second == wantedFingerprint)
        return LaunchAction::Reuse;
    return LaunchAction::Replace;
}
inline std::map<pid_t, std::pair<pid_t, QString>>
monitors(const QString& script, const QString& procRoot = "/proc") {
    std::map<pid_t, std::pair<pid_t, QString>> result;
    const QByteArray scriptName = QFile::encodeName(script);
    for (pid_t pid : processIds(procRoot)) {
        const QString path = procRoot + "/" + QString::number(pid);
        const QList<QByteArray> args = readFile(path + "/cmdline").split('\0');
        if (!args.contains(scriptName)) continue;
        const int index = args.indexOf("--backend-pid");
        if (index < 0 || index + 1 >= args.size()) continue;
        bool ok = false;
        const int backend = args[index + 1].toInt(&ok);
        if (!ok || backend <= 0) continue;
        const QString tag = envValue(readFile(path + "/environ"),
                                     "VIPER_AUDIO_OUTPUT_MANAGER_SHA=");
        result[pid] = {backend, tag};
    }
    return result;
}
class Lock {
    int fd_ = -1;
public:
    explicit Lock(const QString& filename) {
        const QByteArray bytes = QFile::encodeName(filename);
        fd_ = ::open(bytes.constData(), O_CREAT | O_RDWR | O_CLOEXEC, 0600);
        if (fd_ < 0 || ::flock(fd_, LOCK_EX) != 0)
            throw Failure(("Cannot lock ViPER lifecycle: " + filename).toStdString());
    }
    Lock(const Lock&) = delete;
    Lock& operator=(const Lock&) = delete;
    ~Lock() { if (fd_ >= 0) { ::flock(fd_, LOCK_UN); ::close(fd_); } }
};
inline void verifyRuntime() {
    if (!QFileInfo(runtime()).isDir())
        throw Failure(("PipeWire runtime directory does not exist: " + runtime()).toStdString());
}
inline void record(const Paths& p, const QString& message) {
    (void)QProcess::execute("/usr/bin/python3", {p.retention, "record", message});
}
inline bool waitClientsGone(const std::vector<pid_t>& previous, int timeoutMs = 5000) {
    QElapsedTimer elapsed;
    elapsed.start();
    do {
        const auto found = clients();
        bool remaining = false;
        for (pid_t pid : previous)
            if (found.count(pid)) { remaining = true; break; }
        if (!remaining) return true;
        QThread::msleep(100);
    } while (elapsed.elapsed() < timeoutMs);
    const auto found = clients();
    for (pid_t pid : previous) if (found.count(pid)) return false;
    return true;
}
inline void stopBackend(const Paths& p, const std::map<pid_t, QString>& found) {
    std::vector<pid_t> previous;
    for (const auto& item : found) {
        std::string error;
        const auto response = viper_gui_exit::stopMarkedBackend(item.first, error);
        if (response == viper_gui_exit::StopResult::Refused ||
            response == viper_gui_exit::StopResult::Failed)
            throw Failure("Refusing unsafe backend stop: " + error);
        previous.push_back(item.first);
        record(p, "stop requested for marked backend PID " + QString::number(item.first));
    }
    if (!waitClientsGone(previous))
        throw Failure("Old ViPER backend did not stop; refusing to launch a duplicate.");
}
inline void validateExecutable(const QString& path, const char* title) {
    const QFileInfo file(path);
    if (!file.isFile() || !file.isExecutable())
        throw Failure((QString(title) + " missing or not executable: " + path).toStdString());
}
inline pid_t ensureBackend(const Paths& p, bool restart = false) {
    verifyRuntime();
    const QString desired = fingerprint(p);
    Lock lock(runtime() + "/viper4linux-launch.lock");
    const auto existing = clients();
    if (!restart && launchAction(existing, desired) == LaunchAction::Reuse)
        return existing.begin()->first;
    if (!existing.empty()) stopBackend(p, existing);
    validateExecutable(p.backendStart, "Backend startup command");
    QDir().mkpath(stateDir());
    // The preexisting log writer/pruner remains the single retention mechanism.
    (void)QProcess::execute("/usr/bin/python3", {p.retention, "prune"});
    QProcess start;
    start.setProgram(p.backendStart);
    start.setWorkingDirectory(p.root);
    if (!p.system) {
        QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
        environment.insert("VIPER_SOURCE_BUILD_DIR", QFileInfo(p.ctl).absolutePath());
        start.setProcessEnvironment(environment);
    }
    start.setProcessChannelMode(QProcess::MergedChannels);
    start.setStandardOutputFile(stateDir() + "/backend.log", QIODevice::Append);
    qint64 newPid = 0;
    record(p, restart ? "explicit backend restart requested" : "backend startup requested");
    if (!start.startDetached(&newPid) || newPid <= 0)
        throw Failure("Could not start the marked ViPER filter-chain client.");
    QElapsedTimer elapsed;
    elapsed.start();
    qint64 stableSince = -1;
    while (elapsed.elapsed() < 7000) {
        const auto active = clients();
        if (active.size() == 1 && active.count(static_cast<pid_t>(newPid)) &&
            active.at(static_cast<pid_t>(newPid)) == desired) {
            if (stableSince < 0) stableSince = elapsed.elapsed();
            if (elapsed.elapsed() - stableSince >= 350) {
                record(p, "backend ready PID " + QString::number(newPid));
                return static_cast<pid_t>(newPid);
            }
        } else stableSince = -1;
        QThread::msleep(100);
    }
    // Do not kill an unrelated PID after startup failure: the normal marked
    // client discovery and pidfd checks are the only allowed stop path.
    const auto active = clients();
    if (active.count(static_cast<pid_t>(newPid))) {
        std::map<pid_t, QString> marked{{static_cast<pid_t>(newPid), active.at(static_cast<pid_t>(newPid))}};
        stopBackend(p, marked);
    }
    throw Failure("Backend did not become ready; see backend.log.");
}
inline bool markedMonitor(pid_t pid, const QString& script) {
    const auto found = monitors(script);
    return found.count(pid) != 0;
}
inline bool signalMonitor(pid_t pid, const QString& script) {
#if defined(SYS_pidfd_open) && defined(SYS_pidfd_send_signal)
    int fd = static_cast<int>(::syscall(SYS_pidfd_open, pid, 0));
    if (fd < 0) return errno == ESRCH;
    const bool verified = markedMonitor(pid, script);
    const int result = verified ? static_cast<int>(::syscall(SYS_pidfd_send_signal,
                                       fd, SIGTERM, nullptr, 0)) : -1;
    const int saved = errno;
    ::close(fd);
    return verified && (result == 0 || saved == ESRCH);
#else
    (void)pid; (void)script;
    return false;
#endif
}
inline pid_t spawnMonitor(const Paths& p, pid_t backend, const QString& tag) {
    int pipefd[2];
    if (::pipe2(pipefd, O_CLOEXEC) != 0)
        throw Failure("Cannot create Audio Output monitor log pipe.");
    const QByteArray python = "/usr/bin/python3";
    const QByteArray retention = QFile::encodeName(p.retention);
    const QByteArray script = QFile::encodeName(p.monitor);
    const QByteArray pidText = QByteArray::number(backend);
    // Preserve the coordinated log-retention writer and its fallback drain.
    const pid_t writer = ::fork();
    if (writer == 0) {
        ::setsid();
        ::close(pipefd[1]);
        ::dup2(pipefd[0], STDIN_FILENO);
        ::close(pipefd[0]);
        const int nullfd = ::open("/dev/null", O_WRONLY);
        if (nullfd >= 0) {
            ::dup2(nullfd, STDOUT_FILENO);
            ::dup2(nullfd, STDERR_FILENO);
            if (nullfd > STDERR_FILENO) ::close(nullfd);
        }
        ::execl("/bin/bash", "bash", "-c",
                "\"$1\" \"$2\" stream output-monitor || cat >/dev/null",
                "viper-monitor-log-writer", python.constData(), retention.constData(),
                static_cast<char*>(nullptr));
        ::_exit(127);
    }
    if (writer < 0) {
        ::close(pipefd[0]); ::close(pipefd[1]);
        throw Failure("Unable to start Audio Output log writer.");
    }
    const pid_t monitor = ::fork();
    if (monitor == 0) {
        ::setsid();
        ::close(pipefd[0]);
        int nullfd = ::open("/dev/null", O_RDONLY);
        if (nullfd >= 0) {
            ::dup2(nullfd, STDIN_FILENO);
            if (nullfd > STDERR_FILENO) ::close(nullfd);
        }
        ::dup2(pipefd[1], STDOUT_FILENO);
        ::dup2(pipefd[1], STDERR_FILENO);
        ::close(pipefd[1]);
        ::setenv("VIPER_AUDIO_OUTPUT_MANAGER_SHA", tag.toLatin1().constData(), 1);
        ::execl(python.constData(), "python3", script.constData(),
                "--backend-pid", pidText.constData(), static_cast<char*>(nullptr));
        ::_exit(127);
    }
    ::close(pipefd[0]); ::close(pipefd[1]);
    if (monitor < 0) throw Failure("Unable to start Audio Output monitor.");
    QThread::msleep(250);
    int status = 0;
    if (::waitpid(monitor, &status, WNOHANG) == monitor)
        throw Failure("Audio Output monitor exited during startup.");
    return monitor;
}
inline pid_t ensureMonitor(const Paths& p, pid_t backend, bool restart = false) {
    verifyRuntime();
    if (!QFileInfo(p.monitor).isFile())
        throw Failure(("Audio Output monitor missing: " + p.monitor).toStdString());
    const QString desired = shaFile(p.monitor);
    Lock lock(runtime() + "/viper4linux-output-launch.lock");
    const auto found = monitors(p.monitor);
    if (!restart && found.size() == 1 &&
        found.begin()->second.first == backend &&
        found.begin()->second.second == desired)
        return found.begin()->first;
    for (const auto& entry : found) {
        if (!signalMonitor(entry.first, p.monitor))
            throw Failure("Refusing to signal an unverified Audio Output monitor.");
    }
    QElapsedTimer stopElapsed;
    stopElapsed.start();
    while (!monitors(p.monitor).empty() && stopElapsed.elapsed() < 5000)
        QThread::msleep(100);
    if (!monitors(p.monitor).empty())
        throw Failure("Previous Audio Output monitor did not stop.");
    (void)QProcess::execute("/usr/bin/python3", {p.retention, "migrate"});
    return spawnMonitor(p, backend, desired);
}
inline void waitHeartbeat(const Paths& p, pid_t backend, pid_t manager, qint64 sinceMs) {
    QElapsedTimer elapsed;
    elapsed.start();
    const QString statusFile = runtime() + "/viper4linux-output-status.json";
    while (elapsed.elapsed() < 8000) {
        if (!monitors(p.monitor).count(manager))
            throw Failure("Audio Output monitor exited before publishing a fresh heartbeat.");
        const QJsonObject info = QJsonDocument::fromJson(readFile(statusFile)).object();
        if (info.value("format").toInt() == 1 &&
            info.value("backend_pid").toInt() == backend &&
            info.value("manager_pid").toInt() == manager &&
            info.value("timestamp").toDouble() >= sinceMs / 1000.0)
            return;
        QThread::msleep(100);
    }
    throw Failure("Audio Output monitor did not publish a fresh heartbeat.");
}
inline bool guiQuitRequest(bool stop, QString& diagnostic) {
    const QString socketName = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation)
                               + "/viper4linux-re-gui.sock";
    QLocalSocket socket;
    socket.connectToServer(socketName, QIODevice::ReadWrite);
    if (!socket.waitForConnected(300)) {
        if (QFileInfo::exists(socketName)) {
            diagnostic = "GUI control socket exists but is not responding.";
            return false;
        }
        diagnostic = "GUI not running.";
        return true;
    }
    const QByteArray request = stop ? "V4L-QUIT/1 stop\n" : "V4L-QUIT/1 keep\n";
    socket.write(request);
    if (!socket.waitForBytesWritten(500) || !socket.waitForReadyRead(5000)) {
        diagnostic = "GUI did not acknowledge the quit request.";
        return false;
    }
    const QByteArray response = socket.readAll();
    if (!response.startsWith("OK\n")) {
        diagnostic = "GUI refused quit request: " + QString::fromUtf8(response).trimmed();
        return false;
    }
    diagnostic = stop ? "GUI stopping with backend." : "GUI closing; backend remains running.";
    return true;
}
inline QString preflight(const Paths& p, const QString& procRoot = "/proc") {
    const QString desired = fingerprint(p);
    validateExecutable(p.gui, "GUI");
    validateExecutable(p.backendStart, "Backend startup command");
    verifyRuntime();
    const auto found = clients(procRoot);
    QStringList report{
        "Viper4Linux launch preflight (read-only snapshot; no actions taken):",
        QString("  Build: ") + (p.system ? "system package" : "source checkout"),
        "  Launcher backend: " + p.backendStart,
        "  Backend fingerprint: " + desired
    };
    if (launchAction(found, desired) == LaunchAction::Start)
        report << "  Action on launch: START a backend (none detected).";
    else if (launchAction(found, desired) == LaunchAction::Reuse)
        report << "  Action on launch: REUSE matching backend PID " + QString::number(found.begin()->first) + ".";
    else {
        QStringList pids;
        for (const auto& entry : found) pids << QString::number(entry.first);
        report << "  Action on launch: REPLACE existing backend PID(s) " + pids.join(", ") + "."
               << "  WARNING: Launch would stop the existing ViPER backend, briefly interrupting audio.";
    }
    report << "  No GUI or backend was started, stopped, or modified."
           << "  Process state may change before an actual launch.";
    return report.join('\n');
}
inline bool isLifecycleCommand(const std::string& command) {
    return command == "--launch" || command == "--fingerprint" ||
           command == "--preflight" || command == "--restart-backend" ||
           command == "--restart-audio" || command == "--quit-gui" ||
           command == "--quit-all";
}
inline int run(const QString& command, const QString& argv0) {
    try {
        const Paths p = Paths::forExecutable(argv0);
        if (command == "--fingerprint") {
            const QByteArray bytes = fingerprint(p).toUtf8();
            ::write(STDOUT_FILENO, bytes.constData(), size_t(bytes.size()));
            ::write(STDOUT_FILENO, "\n", 1);
            return 0;
        }
        if (command == "--preflight") {
            const QByteArray bytes = (preflight(p) + "\n").toUtf8();
            ::write(STDOUT_FILENO, bytes.constData(), size_t(bytes.size()));
            return 0;
        }
        if (command == "--quit-gui" || command == "--quit-all") {
            const bool stop = command == "--quit-all";
            // Hold the same lifecycle lock across GUI stop and backend stop,
            // never allowing a concurrent native restart to slip between them.
            std::optional<Lock> lifecycleLock;
            if (stop) {
                verifyRuntime();
                lifecycleLock.emplace(runtime() + "/viper4linux-launch.lock");
            }
            QString diagnostic;
            if (!guiQuitRequest(stop, diagnostic))
                throw Failure(diagnostic.toStdString());
            if (stop) {
                const auto found = clients();
                if (!found.empty()) stopBackend(p, found);
            }
            const QByteArray bytes = (diagnostic + "\n").toUtf8();
            ::write(STDOUT_FILENO, bytes.constData(), size_t(bytes.size()));
            return 0;
        }
        if (command == "--restart-backend" || command == "--restart-audio") {
            const qint64 since = QDateTime::currentMSecsSinceEpoch();
            const pid_t backend = ensureBackend(p, true);
            const pid_t monitor = ensureMonitor(p, backend, true);
            waitHeartbeat(p, backend, monitor, since);
            const QByteArray bytes = QString("ViPER backend restarted (PID %1); Audio Output monitor PID %2.\n")
                                         .arg(backend).arg(monitor).toUtf8();
            ::write(STDOUT_FILENO, bytes.constData(), size_t(bytes.size()));
            return 0;
        }
        if (command == "--launch") {
            const pid_t backend = ensureBackend(p);
            try {
                (void)ensureMonitor(p, backend);
            } catch (const std::exception& e) {
                const QByteArray warning = QByteArray("viper4linux: Audio Output monitor unavailable: ") +
                                           e.what() + "\n";
                ::write(STDERR_FILENO, warning.constData(), size_t(warning.size()));
            }
            validateExecutable(p.gui, "GUI");
            const QByteArray gui = QFile::encodeName(p.gui);
            ::execl(gui.constData(), gui.constData(), static_cast<char*>(nullptr));
            throw Failure(("Unable to launch GUI: " + p.gui).toStdString());
        }
        throw Failure("Unknown lifecycle operation.");
    } catch (const std::exception& error) {
        const QByteArray message = QByteArray("viper4linux: ") + error.what() + "\n";
        ::write(STDERR_FILENO, message.constData(), size_t(message.size()));
        return 1;
    }
}
} // namespace viper_lifecycle
