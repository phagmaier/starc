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
EVIDENCE=$(mktemp -d "$ROOT/.build/evidence/review-comments-XXXXXX")
printf 'Evidence: %s\n' "$EVIDENCE"
mkdir -p "$EVIDENCE/profile/config" "$EVIDENCE/profile/data" "$EVIDENCE/profile/cache" \
    "$EVIDENCE/profile/documents"
ln -s "$OUTPUT/plugins" "$EVIDENCE/plugins"
read -r -a QT_CFLAGS <<< "$(pkg-config --cflags Qt6Core Qt6Gui Qt6Widgets Qt6Sql Qt6Xml Qt6Network)"
read -r -a QT_LIBS <<< "$(pkg-config --libs Qt6Core Qt6Gui Qt6Widgets Qt6Sql Qt6Xml Qt6Network)"
if ! "$CXX" -std=c++17 -g -fPIC -I "$ROOT/src" -I "$ROOT/src/core" -I "$ROOT/src/corelib" \
    "${QT_CFLAGS[@]}" "$ROOT/tests/review_comments_test.cpp" \
    -L "$OUTPUT" -L "$OUTPUT/plugins" -lcoreplugin -lscreenplaytextplugin -lcorelib \
    "${QT_LIBS[@]}" -o "$EVIDENCE/review-comments-test" > "$EVIDENCE/compile.log" 2>&1; then
    cat "$EVIDENCE/compile.log" >&2
    exit 1
fi
export XDG_CONFIG_HOME="$EVIDENCE/profile/config"
export XDG_DATA_HOME="$EVIDENCE/profile/data"
export XDG_CACHE_HOME="$EVIDENCE/profile/cache"
export STARC_PROFILE_DIR="$EVIDENCE/profile"
export DBUS_SESSION_BUS_ADDRESS="unix:path=$EVIDENCE/absent-session-bus"
printf 'XDG_DOCUMENTS_DIR="%s"\n' "$EVIDENCE/profile/documents" > "$XDG_CONFIG_HOME/user-dirs.dirs"
export QT_QPA_PLATFORM=${STARC_REVIEW_COMMENTS_PLATFORM:-offscreen}
export LD_LIBRARY_PATH="$OUTPUT:$OUTPUT/libs:$OUTPUT/plugins${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
for mode in exercise reopen; do
    if ! "$EVIDENCE/review-comments-test" "--$mode" "$EVIDENCE" > "$EVIDENCE/$mode.log" 2>&1; then
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

FIXTURE_DATES = ("2020-01-02T03:04:05", "2021-02-03T04:05:06")
root = pathlib.Path(sys.argv[1])
manifest = json.loads((root / "manifest.json").read_text())
for entry in manifest:
    name = entry["name"]
    tree = ET.fromstring((root / (name + "-after.xml")).read_bytes())
    # Saved marks for every character of the script, independently of the application.
    saved = []
    for paragraph in tree:
        marks = [None] * len(paragraph.findtext("v") or "")
        for mark in paragraph.iter("rm"):
            messages = [comment.text or "" for comment in mark.findall("c")]
            start = int(mark.attrib["from"])
            for position in range(start, start + int(mark.attrib["length"])):
                assert marks[position] is None, (name, "overlapping saved marks")
                marks[position] = (messages, mark.attrib.get("bgcolor"),
                                   mark.attrib.get("done") == "true")
        saved += marks + [None]
    saved.pop()
    assert len(saved) == len(entry["text"]), (name, "saved script text length")
    expected = [None] * len(saved)
    for thread in entry["threads"]:
        for position in range(thread["from"], thread["to"]):
            if entry["text"][position] != "\n":
                expected[position] = (thread["messages"], thread["color"], thread["done"])
    assert saved == expected, (name, "saved threads differ from the expected anchors")
    if not name.endswith("-forward"):
        continue
    reverse = ET.fromstring((root / (name[:-8] + "-reverse-after.xml")).read_bytes())
    for document in (tree, reverse):
        for comment in document.iter("c"):
            # Independently authored commands have different wall-clock dates.
            if comment.attrib.get("date") not in FIXTURE_DATES:
                comment.attrib.pop("date", None)
    assert ET.tostring(tree) == ET.tostring(reverse), (name, "selection direction")
print("PASS: independent XML anchors, payloads and selection-direction comparison")
PY
