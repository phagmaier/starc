#!/usr/bin/env bash
# Real sync manager, localhost mock API, fake credentials and disposable files only.
set -euo pipefail

ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
OUTPUT="$ROOT/.build/debug/_build"
CXX=${CXX:-g++}
mkdir -p "$ROOT/.build/evidence"
EVIDENCE=$(mktemp -d "$ROOT/.build/evidence/github-download-safety-XXXXXX")
printf 'Evidence: %s\n' "$EVIDENCE"
read -r -a QT_CFLAGS <<< "$(pkg-config --cflags Qt6Core Qt6Gui Qt6Widgets Qt6Sql Qt6Network)"
read -r -a QT_LIBS <<< "$(pkg-config --libs Qt6Core Qt6Gui Qt6Widgets Qt6Sql Qt6Network)"
if ! "$CXX" -std=c++17 -g -fPIC -I "$ROOT/src" -I "$ROOT/src/core" -I "$ROOT/src/corelib" \
    "${QT_CFLAGS[@]}" "$ROOT/tests/github_download_safety_test.cpp" \
    -L "$OUTPUT" -L "$OUTPUT/plugins" -lcoreplugin -lcorelib "${QT_LIBS[@]}" -ldl \
    -Wl,-rpath,"$OUTPUT" -Wl,-rpath,"$OUTPUT/plugins" -o "$EVIDENCE/download-test" \
    > "$EVIDENCE/compile.log" 2>&1; then
    cat "$EVIDENCE/compile.log" >&2
    exit 1
fi
export LD_LIBRARY_PATH="${STARC_DOWNLOAD_LIBRARY_DIR:-$OUTPUT/plugins}:$OUTPUT:$OUTPUT/libs"
export QT_QPA_PLATFORM=offscreen
SERVER=
trap '[[ -z "$SERVER" ]] || kill "$SERVER" 2>/dev/null || true' EXIT
read -r -a CASES <<< "${STARC_DOWNLOAD_CASES:-success locked-absent locked-existing exists dangling symlink directory race-file race-symlink race-directory unwritable-stage write-failure publish-failure}"
read -r -a ROUTES <<< "${STARC_DOWNLOAD_ROUTES:-new copy}"
for route in "${ROUTES[@]}"; do
    for scenario in "${CASES[@]}"; do
        case_root="$EVIDENCE/$route-$scenario"
        export STARC_PROFILE_DIR="$case_root"
        export XDG_CONFIG_HOME="$case_root/config" XDG_DATA_HOME="$case_root/data"
        export XDG_CACHE_HOME="$case_root/cache"
        export DBUS_SESSION_BUS_ADDRESS="unix:path=$case_root/absent-session-bus"
        mkdir -p "$XDG_CONFIG_HOME" "$XDG_DATA_HOME" "$XDG_CACHE_HOME" "$case_root/documents"
        printf 'XDG_DOCUMENTS_DIR="%s"\n' "$case_root/documents" > "$XDG_CONFIG_HOME/user-dirs.dirs"
        python3 "$ROOT/tests/github_mock_server.py" "$case_root/repository" \
            > "$case_root/server.log" 2>&1 &
        SERVER=$!
        port=
        for _ in $(seq 1 50); do
            port=$(sed -n 's/^PORT //p;q' "$case_root/server.log" 2>/dev/null || true)
            [[ -n "$port" ]] && break
            sleep 0.1
        done
        [[ -n "$port" ]] || { cat "$case_root/server.log" >&2; exit 1; }
        export STARC_GITHUB_API_URL="http://127.0.0.1:$port"
        export STARC_DOWNLOAD_ROUTE="$route" STARC_DOWNLOAD_CASE="$scenario"
        if ! "$EVIDENCE/download-test" > "$case_root/run.log" 2>&1; then
            cat "$case_root/run.log" >&2
            exit 1
        fi
        kill "$SERVER"
        wait "$SERVER" 2>/dev/null || true
        SERVER=
        if ! python3 - "$case_root" "$route" "$scenario" \
            > "$case_root/reopen.log" 2>&1 <<'PY'
import pathlib
import sqlite3
import sys

root, route, scenario = pathlib.Path(sys.argv[1]), *sys.argv[2:]
target = pathlib.Path((root / "target.txt").read_text())
remote = (root / "remote-fixture.starc").read_bytes()
assert target.read_bytes() == remote
assert (root / "repository" / ("story.starc" if route == "copy" else "new-story.starc")).read_bytes() == remote
refused = target.with_name(target.name + ".refused")
if scenario in {"exists", "locked-existing", "race-file"}:
    assert refused.read_bytes() == (root / "competitor.starc").read_bytes()
elif scenario in {"symlink", "dangling", "race-symlink"}:
    assert refused.is_symlink() and refused.readlink() == root / "external.starc"
    if scenario == "symlink":
        assert (root / "external.starc").read_bytes() == (root / "competitor.starc").read_bytes()
    else:
        assert not (root / "external.starc").exists()
elif scenario in {"directory", "race-directory"}:
    assert refused.is_dir() and not list(refused.iterdir())
else:
    assert not refused.exists()
paths = [root / "remote-fixture.starc", target]
if scenario in {"exists", "locked-existing", "race-file"}:
    paths.append(refused)
if route == "copy":
    paths.append(root / "story.starc")
for path in paths:
    with sqlite3.connect(f"{path.as_uri()}?mode=ro", uri=True) as db:
        assert db.execute("PRAGMA integrity_check").fetchall() == [("ok",)]
        expected = (b"LOCAL conflicting script with retained edits" if path.name == "story.starc"
                    else b"CONCURRENT writer owns these exact script bytes" if path == refused
                    else "REMOTE script: café — 雪\n\nOriginal annotations and IDs".encode())
        assert db.execute("SELECT content FROM documents WHERE uuid='{script}'").fetchone() == (expected,)
        assert db.execute("SELECT uuid, type, content FROM documents WHERE uuid='{review}'").fetchone() == (
            "{review}", 10105, b'<review id="review-1" from="2" length="5">Note</review>')
        assert db.execute("SELECT uuid, type, content FROM documents WHERE uuid='{opaque}'").fetchone() == (
            "{opaque}", 99999, bytes.fromhex("00fffe5c5b0d0a80"))
        assert db.execute("SELECT uuid, patch FROM history").fetchall() == [
            ("{history-1}", bytes.fromhex("005061746368ff0d0a"))]
assert not target.with_name(target.name + ".lock").exists()
assert not [p for p in target.parent.glob(target.name + ".*") if p != refused]
print("Fresh-process byte, SQLite, identity and collision oracle passed.")
PY
        then
            cat "$case_root/reopen.log" >&2
            exit 1
        fi
        printf 'PASS %s/%s (ordinary exit and fresh-process oracle)\n' "$route" "$scenario"
    done
done
