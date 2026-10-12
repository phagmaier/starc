#!/usr/bin/env bash
# Linux development entry point. Keep generated files out of the source tree.
set -euo pipefail

ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
JOBS=${JOBS:-4}
QMAKE=${QMAKE:-qmake6}
QTKEYCHAIN_PREFIX=${QTKEYCHAIN_PREFIX:-}
# Use the prepared unprivileged development dependency when the system package
# is absent. Report this prefix in doctor; this is not a system installation.
if [[ -z "$QTKEYCHAIN_PREFIX" && ! -f /usr/include/qt6keychain/keychain.h && \
      -f "$ROOT/.build/keyring-deps/usr/include/qt6keychain/keychain.h" && \
      -f "$ROOT/.build/keyring-deps/usr/lib/libqt6keychain.so" ]]; then
    QTKEYCHAIN_PREFIX="$ROOT/.build/keyring-deps/usr"
fi

usage() {
    cat <<'EOF'
Usage: ./build/dev.sh COMMAND [debug|release] [application arguments]
  doctor              Report tools, Qt modules, dependencies, and GUI session
  deps                Initialize only the two public PDF submodules
  build MODE          Configure and build in .build/MODE (default: debug)
  run MODE [ARGS...]   Launch .build/MODE/_build/starcapp (default: debug)
  compdb MODE         Build, then extract compile_commands.json from a dry run

Environment: JOBS=4, QMAKE=qmake6.
STARC_PROFILE_DIR=/absolute/path gives run a separate XDG configuration,
data, and cache directory. Without it, the application's normal profile is used.
EOF
}

fail() { printf 'Error: %s\n' "$*" >&2; exit 1; }
require_work_branch() {
    local branch
    branch=$(git -C "$ROOT" symbolic-ref --short HEAD) || fail 'Use dev or a feature branch; detached HEAD is read only.'
    [[ "$branch" != master ]] || fail 'master is frozen. Switch to dev or a feature branch first.'
}
require_jobs() {
    [[ "$JOBS" =~ ^[1-9][0-9]*$ ]] || fail 'JOBS must be a positive integer.'
}
require_mode() {
    [[ "$MODE" == debug || "$MODE" == release ]] || fail 'Configuration must be debug or release.'
}
pdf_ready() {
    [[ -f "$ROOT/src/3rd_party/pdfhummus/pdfhummus.pro" && \
       -f "$ROOT/src/3rd_party/pdftextextraction/pdftextextraction.pro" ]]
}

doctor() {
    local missing=0 tool qt_version headers qt_data qt_bins plugins module
    printf 'Repository: %s\nBranch: %s\n' "$ROOT" "$(git -C "$ROOT" branch --show-current)"
    for tool in git g++ clang++ make cmake pkg-config python3 ninja "$QMAKE"; do
        if command -v "$tool" >/dev/null 2>&1; then
            printf 'OK tool: %s (%s)\n' "$tool" "$(command -v "$tool")"
        else
            printf 'MISSING tool: %s\n' "$tool"
            missing=1
        fi
    done
    for tool in clangd clang-format gdb shellcheck; do
        if command -v "$tool" >/dev/null 2>&1; then
            printf 'OK development tool: %s\n' "$tool"
        else
            printf 'Optional development tool missing: %s\n' "$tool"
        fi
    done
    if command -v "$QMAKE" >/dev/null 2>&1; then
        qt_version=$("$QMAKE" -query QT_VERSION)
        printf 'Qt: %s\n' "$qt_version"
        [[ "$qt_version" == 6.* ]] || { printf 'MISSING: this helper requires Qt 6\n'; missing=1; }
        headers=$("$QMAKE" -query QT_INSTALL_HEADERS)
        qt_data=$("$QMAKE" -query QT_HOST_DATA)
        qt_bins=$("$QMAKE" -query QT_HOST_BINS)
        plugins=$("$QMAKE" -query QT_INSTALL_PLUGINS)
        for module in core gui widgets concurrent multimedia network sql xml core5compat; do
            if [[ -f "$qt_data/mkspecs/modules/qt_lib_$module.pri" ]]; then
                printf 'OK Qt module: %s\n' "$module"
            else
                printf 'MISSING Qt module: %s\n' "$module"; missing=1
            fi
        done
        for module in Gui Widgets; do
            if [[ -d "$headers/Qt$module/$qt_version/Qt$module/private" ]]; then
                printf 'OK Qt private headers: Qt%s\n' "$module"
            else
                printf 'MISSING matching Qt private headers: Qt%s %s\n' "$module" "$qt_version"; missing=1
            fi
        done
        [[ -f "$plugins/sqldrivers/libqsqlite.so" ]] || { printf 'MISSING Qt SQLite driver\n'; missing=1; }
        if [[ -x "$qt_bins/lrelease" ]]; then
            printf 'OK translation tool: %s/lrelease\n' "$qt_bins"
        else
            printf 'Optional translation tool missing: %s/lrelease (Arch: qt6-tools)\n' "$qt_bins"
        fi
    fi
    if command -v pkg-config >/dev/null 2>&1; then
        for module in openssl zlib; do
            if pkg-config --exists "$module"; then
                printf 'OK native dependency: %s\n' "$module"
            else
                printf 'MISSING native dependency: %s\n' "$module"; missing=1
            fi
        done
    fi
    if [[ -f "${QTKEYCHAIN_PREFIX:-/usr}/include/qt6keychain/keychain.h" && \
          -f "${QTKEYCHAIN_PREFIX:-/usr}/lib/libqt6keychain.so" ]]; then
        printf 'OK native dependency: Qt6Keychain (%s)\n' "${QTKEYCHAIN_PREFIX:-/usr}"
    else
        printf 'MISSING Qt6Keychain: install qtkeychain-qt6 or set QTKEYCHAIN_PREFIX\n'; missing=1
    fi
    if pdf_ready; then
        printf 'OK public PDF submodules\n'
    else
        printf 'MISSING public PDF submodules: run ./build/dev.sh deps\n'; missing=1
    fi
    printf 'GUI session: DISPLAY=%s WAYLAND_DISPLAY=%s XDG_SESSION_TYPE=%s\n' \
        "${DISPLAY:-unset}" "${WAYLAND_DISPLAY:-unset}" "${XDG_SESSION_TYPE:-unset}"
    if [[ -z ${DISPLAY:-} && -z ${WAYLAND_DISPLAY:-} ]]; then
        printf 'No desktop display inherited; launch from a desktop terminal for a visible window.\n'
    fi
    return "$missing"
}

