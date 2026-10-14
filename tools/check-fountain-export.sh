#!/usr/bin/env bash
# Explicit Fountain export, ordinary close, and fresh application import with annotations,
# forced Action paragraphs, literal syntax, reserved-start dialogue and inline-note reviews.
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
OUTPUT="$ROOT/.build/debug/_build"
CXX=${CXX:-g++}
mkdir -p "$ROOT/.build/evidence"
EVIDENCE=$(mktemp -d "$ROOT/.build/evidence/fountain-export-XXXXXX")
printf 'Evidence: %s\n' "$EVIDENCE"
ln -s "$OUTPUT/plugins" "$EVIDENCE/plugins"
read -r -a QT_CFLAGS <<< "$(pkg-config --cflags Qt6Core Qt6Gui Qt6Widgets Qt6Sql Qt6Xml Qt6Network)"
read -r -a QT_LIBS <<< "$(pkg-config --libs Qt6Core Qt6Gui Qt6Widgets Qt6Sql Qt6Xml Qt6Network)"
if ! "$CXX" -std=c++17 -g -fPIC -I "$ROOT/src" -I "$ROOT/src/core" -I "$ROOT/src/corelib" \
    "${QT_CFLAGS[@]}" "$ROOT/tests/fountain_export_test.cpp" "$ROOT/src/app/application.cpp" \
    "$ROOT/.build/debug/app/moc_application.cpp" \
    -L "$OUTPUT" -L "$OUTPUT/plugins" -lcoreplugin -lscreenplaytextplugin -lcorelib \
    "${QT_LIBS[@]}" -o "$EVIDENCE/fountain-export-test" > "$EVIDENCE/compile.log" 2>&1; then
    cat "$EVIDENCE/compile.log" >&2
    exit 1
fi
export QT_QPA_PLATFORM=${STARC_FOUNTAIN_EXPORT_PLATFORM:-offscreen}
export QT_FORCE_STDERR_LOGGING=1
export LD_LIBRARY_PATH="$OUTPUT:$OUTPUT/libs:$OUTPUT/plugins${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export DBUS_SESSION_BUS_ADDRESS="unix:path=$EVIDENCE/absent-session-bus"
for fixture in ${STARC_FOUNTAIN_EXPORT_FIXTURES:-emphasis-review review-offsets forced-action forced-action-first literal-syntax reserved-dialogue inline-note-review handwritten-markers}; do
    case_root="$EVIDENCE/$fixture"
    mkdir -p "$case_root"
    cp "$ROOT/tests/fixtures/interchange/$fixture.fountain" "$case_root/source.fountain"
    if [[ "$fixture" == inline-note-review ]]; then
        printf '\nFinal action.\n' >> "$case_root/source.fountain"
    fi
    modes=${STARC_FOUNTAIN_EXPORT_MODES:-save reopen}
    if [[ ( "$fixture" == handwritten-markers || "$fixture" == inline-note-review )
        && -z "${STARC_FOUNTAIN_EXPORT_MODES:-}" ]]; then
        modes+=' native-reopen'
    fi
    for mode in $modes; do
        export STARC_PROFILE_DIR="$case_root/$mode-profile"
        export XDG_CONFIG_HOME="$STARC_PROFILE_DIR/config"
        export XDG_DATA_HOME="$STARC_PROFILE_DIR/data"
        export XDG_CACHE_HOME="$STARC_PROFILE_DIR/cache"
        mkdir -p "$XDG_CONFIG_HOME" "$XDG_DATA_HOME" "$XDG_CACHE_HOME" "$STARC_PROFILE_DIR/documents"
        printf 'XDG_DOCUMENTS_DIR="%s"\n' "$STARC_PROFILE_DIR/documents" > "$XDG_CONFIG_HOME/user-dirs.dirs"
        if ! "$EVIDENCE/fountain-export-test" "$case_root" "$mode" > "$case_root/$mode.log" 2>&1; then
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
for name in ("emphasis-review", "review-offsets", "forced-action", "forced-action-first",
             "literal-syntax", "reserved-dialogue", "inline-note-review", "handwritten-markers"):
    if not (evidence / name).exists():
        continue
    expected = (fixtures / (name + ("-export" if name == "handwritten-markers" else "") + ".fountain")).read_text()
    if name == "emphasis-review":
        expected = expected.replace("!A **bold** clue.", "A **bold** clue.")
    if name == "reserved-dialogue":
        expected = expected.replace(" #17A#", "", 1)
    if name == "inline-note-review":
        expected = expected.replace(" #18A#", "", 1) + "\nFinal action.\n"
    if name == "handwritten-markers":
        expected = expected.replace(" #19A#", "", 1)
        expected = expected.replace("The calculation ends.",
            r"The calculation ends. PASTE 2 \* 3; \*literal\* \_name\_ \[\[note\]\] C:\\temp")
    body, _, last = expected.rstrip("\n").rpartition("\n")
    # Typing at the end of the fully reviewed final line naturally extends its
    # review coverage to the marker. The note must follow the added characters.
    anchor = last.rfind("[[") if "[[" in last else len(last)
    # The literal fixture has a note-like word and a path typed after the marker.
    typed = " SAVEMARKER \\[\\[typed\\]\\] D:\\\\x" if name == "literal-syntax" else " SAVEMARKER"
    expected = body + "\n" + last[:anchor] + typed + last[anchor:] + "\n"
    actual = (evidence / name / "export.fountain").read_text()
    assert actual == expected, (name, actual, expected)
