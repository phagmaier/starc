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
EVIDENCE=$(mktemp -d "$ROOT/.build/evidence/track-changes-XXXXXX")
printf 'Evidence: %s\n' "$EVIDENCE"
mkdir -p "$EVIDENCE/profile/config" "$EVIDENCE/profile/data" "$EVIDENCE/profile/cache" \
    "$EVIDENCE/profile/documents"
ln -s "$OUTPUT/plugins" "$EVIDENCE/plugins"
read -r -a QT_CFLAGS <<< "$(pkg-config --cflags Qt6Core Qt6Gui Qt6Widgets Qt6Sql Qt6Xml Qt6Network)"
read -r -a QT_LIBS <<< "$(pkg-config --libs Qt6Core Qt6Gui Qt6Widgets Qt6Sql Qt6Xml Qt6Network)"
if ! "$CXX" -std=c++17 -g -fPIC -I "$ROOT/src" -I "$ROOT/src/core" -I "$ROOT/src/corelib" \
    "${QT_CFLAGS[@]}" "$ROOT/tests/track_changes_test.cpp" \
    -L "$OUTPUT" -L "$OUTPUT/plugins" -lcoreplugin -lscreenplaytextplugin -lcorelib \
    "${QT_LIBS[@]}" -o "$EVIDENCE/track-changes-test" > "$EVIDENCE/compile.log" 2>&1; then
    cat "$EVIDENCE/compile.log" >&2
    exit 1
fi
export XDG_CONFIG_HOME="$EVIDENCE/profile/config"
export XDG_DATA_HOME="$EVIDENCE/profile/data"
export XDG_CACHE_HOME="$EVIDENCE/profile/cache"
export STARC_PROFILE_DIR="$EVIDENCE/profile"
export DBUS_SESSION_BUS_ADDRESS="unix:path=$EVIDENCE/absent-session-bus"
printf 'XDG_DOCUMENTS_DIR="%s"\n' "$EVIDENCE/profile/documents" > "$XDG_CONFIG_HOME/user-dirs.dirs"
export QT_QPA_PLATFORM=${STARC_TRACK_CHANGES_PLATFORM:-offscreen}
export LD_LIBRARY_PATH="$OUTPUT:$OUTPUT/libs:$OUTPUT/plugins${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
for mode in exercise reopen; do
    if ! "$EVIDENCE/track-changes-test" "--$mode" "$EVIDENCE" > "$EVIDENCE/$mode.log" 2>&1; then
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


def normalized(name):
    tree = ET.fromstring((root / (name + "-after.xml")).read_bytes())
    for comment in tree.iter("c"):
        # Separately exercised commands have different wall-clock review dates.
        comment.attrib.pop("date", None)
    if name.startswith("all-added-"):
        # Deleting the whole script creates a new default scene in each independent
        # fixture. Within-case IDs remain byte-exact in Undo/Redo and reopening checks.
        for scene in tree.iter("scene"):
            assert scene.attrib.get("uuid"), "default scene must have an identity"
            scene.attrib.pop("uuid")
    return ET.tostring(tree)


for entry in manifest:
    name = entry["name"]
    if "-forward-" in name:
        reverse = name.replace("-forward-", "-reverse-")
        assert normalized(name) == normalized(reverse), name + " differs by selection direction"
print("PASS: independent XML comparison of both selection directions")
PY
