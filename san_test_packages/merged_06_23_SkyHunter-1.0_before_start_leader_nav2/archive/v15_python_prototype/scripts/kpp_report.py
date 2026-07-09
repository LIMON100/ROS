"""Render a v1.1 KPP acceptance report as standalone HTML.

Two data sources are combined:

  1. ``--input`` (default ``.github/kpp_report.json``) — measured KPP
     values produced by CI iterations and by ``sim/scripts/measure_kpp_in_sim.py``.
     Schema is the legacy ``{KPP-N: {measured, threshold, passed, unit}}``
     dict that ``scripts/aggregate_kpp.py`` already consumes.

  2. **Declared budgets** introspected from source-of-truth modules:

       * ``streaming/latency_budget.py``     → KPP-7 video latency
       * ``safety/hub_health_monitor.py``    → S15-3 heartbeat timeout
       * ``mapping/slam_aggregator.py``      → S15-4 aggregation cadence
       * ``swarm/sector_assign.py``          → S15-1 360° coverage target

     These are *declared* numbers — what the codebase asserts in the
     test suite. The live counterparts (S15-2/5/6) need a self-hosted
     runner; rows for them are emitted as ``LIVE-ONLY`` placeholders
     when ``--include-deferred`` is set.

The spec's original ``--rosbag latest`` argument is accepted but unused
(this codebase does not produce rosbags). The HTML header carries a
"data sources" line so reviewers know what was actually inspected.

Usage::

    python scripts/kpp_report.py --output kpp_v1.1_report.html
    python scripts/kpp_report.py --input .github/kpp_report.json \\
        --output kpp_v1.1_report.html --include-deferred
"""
from __future__ import annotations

import argparse
import html
import json
import sys
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Dict, List, Optional

REPO_ROOT = Path(__file__).resolve().parent.parent
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))


@dataclass(frozen=True)
class KppRow:
    """One row in the rendered table."""
    kpp_id: str
    name: str
    threshold: str
    measured: str
    unit: str
    status: str          # PASS / FAIL / DECLARED / LIVE-ONLY / NO-DATA
    source: str          # short note: where the row came from


def _status_badge(status: str) -> str:
    palette = {
        "PASS":      ("#1f7a1f", "#e8f5e8"),
        "FAIL":      ("#a40000", "#fbe6e6"),
        "DECLARED":  ("#1d4ed8", "#e6effb"),
        "LIVE-ONLY": ("#7c4a00", "#fbeed7"),
        "NO-DATA":   ("#555",    "#eee"),
    }
    fg, bg = palette.get(status, ("#000", "#eee"))
    return (f'<span style="color:{fg};background:{bg};padding:2px 6px;'
            f'border-radius:3px;font-weight:600;font-size:12px;">{status}'
            f'</span>')


