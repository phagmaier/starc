#!/usr/bin/env bash
# Window layout restoration through the production application view and splitter.
set -euo pipefail

ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
CXX=${CXX:-g++}
OUTPUT="$ROOT/.build/debug/_build"
[[ -f "$OUTPUT/libcorelib.so" && -f "$OUTPUT/plugins/libcoreplugin.so" ]] || {
    printf 'Run ./build/dev.sh build debug first.\n' >&2
    exit 1
}
mkdir -p "$ROOT/.build/evidence"
EVIDENCE=$(mktemp -d "$ROOT/.build/evidence/window-restore-XXXXXX")
printf 'Evidence: %s\n' "$EVIDENCE"
mkdir -p "$EVIDENCE/profile/config" "$EVIDENCE/profile/data" "$EVIDENCE/profile/cache" \
    "$EVIDENCE/profile/documents"
read -r -a QT_CFLAGS <<< "$(pkg-config --cflags Qt6Core Qt6Gui Qt6Widgets)"
read -r -a QT_LIBS <<< "$(pkg-config --libs Qt6Core Qt6Gui Qt6Widgets)"
if ! "$CXX" -std=c++17 -g -fPIC -I "$ROOT/src" -I "$ROOT/src/core" -I "$ROOT/src/corelib" \
    "${QT_CFLAGS[@]}" "$ROOT/tests/window_restore_test.cpp" \
    -L "$OUTPUT" -L "$OUTPUT/plugins" -lcoreplugin -lcorelib \
    "${QT_LIBS[@]}" -o "$EVIDENCE/window-restore-test" > "$EVIDENCE/compile.log" 2>&1; then
    cat "$EVIDENCE/compile.log" >&2
    exit 1
fi
export XDG_CONFIG_HOME="$EVIDENCE/profile/config"
export XDG_DATA_HOME="$EVIDENCE/profile/data"
export XDG_CACHE_HOME="$EVIDENCE/profile/cache"
export STARC_PROFILE_DIR="$EVIDENCE/profile"
export DBUS_SESSION_BUS_ADDRESS="unix:path=$EVIDENCE/absent-session-bus"
printf 'XDG_DOCUMENTS_DIR="%s"\n' "$EVIDENCE/profile/documents" > "$XDG_CONFIG_HOME/user-dirs.dirs"
export QT_QPA_PLATFORM=${STARC_WINDOW_RESTORE_PLATFORM:-offscreen}
# Keep Qt's assertion text in the case log instead of the system journal.
export QT_FORCE_STDERR_LOGGING=1
export LD_LIBRARY_PATH="$OUTPUT:$OUTPUT/libs:$OUTPUT/plugins${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

# One process per case: an abort in one case cannot hide the others.
mapfile -t cases < <("$EVIDENCE/window-restore-test" --list 2> "$EVIDENCE/list.log")
(( ${#cases[@]} )) || { printf 'No window restore cases found.\n' >&2; exit 1; }
failed=()
for name in "${cases[@]}"; do
    status=0
    "$EVIDENCE/window-restore-test" "$name" > "$EVIDENCE/$name.log" 2>&1 || status=$?
    if (( status )); then
        failed+=("$name")
        printf 'FAIL: %s (exit %d)\n' "$name" "$status" >&2
        tail -3 "$EVIDENCE/$name.log" >&2
    fi
done
if (( ${#failed[@]} )); then
    printf 'FAIL: %d of %d window restore cases\n' "${#failed[@]}" "${#cases[@]}" >&2
    exit 1
fi
printf 'PASS: %d window restore cases\n' "${#cases[@]}"
