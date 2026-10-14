#!/usr/bin/env bash
# System Qt codecs and the private PDF JPEG codec must coexist without corrupting images.
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
OUTPUT="$ROOT/.build/debug/_build"
mkdir -p "$ROOT/.build/evidence"
EVIDENCE=$(mktemp -d "$ROOT/.build/evidence/image-codecs-XXXXXX")
printf 'Evidence: %s\n' "$EVIDENCE"
read -r -a QT_CFLAGS <<< "$(pkg-config --cflags Qt6Core Qt6Gui Qt6Widgets Qt6Sql)"
read -r -a QT_LIBS <<< "$(pkg-config --libs Qt6Core Qt6Gui Qt6Widgets Qt6Sql)"
if ! "${CXX:-g++}" -std=c++17 -g -fPIC -I "$ROOT/src/corelib" \
    -I "$ROOT/src/3rd_party/pdfhummus/PDFWriter" -I "$ROOT/src/3rd_party/pdfhummus/LibJpeg" \
    "${QT_CFLAGS[@]}" "$ROOT/tests/image_codec_test.cpp" -L "$OUTPUT" -lcorelib \
    "${QT_LIBS[@]}" -o "$EVIDENCE/image-codec-test" > "$EVIDENCE/compile.log" 2>&1; then
    cat "$EVIDENCE/compile.log" >&2
    exit 1
fi
export QT_QPA_PLATFORM=offscreen
export QT_FORCE_STDERR_LOGGING=1
export LD_LIBRARY_PATH="$OUTPUT:$OUTPUT/libs${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export XDG_CONFIG_HOME="$EVIDENCE/profile/config"
export XDG_DATA_HOME="$EVIDENCE/profile/data"
export XDG_CACHE_HOME="$EVIDENCE/profile/cache"
export DBUS_SESSION_BUS_ADDRESS="unix:path=$EVIDENCE/absent-session-bus"
mkdir -p "$XDG_CONFIG_HOME" "$XDG_DATA_HOME" "$XDG_CACHE_HOME"
for mode in create reopen; do
    if ! "$EVIDENCE/image-codec-test" "$EVIDENCE/checks" "$mode" \
        > "$EVIDENCE/$mode.log" 2>&1; then
        tail -30 "$EVIDENCE/$mode.log" >&2
        exit 1
    fi
    tail -1 "$EVIDENCE/$mode.log"
done
python3 - "$EVIDENCE/checks" <<'PY'
import json
from pathlib import Path
import sqlite3
import sys
import zlib

root = Path(sys.argv[1])
identities = json.loads((root / "identities.json").read_text())
with sqlite3.connect(f'{(root / "story.starc").as_uri()}?mode=ro', uri=True) as db:
    assert db.execute("PRAGMA integrity_check").fetchall() == [("ok",)]
    rows = {row[0]: row[1:] for row in db.execute("SELECT uuid,id,type,CAST(content AS BLOB) FROM documents")}
    assert set(rows) == {row["uuid"] for row in identities}
    for row in identities:
        stored_id, stored_type, content = rows[row["uuid"]]
        assert stored_id == row["id"] and content == (root / row["file"]).read_bytes()
        assert stored_type == (10104 if row["file"] == "script.xml" else 101)
    history = db.execute("SELECT undo_patch,redo_patch FROM documents_changes").fetchall()
    assert len(history) == 1
    assert zlib.decompress(history[0][0][4:]) == b"authored-undo"
    assert zlib.decompress(history[0][1][4:]) == b"authored-redo"
print("PASS: independent fresh-process image/script bytes, identities and history")
PY
# No bundled jpeg_* entry point may interpose on the Qt plugin's system libjpeg.
nm -D --defined-only "$OUTPUT/libcorelib.so" > "$EVIDENCE/dynamic-symbols.txt"
if rg ' [A-Z] jpeg_' "$EVIDENCE/dynamic-symbols.txt" > "$EVIDENCE/leaked-jpeg-symbols.txt"; then
    printf 'FAIL: corelib exports private JPEG symbols\n' >&2
    exit 1
fi
printf 'PASS: bundled JPEG symbols are private\nEvidence: %s\n' "$EVIDENCE"
