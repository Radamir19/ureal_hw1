#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
: "${UE_ROOT:?Set UE_ROOT to the Unreal Engine 5.8 installation directory}"
test_started_at="$(date +%s)"

"$UE_ROOT/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" \
    "$project_dir/SprintDemo.uproject" -unattended -nop4 -nosplash -nullrhi -nosound \
    -stdout -FullStdOutLogOutput \
    '-ExecCmds=Automation RunTests SprintDemo' \
    '-TestExit=Automation Test Queue Empty' \
    "-ReportExportPath=$project_dir/Saved/Automation" "$@"

# TestExit can return zero even when individual automation tests failed.
python3 - "$project_dir/Saved/Automation/index.json" "$test_started_at" <<'PY'
import json
from pathlib import Path
import sys

report_path = Path(sys.argv[1])
if not report_path.is_file() or report_path.stat().st_mtime < int(sys.argv[2]):
    sys.exit("No fresh automation report was produced.")
report = json.loads(report_path.read_text(encoding="utf-8-sig"))
tests = report.get("tests", [])
failed = [test.get("fullTestPath", test.get("testDisplayName", "unknown"))
          for test in tests if test.get("state") != "Success"]
if not tests or failed or any(report.get(key, 0) for key in ("failed", "notRun", "inProcess")):
    sys.exit("Automation failed or incomplete: " + ", ".join(failed))
print(f"Automation PASS: {len(tests)} tests.")
PY
