#!/usr/bin/env bash
# Actual source Save, ordinary close, and fresh application reopening with annotations
# and forced Action paragraphs.
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
OUTPUT="$ROOT/.build/debug/_build"
CXX=${CXX:-g++}
mkdir -p "$ROOT/.build/evidence"
EVIDENCE=$(mktemp -d "$ROOT/.build/evidence/fountain-review-source-XXXXXX")
printf 'Evidence: %s\n' "$EVIDENCE"
ln -s "$OUTPUT/plugins" "$EVIDENCE/plugins"
read -r -a QT_CFLAGS <<< "$(pkg-config --cflags Qt6Core Qt6Gui Qt6Widgets Qt6Sql Qt6Xml Qt6Network)"
read -r -a QT_LIBS <<< "$(pkg-config --libs Qt6Core Qt6Gui Qt6Widgets Qt6Sql Qt6Xml Qt6Network)"
if ! "$CXX" -std=c++17 -g -fPIC -I "$ROOT/src" -I "$ROOT/src/core" -I "$ROOT/src/corelib" \
    "${QT_CFLAGS[@]}" "$ROOT/tests/fountain_review_source_test.cpp" "$ROOT/src/app/application.cpp" \
    "$ROOT/.build/debug/app/moc_application.cpp" \
    -L "$OUTPUT" -L "$OUTPUT/plugins" -lcoreplugin -lscreenplaytextplugin -lcorelib \
    "${QT_LIBS[@]}" -o "$EVIDENCE/fountain-review-source-test" > "$EVIDENCE/compile.log" 2>&1; then
    cat "$EVIDENCE/compile.log" >&2
    exit 1
fi
export QT_QPA_PLATFORM=${STARC_REVIEW_SOURCE_PLATFORM:-offscreen}
export QT_FORCE_STDERR_LOGGING=1
export LD_LIBRARY_PATH="$OUTPUT:$OUTPUT/libs:$OUTPUT/plugins${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export DBUS_SESSION_BUS_ADDRESS="unix:path=$EVIDENCE/absent-session-bus"
for fixture in emphasis-review review-offsets forced-action forced-action-first; do
    case_root="$EVIDENCE/$fixture"
    mkdir -p "$case_root"
    cp "$ROOT/tests/fixtures/interchange/$fixture.fountain" "$case_root/source.fountain"
    for mode in ${STARC_REVIEW_SOURCE_MODES:-save reopen}; do
        export STARC_PROFILE_DIR="$case_root/$mode-profile"
        export XDG_CONFIG_HOME="$STARC_PROFILE_DIR/config"
        export XDG_DATA_HOME="$STARC_PROFILE_DIR/data"
        export XDG_CACHE_HOME="$STARC_PROFILE_DIR/cache"
        mkdir -p "$XDG_CONFIG_HOME" "$XDG_DATA_HOME" "$XDG_CACHE_HOME" "$STARC_PROFILE_DIR/documents"
        printf 'XDG_DOCUMENTS_DIR="%s"\n' "$STARC_PROFILE_DIR/documents" > "$XDG_CONFIG_HOME/user-dirs.dirs"
        if ! "$EVIDENCE/fountain-review-source-test" "$case_root" "$mode" > "$case_root/$mode.log" 2>&1; then
            tail -80 "$case_root/$mode.log" >&2
            exit 1
        fi
        printf 'PASS: %s/%s (ordinary exit 0)\n' "$fixture" "$mode"
    done
done
python3 - "$EVIDENCE" "$ROOT/tests/fixtures/interchange" <<'PY'
import json
from pathlib import Path
import sys
evidence, fixtures = map(Path, sys.argv[1:])
for name in ("emphasis-review", "review-offsets", "forced-action", "forced-action-first"):
    expected = (fixtures / (name + ".fountain")).read_text()
    if name == "emphasis-review":
        expected = expected.replace("!A **bold** clue.", "A **bold** clue.")
    body, _, last = expected.rstrip("\n").rpartition("\n")
    # Typing at the end of the fully reviewed final line naturally extends its
    # review coverage to the marker. The note must follow the added characters.
    anchor = last.rfind("[[") if "[[" in last else len(last)
    expected = body + "\n" + last[:anchor] + " SAVEMARKER" + last[anchor:] + "\n"
    actual = (evidence / name / "source.fountain").read_text()
    assert actual == expected, (name, actual, expected)
# The saved model, which the fresh application import must equal, holds the authored
# paragraph types and complete text: no forced Action became another kind of paragraph.
rows = json.loads((evidence / "forced-action" / "expected.json").read_text())
assert [(row[0], row[1]) for row in rows] == [
    ("scene_heading", "INT. STUDIO - DAY"), ("action", "# A private instruction"),
    ("action", "@address"), ("action", ">A direction"), ("action", "= A hint"),
    ("action", "~ A hummed sound"), ("action", "INT. The camera opens - Day"),
    ("action", ".A dotted remark"), ("action", "...and then silence."),
    ("action", "!Twice forced"), ("action", "EXT is short for exterior."),
    ("action", "ESTHER walks in."), ("action", "I/E reads as both."),
    ("character", "MARA"), ("dialogue", "A spoken line."),
    ("action", "(Later that night)"), ("action", ">Centered words<"), ("action", "==="),
    ("action", "# A bold marker"), ("action", "@noted address"),
    ("action", "THE WIND RISES"), ("action", "A plain action stays plain."),
    ("action", "Into the room, a # sign and an @ sign."),
    ("action", "# The final instruction SAVEMARKER"),
], rows
rows = json.loads((evidence / "forced-action-first" / "expected.json").read_text())
assert [(row[0], row[1]) for row in rows] == [
    ("action", "Source: a live broadcast."), ("action", "Title: the door is ajar."),
    ("action", "Titles roll over black."), ("scene_heading", "INT. STUDIO - DAY"),
    ("action", "Credit: none is given."),
    ("action", "source: lowercase is not a key. SAVEMARKER"),
], rows
print("PASS: independently authored source syntax, exact review anchors, emphasis, replies and forced Action")
PY
printf 'Evidence: %s\n' "$EVIDENCE"
