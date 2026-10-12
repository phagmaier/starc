#!/usr/bin/env bash
# Injected-backend checks, real settings persistence, UI population and local GitHub API.
# The native adapter has a separate private-bus runner; no ordinary keyring is accessed.
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
OUTPUT="$ROOT/.build/debug/_build"
mkdir -p "$ROOT/.build/evidence"
EVIDENCE=$(mktemp -d "$ROOT/.build/evidence/github-credentials-XXXXXX")
mkdir -p "$EVIDENCE/profile" "$EVIDENCE/repository"
read -r -a QT_CFLAGS <<< "$(pkg-config --cflags Qt6Core Qt6Gui Qt6Widgets Qt6Sql Qt6Xml Qt6Network)"
read -r -a QT_LIBS <<< "$(pkg-config --libs Qt6Core Qt6Gui Qt6Widgets Qt6Sql Qt6Xml Qt6Network)"
if ! "${CXX:-g++}" -std=c++17 -g -fPIC -I "$ROOT/src" -I "$ROOT/src/core" -I "$ROOT/src/corelib" \
    "${QT_CFLAGS[@]}" "$ROOT/tests/github_credential_store_test.cpp" \
    -L "$OUTPUT" -L "$OUTPUT/plugins" -lcoreplugin -lcorelib "${QT_LIBS[@]}" \
    -Wl,-rpath,"$OUTPUT" -Wl,-rpath,"$OUTPUT/plugins" -o "$EVIDENCE/credential-test" \
    > "$EVIDENCE/compile.log" 2>&1; then
    cat "$EVIDENCE/compile.log" >&2; printf 'Evidence: %s\n' "$EVIDENCE" >&2; exit 1
fi
export XDG_CONFIG_HOME="$EVIDENCE/profile/config" XDG_DATA_HOME="$EVIDENCE/profile/data"
export XDG_CACHE_HOME="$EVIDENCE/profile/cache" QT_QPA_PLATFORM=offscreen
export STARC_PROFILE_DIR="$EVIDENCE/profile"
# Disable access to any desktop session bus, including automatic activation.
export DBUS_SESSION_BUS_ADDRESS="unix:path=$EVIDENCE/absent-session-bus"
export LD_LIBRARY_PATH="$OUTPUT:$OUTPUT/libs:$OUTPUT/plugins${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
mkdir -p "$XDG_CONFIG_HOME" "$XDG_DATA_HOME" "$XDG_CACHE_HOME" "$EVIDENCE/profile/documents"
printf 'XDG_DOCUMENTS_DIR="%s"\n' "$EVIDENCE/profile/documents" > "$XDG_CONFIG_HOME/user-dirs.dirs"
python3 "$ROOT/tests/github_mock_server.py" "$EVIDENCE/repository" > "$EVIDENCE/server.log" 2>&1 &
SERVER=$!
trap 'kill "$SERVER" 2>/dev/null || true' EXIT
PORT=
for _ in $(seq 1 50); do
    PORT=$(sed -n 's/^PORT //p;q' "$EVIDENCE/server.log" 2>/dev/null || true)
    [[ -n "$PORT" ]] && break
    sleep 0.1
done
[[ -n "$PORT" ]] || { printf 'Local server did not start. Evidence: %s\n' "$EVIDENCE" >&2; exit 1; }
export STARC_GITHUB_API_URL="http://127.0.0.1:$PORT"
if ! "$EVIDENCE/credential-test" > "$EVIDENCE/injected.log" 2>&1; then
    cat "$EVIDENCE/injected.log" >&2; printf 'Evidence: %s\n' "$EVIDENCE" >&2; exit 1
fi
cat "$EVIDENCE/injected.log"
printf 'Injected evidence: %s\nNative adapter: python3 tests/isolated_keyring.py %s -- %s/credential-test --native app.starc.test.credentials\n' \
    "$EVIDENCE" "$EVIDENCE/native" "$EVIDENCE"
