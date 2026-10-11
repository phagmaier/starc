#!/usr/bin/env bash
# Focused check of the story sync logic and the GitHub client against a local
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
read -r -a QT_CFLAGS <<< "$(pkg-config --cflags Qt6Core Qt6Gui Qt6Sql Qt6Network)"
read -r -a QT_LIBS <<< "$(pkg-config --libs Qt6Core Qt6Gui Qt6Sql Qt6Network)"

if ! "$CXX" -std=c++17 -g -fPIC -I "$ROOT/src" -I "$ROOT/src/corelib" \
    "${QT_CFLAGS[@]}" "$ROOT/tests/project_sync_test.cpp" \
    -L "$OUTPUT" -lcorelib "${QT_LIBS[@]}" -o "$EVIDENCE/project-sync-test" \
    > "$EVIDENCE/compile.log" 2>&1; then
    cat "$EVIDENCE/compile.log" >&2
    printf 'Evidence: %s\n' "$EVIDENCE" >&2
    exit 1
fi

python3 "$ROOT/tests/github_mock_server.py" "$EVIDENCE/repository" > "$EVIDENCE/server.log" 2>&1 &
SERVER=$!
trap 'kill "$SERVER" 2>/dev/null || true' EXIT
PORT=
for _ in $(seq 1 50); do
    PORT=$(sed -n 's/^PORT //p;q' "$EVIDENCE/server.log")
    [[ -n "$PORT" ]] && break
    sleep 0.1
done
[[ -n "$PORT" ]] || {
    cat "$EVIDENCE/server.log" >&2
    printf 'The stand-in server did not start. Evidence: %s\n' "$EVIDENCE" >&2
    exit 1
}

export STARC_GITHUB_API_URL="http://127.0.0.1:$PORT"
export LD_LIBRARY_PATH="$OUTPUT${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
if ! "$EVIDENCE/project-sync-test" > "$EVIDENCE/test.log" 2>&1; then
    cat "$EVIDENCE/test.log" >&2
    printf 'Evidence: %s\n' "$EVIDENCE" >&2
    exit 1
fi
cat "$EVIDENCE/test.log"
printf 'Evidence: %s\n' "$EVIDENCE"
