#!/usr/bin/env bash
set -euo pipefail

source "$(dirname -- "${BASH_SOURCE[0]}")/ue_env.sh"

if [[ ! -x "$editor" ]]; then
    printf 'UnrealEditor not found at %s\n' "$editor" >&2
    exit 1
fi

exec "$editor" "$project" -run=SprintDemoSetup \
    -unattended -nop4 -nosplash -nullrhi -nosound -NoShaderCompile \
    -stdout -FullStdOutLogOutput "$@"
