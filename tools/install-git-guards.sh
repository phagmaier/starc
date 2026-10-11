#!/usr/bin/env bash
set -euo pipefail
repo_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$repo_dir"
current=$(git config --get core.hooksPath || true)
if [[ -n $current && $current != .githooks ]]; then
    echo "Existing core.hooksPath: $current. Integrate the guards there before changing it." >&2
    exit 1
fi
default_hooks=$(git rev-parse --git-path hooks)
if [[ -z $current && -d $default_hooks ]]; then
    for hook in "$default_hooks"/*; do
        [[ -f $hook && -x $hook && $hook != *.sample ]] || continue
        echo "Existing hook: $hook. Integrate it into .githooks before changing hooksPath." >&2
        exit 1
    done
fi
git config --local core.hooksPath .githooks
echo 'Installed repository Git guards for frozen master.'
