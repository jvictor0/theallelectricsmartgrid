#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"
if [[ "${SMARTGRID_LEAN_STUDIO_WORKER:-}" == 1 ]]; then
    exec python3 -B "${SMARTGRID_LEAN_STUDIO_SERVICE:-$repo_root/scripts/lean_studio_worker.py}" lake-command "$@"
fi
exec python3 -B "$repo_root/scripts/lean_studio.py" lake -- "$@"
