#!/usr/bin/env python3
"""Source entrypoint integration: use a disposable executable, never real audio."""
from pathlib import Path
from tempfile import TemporaryDirectory
import subprocess

root = Path(__file__).resolve().parents[1]
assert not (root / "src/viper_launcher.py").exists(), "Retired launcher returned"
assert not (root / "packaging/arch/viper4linux").exists(), "Retired wrapper returned"

with TemporaryDirectory(prefix="viper-source-launch-test-") as temp:
    checkout = Path(temp)
    (checkout / "build").mkdir()
    wrapper = checkout / "viper4linux"
    wrapper.write_bytes((root / "viper4linux").read_bytes())
    wrapper.chmod(0o755)
    controller = checkout / "build/viper-ctl"
    controller.write_text('#!/bin/sh\nprintf "ARG:%s\\n" "$@"\n')
    controller.chmod(0o755)

    def invoke(*args):
        result = subprocess.run([str(wrapper), *args], capture_output=True,
                                text=True, timeout=5, check=True)
        assert result.stderr == "", result.stderr
        return result.stdout.splitlines()

    assert invoke() == ["ARG:--launch"]
    assert invoke("--preflight") == ["ARG:--preflight"]
    assert invoke("--fingerprint") == ["ARG:--fingerprint"]
    assert invoke("--status", "--full") == ["ARG:--status", "ARG:--full"]

print("PASS: source wrapper forwards launch and CLI arguments to native controller")
