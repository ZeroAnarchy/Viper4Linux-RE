// Verify native fingerprint parity, strict own-process discovery and preflight
// against disposable fake files. Never starts or signals real PipeWire.
#include "viper-lifecycle.h"
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <cassert>
#include <iostream>

static void writeFile(const QString& file, const QByteArray& value) {
    assert(QDir().mkpath(QFileInfo(file).absolutePath()));
    QFile out(file);
    assert(out.open(QIODevice::WriteOnly | QIODevice::Truncate));
    assert(out.write(value) == value.size());
}
static void executable(const QString& file) {
    writeFile(file, "#!/bin/sh\nexit 0\n");
    assert(QFile::setPermissions(file, QFileDevice::ReadOwner |
        QFileDevice::WriteOwner | QFileDevice::ExeOwner));
}
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir temporary;
    assert(temporary.isValid());
    const QString root = temporary.path();
    viper_lifecycle::Paths paths;
    paths.root = root;
    paths.dsp = root + "/build/viper_dsp_ladspa.so";
    paths.ctl = root + "/build/viper-ctl";
    paths.routing = root + "/build/routing-config";
    paths.gui = root + "/build/viper-gui";
    paths.backendStart = root + "/scripts/start.sh";
    paths.monitor = root + "/src/viper_output_monitor.py";
    paths.retention = root + "/src/viper_log_retention.py";
    writeFile(paths.dsp, "DSP-A");
    writeFile(paths.ctl, "CTL-A");
    writeFile(paths.routing + "/filter-chain.conf", "CONFIG-A");
    writeFile(paths.routing + "/filter-chain.conf.d/99-viper-smart.conf", "ROUTE-A");
    executable(paths.gui);
    executable(paths.backendStart);
    // Deliberate protocol golden vector: existing running backends carry this
    // fingerprint across independent launches. This is not presentation text.
    const QString expected = "6a3b9c353535e4af4a60636a1d6f8e0eced5aa080ae898324e571f92cd51bc31";
    assert(viper_lifecycle::fingerprint(paths) == expected);
    writeFile(paths.dsp, "DSP-B");
    assert(viper_lifecycle::fingerprint(paths) != expected);
    writeFile(paths.dsp, "DSP-A");
    writeFile(paths.routing + "/filter-chain.conf.d/99-viper-smart.conf", "ROUTE-B");
    assert(viper_lifecycle::fingerprint(paths) != expected);
    writeFile(paths.routing + "/filter-chain.conf.d/99-viper-smart.conf", "ROUTE-A");
    writeFile(paths.ctl, "CTL-B");
    assert(viper_lifecycle::fingerprint(paths) != expected);
    writeFile(paths.ctl, "CTL-A");
    assert(viper_lifecycle::fingerprint(paths) == expected);

    // Test semantic launch choices, not human-facing preflight wording.
    using Action = viper_lifecycle::LaunchAction;
    assert(viper_lifecycle::launchAction({}, expected) == Action::Start);
    assert(viper_lifecycle::launchAction({{123, expected}}, expected) == Action::Reuse);
    assert(viper_lifecycle::launchAction({{123, "old-build"}}, expected) == Action::Replace);
    assert(viper_lifecycle::launchAction({{123, expected}, {125, expected}}, expected)
           == Action::Replace);

    const QString proc = root + "/proc";
    auto makePipewire = [](const QString& prefix, const QByteArray& env) {
        QByteArray command;
        command += "/usr/bin/pipewire";
        command.append('\0');
        command += "-c";
        command.append('\0');
        command += "filter-chain.conf";
        command.append('\0');
        writeFile(prefix + "/cmdline", command);
        writeFile(prefix + "/environ", env);
    };
    QByteArray marked = "VIPER_CLEAN_INSTANCE=viperlinux-unified-v1";
    marked.append('\0');
    marked += "VIPER_BACKEND_FINGERPRINT=" + expected.toLatin1();
    marked.append('\0');
    makePipewire(proc + "/123", marked);
    makePipewire(proc + "/124", "OTHER_INSTANCE=1");
    assert(viper_lifecycle::clients(proc).size() == 1);
    assert(viper_lifecycle::clients(proc).at(123) == expected);
    // The monitor is only eligible when its exact script and backend PID are in argv.
    QByteArray command = "/usr/bin/python3";
    command.append('\0');
    command += paths.monitor.toUtf8();
    command.append('\0');
    command += "--backend-pid";
    command.append('\0');
    command += "123";
    command.append('\0');
    writeFile(proc + "/125/cmdline", command);
    writeFile(proc + "/125/environ", "VIPER_AUDIO_OUTPUT_MANAGER_SHA=tag");
    auto found = viper_lifecycle::monitors(paths.monitor, proc);
    assert(found.size() == 1 && found.at(125).first == 123 &&
           found.at(125).second == "tag");
    assert(viper_lifecycle::monitors(paths.monitor + ".other", proc).empty());

    const QString runtime = root + "/runtime";
    assert(QDir().mkpath(runtime));
    qputenv("XDG_RUNTIME_DIR", runtime.toLocal8Bit());
    qputenv("XDG_STATE_HOME", (root + "/state").toLocal8Bit());
    const QString preview = viper_lifecycle::preflight(paths, proc);
    assert(!preview.isEmpty()); // Output formatting is deliberately not pinned.
    assert(!QFileInfo::exists(runtime + "/viper4linux-launch.lock"));
    assert(!QFileInfo::exists(root + "/state"));
    for (const auto& command : {"--quit-gui", "--quit-all", "--restart-backend",
                                "--launch", "--preflight", "--fingerprint"})
        assert(viper_lifecycle::isLifecycleCommand(command));
    assert(!viper_lifecycle::isLifecycleCommand("--deq"));
    std::cout << "PASS: native lifecycle fingerprints, strict marked process discovery and safe preflight\n";
}