configure() {
    require_work_branch
    require_mode
    require_jobs
    command -v "$QMAKE" >/dev/null 2>&1 || fail "Missing $QMAKE; install Qt 6 or set QMAKE."
    [[ $("$QMAKE" -query QT_VERSION) == 6.* ]] || fail 'QMAKE must select Qt 6.'
    pdf_ready || fail 'Run ./build/dev.sh deps first.'
    mkdir -p "$ROOT/.build/$MODE"
    cd "$ROOT/.build/$MODE"
    local qmake_args signature
    qmake_args=("$ROOT/src/starc.pro" "CONFIG-=$([[ "$MODE" == debug ]] && printf release || printf debug)"
        "CONFIG+=$MODE" CONFIG-=debug_and_release
        "QMAKE_CFLAGS+=-DHAVE_UNISTD_H=1")
    [[ -z "$QTKEYCHAIN_PREFIX" ]] || qmake_args+=("QTKEYCHAIN_PREFIX=$QTKEYCHAIN_PREFIX")
    signature=$(printf '%s\n' "$(command -v "$QMAKE")" "$("$QMAKE" -query QT_VERSION)" "${qmake_args[@]}")
    "$QMAKE" "${qmake_args[@]}"
    # Existing subproject Makefiles retain old command-line options. Refresh
    # them when configuration changes, without regenerating timestamps on
    # every ordinary incremental build.
    if [[ ! -f .dev-config || $(cat .dev-config) != "$signature" ]]; then
        make qmake_all
        printf '%s\n' "$signature" > .dev-config
    fi
}

COMMAND=${1:-help}
if (( $# )); then shift; fi
MODE=${1:-debug}
case "$COMMAND" in
    help|-h|--help) usage ;;
    doctor) (( $# == 0 )) || fail 'doctor takes no arguments.'; doctor ;;
    deps)
        (( $# == 0 )) || fail 'deps takes no arguments.'
        require_work_branch
        git -C "$ROOT" submodule update --init --recursive -- \
            src/3rd_party/pdfhummus src/3rd_party/pdftextextraction
        ;;
    build|compdb)
        (( $# <= 1 )) || fail "$COMMAND takes one configuration."
        configure
        make -j "$JOBS"
        if [[ "$COMMAND" == compdb ]]; then
            python3 "$ROOT/tools/compdb-from-make.py" "$ROOT/.build/$MODE"
        fi
        ;;
    run)
        require_work_branch; require_mode
        if (( $# )); then shift; fi
        output="$ROOT/.build/$MODE/_build"
        [[ -x "$output/starcapp" ]] || fail "Run ./build/dev.sh build $MODE first."
        if [[ -n ${STARC_PROFILE_DIR:-} ]]; then
            [[ "$STARC_PROFILE_DIR" == /* ]] || fail 'STARC_PROFILE_DIR must be an absolute path.'
            export XDG_CONFIG_HOME="$STARC_PROFILE_DIR/config"
            export XDG_DATA_HOME="$STARC_PROFILE_DIR/data"
            export XDG_CACHE_HOME="$STARC_PROFILE_DIR/cache"
            mkdir -p "$XDG_CONFIG_HOME" "$XDG_DATA_HOME" "$XDG_CACHE_HOME" "$STARC_PROFILE_DIR/documents"
            # STARC logs to QStandardPaths::DocumentsLocation. XDG data alone
            # does not redirect it; a profile-specific user-dirs file does.
            if [[ ! -f "$XDG_CONFIG_HOME/user-dirs.dirs" ]]; then
                printf 'XDG_DOCUMENTS_DIR="%s"\n' "$STARC_PROFILE_DIR/documents" > "$XDG_CONFIG_HOME/user-dirs.dirs"
            fi
        fi
        export LD_LIBRARY_PATH="$output:$output/libs${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
        if [[ -n "$QTKEYCHAIN_PREFIX" ]]; then
            export LD_LIBRARY_PATH="$QTKEYCHAIN_PREFIX/lib:$LD_LIBRARY_PATH"
        fi
        # Keep caller-relative project filenames meaningful. Plugins/assets
        # resolve via applicationDirPath, resources, or QStandardPaths.
        exec "$output/starcapp" "$@"
        ;;
    *) usage >&2; fail "Unknown command: $COMMAND" ;;
esac
