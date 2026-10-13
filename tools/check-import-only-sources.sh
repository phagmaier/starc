#!/usr/bin/env bash
# Real application direct-open/import, source preservation, editing and ordinary reopen.
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
OUTPUT="$ROOT/.build/debug/_build"
mkdir -p "$ROOT/.build/evidence"
EVIDENCE=$(mktemp -d "$ROOT/.build/evidence/import-only-sources-XXXXXX")
printf 'Evidence: %s\n' "$EVIDENCE"
ln -s "$OUTPUT/plugins" "$EVIDENCE/plugins"
read -r -a QT_CFLAGS <<< "$(pkg-config --cflags Qt6Core Qt6Gui Qt6Widgets Qt6Sql Qt6Xml Qt6Network)"
read -r -a QT_LIBS <<< "$(pkg-config --libs Qt6Core Qt6Gui Qt6Widgets Qt6Sql Qt6Xml Qt6Network)"
if ! "${CXX:-g++}" -std=c++17 -g -fPIC -I "$ROOT/src" -I "$ROOT/src/core" -I "$ROOT/src/corelib" \
    "${QT_CFLAGS[@]}" "$ROOT/tests/import_only_source_test.cpp" "$ROOT/src/app/application.cpp" \
    "$ROOT/.build/debug/app/moc_application.cpp" \
    -L "$OUTPUT" -L "$OUTPUT/plugins" -lcoreplugin -lscreenplaytextplugin -lcorelib \
    "${QT_LIBS[@]}" -o "$EVIDENCE/import-only-test" > "$EVIDENCE/compile.log" 2>&1; then
    cat "$EVIDENCE/compile.log" >&2
    exit 1
fi
export QT_QPA_PLATFORM=${STARC_IMPORT_ONLY_PLATFORM:-offscreen}
export LD_LIBRARY_PATH="$OUTPUT:$OUTPUT/libs:$OUTPUT/plugins${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export DBUS_SESSION_BUS_ADDRESS="unix:path=$EVIDENCE/absent-session-bus"
run_case() {
    local mode=$1 root=$2
    export STARC_IMPORT_ONLY_ROOT="$root"
    export STARC_PROFILE_DIR="$root/profile"
    export XDG_CONFIG_HOME="$STARC_PROFILE_DIR/config"
    export XDG_DATA_HOME="$STARC_PROFILE_DIR/data"
    export XDG_CACHE_HOME="$STARC_PROFILE_DIR/cache"
    mkdir -p "$XDG_CONFIG_HOME" "$XDG_DATA_HOME" "$XDG_CACHE_HOME" "$STARC_PROFILE_DIR/documents"
    printf 'XDG_DOCUMENTS_DIR="%s"\n' "$STARC_PROFILE_DIR/documents" > "$XDG_CONFIG_HOME/user-dirs.dirs"
    if ! STARC_IMPORT_ONLY_MODE="$mode" "$EVIDENCE/import-only-test" > "$root/$mode.log" 2>&1; then
        tail -100 "$root/$mode.log" >&2
        printf 'Failed case: %s; evidence: %s\n' "$mode" "$EVIDENCE" >&2
        exit 1
    fi
    printf 'PASS: %s (ordinary exit 0)\n' "$mode"
}
for mode in ${STARC_IMPORT_ONLY_CASES:-docx-startup docx-event docx-onboarding docx-cancel docx-existing docx-locked docx-readonly docx-no-extension docx-repeated DOCX-event pdf-event md-event}; do
    root="$EVIDENCE/$mode"
    python3 "$ROOT/tests/import_only_fixtures.py" "$root"
    mkdir -p "$root/copies"
    [[ $mode != docx-readonly ]] || chmod 444 "$root/source.docx"
    python3 - "$root" <<'PY'
from pathlib import Path
import hashlib
import json
import sys
root = Path(sys.argv[1])
(root / 'source-hashes.json').write_text(json.dumps({p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in root.glob('source.*')}))
PY
    # The onboarding case needs genuinely fresh settings, rather than a completed profile
    # whose configured flag was reset just for the test.
    [[ $mode == docx-onboarding ]] || run_case prepare "$root"
    run_case "$mode" "$root"
    run_case reopen "$root"
done
python3 - "$EVIDENCE" <<'PY'
from pathlib import Path
import hashlib
import json
import sqlite3
import sys
import zipfile
root = Path(sys.argv[1])
for case in root.iterdir():
    if not (case / 'source-hashes.json').exists():
        continue
    for name, expected in json.loads((case / 'source-hashes.json').read_text()).items():
        assert hashlib.sha256((case / name).read_bytes()).hexdigest() == expected, (case, name)
    with zipfile.ZipFile(case / 'source.docx') as archive:
        assert archive.testzip() is None
        assert b'Preserve this source-only content.' in archive.read('customXml/retained.xml')
    path = Path((case / 'opened-path.txt').read_text())
    with sqlite3.connect(path.as_uri() + '?mode=ro', uri=True) as db:
        content = db.execute('SELECT content FROM documents WHERE type = 10104').fetchone()[0]
        if isinstance(content, str):
            content = content.encode()
        assert content == (case / 'expected.xml').read_bytes(), case
        ids = ''.join(f'{row[0]}\n' for row in db.execute('SELECT uuid FROM documents ORDER BY uuid')).encode()
        assert ids == (case / 'identities.txt').read_bytes(), case
    assert not list(case.rglob('*.starc.lock')), case
print('PASS: independent source hashes, DOCX archives, exact native story content/IDs and released locks')
PY
printf 'Evidence: %s\n' "$EVIDENCE"
