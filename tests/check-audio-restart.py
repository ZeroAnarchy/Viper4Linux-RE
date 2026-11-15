#!/usr/bin/env python3
"""CLI lifecycle entrypoint pass-through, isolated from real DSP processes.

The native C++ harness tests launch decisions and fingerprints; the real Qt
socket harness tests GUI quit-message handling. Avoid source-string checks.
"""
from pathlib import Path
from tempfile import TemporaryDirectory
import subprocess

root = Path(__file__).resolve().parents[1]
with TemporaryDirectory(prefix="viper-lifecycle-cli-test-") as temp:
    checkout = Path(temp)
    (checkout / "build").mkdir()
    wrapper = checkout / "viper4linux"
    wrapper.write_bytes((root / "viper4linux").read_bytes())
    wrapper.chmod(0o755)
    controller = checkout / "build/viper-ctl"
    controller.write_text('#!/bin/sh\nprintf "ARG:%s\\n" "$@"\n')
    controller.chmod(0o755)
    for command in ("--restart-backend", "--quit-gui", "--quit-all"):
        result = subprocess.run([str(wrapper), command], capture_output=True,
                                text=True, check=True, timeout=5)
        assert result.stdout.splitlines() == [f"ARG:{command}"]
        assert result.stderr == "", result.stderr

print("PASS: lifecycle flags pass unchanged to the native controller")
