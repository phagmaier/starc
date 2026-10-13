#!/usr/bin/env bash
# Real editor/coordinator failure, recovery, ordinary close and fresh-process reopen.
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
CXX=${CXX:-g++}
OUTPUT="$ROOT/.build/debug/_build"
mkdir -p "$ROOT/.build/evidence"
EVIDENCE=$(mktemp -d "$ROOT/.build/evidence/save-failure-XXXXXX")
printf 'Evidence: %s\n' "$EVIDENCE"
ln -s "$OUTPUT/plugins" "$EVIDENCE/plugins"
read -r -a QT_CFLAGS <<< "$(pkg-config --cflags Qt6Core Qt6Gui Qt6Widgets Qt6Sql Qt6Xml Qt6Network)"
read -r -a QT_LIBS <<< "$(pkg-config --libs Qt6Core Qt6Gui Qt6Widgets Qt6Sql Qt6Xml Qt6Network)"
if ! "$CXX" -std=c++17 -g -fPIC -I "$ROOT/src" -I "$ROOT/src/core" -I "$ROOT/src/corelib" \
    "${QT_CFLAGS[@]}" "$ROOT/tests/save_failure_test.cpp" "$ROOT/src/app/application.cpp" \
    "$ROOT/.build/debug/app/moc_application.cpp" \
    -L "$OUTPUT" -L "$OUTPUT/plugins" -lcoreplugin -lscreenplaytextplugin -lcorelib \
    "${QT_LIBS[@]}" -o "$EVIDENCE/save-failure-test" > "$EVIDENCE/compile.log" 2>&1; then
    cat "$EVIDENCE/compile.log" >&2
    exit 1
fi
export QT_QPA_PLATFORM=${STARC_SAVE_FAILURE_PLATFORM:-offscreen}
export LD_LIBRARY_PATH="$OUTPUT:$OUTPUT/libs:$OUTPUT/plugins${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export DBUS_SESSION_BUS_ADDRESS="unix:path=$EVIDENCE/absent-session-bus"
run_case() {
    local mode=$1 root=$2 marker=$3
    export STARC_SAVE_FAILURE_ROOT="$root"
    export STARC_PROFILE_DIR="$root/profile"
    export XDG_CONFIG_HOME="$STARC_PROFILE_DIR/config"
    export XDG_DATA_HOME="$STARC_PROFILE_DIR/data"
    export XDG_CACHE_HOME="$STARC_PROFILE_DIR/cache"
    mkdir -p "$XDG_CONFIG_HOME" "$XDG_DATA_HOME" "$XDG_CACHE_HOME" "$STARC_PROFILE_DIR/documents"
    printf 'XDG_DOCUMENTS_DIR="%s"\n' "$STARC_PROFILE_DIR/documents" > "$XDG_CONFIG_HOME/user-dirs.dirs"
    if ! STARC_SAVE_FAILURE_MODE="$mode" STARC_EXPECT_MARKER="$marker" \
        "$EVIDENCE/save-failure-test" > "$root/$mode.log" 2>&1; then
        tail -100 "$root/$mode.log" >&2
        printf 'Failed case: %s; evidence: %s\n' "$mode" "$EVIDENCE" >&2
        exit 1
    fi
    printf 'PASS: %s (ordinary exit 0)\n' "$mode"
}
for mode in ${STARC_SAVE_FAILURE_CASES:-close-retry close-dismiss close-discard commit-retry commit-dismiss commit-discard switch-failure sync-failure sync-success}; do
    if [[ $mode == sync-* ]]; then
        mkdir -p "$EVIDENCE/$mode"
        python3 "$ROOT/tests/github_mock_server.py" "$EVIDENCE/$mode/repository" \
            > "$EVIDENCE/$mode/server.log" 2>&1 &
        SERVER=$!
        trap 'kill "$SERVER" 2>/dev/null || true' EXIT
        PORT=
        for _ in $(seq 1 50); do
            PORT=$(sed -n 's/^PORT //p;q' "$EVIDENCE/$mode/server.log" 2>/dev/null || true)
            [[ -n "$PORT" ]] && break
            sleep 0.1
        done
        [[ -n "$PORT" ]] || { cat "$EVIDENCE/$mode/server.log" >&2; exit 1; }
        export STARC_GITHUB_API_URL="http://127.0.0.1:$PORT"
    fi
    marker=1
    [[ $mode != close-discard && $mode != commit-discard && $mode != sync-success ]] || marker=0
    run_case "$mode" "$EVIDENCE/$mode" "$marker"
    run_case reopen "$EVIDENCE/$mode" "$marker"
    if [[ $mode == sync-* ]]; then
        kill "$SERVER"
        wait "$SERVER" 2>/dev/null || true
        trap - EXIT
    fi
done
printf 'Evidence: %s\n' "$EVIDENCE"
