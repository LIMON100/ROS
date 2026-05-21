"""Aggregate KPP results from multiple CI iterations (P2-13).

Reads .github/kpp_report.json (overwritten by each iteration) and prints a
human-readable summary. In a multi-iteration workflow this script runs
after every iteration writes the same path; for now we just print the
final state.
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

REPORT_PATH = Path(".github") / "kpp_report.json"


def main() -> int:
    if not REPORT_PATH.exists():
        print(f"No KPP report at {REPORT_PATH}", file=sys.stderr)
        return 1

    try:
        report = json.loads(REPORT_PATH.read_text(encoding="utf-8"))
    except json.JSONDecodeError as e:
        print(f"Invalid report JSON: {e}", file=sys.stderr)
        return 1

    print("KPP Summary:")
    any_failed = False
    for name in sorted(report.keys()):
        data = report[name]
        passed = bool(data.get("passed"))
        if not passed:
            any_failed = True
        status = "PASS" if passed else "FAIL"
        unit = data.get("unit", "")
        unit_suffix = f" {unit}" if unit else ""
        print(f"  {name}: {data['measured']}{unit_suffix} "
              f"(threshold {data['threshold']}{unit_suffix}) [{status}]")

    return 1 if any_failed else 0


if __name__ == "__main__":
    sys.exit(main())
