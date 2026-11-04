#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"
export ELAN_HOME="$repo_root/build/lean/elan"
export MATHLIB_CACHE_DIR="$repo_root/build/lean/mathlib-cache"

if [[ "${1:-}" == setup ]]; then
    if [[ ! -x "$ELAN_HOME/bin/elan" ]]; then
        mkdir -p "$repo_root/build/lean"
        curl --fail --show-error --location \
            https://raw.githubusercontent.com/leanprover/elan/master/elan-init.sh \
            --output "$repo_root/build/lean/elan-init.sh"
        sh "$repo_root/build/lean/elan-init.sh" -y --no-modify-path --default-toolchain none
    fi
    "$ELAN_HOME/bin/elan" toolchain install "$(cat lean-toolchain)"
    exec "$ELAN_HOME/bin/lake" --version
fi

if [[ ! -x "$ELAN_HOME/bin/lake" ]]; then
    echo 'Run make lean-setup to install the worktree-local Lean toolchain.' >&2
    exit 1
fi

exec "$ELAN_HOME/bin/lake" "$@"
