#!/usr/bin/env bash
# Creation/versioning and legacy element migrations: disposable SQLite files, fresh processes.
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
OUTPUT="$ROOT/.build/debug/_build"
mkdir -p "$ROOT/.build/evidence"
EVIDENCE=$(mktemp -d "$ROOT/.build/evidence/database-migrations-XXXXXX")
printf 'Evidence: %s\n' "$EVIDENCE"
mkdir -p "$EVIDENCE/profile/config" "$EVIDENCE/profile/data" "$EVIDENCE/profile/cache"
export XDG_CONFIG_HOME="$EVIDENCE/profile/config"
export XDG_DATA_HOME="$EVIDENCE/profile/data"
export XDG_CACHE_HOME="$EVIDENCE/profile/cache"
export DBUS_SESSION_BUS_ADDRESS="unix:path=$EVIDENCE/absent-session-bus"
export LD_LIBRARY_PATH="$OUTPUT:$OUTPUT/libs${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
read -r -a QT_CFLAGS <<< "$(pkg-config --cflags Qt6Core Qt6Sql)"
read -r -a QT_LIBS <<< "$(pkg-config --libs Qt6Core Qt6Sql)"
if ! "${CXX:-g++}" -std=c++17 -g -fPIC -I "$ROOT/src/corelib" "${QT_CFLAGS[@]}" \
    "$ROOT/tests/database_migration_test.cpp" -L "$OUTPUT" -lcorelib "${QT_LIBS[@]}" \
    -o "$EVIDENCE/database-migration-test" > "$EVIDENCE/compile.log" 2>&1; then
    cat "$EVIDENCE/compile.log" >&2
    exit 1
fi
if ! python3 "$ROOT/tests/database_migration_oracle.py" "$EVIDENCE" \
    "$EVIDENCE/database-migration-test" > "$EVIDENCE/oracle.log" 2>&1; then
    cat "$EVIDENCE/oracle.log" >&2
    exit 1
fi
cat "$EVIDENCE/oracle.log"
printf 'Evidence: %s\n' "$EVIDENCE"
