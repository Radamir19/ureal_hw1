# Sourced by the project scripts.
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
: "${UE_ROOT:?Set UE_ROOT to the Unreal Engine 5.8 installation directory}"

case "$(uname -s)" in
    Darwin)
        platform=Mac
        editor="$UE_ROOT/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
        ;;
    Linux)
        platform=Linux
        editor="$UE_ROOT/Engine/Binaries/Linux/UnrealEditor"
        ;;
    *)
        printf 'These scripts support macOS and Linux. On Windows, use the SprintDemoEditor target.\n' >&2
        return 1
        ;;
esac

build_script="$UE_ROOT/Engine/Build/BatchFiles/$platform/Build.sh"
project="$project_dir/SprintDemo.uproject"
