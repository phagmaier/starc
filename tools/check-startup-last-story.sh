#!/usr/bin/env bash
# Real application coordinator, disposable profiles, successive process launches and
# ordinary close. Defaults to offscreen; use STARC_STARTUP_PLATFORM=xcb on private Xvfb.
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
CXX=${CXX:-g++}
OUTPUT="$ROOT/.build/debug/_build"
[[ -f "$OUTPUT/libcorelib.so" && -f "$OUTPUT/plugins/libcoreplugin.so" ]] || {
    printf 'Run ./build/dev.sh build debug first.\n' >&2
    exit 1
}
mkdir -p "$ROOT/.build/evidence"
EVIDENCE=$(mktemp -d "$ROOT/.build/evidence/startup-last-story-XXXXXX")
printf 'Evidence: %s\n' "$EVIDENCE"
mkdir -p "$EVIDENCE/stories"
ln -s "$OUTPUT/plugins" "$EVIDENCE/plugins"
read -r -a QT_CFLAGS <<< "$(pkg-config --cflags Qt6Core Qt6Gui Qt6Widgets Qt6Sql Qt6Xml Qt6Network)"
read -r -a QT_LIBS <<< "$(pkg-config --libs Qt6Core Qt6Gui Qt6Widgets Qt6Sql Qt6Xml Qt6Network)"
if ! "$CXX" -std=c++17 -g -fPIC -I "$ROOT/src" -I "$ROOT/src/core" -I "$ROOT/src/corelib" \
    "${QT_CFLAGS[@]}" "$ROOT/tests/startup_last_story_test.cpp" "$ROOT/src/app/application.cpp" \
    "$ROOT/.build/debug/app/moc_application.cpp" \
    -L "$OUTPUT" -L "$OUTPUT/plugins" -lcoreplugin -lscreenplaytextplugin -lcorelib \
    "${QT_LIBS[@]}" -o "$EVIDENCE/startup-test" > "$EVIDENCE/compile.log" 2>&1; then
    cat "$EVIDENCE/compile.log" >&2
    exit 1
fi
if ! "$CXX" -std=c++17 -g -fPIC -shared "${QT_CFLAGS[@]}" \
    "$ROOT/tests/import_publish_collision.cpp" "${QT_LIBS[@]}" -ldl \
    -o "$EVIDENCE/import-collision.so" > "$EVIDENCE/collision-compile.log" 2>&1; then
    cat "$EVIDENCE/collision-compile.log" >&2
    exit 1
fi
export QT_QPA_PLATFORM=${STARC_STARTUP_PLATFORM:-offscreen}
export LD_LIBRARY_PATH="$OUTPUT:$OUTPUT/libs:$OUTPUT/plugins${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export STARC_STARTUP_TEST_ROOT="$EVIDENCE/stories"
export DBUS_SESSION_BUS_ADDRESS="unix:path=$EVIDENCE/absent-session-bus"
run_case() {
    local mode=$1 profile=$2
    local preload=${LD_PRELOAD:-}
    if [[ $mode == import-race ]]; then
        preload="$EVIDENCE/import-collision.so${preload:+:$preload}"
    fi
    export STARC_PROFILE_DIR="$EVIDENCE/$profile"
    if [[ ! -d "$STARC_PROFILE_DIR" && $mode != create && $mode != onboarding* ]]; then
        cp -a "$EVIDENCE/shared-profile" "$STARC_PROFILE_DIR"
    fi
    export XDG_CONFIG_HOME="$STARC_PROFILE_DIR/config"
    export XDG_DATA_HOME="$STARC_PROFILE_DIR/data"
    export XDG_CACHE_HOME="$STARC_PROFILE_DIR/cache"
    mkdir -p "$XDG_CONFIG_HOME" "$XDG_DATA_HOME" "$XDG_CACHE_HOME" "$STARC_PROFILE_DIR/documents"
    printf 'XDG_DOCUMENTS_DIR="%s"\n' "$STARC_PROFILE_DIR/documents" > "$XDG_CONFIG_HOME/user-dirs.dirs"
    if ! LD_PRELOAD="$preload" STARC_STARTUP_TEST_MODE="$mode" "$EVIDENCE/startup-test" \
        > "$EVIDENCE/$mode.log" 2>&1; then
        cat "$EVIDENCE/$mode.log" >&2
        printf 'Failed case: %s; evidence: %s\n' "$mode" "$EVIDENCE" >&2
        exit 1
    fi
    printf 'PASS: %s (ordinary exit 0)\n' "$mode"
}
run_case create shared-profile
cp "$EVIDENCE/stories/remembered.starc" "$EVIDENCE/stories/explicit.starc"
printf '<FinalDraft><Content><Paragraph>' > "$EVIDENCE/stories/malformed.fdx"
printf 'An unsupported source' > "$EVIDENCE/stories/unsupported.xyz"
printf 'Not a SQLite project' > "$EVIDENCE/stories/corrupt.starc"
printf 'INT. ROOM - DAY\n\nA source story survives.\n' > "$EVIDENCE/stories/source.fountain"
cp "$ROOT/tests/fixtures/interchange/authored.fdx" "$EVIDENCE/stories/source.fdx"
python3 - "$EVIDENCE/stories" <<'PY'
import hashlib
import json
from pathlib import Path
import shutil
import sqlite3
import sys
root = Path(sys.argv[1])
shutil.copyfile(root / 'remembered.starc', root / 'newer.starc')
with sqlite3.connect(root / 'newer.starc') as db:
    db.execute("INSERT OR REPLACE INTO system_variables VALUES ('application-version', '99.0.0')")
names = ['malformed.fdx', 'unsupported.xyz', 'corrupt.starc', 'newer.starc']
(root / 'failure-hashes.json').write_text(json.dumps(
    {name: hashlib.sha256((root / name).read_bytes()).hexdigest() for name in names}))
PY
run_case persisted shared-profile
for mode in disabled explicit explicit-failure unavailable early-event queued-event duplicate \
    onboarding onboarding-explicit locked failures save-as fountain fdx legacy-fountain legacy-fdx \
    auto-corrupt auto-malformed auto-newer import-existing import-locked \
    import-lock-only import-symlink import-dangling import-unwritable \
    import-race import-success import-copy import-copy-failure; do
    run_case "$mode" "$mode-profile"
done
run_case persisted-disabled disabled-profile
run_case persisted-explicit explicit-profile
run_case persisted-unavailable unavailable-profile
run_case persisted-fountain fountain-profile
run_case persisted-fdx fdx-profile
chmod 444 "$EVIDENCE/stories/remembered.starc"
run_case read-only read-only-profile
chmod 644 "$EVIDENCE/stories/remembered.starc"
python3 - "$EVIDENCE" <<'PY'
from pathlib import Path
import hashlib
import json
import sys
root = Path(sys.argv[1])
for path in root.glob('*.log'):
    if path.name in ('compile.log', 'collision-compile.log'):
        continue
    assert 'PASS: startup last story regression' in path.read_text(), path
print('PASS: all cases include ordinary shutdown and persisted-settings checks')
for name, expected in json.loads((root / 'stories/failure-hashes.json').read_text()).items():
    assert hashlib.sha256((root / 'stories' / name).read_bytes()).hexdigest() == expected, name
print('PASS: unavailable-format, corrupt and newer-version source bytes are unchanged')
PY
printf 'Evidence: %s\n' "$EVIDENCE"
