#!/usr/bin/env bash
# Real screenplay editor/coordinator regression, with disposable data and fresh-process reopen.
set -euo pipefail

ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
CXX=${CXX:-g++}
OUTPUT="$ROOT/.build/debug/_build"
[[ -f "$OUTPUT/libcorelib.so" && -f "$OUTPUT/plugins/libscreenplaytextplugin.so" ]] || {
    printf 'Run ./build/dev.sh build debug first.\n' >&2
    exit 1
}
mkdir -p "$ROOT/.build/evidence"
EVIDENCE=$(mktemp -d "$ROOT/.build/evidence/hidden-beats-XXXXXX")
printf 'Evidence: %s\n' "$EVIDENCE"
mkdir -p "$EVIDENCE/profile/config" "$EVIDENCE/profile/data" "$EVIDENCE/profile/cache" \
    "$EVIDENCE/profile/documents"
ln -s "$OUTPUT/plugins" "$EVIDENCE/plugins"
read -r -a QT_CFLAGS <<< "$(pkg-config --cflags Qt6Core Qt6Gui Qt6Widgets Qt6Sql Qt6Xml Qt6Network)"
read -r -a QT_LIBS <<< "$(pkg-config --libs Qt6Core Qt6Gui Qt6Widgets Qt6Sql Qt6Xml Qt6Network)"
if ! "$CXX" -std=c++17 -g -fPIC -I "$ROOT/src" -I "$ROOT/src/core" -I "$ROOT/src/corelib" \
    "${QT_CFLAGS[@]}" "$ROOT/tests/hidden_beats_test.cpp" \
    -L "$OUTPUT" -L "$OUTPUT/plugins" -lcoreplugin -lscreenplaytextplugin -lcorelib \
    "${QT_LIBS[@]}" -o "$EVIDENCE/hidden-beats-test" > "$EVIDENCE/compile.log" 2>&1; then
    cat "$EVIDENCE/compile.log" >&2
    exit 1
fi
export XDG_CONFIG_HOME="$EVIDENCE/profile/config"
export XDG_DATA_HOME="$EVIDENCE/profile/data"
export XDG_CACHE_HOME="$EVIDENCE/profile/cache"
export STARC_PROFILE_DIR="$EVIDENCE/profile"
export DBUS_SESSION_BUS_ADDRESS="unix:path=$EVIDENCE/absent-session-bus"
printf 'XDG_DOCUMENTS_DIR="%s"\n' "$EVIDENCE/profile/documents" > "$XDG_CONFIG_HOME/user-dirs.dirs"
export QT_QPA_PLATFORM=${STARC_HIDDEN_BEATS_PLATFORM:-offscreen}
export LD_LIBRARY_PATH="$OUTPUT:$OUTPUT/libs:$OUTPUT/plugins${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
for mode in exercise reopen; do
    if ! "$EVIDENCE/hidden-beats-test" "--$mode" "$EVIDENCE" > "$EVIDENCE/$mode.log" 2>&1; then
        tail -60 "$EVIDENCE/$mode.log" >&2
        exit 1
    fi
    tail -1 "$EVIDENCE/$mode.log"
done

python3 - "$EVIDENCE" <<'PY'
import json
import pathlib
import sys
import xml.etree.ElementTree as ET
root = pathlib.Path(sys.argv[1])
manifest = json.loads((root / "manifest.json").read_text())
for entry in manifest:
    name = entry["name"]
    before = ET.fromstring((root / (name + "-before.xml")).read_bytes())
    after = ET.fromstring((root / (name + "-after.xml")).read_bytes())
    if entry["hidden"]:
        # Compare identities, plot metadata and all annotated headings independently.
        def beats(tree):
            return [(beat.attrib, ET.tostring(beat.find("./content/beat_heading")))
                    for beat in tree.iter("beat")]
        assert beats(before), name + " must contain authored beats"
        assert beats(before) == beats(after), name + " altered hidden outline data"
    else:
        assert list(before.iter("beat")) and not list(after.iter("beat")), name
    if "-forward-" in name and not entry["singleKey"]:
        reverse = name.replace("-forward-", "-reverse-")
        other = ET.fromstring((root / (reverse + "-after.xml")).read_bytes())
        # Independent fixtures have different beat IDs and comment timestamps.
        for tree in (after, other):
            for beat in tree.iter("beat"):
                beat.attrib.pop("uuid", None)
            for comment in tree.iter("c"):
                comment.attrib.pop("date", None)
        assert ET.tostring(after) == ET.tostring(other), name + " depends on direction"
print("PASS: independent XML comparison of beat identities, metadata and selection direction")
PY
