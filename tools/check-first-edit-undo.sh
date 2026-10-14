#!/usr/bin/env bash
# First edit after importing annotated Fountain (archive/audit-2 issue 15): one-step Undo/Redo
# through the real coordinator and editor before and after an initial Save and after a fresh
# native reopening, then Save and fresh-process reopening of the undone and redone screenplays.
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
OUTPUT="$ROOT/.build/debug/_build"
CXX=${CXX:-g++}
mkdir -p "$ROOT/.build/evidence"
EVIDENCE=$(mktemp -d "$ROOT/.build/evidence/first-edit-undo-XXXXXX")
printf 'Evidence: %s\n' "$EVIDENCE"
ln -s "$OUTPUT/plugins" "$EVIDENCE/plugins"
read -r -a QT_CFLAGS <<< "$(pkg-config --cflags Qt6Core Qt6Gui Qt6Widgets Qt6Sql Qt6Xml Qt6Network)"
read -r -a QT_LIBS <<< "$(pkg-config --libs Qt6Core Qt6Gui Qt6Widgets Qt6Sql Qt6Xml Qt6Network)"
if ! "$CXX" -std=c++17 -g -fPIC -I "$ROOT/src" -I "$ROOT/src/core" -I "$ROOT/src/corelib" \
    "${QT_CFLAGS[@]}" "$ROOT/tests/first_edit_undo_test.cpp" "$ROOT/src/app/application.cpp" \
    "$ROOT/.build/debug/app/moc_application.cpp" \
    -L "$OUTPUT" -L "$OUTPUT/plugins" -lcoreplugin -lscreenplaytextplugin -lcorelib \
    "${QT_LIBS[@]}" -o "$EVIDENCE/first-edit-undo-test" > "$EVIDENCE/compile.log" 2>&1; then
    cat "$EVIDENCE/compile.log" >&2
    exit 1
fi
export QT_QPA_PLATFORM=${STARC_FIRST_EDIT_UNDO_PLATFORM:-offscreen}
export QT_FORCE_STDERR_LOGGING=1
export LD_LIBRARY_PATH="$OUTPUT:$OUTPUT/libs:$OUTPUT/plugins${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export DBUS_SESSION_BUS_ADDRESS="unix:path=$EVIDENCE/absent-session-bus"
FIXTURES=${STARC_FIRST_EDIT_UNDO_FIXTURES:-emphasis-review review-offsets notes-dual literal-syntax}
CASES=${STARC_FIRST_EDIT_UNDO_CASES:-import-undone import-redone saved-undone saved-redone \
native-undone native-redone}
BASELINE=${STARC_FIRST_EDIT_UNDO_BASELINE:-}
# Each phase is a separate application process with its own disposable profile.
run_phase() {
    local case_root=$1 phase=$2 label=${3:-$2}
    export STARC_PROFILE_DIR="$case_root/$label-profile"
    export XDG_CONFIG_HOME="$STARC_PROFILE_DIR/config"
    export XDG_DATA_HOME="$STARC_PROFILE_DIR/data"
    export XDG_CACHE_HOME="$STARC_PROFILE_DIR/cache"
    mkdir -p "$XDG_CONFIG_HOME" "$XDG_DATA_HOME" "$XDG_CACHE_HOME" "$STARC_PROFILE_DIR/documents"
    printf 'XDG_DOCUMENTS_DIR="%s"\n' "$STARC_PROFILE_DIR/documents" > "$XDG_CONFIG_HOME/user-dirs.dirs"
    local status=0
    "$EVIDENCE/first-edit-undo-test" "$case_root" "$phase" > "$case_root/$label.log" 2>&1 \
        || status=$?
    if [[ $status -ne 0 ]]; then
        if [[ -n $BASELINE ]]; then
            printf 'BASELINE FAIL: %s/%s exit %s: %s\n' "${case_root#"$EVIDENCE"/}" "$label" \
                "$status" "$(grep -m1 -E '^FAIL: |ASSERT' "$case_root/$label.log" || true)"
            return 1
        fi
        tail -80 "$case_root/$label.log" >&2
        printf 'FAIL: %s/%s (log retained: %s)\n' "${case_root#"$EVIDENCE"/}" "$label" \
            "$case_root/$label.log" >&2
        exit 1
    fi
    printf 'PASS: %s/%s (ordinary exit 0)\n' "${case_root#"$EVIDENCE"/}" "$label"
}
for fixture in $FIXTURES; do
    for case_name in $CASES; do
        case_root="$EVIDENCE/$fixture/$case_name"
        mkdir -p "$case_root"
        cp "$ROOT/tests/fixtures/interchange/$fixture.fountain" "$case_root/source.fountain"
        if [[ $case_name == native-* ]]; then
            run_phase "$case_root" prepare || continue
        fi
        run_phase "$case_root" "$case_name" || continue
        run_phase "$case_root" verify || continue
        run_phase "$case_root" verify verify-again || continue
    done
