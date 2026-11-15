#!/usr/bin/env bash
# Independently stage the same real CMake build twice and compare full file,
# permission and symlink manifests for deterministic repeatable installation.
# Usage: bash tests/check-cmake-install-parity.sh BUILD_DIRECTORY
set -Eeuo pipefail
if (( $# != 1 )); then
    echo 'Usage: tests/check-cmake-install-parity.sh BUILD_DIRECTORY' >&2
    exit 2
fi
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)
built=$(cd -- "$1" && pwd -P)
[[ -f "$built/CMakeCache.txt" ]] || { echo 'Missing CMake build cache.' >&2; exit 1; }
grep -Fxq 'VIPER_ENABLE_SYSTEM_INSTALL:BOOL=ON' "$built/CMakeCache.txt" || {
    echo 'System staging option is not enabled in the build.' >&2; exit 1;
}
grep -Fxq 'CMAKE_INSTALL_PREFIX:PATH=/usr' "$built/CMakeCache.txt" || {
    echo 'System staging must use /usr.' >&2; exit 1;
}
for file in viper-gui viper-ctl viper-output-state viper_dsp_ladspa.so; do
    [[ -s "$built/$file" ]] || { echo "Missing compiled file: $file" >&2; exit 1; }
done

work=$(mktemp -d /tmp/viper-stage-parity-XXXXXXXX)
trap 'rm -rf -- "$work"' EXIT
first="$work/stage-first"
second="$work/stage-second"
mkdir -p -- "$first" "$second"

DESTDIR="$first" cmake --install "$built"
DESTDIR="$second" cmake --install "$built"

python3 - "$first" "$second" <<'PY'
import hashlib
import os
from pathlib import Path
import stat
import sys

def staged_manifest(directory):
    root = Path(directory)
    result = {}
    for path in root.rglob('*'):
        name = path.relative_to(root).as_posix()
        mode = stat.S_IMODE(path.lstat().st_mode)
        if path.is_symlink():
            result[name] = ('symlink', mode, os.readlink(path))
        elif path.is_file():
            digest = hashlib.sha256(path.read_bytes()).hexdigest()
            result[name] = ('file', mode, digest)
        elif path.is_dir():
            result[name] = ('directory', mode)
        else:
            result[name] = ('unknown', mode)
    return result

first = staged_manifest(sys.argv[1])
second = staged_manifest(sys.argv[2])
if first != second:
    diff = sorted(set(first) | set(second))
    mismatches = [path for path in diff if first.get(path) != second.get(path)]
    print(f'FAIL: {len(mismatches)} CMake staging differences:', file=sys.stderr)
    for path in mismatches[:40]:
        print(f'  {path}', file=sys.stderr)
        print(f'    first:  {first.get(path)}', file=sys.stderr)
        print(f'    second: {second.get(path)}', file=sys.stderr)
    sys.exit(1)
# Do not assert a total number of files or symlinks: legitimate packaging
# additions/removals must not fail a reproducibility check. Assert the actual
# installed-entrypoint and audio safety contracts instead.
def require_file(path):
    if first.get(path, (None,))[0] != 'file':
        print(f'FAIL: required staged file missing: {path}', file=sys.stderr)
        sys.exit(1)

def require_link(path, target):
    entry = first.get(path)
    if entry is None or entry[0] != 'symlink' or entry[2] != target:
        print(f'FAIL: required relative symlink {path} -> {target}: {entry}',
              file=sys.stderr)
        sys.exit(1)

for filename in (
    'usr/bin/viper4linux-gui',
    'usr/bin/viper4linux-ctl',
    'usr/bin/viper4linux-output-state',
    'usr/lib/ladspa/viper_dsp_ladspa.so',
    'usr/lib/viper4linux/start-backend',
    'usr/lib/viper4linux/viper_output_monitor.py',
    'usr/share/applications/viper4linux.desktop',
):
    require_file(filename)
require_link('usr/bin/v4l-cli', 'viper4linux-ctl')
require_link('usr/bin/viper4linux', 'viper4linux-ctl')
manpages = (
    ('usr/share/man/man1/v4l-cli.1', 'viper4linux-ctl.1'),
    ('usr/share/man/man1/v4l-cli.1.gz', 'viper4linux-ctl.1.gz'),
)
present = [(name, target) for name, target in manpages if name in first]
if len(present) != 1:
    print(f'FAIL: expected one v4l-cli manpage alias, found {present}',
          file=sys.stderr)
    sys.exit(1)
require_link(*present[0])

files = sum(kind[0] == 'file' for kind in first.values())
links = sum(kind[0] == 'symlink' for kind in first.values())
print(f'PASS: two independent direct CMake stages: {files} files, {links} symlinks; '
      'required entries verified; identical hashes, permissions and tree')
PY
