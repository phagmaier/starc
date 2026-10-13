#!/usr/bin/env bash
# Focused check of the story sync logic, manager and GitHub client against a local
# stand-in server; requires the debug development build. Nothing leaves the machine.
set -euo pipefail

ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
CXX=${CXX:-g++}
OUTPUT="$ROOT/.build/debug/_build"
[[ -f "$OUTPUT/libcorelib.so" ]] || {
    printf 'Run ./build/dev.sh build debug first.\n' >&2
    exit 1
}
mkdir -p "$ROOT/.build/evidence"
EVIDENCE=$(mktemp -d "$ROOT/.build/evidence/project-sync-XXXXXX")
read -r -a QT_CFLAGS <<< "$(pkg-config --cflags Qt6Core Qt6Gui Qt6Widgets Qt6Sql Qt6Network)"
read -r -a QT_LIBS <<< "$(pkg-config --libs Qt6Core Qt6Gui Qt6Widgets Qt6Sql Qt6Network)"

if ! "$CXX" -std=c++17 -g -fPIC -I "$ROOT/src" -I "$ROOT/src/corelib" \
    "${QT_CFLAGS[@]}" "$ROOT/tests/project_sync_test.cpp" \
    -L "$OUTPUT" -lcorelib "${QT_LIBS[@]}" -o "$EVIDENCE/project-sync-test" \
    > "$EVIDENCE/compile.log" 2>&1; then
    cat "$EVIDENCE/compile.log" >&2
    printf 'Evidence: %s\n' "$EVIDENCE" >&2
    exit 1
fi

if ! "$CXX" -std=c++17 -g -fPIC -I "$ROOT/src" -I "$ROOT/src/core" -I "$ROOT/src/corelib" \
    "${QT_CFLAGS[@]}" "$ROOT/tests/github_sync_manager_test.cpp" \
    -L "$OUTPUT" -L "$OUTPUT/plugins" -lcoreplugin -lcorelib "${QT_LIBS[@]}" \
    -Wl,-rpath,"$OUTPUT" -Wl,-rpath,"$OUTPUT/plugins" -o "$EVIDENCE/sync-manager-test" \
    > "$EVIDENCE/manager-compile.log" 2>&1; then
    cat "$EVIDENCE/manager-compile.log" >&2
    printf 'Evidence: %s\n' "$EVIDENCE" >&2
    exit 1
fi

start_server() {
    local repository=$1 log=$2 port=
    python3 "$ROOT/tests/github_mock_server.py" "$repository" > "$log" 2>&1 &
    SERVER=$!
    for _ in $(seq 1 50); do
        # The log may not exist yet while the server starts.
        port=$(sed -n 's/^PORT //p;q' "$log" 2>/dev/null || true)
        [[ -n "$port" ]] && break
        sleep 0.1
    done
    [[ -n "$port" ]] || {
        cat "$log" >&2
        printf 'The stand-in server did not start. Evidence: %s\n' "$EVIDENCE" >&2
        exit 1
    }
    export STARC_GITHUB_API_URL="http://127.0.0.1:$port"
}
SERVER=
trap '[[ -z "$SERVER" ]] || kill "$SERVER" 2>/dev/null || true' EXIT
start_server "$EVIDENCE/repository" "$EVIDENCE/server.log"
export LD_LIBRARY_PATH="$OUTPUT:$OUTPUT/libs:$OUTPUT/plugins${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
if ! "$EVIDENCE/project-sync-test" > "$EVIDENCE/test.log" 2>&1; then
    cat "$EVIDENCE/test.log" >&2
    printf 'Evidence: %s\n' "$EVIDENCE" >&2
    exit 1
fi
cat "$EVIDENCE/test.log"

# The client fixtures deliberately contain non-story bytes. Give the manager its
# own repository so its download checks use only the SQLite stories it creates.
kill "$SERVER"
wait "$SERVER" 2>/dev/null || true
start_server "$EVIDENCE/manager-repository" "$EVIDENCE/manager-server.log"

export STARC_PROFILE_DIR="$EVIDENCE/profile"
export XDG_CONFIG_HOME="$STARC_PROFILE_DIR/config" XDG_DATA_HOME="$STARC_PROFILE_DIR/data"
export XDG_CACHE_HOME="$STARC_PROFILE_DIR/cache" QT_QPA_PLATFORM=offscreen
# No desktop session bus or native keyring is used by the injected-credential check.
export DBUS_SESSION_BUS_ADDRESS="unix:path=$EVIDENCE/absent-session-bus"
mkdir -p "$XDG_CONFIG_HOME" "$XDG_DATA_HOME" "$XDG_CACHE_HOME" "$STARC_PROFILE_DIR/documents"
printf 'XDG_DOCUMENTS_DIR="%s"\n' "$STARC_PROFILE_DIR/documents" > "$XDG_CONFIG_HOME/user-dirs.dirs"
if ! "$EVIDENCE/sync-manager-test" > "$EVIDENCE/manager-test.log" 2>&1; then
    cat "$EVIDENCE/manager-test.log" >&2
    printf 'Evidence: %s\n' "$EVIDENCE" >&2
    exit 1
fi
cat "$EVIDENCE/manager-test.log"
printf 'Evidence: %s\n' "$EVIDENCE"
