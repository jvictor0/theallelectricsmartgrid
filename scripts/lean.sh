#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"
cache_dir="$(python3 "$repo_root/scripts/build_cache.py" root)"
export ELAN_HOME="${ELAN_HOME:-$cache_dir/elan}"
export MATHLIB_CACHE_DIR="${MATHLIB_CACHE_DIR:-$cache_dir/mathlib}"
export LAKE_ARTIFACT_CACHE="${LAKE_ARTIFACT_CACHE:-true}"
export LAKE_RESTORE_ARTIFACTS="${LAKE_RESTORE_ARTIFACTS:-true}"

if [[ "${1:-}" == setup ]]; then
    exec python3 "$repo_root/scripts/build_cache.py" lean-setup
fi

if [[ ! -x "$ELAN_HOME/bin/lake" ]]; then
    echo 'Run make lean-setup to install the shared Lean toolchain.' >&2
    exit 1
fi

exec "$ELAN_HOME/bin/lake" "$@"
