#!/usr/bin/env python3
"""Runtime pidfd stop-guard test using a harmless child process.

Real Qt activation/quit IPC is separately tested in check-single-gui.py.
No C++-source-text matching: incidental refactors must not break this test.
"""
from pathlib import Path
from tempfile import TemporaryDirectory
import os
import subprocess
import signal

root = Path(__file__).resolve().parents[1]

source = r'''
#include "gui-backend-exit.h"
#include <cassert>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

static void writeBinary(const std::filesystem::path& path, const std::string& s) {
    std::ofstream out(path, std::ios::binary);
    assert(out);
    out.write(s.data(), static_cast<std::streamsize>(s.size()));
    assert(out);
}
int main(int argc, char** argv) {
    assert(argc == 2);
    const std::string fakeProc = argv[1];
    const pid_t child = fork();
    assert(child >= 0);
    if (child == 0) {
        for (;;) pause();
    }
    const auto directory = std::filesystem::path(fakeProc) / std::to_string(child);
    std::filesystem::create_directories(directory);
    const char argvData[] = "pipewire\0-c\0filter-chain.conf\0";
    writeBinary(directory / "cmdline", std::string(argvData, sizeof(argvData) - 1));
    writeBinary(directory / "environ", "UNRELATED_PROCESS=1");
    std::string error;
    assert(!viper_gui_exit::isMarkedBackend(child, fakeProc));
    assert(viper_gui_exit::stopMarkedBackend(child, error, fakeProc)
        == viper_gui_exit::StopResult::Refused);
    assert(kill(child, 0) == 0); // An unmarked process was not touched.
    const char marker[] = "VIPER_CLEAN_INSTANCE=viperlinux-unified-v1\0";
    writeBinary(directory / "environ", std::string(marker, sizeof(marker) - 1));
    assert(viper_gui_exit::isMarkedBackend(child, fakeProc));
    assert(viper_gui_exit::stopMarkedBackend(child, error, fakeProc)
        == viper_gui_exit::StopResult::Signalled);
    int status = 0;
    assert(waitpid(child, &status, 0) == child);
    assert(WIFSIGNALED(status) && WTERMSIG(status) == SIGTERM);
    assert(viper_gui_exit::stopMarkedBackend(child, error, fakeProc)
        == viper_gui_exit::StopResult::AlreadyStopped);
    return 0;
}
'''
with TemporaryDirectory(prefix="viper-gui-exit-") as dirname:
    directory = Path(dirname)
    cpp = directory / "check.cpp"
    executable = directory / "check"
    fixture = directory / "fake-proc"
    fixture.mkdir()
    cpp.write_text(source)
    subprocess.run(
        [os.environ.get("CXX", "c++"), "-std=c++17", "-Wall", "-Wextra",
         "-Werror", "-I", str(root / "src"), str(cpp), "-o", str(executable)],
        check=True, timeout=40)
    # The test forks a dummy child; clean up its entire isolated process group
    # even if an assertion in the C++ helper aborts before waitpid().
    process = subprocess.Popen([str(executable), str(fixture)], start_new_session=True)
    try:
        assert process.wait(timeout=10) == 0, "isolated backend stop test failed"
    finally:
        try:
            os.killpg(process.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        if process.poll() is None:
            process.wait(timeout=3)
print("PASS: isolated pidfd stop guard rejects unmarked and stops marked child")
