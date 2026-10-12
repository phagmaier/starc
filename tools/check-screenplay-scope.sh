#!/usr/bin/env bash
# Focused check of the screenplay-only scope: creation choices, real screenplay
# editor key handling, Undo/Redo, pagination, exports, and byte-exact retention
# of retired-format documents across save and reopen. Requires the debug
# development build. Everything runs offscreen, in process, on a disposable
# project and profile; it is not evidence of visible desktop behavior.
set -euo pipefail

ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
CXX=${CXX:-g++}
OUTPUT="$ROOT/.build/debug/_build"
[[ -f "$OUTPUT/libcorelib.so" && -f "$OUTPUT/plugins/libcoreplugin.so" ]] || {
    printf 'Run ./build/dev.sh build debug first.\n' >&2
    exit 1
}
mkdir -p "$ROOT/.build/evidence"
EVIDENCE=$(mktemp -d "$ROOT/.build/evidence/screenplay-scope-XXXXXX")
mkdir -p "$EVIDENCE/profile/config" "$EVIDENCE/profile/data" "$EVIDENCE/profile/cache" \
    "$EVIDENCE/profile/documents" "$EVIDENCE/project"
# Document plugins are looked up next to the executable.
ln -s "$OUTPUT/plugins" "$EVIDENCE/plugins"
read -r -a QT_CFLAGS <<< "$(pkg-config --cflags Qt6Core Qt6Gui Qt6Widgets Qt6Sql Qt6Xml Qt6Network)"
read -r -a QT_LIBS <<< "$(pkg-config --libs Qt6Core Qt6Gui Qt6Widgets Qt6Sql Qt6Xml Qt6Network)"

if ! "$CXX" -std=c++17 -g -fPIC -I "$ROOT/src" -I "$ROOT/src/core" -I "$ROOT/src/corelib" \
    "${QT_CFLAGS[@]}" "$ROOT/tests/screenplay_scope_test.cpp" \
    -L "$OUTPUT" -L "$OUTPUT/plugins" -lcoreplugin -lscreenplaytextplugin -lcorelib \
    "${QT_LIBS[@]}" -o "$EVIDENCE/screenplay-scope-test" \
    > "$EVIDENCE/compile.log" 2>&1; then
    cat "$EVIDENCE/compile.log" >&2
    printf 'Evidence: %s\n' "$EVIDENCE" >&2
    exit 1
fi

export XDG_CONFIG_HOME="$EVIDENCE/profile/config"
export XDG_DATA_HOME="$EVIDENCE/profile/data"
export XDG_CACHE_HOME="$EVIDENCE/profile/cache"
printf 'XDG_DOCUMENTS_DIR="%s"\n' "$EVIDENCE/profile/documents" > "$XDG_CONFIG_HOME/user-dirs.dirs"
export QT_QPA_PLATFORM=offscreen
export LD_LIBRARY_PATH="$OUTPUT:$OUTPUT/libs:$OUTPUT/plugins${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
if ! "$EVIDENCE/screenplay-scope-test" "$EVIDENCE/project" > "$EVIDENCE/test.log" 2>&1; then
    cat "$EVIDENCE/test.log" >&2
    printf 'Evidence: %s\n' "$EVIDENCE" >&2
    exit 1
fi

# The exporters ran in process; check what they wrote with independent tools.
python3 - "$EVIDENCE/project" >> "$EVIDENCE/test.log" 2>&1 <<'PY' || {
import subprocess
import sys
import zipfile

directory = sys.argv[1]
expected = "We keep the screenplay."

with zipfile.ZipFile(directory + "/screenplay.docx") as docx:
    assert docx.testzip() is None, "DOCX archive is damaged"
    body = docx.read("word/document.xml").decode("utf-8")
assert expected in body, "DOCX body lacks the typed dialogue"
assert "INT. ROOM - DAY" in body, "DOCX body lacks the scene heading"
print("ok: DOCX archive and document text")

text = subprocess.run(["pdftotext", "-layout", directory + "/screenplay.pdf", "-"],
                      check=True, capture_output=True, text=True).stdout
pages = text.count("\f")
# Layout mode pads words to their positions on the page.
text = " ".join(text.split())
assert expected in text, "PDF text lacks the typed dialogue"
assert "INT. ROOM - DAY" in text, "PDF text lacks the scene heading"
assert pages > 1, "PDF has %d page(s), expected several" % pages
print("ok: PDF text on %d pages" % pages)
PY
    cat "$EVIDENCE/test.log" >&2
    printf 'Evidence: %s\n' "$EVIDENCE" >&2
    exit 1
}
cat "$EVIDENCE/test.log"
printf 'Evidence: %s\n' "$EVIDENCE"