done
# Baseline mode runs every case against an application without the fix and retains each
# case's first failed assertion; it reports failure, never a pass.
if [[ -n $BASELINE ]]; then
    printf 'BASELINE: every result above is retained under %s; baseline mode never passes\n' \
        "$EVIDENCE"
    exit 1
fi
python3 - "$EVIDENCE" "$ROOT/tests/fixtures/interchange" "$FIXTURES" "$CASES" <<'PY'
import html
import sqlite3
import sys
import xml.etree.ElementTree as ElementTree
from pathlib import Path

evidence, fixtures = Path(sys.argv[1]), Path(sys.argv[2])
TYPED = " SAVEMARKER"


def paragraphs(xml):
    """Paragraph type, text, per-character emphasis and per-character review messages."""
    result = []
    for element in ElementTree.fromstring(xml).iter():
        value = element.find("v")
        if value is None:
            continue
        text = html.unescape(value.text or "")
        # Model ranges count UTF-16 code units, as Qt strings do.
        units = len(text.encode("utf-16-le")) // 2
        styles = [0] * units
        for fm in element.iterfind("fms/fm"):
            start, length = int(fm.get("from")), int(fm.get("length"))
            assert 0 <= start and start + length <= units, (text, fm.attrib)
            bits = sum(bit for name, bit in (("bold", 1), ("italic", 2), ("underline", 4),
                                             ("strikethrough", 8)) if fm.get(name) == "true")
            for position in range(start, start + length):
                styles[position] |= bits
        notes = [()] * units
        for rm in element.iterfind("rms/rm"):
            start, length = int(rm.get("from")), int(rm.get("length"))
            assert 0 <= start and length > 0 and start + length <= units, (text, rm.attrib)
            messages = tuple((html.unescape(c.text or ""), c.get("author"), c.get("date"))
                             for c in rm.iterfind("c"))
            for position in range(start, start + length):
                notes[position] = messages
        result.append((element.tag, text, styles, notes))
    return result


checked = 0
for fixture in sys.argv[3].split():
    fixture_bytes = (fixtures / f"{fixture}.fountain").read_bytes()
    for case in sys.argv[4].split():
        root = evidence / fixture / case
        original = (root / "original.xml").read_bytes()
        edited = (root / "edited.xml").read_bytes()
        expected = (root / "expected.xml").read_bytes()
        assert expected == (original if case.endswith("-undone") else edited), root
        if case.startswith("native-"):
            assert (root / "prepared.xml").read_bytes() == original, root
        # The authored edit appends the typed text to the last paragraph: its text grows,
        # its styles and review notes stay, and the typed text continues its final character.
        before, after = paragraphs(original), paragraphs(edited)
        assert len(before) == len(after) and before[:-1] == after[:-1], root
        kind, text, styles, notes = before[-1]
        assert after[-1] == (kind, text + TYPED, styles + [0] * len(TYPED),
                             notes + [notes[-1] if notes else ()] * len(TYPED)), (root, after[-1])
        assert any(any(paragraph[3]) for paragraph in before), (root, "fixture has review notes")
        with sqlite3.connect(f"{(root / 'story.starc').as_uri()}?mode=ro", uri=True) as db:
            assert db.execute("SELECT value FROM system_variables WHERE variable ="
                              " 'application-version'").fetchall() == [('0.8.4',)], root
            assert not db.execute("SELECT variable FROM system_variables WHERE variable LIKE"
                                  " 'application-updated-to-version-%'").fetchall(), root
            stored = [content if isinstance(content, bytes) else content.encode() for (content,)
                      in db.execute("SELECT content FROM documents WHERE type = 10104")]
            changes = db.execute(
                "SELECT COUNT(*) FROM documents_changes WHERE fk_document_uuid ="
                " (SELECT uuid FROM documents WHERE type = 10104)").fetchone()[0]
        assert stored == [expected], (root, "committed screenplay")
        assert changes >= 2, (root, "import and the first edit are in the stored history")
        assert (root / "source.fountain").read_bytes() == fixture_bytes, root
        assert not list(root.glob("*.lock")), (root, "locks released")
        checked += 1
print(f"PASS: independent SQLite/XML oracle for {checked} cases: exact committed undone/redone"
      " screenplay, only the typed paragraph edited, styles and review notes retained,"
      " source bytes and locks")
PY
printf 'Evidence: %s\n' "$EVIDENCE"
