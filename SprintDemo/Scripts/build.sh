#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
: "${UE_ROOT:?Set UE_ROOT to the Unreal Engine 5.8 installation directory}"

bash "$UE_ROOT/Engine/Build/BatchFiles/Mac/Build.sh" \
    SprintDemoEditor Mac Development "$project_dir/SprintDemo.uproject" \
    -WaitMutex -NoHotReloadFromIDE "$@"
