#!/usr/bin/env bash
# Lost SQLite transactions must restore every accepted persistence operation.
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
OUTPUT="$ROOT/.build/debug/_build"
mkdir -p "$ROOT/.build/evidence"
EVIDENCE=$(mktemp -d "$ROOT/.build/evidence/transaction-recovery-XXXXXX")
printf 'Evidence: %s\n' "$EVIDENCE"
mkdir -p "$EVIDENCE/profile/config" "$EVIDENCE/profile/data" "$EVIDENCE/profile/cache"
export XDG_CONFIG_HOME="$EVIDENCE/profile/config"
export XDG_DATA_HOME="$EVIDENCE/profile/data"
export XDG_CACHE_HOME="$EVIDENCE/profile/cache"
export DBUS_SESSION_BUS_ADDRESS="unix:path=$EVIDENCE/absent-session-bus"
export QT_QPA_PLATFORM=offscreen
export LD_LIBRARY_PATH="$OUTPUT:$OUTPUT/libs${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
read -r -a QT_CFLAGS <<< "$(pkg-config --cflags Qt6Core Qt6Gui Qt6Widgets Qt6Sql)"
read -r -a QT_LIBS <<< "$(pkg-config --libs Qt6Core Qt6Gui Qt6Widgets Qt6Sql)"
if ! "${CXX:-g++}" -std=c++17 -g -fPIC -I "$ROOT/src/corelib" "${QT_CFLAGS[@]}" \
    "$ROOT/tests/transaction_recovery_test.cpp" -L "$OUTPUT" -lcorelib "${QT_LIBS[@]}" \
    -o "$EVIDENCE/transaction-recovery-test" > "$EVIDENCE/compile.log" 2>&1; then
    cat "$EVIDENCE/compile.log" >&2
    exit 1
fi
if ! "$EVIDENCE/transaction-recovery-test" "$EVIDENCE/checks" > "$EVIDENCE/test.log" 2>&1; then
    tail -100 "$EVIDENCE/test.log" >&2
    exit 1
fi
if ! python3 - "$EVIDENCE/checks" >> "$EVIDENCE/test.log" 2>&1 <<'PY'
from pathlib import Path
import sqlite3
import sys
import zlib

for mode in ('full', 'rollback', 'abort', 'commit'):
    root = Path(sys.argv[1]) / mode
    identities = (root / 'identities.txt').read_text().splitlines()
    with sqlite3.connect(f'{(root / "story.starc").as_uri()}?mode=ro', uri=True) as db:
        assert db.execute('PRAGMA integrity_check').fetchall() == [('ok',)]
        rows = dict(db.execute('SELECT uuid,CAST(content AS BLOB) FROM documents').fetchall())
        assert set(rows) == set(identities[:5]), (mode, rows.keys())
        assert rows[identities[0]] == (root / 'script.bin').read_bytes() == b'new-script plus later edit\x00\r\n'
        assert rows[identities[1]] == (root / 'later.bin').read_bytes() == (
            b'X' * (2 * 1024 * 1024) if mode == 'full' else b'new-notes')
        assert rows[identities[2]] == b'new-document'
        assert rows[identities[3]] == (root / 'image.png').read_bytes()
        assert rows[identities[4]] == (root / 'attachment.bin').read_bytes() == b'new-attachment\x00\r\n'
        history = db.execute('SELECT uuid,undo_patch,redo_patch FROM documents_changes ORDER BY id').fetchall()
        names = ['baseline', 'first', 'second', 'extra'] if mode == 'commit' else [
            'baseline', 'first', 'second', 'blocked', 'extra', 'blocked']
        assert [zlib.decompress(row[1][4:]) for row in history] == [
            f'{name}-undo'.encode() for name in names], (mode, history)
        assert [zlib.decompress(row[2][4:]) for row in history] == [
            f'{name}-redo'.encode() for name in names]
        assert history[1][0] == identities[5] and history[2][0] == identities[6]
        assert next(row for row in history if zlib.decompress(row[1][4:]) == b'extra-undo')[0] == identities[7]
        current_ids = dict(db.execute('SELECT uuid,id FROM documents'))
    with sqlite3.connect(f'{(root / "before.starc").as_uri()}?mode=ro', uri=True) as db:
        original_ids = dict(db.execute('SELECT uuid,id FROM documents'))
        assert current_ids[identities[0]] == original_ids[identities[0]]
        assert current_ids[identities[1]] == original_ids[identities[1]]
    print(f'ok: fresh-process exact content, attachment, deletion and ordered-history oracle: {mode}')
PY
then
    tail -100 "$EVIDENCE/test.log" >&2
    exit 1
fi
tail -5 "$EVIDENCE/test.log"
printf 'Evidence: %s\n' "$EVIDENCE"
