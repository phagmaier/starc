#!/usr/bin/env bash
# Actual exporters and coordinator: failed source output, Retry, close and fresh reopen.
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
CXX=${CXX:-g++}
OUTPUT="$ROOT/.build/debug/_build"
mkdir -p "$ROOT/.build/evidence"
EVIDENCE=$(mktemp -d "$ROOT/.build/evidence/source-export-failure-XXXXXX")
printf 'Evidence: %s\n' "$EVIDENCE"
ln -s "$OUTPUT/plugins" "$EVIDENCE/plugins"
read -r -a QT_CFLAGS <<< "$(pkg-config --cflags Qt6Core Qt6Gui Qt6Widgets Qt6Sql Qt6Xml Qt6Network)"
read -r -a QT_LIBS <<< "$(pkg-config --libs Qt6Core Qt6Gui Qt6Widgets Qt6Sql Qt6Xml Qt6Network)"
if ! "$CXX" -std=c++17 -g -fPIC -I "$ROOT/src" -I "$ROOT/src/core" -I "$ROOT/src/corelib" \
    "${QT_CFLAGS[@]}" "$ROOT/tests/source_export_failure_test.cpp" "$ROOT/src/app/application.cpp" \
    "$ROOT/.build/debug/app/moc_application.cpp" \
    -L "$OUTPUT" -L "$OUTPUT/plugins" -lcoreplugin -lscreenplaytextplugin -lcorelib \
    "${QT_LIBS[@]}" -o "$EVIDENCE/source-export-failure-test" > "$EVIDENCE/compile.log" 2>&1; then
    cat "$EVIDENCE/compile.log" >&2
    exit 1
fi
if ! "$CXX" -std=c++17 -g -fPIC -shared "${QT_CFLAGS[@]}" \
    "$ROOT/tests/source_export_failure_fault.cpp" "${QT_LIBS[@]}" -ldl \
    -o "$EVIDENCE/source-export-fault.so" > "$EVIDENCE/fault-compile.log" 2>&1; then
    cat "$EVIDENCE/fault-compile.log" >&2
    exit 1
fi
export QT_QPA_PLATFORM=${STARC_SOURCE_EXPORT_PLATFORM:-offscreen}
export LD_LIBRARY_PATH="$OUTPUT:$OUTPUT/libs:$OUTPUT/plugins${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export DBUS_SESSION_BUS_ADDRESS="unix:path=$EVIDENCE/absent-session-bus"
run_case() {
    local mode=$1 root=$2 suffix=$3 marker=$4
    export STARC_SOURCE_EXPORT_ROOT="$root" STARC_SOURCE_EXPORT_SUFFIX="$suffix"
    export STARC_PROFILE_DIR="$root/profile"
    export XDG_CONFIG_HOME="$STARC_PROFILE_DIR/config"
    export XDG_DATA_HOME="$STARC_PROFILE_DIR/data"
    export XDG_CACHE_HOME="$STARC_PROFILE_DIR/cache"
    mkdir -p "$XDG_CONFIG_HOME" "$XDG_DATA_HOME" "$XDG_CACHE_HOME" "$STARC_PROFILE_DIR/documents"
    printf 'XDG_DOCUMENTS_DIR="%s"\n' "$STARC_PROFILE_DIR/documents" > "$XDG_CONFIG_HOME/user-dirs.dirs"
    if ! STARC_SOURCE_EXPORT_MODE="$mode" STARC_EXPECT_MARKER="$marker" \
        LD_PRELOAD="$EVIDENCE/source-export-fault.so${LD_PRELOAD:+:$LD_PRELOAD}" \
        "$EVIDENCE/source-export-failure-test" > "$root/$mode.log" 2>&1; then
        tail -100 "$root/$mode.log" >&2
        printf 'Failed case: %s/%s; evidence: %s\n' "$suffix" "$mode" "$EVIDENCE" >&2
        exit 1
    fi
    if [[ $mode == partial-* || $mode == publish-* ]]; then
        rg -q "fault: ${mode%%-*} at source publication; result=0" "$root/$mode.log"
    fi
    printf 'PASS: %s/%s (ordinary exit 0)\n' "$suffix" "$mode"
}
for suffix in ${STARC_SOURCE_EXPORT_SUFFIXES:-fountain txt fdx fdxt}; do
    for mode in ${STARC_SOURCE_EXPORT_CASES:-permission-retry permission-close permission-discard partial-retry partial-close partial-discard publish-retry publish-close publish-discard export clipboard}; do
        root="$EVIDENCE/$suffix/$mode"
        mkdir -p "$root"
        if [[ $suffix == fdx || $suffix == fdxt ]]; then
            cat > "$root/source.$suffix" <<'FDX'
<?xml version="1.0" encoding="UTF-8"?>
<FinalDraft DocumentType="Script" Version="1"><Content>
<Paragraph Type="Scene Heading"><Text>INT. ROOM - DAY</Text></Paragraph>
<Paragraph Type="Action"><Text>A quiet room.</Text></Paragraph>
<Paragraph Type="Action"><Text Style="Bold">A bold clue.</Text></Paragraph>
</Content></FinalDraft>
FDX
        else
            cat > "$root/source.$suffix" <<'FOUNTAIN'
INT. ROOM - DAY

!A quiet room.

!A **bold** clue.

!Another clue.[[Check clue]]
FOUNTAIN
        fi
        marker=1
        [[ $mode != *discard && $mode != export && $mode != clipboard ]] || marker=0
        run_case "$mode" "$root" "$suffix" "$marker"
        run_case reopen "$root" "$suffix" "$marker"
    done
done
printf 'Evidence: %s\n' "$EVIDENCE"
