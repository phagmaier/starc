#!/usr/bin/env bash
# Run after the host's normal full Arch upgrade (sudo pacman -Syu).
set -euo pipefail

ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
branch=$(git -C "$ROOT" symbolic-ref --short HEAD) || { printf 'Use dev or a feature branch.\n' >&2; exit 1; }
[[ "$branch" != master ]] || { printf 'master is frozen; switch to dev first.\n' >&2; exit 1; }
[[ -f /etc/arch-release ]] || { printf 'This installer is for Arch Linux.\n' >&2; exit 1; }

options=()
case "${1:-}" in
    '') ;;
    --yes) options+=(--noconfirm) ;;
    *) printf 'Usage: ./build/setup_arch.sh [--yes]\n' >&2; exit 1 ;;
esac
(( $# <= 1 )) || { printf 'Too many arguments.\n' >&2; exit 1; }

printf 'Install missing development packages from current package databases.\n'
printf 'Prerequisite: finish a full Arch upgrade with sudo pacman -Syu first.\n'
packages=(
    base-devel git cmake ninja pkgconf python curl openssl zlib
    qt6-base qt6-5compat qt6-multimedia qt6-tools qt6-wayland
    libxkbcommon-x11 xcb-util-cursor
    clang gdb shellcheck
)
if (( EUID == 0 )); then
    pacman -S --needed "${options[@]}" "${packages[@]}"
else
    sudo pacman -S --needed "${options[@]}" "${packages[@]}"
fi
"$ROOT/build/dev.sh" deps
printf 'Packages and public PDF sources are ready. Next: ./build/dev.sh crashpad debug\n'
