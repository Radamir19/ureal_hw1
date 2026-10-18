#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
: "${UE_ROOT:?Set UE_ROOT to the Unreal Engine 5.8 installation directory}"

"$UE_ROOT/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" \
    "$project_dir/SprintDemo.uproject" -run=SprintDemoSetup \
    -unattended -nop4 -nosplash -nullrhi -nosound -NoShaderCompile \
    -stdout -FullStdOutLogOutput "$@"