# Focused fixture selection retains all requested editor/oracle checks above.
if any(not (evidence / name).exists() for name in (
    "emphasis-review", "review-offsets", "forced-action", "forced-action-first",
    "literal-syntax", "reserved-dialogue", "inline-note-review")):
    print("PASS: selected fixtures retain exact canonical export syntax")
    sys.exit(0)
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
# Literal brackets, backslashes and slashes are script text in the saved model, not notes,
# escapes or an inline note, and every paragraph after them is still there.
rows = json.loads((evidence / "literal-syntax" / "expected.json").read_text())
assert [(row[0], row[1]) for row in rows] == [
    ("scene_heading", "INT. ARCHIVE - DAY"),
    ("action", "Screen shows [[message]] then continues."),
    ("action", "Triple [[[x]]] too."), ("action", "The folder is C:\\temp."),
    ("action", "\\\\server\\share opens."),
    ("action", "A lone [bracket], a path/name and 2 + 2 stay plain."),
    ("action", "He types /* remark */ and moves on."),
    ("action", "C:\\bold follows a backslash."), ("action", "Either and/or is fine."),
    ("action", "The strong/ ending."), ("action", "A [ opens."),
    ("action", "Path C:\\ here."), ("action", "A clue."), ("character", "MARA"),
    ("parenthetical", "reading [[aloud]]"), ("dialogue", "Read [[this]] from D:\\notes."),
    ("action", "Stressed first word."), ("action", "@home [[x]] waits."),
    ("action", "The last line keeps a\\b. SAVEMARKER [[typed]] D:\\x"),
], rows
notes = {row[1]: sorted({message for position in row[3] for message in position}) for row in rows}
assert notes["A [ opens."] == ["Bracket note"] and notes["Path C:\\ here."] == ["Drive note"]
assert notes["A clue."] == ["Check [[tag]] in C:\\dir, 2 * 3 and file_name]"]
assert sum(1 for messages in notes.values() if messages) == 3, notes
rows = json.loads((evidence / "reserved-dialogue" / "expected.json").read_text())
assert [row[1] for row in rows if row[0] == "dialogue"] == [
    "#1 is the best.", "@home, he says.", "...and then nothing.", ".A dotted remark.",
    "!Bang.", ">Onward.", "= Equal.", "~ Hum.", "===", ">Centered words<", "(I said that.)",
    "INT. is short for interior.", "EXT. is short for exterior.", "EST. is another prefix.",
    "INT./EXT. stays spoken.", "INT/EXT. stays spoken.", "EXT./INT. stays spoken.",
    "EXT/INT. stays spoken.", "I/E stays spoken.", "#1 café 🙂.", "INT. stays spoken.",
    "#First line.", "@Second line.", "INT. last line.", "An ordinary answer with # and @ inside.",
], rows
assert [(row[0], row[1]) for row in rows if row[0] != "dialogue"] == [
    ("scene_heading", "INT. RECORDING - DAY"), *[("character", "MARA")] * 21,
    ("character", "MCKAY"), ("character", "ADA"), ("action", "The recording stops. SAVEMARKER"),
], rows
rows = json.loads((evidence / "inline-note-review" / "expected.json").read_text())
assert [(row[0], row[1]) for row in rows] == [
    ("scene_heading", "INT. STUDIO - DAY"), ("action", "Before."),
    ("inline_note", "private "),
    ("inline_note", "first then Ω🙂 [[literal]] at C:\\temp "),
    ("action", "After."), ("inline_note", "line one\u2028line two "),
    ("inline_note", "last "), ("action", "Final action. SAVEMARKER"),
], rows
for row, ranges in zip(rows, [
    [], [(0, 7, ["before"])], [(0, 7, ["about it"])],
    [(0, 5, ["one", "reply"]), (5, 32, ["two [[x]] * and _ and \\"])],
    [(0, 6, ["after"])], [(0, 8, ["line comment\nsecond */ inside review"]), (8, 9, ["tail"])],
    [(0, 4, ["end"])], [],
]):
    expected_coverage = [[] for _ in row[3]]
    for start, length, comments in ranges:
        expected_coverage[start:start + length] = [comments] * length
    assert row[3] == expected_coverage, row
assert rows[3][2] == "11111000000444000000000000000000000000", rows[3]
print("PASS: independently authored export syntax, exact review anchors, emphasis, replies, forced Action, literal syntax and reviewed inline notes")
PY
printf 'Evidence: %s\n' "$EVIDENCE"
