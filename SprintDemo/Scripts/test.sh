#!/usr/bin/env bash
set -euo pipefail

source "$(dirname -- "${BASH_SOURCE[0]}")/ue_env.sh"

if [[ ! -x "$editor" ]]; then
    printf 'UnrealEditor not found at %s\n' "$editor" >&2
    exit 1
fi

mkdir -p "$project_dir/Saved/Automation"
report_dir="$(mktemp -d "$project_dir/Saved/Automation/Run.XXXXXX")"
printf 'Report directory %s\n' "$report_dir"

"$editor" "$project" -unattended -nop4 -nosplash -nullrhi -nosound \
    -stdout -FullStdOutLogOutput \
    '-ExecCmds=Automation RunTests SprintDemo' \
    '-TestExit=Automation Test Queue Empty' \
    "-ReportExportPath=$report_dir" "$@"

# TestExit can return zero even when individual automation tests failed.
python3 - "$report_dir/index.json" <<'PY'
import json
from pathlib import Path
import sys

report_path = Path(sys.argv[1])
if not report_path.is_file():
    sys.exit("No fresh automation report was produced.")
report = json.loads(report_path.read_text(encoding="utf-8-sig"))
tests = report.get("tests", [])
required = {"SprintDemo.Integration.InputBlueprintAndAnimation", "SprintDemo.Integration.PCGGuardBehavior"}
missing = required - {test.get("fullTestPath") for test in tests}
if missing:
    sys.exit("Missing automation tests: " + ", ".join(sorted(missing)))
failed = [test.get("fullTestPath", test.get("testDisplayName", "unknown"))
          for test in tests if test.get("state") != "Success"]
if failed or any(report.get(key, 0) for key in ("failed", "notRun", "inProcess")):
    sys.exit("Automation failed or incomplete: " + ", ".join(failed))
print(f"Automation PASS: {len(tests)} tests.")
PY
