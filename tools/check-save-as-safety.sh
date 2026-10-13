#!/usr/bin/env bash
# Save As through the real coordinator, ordinary exits and fresh-process reopen.
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
CXX=${CXX:-g++}
OUTPUT="$ROOT/.build/debug/_build"
mkdir -p "$ROOT/.build/evidence"
EVIDENCE=$(mktemp -d "$ROOT/.build/evidence/save-as-safety-XXXXXX")
printf 'Evidence: %s\n' "$EVIDENCE"
ln -s "$OUTPUT/plugins" "$EVIDENCE/plugins"
read -r -a QT_CFLAGS <<< "$(pkg-config --cflags Qt6Core Qt6Gui Qt6Widgets Qt6Sql Qt6Xml Qt6Network)"
read -r -a QT_LIBS <<< "$(pkg-config --libs Qt6Core Qt6Gui Qt6Widgets Qt6Sql Qt6Xml Qt6Network)"
if ! "$CXX" -std=c++17 -g -fPIC -I "$ROOT/src" -I "$ROOT/src/core" -I "$ROOT/src/corelib" \
    "${QT_CFLAGS[@]}" "$ROOT/tests/startup_last_story_test.cpp" "$ROOT/src/app/application.cpp" \
    "$ROOT/.build/debug/app/moc_application.cpp" \
    -L "$OUTPUT" -L "$OUTPUT/plugins" -lcoreplugin -lscreenplaytextplugin -lcorelib \
    "${QT_LIBS[@]}" -o "$EVIDENCE/save-as-test" > "$EVIDENCE/compile.log" 2>&1; then
    cat "$EVIDENCE/compile.log" >&2
    exit 1
fi
if ! "$CXX" -std=c++17 -g -fPIC -shared -I "$ROOT/src/corelib" "${QT_CFLAGS[@]}" "$ROOT/tests/save_as_fault.cpp" \
    "${QT_LIBS[@]}" -ldl -o "$EVIDENCE/save-as-fault.so" > "$EVIDENCE/fault-compile.log" 2>&1; then
    cat "$EVIDENCE/fault-compile.log" >&2
    exit 1
fi
export QT_QPA_PLATFORM=${STARC_SAVE_AS_PLATFORM:-offscreen}
export LD_LIBRARY_PATH="$OUTPUT:$OUTPUT/libs:$OUTPUT/plugins${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export DBUS_SESSION_BUS_ADDRESS="unix:path=$EVIDENCE/absent-session-bus"
run_case() {
    local mode=$1 root=$2
    export STARC_STARTUP_TEST_ROOT="$root" STARC_PROFILE_DIR="$root/profile"
    export XDG_CONFIG_HOME="$STARC_PROFILE_DIR/config" XDG_DATA_HOME="$STARC_PROFILE_DIR/data"
    export XDG_CACHE_HOME="$STARC_PROFILE_DIR/cache"
    mkdir -p "$XDG_CONFIG_HOME" "$XDG_DATA_HOME" "$XDG_CACHE_HOME" "$STARC_PROFILE_DIR/documents"
    printf 'XDG_DOCUMENTS_DIR="%s"\n' "$STARC_PROFILE_DIR/documents" > "$XDG_CONFIG_HOME/user-dirs.dirs"
    export STARC_SAVE_AS_TARGET="$root/destination.starc"
    if ! STARC_STARTUP_TEST_MODE="$mode" \
        LD_PRELOAD="$EVIDENCE/save-as-fault.so${LD_PRELOAD:+:$LD_PRELOAD}" \
        "$EVIDENCE/save-as-test" > "$root/$mode.log" 2>&1; then
        tail -100 "$root/$mode.log" >&2
        printf 'Failed case: %s; evidence: %s\n' "$mode" "$EVIDENCE" >&2
        exit 1
    fi
    printf 'PASS: %s (ordinary exit 0)\n' "$mode"
}
mkdir -p "$EVIDENCE/seed"
run_case create "$EVIDENCE/seed"
rg -q 'test: injected asynchronous memory credential store' "$EVIDENCE/seed/create.log"
for mode in ${STARC_SAVE_AS_CASES:-locked lock-only new existing no-suffix cancel replacement-cancel same symlink dangling directory unwritable copy-failure copy-failure-new write-failure write-failure-new publish-failure publish-failure-new race}; do
    case_root="$EVIDENCE/$mode"
    mkdir -p "$case_root"
    cp "$EVIDENCE/seed/remembered.starc" "$case_root/remembered.starc"
    cp "$EVIDENCE/seed/script.xml" "$case_root/script.xml"
    cp -a "$EVIDENCE/seed/profile" "$case_root/profile"
    python3 - "$case_root" "$mode" <<'PY'
import hashlib
import json
from pathlib import Path
import shutil
import sqlite3
import sys
root, mode = Path(sys.argv[1]), sys.argv[2]
target = root / 'destination.starc'
if mode in ['locked', 'existing', 'replacement-cancel', 'copy-failure', 'write-failure', 'publish-failure']:
    shutil.copyfile(root / 'remembered.starc', target)
    with sqlite3.connect(target) as db:
        db.execute("UPDATE documents SET content = replace(content, 'Zoë keeps', 'DESTINATION keeps') WHERE type = 10104")
if mode in ['symlink', 'dangling']:
    referent = root / 'referent.starc'
    if mode == 'symlink':
        shutil.copyfile(root / 'remembered.starc', referent)
    target.symlink_to(referent)
with sqlite3.connect(root / 'remembered.starc') as db:
    identities = db.execute('SELECT id, uuid, type FROM documents ORDER BY id').fetchall()
(root / 'identities.json').write_text(json.dumps(identities))
(root / 'destination-before.sha256').write_text(hashlib.sha256(target.read_bytes()).hexdigest() if target.is_file() else '')
PY
    run_case "save-as-$mode" "$case_root"
    if [[ $mode == write-failure* ]]; then
        rg -q 'fault: kernel-limited staged write' "$case_root/save-as-$mode.log"
    elif [[ $mode == publish-failure* ]]; then
        rg -q 'fault: final publication refused=1' "$case_root/save-as-$mode.log"
    elif [[ $mode == race ]]; then
        rg -q 'fault: competing destination published' "$case_root/save-as-$mode.log"
    fi
    run_case persisted-save-as "$case_root"
done
python3 - "$EVIDENCE" <<'PY'
import hashlib
import json
from pathlib import Path
import sqlite3
import sys
root = Path(sys.argv[1])
for case in root.iterdir():
    if not case.is_dir() or not (case / 'identities.json').exists():
        continue
    successful = case.name in ['new', 'existing', 'no-suffix']
    story = case / ('destination.starc' if successful else 'remembered.starc')
    with sqlite3.connect(story) as db:
        assert db.execute('PRAGMA integrity_check').fetchall() == [('ok',)], case
        script = db.execute('SELECT content FROM documents WHERE type = 10104').fetchone()[0]
        script_bytes = script.encode('utf-8') if isinstance(script, str) else script
        assert script_bytes == (case / 'script.xml').read_bytes(), case
        assert [list(row) for row in db.execute('SELECT id, uuid, type FROM documents ORDER BY id')] == json.loads((case / 'identities.json').read_text()), case
    expected = (case / 'destination-before.sha256').read_text()
    if expected and not successful:
        assert hashlib.sha256((case / 'destination.starc').read_bytes()).hexdigest() == expected, case
    assert not list(case.glob('*.lock')), case
print('PASS: independent SQLite integrity, script/annotation/identity, destination-byte and released-lock checks')
PY
printf 'Evidence: %s\n' "$EVIDENCE"
