#!/usr/bin/env bash
# Deterministic backup retention, failure safety and disposable project restoration.
# Requires the debug build. Uses controlled save timestamps, never the machine clock.
set -euo pipefail

ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
CXX=${CXX:-g++}
OUTPUT="$ROOT/.build/debug/_build"
[[ -f "$OUTPUT/libcorelib.so" && -f "$OUTPUT/plugins/libcoreplugin.so" ]] || {
    printf 'Run ./build/dev.sh build debug first.\n' >&2
    exit 1
}
mkdir -p "$ROOT/.build/evidence"
EVIDENCE=$(mktemp -d "$ROOT/.build/evidence/backup-retention-XXXXXX")
mkdir -p "$EVIDENCE/profile/config" "$EVIDENCE/profile/data" "$EVIDENCE/profile/cache" \
    "$EVIDENCE/profile/documents" "$EVIDENCE/checks"
ln -s "$OUTPUT/plugins" "$EVIDENCE/plugins"
read -r -a QT_CFLAGS <<< "$(pkg-config --cflags Qt6Core Qt6Gui Qt6Widgets Qt6Sql Qt6Xml Qt6Network)"
read -r -a QT_LIBS <<< "$(pkg-config --libs Qt6Core Qt6Gui Qt6Widgets Qt6Sql Qt6Xml Qt6Network)"
if ! "$CXX" -std=c++17 -g -fPIC -I "$ROOT/src" -I "$ROOT/src/core" -I "$ROOT/src/corelib" \
    "${QT_CFLAGS[@]}" "$ROOT/tests/backup_retention_test.cpp" \
    -L "$OUTPUT" -L "$OUTPUT/plugins" -lcoreplugin -lscreenplaytextplugin -lcorelib \
    "${QT_LIBS[@]}" -o "$EVIDENCE/backup-retention-test" > "$EVIDENCE/compile.log" 2>&1; then
    cat "$EVIDENCE/compile.log" >&2
    printf 'Evidence: %s\n' "$EVIDENCE" >&2
    exit 1
fi
export XDG_CONFIG_HOME="$EVIDENCE/profile/config"
export XDG_DATA_HOME="$EVIDENCE/profile/data"
export XDG_CACHE_HOME="$EVIDENCE/profile/cache"
printf 'XDG_DOCUMENTS_DIR="%s"\n' "$EVIDENCE/profile/documents" > "$XDG_CONFIG_HOME/user-dirs.dirs"
export QT_QPA_PLATFORM=offscreen
export LD_LIBRARY_PATH="$OUTPUT:$OUTPUT/libs:$OUTPUT/plugins${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
if ! "$EVIDENCE/backup-retention-test" "$EVIDENCE/checks" > "$EVIDENCE/test.log" 2>&1; then
    cat "$EVIDENCE/test.log" >&2
    printf 'Evidence: %s\n' "$EVIDENCE" >&2
    exit 1
fi
# A process-local file-size limit deterministically makes a temporary copy fail partway.
# Existing snapshots are seeded before the limit and must remain byte exact afterwards.
python3 - "$EVIDENCE" >> "$EVIDENCE/test.log" 2>&1 <<'PY' || {
import hashlib
import os
from pathlib import Path
import resource
import signal
import subprocess
import sys

root = Path(sys.argv[1])
source = root / "limited" / "story.starc"
backups = source.parent / "backups"
backups.mkdir(parents=True)
source.write_bytes(bytes(range(256)) * 4096)
identity = hashlib.sha256(str(source).encode()).hexdigest()
for second in range(12):
    (backups / f"story_{identity}_2026_04_01_12_00_{second:02}.starc").write_bytes(bytes([second]))
before = {p.name: p.read_bytes() for p in backups.iterdir()}

def limit():
    resource.setrlimit(resource.RLIMIT_FSIZE, (32768, 32768))
    signal.signal(signal.SIGXFSZ, signal.SIG_IGN)

result = subprocess.run([str(root / "backup-retention-test"), "--copy-only", str(source), str(backups)],
                        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, preexec_fn=limit)
print(result.stdout.decode(errors="replace"))
assert result.returncode == 0, f"partial-copy check exit {result.returncode}"
assert "Can't complete backup" in result.stdout.decode(), "did not exercise partial write failure"
assert before == {p.name: p.read_bytes() for p in backups.iterdir()}, "failed copy altered backups"
print("ok: partial write failure cleans temporary file and leaves all recovery copies byte exact")
PY
    cat "$EVIDENCE/test.log" >&2
    printf 'Evidence: %s\n' "$EVIDENCE" >&2
    exit 1
}
cat "$EVIDENCE/test.log"
printf 'Evidence: %s\n' "$EVIDENCE"
