#!/bin/bash
set -euo pipefail
project_dir="$(cd -- "$(dirname -- "$0")" && pwd)"
export UE_ROOT="${UE_ROOT:-/Users/panteleevkirill/Unreal/UE_5.8}"
editor="$UE_ROOT/Engine/Binaries/Mac/UnrealEditor.app"
if [[ ! -d "$editor" ]]; then
    echo "Unreal Engine не найден. Укажи путь через UE_ROOT."
    read -r -p "Нажми Enter для выхода."
    exit 1
fi
if ! bash "$project_dir/Scripts/build.sh"; then
    echo "Сборка завершилась с ошибкой. См. сообщения выше."
    read -r -p "Нажми Enter для выхода."
    exit 1
fi
open -n -a "$editor" --args "$project_dir/SprintDemo.uproject"
