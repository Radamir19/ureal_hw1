#!/usr/bin/env bash
set -euo pipefail

source "$(dirname -- "${BASH_SOURCE[0]}")/ue_env.sh"

if [[ ! -f "$build_script" ]]; then
    printf 'UnrealBuildTool entrypoint not found at %s\n' "$build_script" >&2
    exit 1
fi

exec bash "$build_script" \
    SprintDemoEditor "$platform" Development "$project" \
    -WaitMutex -NoHotReloadFromIDE "$@"
