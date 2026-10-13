#!/usr/bin/env bash
# Concurrent SQLite saves must never tear the production builder's recovery copy.
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
OUTPUT="$ROOT/.build/debug/_build"
[[ -f "$OUTPUT/libcorelib.so" ]] || {
    printf 'Run ./build/dev.sh build debug first.\n' >&2
    exit 1
}
mkdir -p "$ROOT/.build/evidence"
EVIDENCE=$(mktemp -d "$ROOT/.build/evidence/backup-consistency-XXXXXX")
mkdir -p "$EVIDENCE/profile/config" "$EVIDENCE/profile/data" "$EVIDENCE/profile/cache"
export XDG_CONFIG_HOME="$EVIDENCE/profile/config"
export XDG_DATA_HOME="$EVIDENCE/profile/data"
export XDG_CACHE_HOME="$EVIDENCE/profile/cache"
export DBUS_SESSION_BUS_ADDRESS="unix:path=$EVIDENCE/absent-session-bus"
read -r -a QT_CFLAGS <<< "$(pkg-config --cflags Qt6Core Qt6Sql)"
read -r -a QT_LIBS <<< "$(pkg-config --libs Qt6Core Qt6Sql)"
if ! "${CXX:-g++}" -std=c++17 -g -fPIC -I "$ROOT/src/corelib" "${QT_CFLAGS[@]}" \
    "$ROOT/tests/backup_consistency_test.cpp" -L "$OUTPUT" -lcorelib \
    "${QT_LIBS[@]}" -ldl -pthread -o "$EVIDENCE/backup-consistency-test" \
    > "$EVIDENCE/compile.log" 2>&1; then
    cat "$EVIDENCE/compile.log" >&2
    printf 'Evidence: %s\n' "$EVIDENCE" >&2
    exit 1
fi
export LD_LIBRARY_PATH="$OUTPUT:$OUTPUT/libs${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
if ! "$EVIDENCE/backup-consistency-test" "$EVIDENCE/checks" > "$EVIDENCE/test.log" 2>&1; then
    cat "$EVIDENCE/test.log" >&2
    printf 'Evidence: %s\n' "$EVIDENCE" >&2
    exit 1
fi
# Independent fresh-process oracle: every restored document, annotation and history
# must match the authored generation, not just SQLite's structural integrity check.
if ! python3 - "$EVIDENCE/checks" >> "$EVIDENCE/test.log" 2>&1 <<'PY'
from pathlib import Path
import sqlite3
import sys

root = Path(sys.argv[1])
for mode in ('DELETE', 'TRUNCATE', 'PERSIST'):
    directory = root / mode
    paths = [(directory / 'restored.starc', b'A'), (directory / 'source.starc', b'B')]
    paths += [(path, b'A' if '12_00_00' in path.name else b'B')
              for path in (directory / 'backups').glob('*.starc')]
    assert len(paths) == 4, (mode, paths)
    for path, generation in paths:
        with sqlite3.connect(f'{path.as_uri()}?mode=ro', uri=True) as db:
            assert db.execute('PRAGMA integrity_check').fetchall() == [('ok',)]
            assert db.execute('SELECT id,uuid,type,content FROM documents ORDER BY id').fetchall() == [
                (17, 'script', 10104, generation * (256 * 1024)),
                (42, 'annotation', 10106, b'annotation\x00\r\n' + generation),
            ], path
            assert db.execute('SELECT * FROM documents_changes').fetchall() == [
                (31, 'script', 'history', b'undo-' + generation, b'redo-' + generation)
            ], path
            assert db.execute('SELECT * FROM system_variables').fetchall() == [('fixture', 'preserved')]
            assert db.execute('SELECT * FROM unknown_legacy_data').fetchall() == [(99, b'\x00\r\n\xff')]
        print(f'ok: independent fresh-process content/identity/history oracle: {path}')
print('PASS: fresh-process recovery of both complete generations')
PY
then
    cat "$EVIDENCE/test.log" >&2
    printf 'Evidence: %s\n' "$EVIDENCE" >&2
    exit 1
fi
cat "$EVIDENCE/test.log"
printf 'Evidence: %s\n' "$EVIDENCE"
