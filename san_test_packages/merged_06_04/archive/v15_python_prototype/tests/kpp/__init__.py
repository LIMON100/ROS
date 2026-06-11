"""KPP 5종 자동 측정 — CI integration (SDD §5.6.5, P2-13).

KPP §2.1.1 spec:
  KPP-1: 대열 유지 평균 오차 ≤ 2 m
  KPP-2: 근접 위험 회피 ≤ 300 ms
  KPP-3: 군집 제어 통신 지연 ≤ 150 ms
  KPP-4: 리더 이탈 재구성 ≤ 10 s
  KPP-5: 집결 성공률 ≥ 95 %

Each KPP runs as integration test. JSON report at .github/kpp_report.json.
"""
from __future__ import annotations

import json
from pathlib import Path
from typing import Optional

REPORT_PATH = Path(".github") / "kpp_report.json"


def emit_kpp_result(name: str, measured: float, threshold: float,
                    passed: bool, unit: Optional[str] = None) -> None:
    """Merge one KPP result into the shared report file."""
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    report: dict = {}
    if REPORT_PATH.exists():
        try:
            report = json.loads(REPORT_PATH.read_text(encoding="utf-8"))
        except json.JSONDecodeError:
            report = {}
    entry = {
        "measured": round(float(measured), 4),
        "threshold": float(threshold),
        "passed": bool(passed),
    }
    if unit:
        entry["unit"] = unit
    report[name] = entry
    REPORT_PATH.write_text(json.dumps(report, indent=2), encoding="utf-8")
