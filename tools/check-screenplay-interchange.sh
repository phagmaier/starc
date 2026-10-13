#!/usr/bin/env bash
# Independent Fountain/FDX fixtures, application models, and persisted project round trips.
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
CXX=${CXX:-g++}
OUTPUT="$ROOT/.build/debug/_build"
[[ -f "$OUTPUT/libcorelib.so" && -f "$OUTPUT/plugins/libcoreplugin.so" ]] || {
    printf 'Run ./build/dev.sh build debug first.\n' >&2
    exit 1
}
mkdir -p "$ROOT/.build/evidence"
EVIDENCE=$(mktemp -d "$ROOT/.build/evidence/screenplay-interchange-XXXXXX")
printf 'Evidence: %s\n' "$EVIDENCE"
mkdir -p "$EVIDENCE/profile"/{config,data,cache,documents} "$EVIDENCE/checks"
ln -s "$OUTPUT/plugins" "$EVIDENCE/plugins"
read -r -a QT_CFLAGS <<< "$(pkg-config --cflags Qt6Core Qt6Gui Qt6Widgets Qt6Sql Qt6Xml Qt6Network)"
read -r -a QT_LIBS <<< "$(pkg-config --libs Qt6Core Qt6Gui Qt6Widgets Qt6Sql Qt6Xml Qt6Network)"
if ! "$CXX" -std=c++17 -g -fPIC -I "$ROOT/src" -I "$ROOT/src/core" -I "$ROOT/src/corelib" \
    "${QT_CFLAGS[@]}" "$ROOT/tests/screenplay_interchange_test.cpp" \
    -L "$OUTPUT" -L "$OUTPUT/plugins" -lcoreplugin -lscreenplaytextplugin -lcorelib \
    "${QT_LIBS[@]}" -o "$EVIDENCE/interchange-test" > "$EVIDENCE/compile.log" 2>&1; then
    cat "$EVIDENCE/compile.log" >&2
    exit 1
fi
export XDG_CONFIG_HOME="$EVIDENCE/profile/config"
export XDG_DATA_HOME="$EVIDENCE/profile/data"
export XDG_CACHE_HOME="$EVIDENCE/profile/cache"
export STARC_PROFILE_DIR="$EVIDENCE/profile"
export DBUS_SESSION_BUS_ADDRESS="unix:path=$EVIDENCE/absent-session-bus"
printf 'XDG_DOCUMENTS_DIR="%s"\n' "$EVIDENCE/profile/documents" > "$XDG_CONFIG_HOME/user-dirs.dirs"
export QT_QPA_PLATFORM=offscreen
export QT_FORCE_STDERR_LOGGING=1
export LD_LIBRARY_PATH="$OUTPUT:$OUTPUT/libs:$OUTPUT/plugins${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
python3 - "$ROOT/tests/fixtures/interchange" "$EVIDENCE" <<'PY'
import hashlib
import json
from pathlib import Path
import sys
fixtures, evidence = map(Path, sys.argv[1:])
hashes = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in fixtures.iterdir() if p.is_file()}
(evidence / "fixture-hashes.json").write_text(json.dumps(hashes, indent=2) + "\n")
PY
if ! "$EVIDENCE/interchange-test" "$EVIDENCE/checks" "$ROOT/tests/fixtures/interchange" \
    > "$EVIDENCE/test.log" 2>&1; then
    cat "$EVIDENCE/test.log" >&2
    exit 1
fi
if ! "$EVIDENCE/interchange-test" "$EVIDENCE/checks" "$ROOT/tests/fixtures/interchange" reopen \
    > "$EVIDENCE/reopen.log" 2>&1; then
    cat "$EVIDENCE/reopen.log" >&2
    exit 1
fi
if ! python3 "$ROOT/tests/check_interchange_exports.py" "$EVIDENCE" \
    "$ROOT/tests/fixtures/interchange" >> "$EVIDENCE/test.log" 2>&1; then
    cat "$EVIDENCE/test.log" >&2
    exit 1
fi
cat "$EVIDENCE/test.log"
cat "$EVIDENCE/reopen.log"
printf 'Evidence: %s\n' "$EVIDENCE"