def _load_measured(input_path: Path) -> Dict[str, Dict[str, Any]]:
    if not input_path.exists():
        return {}
    try:
        return json.loads(input_path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        print(f"Warning: cannot parse {input_path}: {exc}", file=sys.stderr)
        return {}


def _measured_rows(measured: Dict[str, Dict[str, Any]]) -> List[KppRow]:
    rows: List[KppRow] = []
    for kpp_id in sorted(measured.keys()):
        data = measured[kpp_id]
        passed = bool(data.get("passed"))
        unit = str(data.get("unit", ""))
        rows.append(KppRow(
            kpp_id=kpp_id,
            name=str(data.get("name", kpp_id)),
            threshold=f"{data.get('threshold', '?')}",
            measured=f"{data.get('measured', '?')}",
            unit=unit,
            status="PASS" if passed else "FAIL",
            source="measured (CI / sim)",
        ))
    return rows


def _declared_rows() -> List[KppRow]:
    """Introspect source-of-truth modules for declared v1.1 budgets."""
    rows: List[KppRow] = []

    # KPP-7 — video latency 200 ms.
    try:
        from streaming.latency_budget import (
            KPP_LATENCY_BUDGET_MS,
            budget_remaining_ms,
            total_budget_ms,
        )
        declared = total_budget_ms()
        remaining = budget_remaining_ms()
        rows.append(KppRow(
            kpp_id="KPP-7",
            name="Glass-to-glass video latency",
            threshold=str(KPP_LATENCY_BUDGET_MS),
            measured=f"{declared} (headroom {remaining:+d})",
            unit="ms",
            status="DECLARED" if remaining >= 0 else "FAIL",
            source="streaming/latency_budget.py",
        ))
    except Exception as exc:  # pragma: no cover — kept defensive
        rows.append(KppRow("KPP-7", "Glass-to-glass video latency",
                           "200", "?", "ms", "NO-DATA", f"import failed: {exc}"))

    # S15-1 — 360° coverage from 8-slot V-formation.
    try:
        from swarm.sector_assign import STANDARD_V_FORMATION_ROLES
        rows.append(KppRow(
            kpp_id="S15-1",
            name="V-formation 360° coverage (v1.5: 9 logical slots, "
                 "8-robot squadron max)",
            threshold="360.0",
            measured=f"360.0 (slots={len(STANDARD_V_FORMATION_ROLES)})",
            unit="deg",
            status="DECLARED",
            source="swarm/sector_assign.py + tests/test_s15_1_surveillance.py",
        ))
    except Exception as exc:  # pragma: no cover
        rows.append(KppRow("S15-1", "V-formation 360° coverage",
                           "360.0", "?", "deg", "NO-DATA", f"import failed: {exc}"))

    # S15-3 — Hub UGV dual-SBC heartbeat timeout.
    try:
        from safety.hub_health_monitor import HEARTBEAT_TIMEOUT_SEC
        rows.append(KppRow(
            kpp_id="S15-3",
            name="Hub dual-SBC heartbeat timeout",
            threshold="3.0",
            measured=f"{HEARTBEAT_TIMEOUT_SEC}",
            unit="s",
            status="DECLARED",
            source="safety/hub_health_monitor.py",
        ))
    except Exception as exc:  # pragma: no cover
        rows.append(KppRow("S15-3", "Hub dual-SBC heartbeat timeout",
                           "3.0", "?", "s", "NO-DATA", f"import failed: {exc}"))

    # S15-4 — SLAM aggregation cadence (default + narrow).
    try:
        from mapping.slam_aggregator import (
            MODE_DEFAULT,
            MODE_NARROW,
            PERIOD_BY_MODE,
        )
        rows.append(KppRow(
            kpp_id="S15-4",
            name=f"SLAM aggregation cadence ({MODE_DEFAULT})",
            threshold="30",
            measured=f"{PERIOD_BY_MODE[MODE_DEFAULT]:.0f}",
            unit="s",
            status="DECLARED",
            source="mapping/slam_aggregator.py",
        ))
        rows.append(KppRow(
            kpp_id="S15-4b",
            name=f"SLAM aggregation cadence ({MODE_NARROW})",
            threshold="15",
            measured=f"{PERIOD_BY_MODE[MODE_NARROW]:.0f}",
            unit="s",
            status="DECLARED",
            source="mapping/slam_aggregator.py",
        ))
    except Exception as exc:  # pragma: no cover
        rows.append(KppRow("S15-4", "SLAM aggregation cadence",
                           "30", "?", "s", "NO-DATA", f"import failed: {exc}"))

    return rows


def _deferred_rows() -> List[KppRow]:
    return [
        KppRow("S15-2", "Pan-tilt sweep detection rate",
               "≥ 90", "—", "%", "LIVE-ONLY",
               "robot-lab runner (Gazebo W2 forest + YOLOv5 GPU)"),
        KppRow("S15-5", "SRT glass-to-glass measurement",
               "≤ 200", "—", "ms", "LIVE-ONLY",
               "robot-lab runner (real Wi-Fi/LTE link + tablet)"),
        KppRow("S15-6", "Multi-follower concurrent video",
               "≥ 3 FHD", "—", "streams", "LIVE-ONLY",
               "robot-lab runner (Android app + N real followers)"),
    ]


def _summary_counts(rows: List[KppRow]) -> Dict[str, int]:
    counts: Dict[str, int] = {}
    for r in rows:
        counts[r.status] = counts.get(r.status, 0) + 1
    return counts


def _render_html(rows: List[KppRow], data_sources: List[str]) -> str:
    counts = _summary_counts(rows)
    summary_chips = "".join(
        f'<span style="margin-right:8px">{_status_badge(s)}'
        f'<span style="margin-left:4px">{n}</span></span>'
        for s, n in sorted(counts.items())
    )
    now = datetime.now(timezone.utc).strftime("%Y-%m-%d %H:%M:%SZ")

    body_rows = "\n".join(
        f"<tr>"
        f"<td>{html.escape(r.kpp_id)}</td>"
        f"<td>{html.escape(r.name)}</td>"
        f"<td style='text-align:right'>{html.escape(r.threshold)}</td>"
        f"<td style='text-align:right'>{html.escape(r.measured)}</td>"
        f"<td>{html.escape(r.unit)}</td>"
        f"<td>{_status_badge(r.status)}</td>"
        f"<td><code>{html.escape(r.source)}</code></td>"
        f"</tr>"
        for r in rows
    )

    sources_lis = "\n".join(
        f"<li><code>{html.escape(s)}</code></li>" for s in data_sources)

    return f"""<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>v1.1 KPP Acceptance Report</title>
<style>
  body {{ font-family: -apple-system, Segoe UI, sans-serif;
         max-width: 1100px; margin: 32px auto; padding: 0 16px;
         color: #1a1a1a; }}
  h1 {{ font-size: 22px; margin: 0 0 6px; }}
  .subtitle {{ color: #555; font-size: 13px; margin-bottom: 18px; }}
  .summary {{ margin: 14px 0 24px; font-size: 13px; }}
  table {{ width: 100%; border-collapse: collapse; font-size: 13px; }}
  th, td {{ border-bottom: 1px solid #eee; padding: 7px 10px;
            text-align: left; vertical-align: top; }}
  thead th {{ background: #fafafa; }}
  code {{ font-family: ui-monospace, Menlo, Consolas, monospace; font-size: 12px; }}
  ul {{ font-size: 13px; }}
  .footer {{ color: #888; font-size: 11px; margin-top: 32px; }}
</style>
</head>
<body>
  <h1>v1.1 KPP Acceptance Report</h1>
  <div class="subtitle">
    Generated {html.escape(now)} · SAN-IDS-CMD-001 v1.1 / SAN-TST-INT-001 v1.1
  </div>
  <div class="summary"><strong>Summary:</strong> {summary_chips}</div>

  <table>
    <thead>
      <tr>
        <th>KPP</th><th>Name</th><th>Threshold</th><th>Value</th>
        <th>Unit</th><th>Status</th><th>Source</th>
      </tr>
    </thead>
    <tbody>
{body_rows}
    </tbody>
  </table>

  <h3 style="margin-top:32px;font-size:14px">Data sources</h3>
  <ul>
{sources_lis}
  </ul>

  <div class="footer">
    PASS/FAIL rows come from <code>--input</code> (CI / sim measured).
    DECLARED rows come from source-of-truth modules — the codebase
    asserts these in the test suite. LIVE-ONLY rows need a self-hosted
    runner; see <code>.github/workflows/regression.yml</code>.
  </div>
</body>
</html>
"""


def build_report(
    input_path: Path,
    *,
    include_deferred: bool,
    extra_source_label: Optional[str] = None,
) -> str:
    measured = _load_measured(input_path)
    rows: List[KppRow] = []
    rows.extend(_measured_rows(measured))
    rows.extend(_declared_rows())
    if include_deferred:
        rows.extend(_deferred_rows())

    data_sources: List[str] = []
    if measured:
        data_sources.append(str(input_path))
    else:
        data_sources.append(f"{input_path} (not found — declared-only mode)")
    data_sources.append("streaming/latency_budget.py")
    data_sources.append("safety/hub_health_monitor.py")
    data_sources.append("mapping/slam_aggregator.py")
    data_sources.append("swarm/sector_assign.py")
    if extra_source_label:
        data_sources.append(extra_source_label)

    return _render_html(rows, data_sources)


def main(argv: Optional[List[str]] = None) -> int:
    parser = argparse.ArgumentParser(
        description=("Render the v1.1 KPP acceptance report (HTML). "
                     "Combines pytest-measured numbers with declared "
                     "source-of-truth budgets."))
    parser.add_argument(
        "--input", type=Path,
        default=REPO_ROOT / ".github" / "kpp_report.json",
        help="Measured KPP JSON (default: %(default)s).")
    parser.add_argument(
        "--output", type=Path, required=True,
        help="HTML output path.")
    parser.add_argument(
        "--include-deferred", action="store_true",
        help="Add LIVE-ONLY rows for S15-2/5/6 deferred scenarios.")
    parser.add_argument(
        "--rosbag", type=str, default=None,
        help=("Spec compatibility shim. Accepted but unused — this "
              "codebase has no rosbag pipeline. The label is included "
              "in the data-sources list verbatim."))

    args = parser.parse_args(argv)
    extra = f"rosbag tag: {args.rosbag}" if args.rosbag else None
    html_body = build_report(
        args.input,
        include_deferred=args.include_deferred,
        extra_source_label=extra,
    )
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(html_body, encoding="utf-8")
    print(f"Wrote {args.output} ({args.output.stat().st_size} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
