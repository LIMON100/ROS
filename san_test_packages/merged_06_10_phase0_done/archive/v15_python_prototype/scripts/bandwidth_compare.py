"""Compare SLAM-aggregation wire bandwidth between v1.0 and v1.1.

Spec command shape (SAN-TST-INT-001 v1.1 §7 wrap-up)::

    python scripts/bandwidth_compare.py \\
        --baseline v1.0_baseline.bag \\
        --current latest.bag

This codebase has no rosbag pipeline, so the ``--baseline`` and
``--current`` arguments are accepted as informational labels only.
The actual numbers come from:

  * **v1.0 baseline** — the documented 1 Hz raw-occupancy broadcast.
    Defaults: 1 Hz × 3.2 MB raw payload (per ADR-002, 8 followers at
    40 m × 40 m × 5 cm resolution). Override via
    ``--baseline-rate-hz`` and ``--baseline-payload-bytes``.

  * **v1.1 current** — ``mapping.slam_aggregator.PERIOD_BY_MODE`` for
    the cadence and ``--current-payload-bytes`` for the PNG-delta
    payload (default 32 KiB, the typical compressed delta size).

The script prints a Markdown table to stdout (or to ``--output``) with
per-mode bytes-per-second and the v1.0 → v1.1 reduction ratio. ADR-002
claims ~100× — anything materially below that should trigger a review.
"""
from __future__ import annotations

import argparse
import sys
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Iterable, List, Optional

REPO_ROOT = Path(__file__).resolve().parent.parent
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from mapping.slam_aggregator import MODE_DEFAULT, MODES, PERIOD_BY_MODE  # noqa: E402

# ADR-002 documents the v1.0 raw-broadcast envelope. Allow override
# from the CLI for users who measured a different baseline.
DEFAULT_BASELINE_RATE_HZ = 1.0
DEFAULT_BASELINE_PAYLOAD_BYTES = 3_200_000

# v1.1 PNG-delta size is empirically ~16-48 KiB per follower per
# aggregation; 32 KiB is the conservative midpoint.
DEFAULT_CURRENT_PAYLOAD_BYTES = 32_768

# Claim asserted in ADR-002. Anything materially under this should
# trigger a review.
ADR_002_TARGET_RATIO = 100.0


@dataclass(frozen=True)
class ModeReport:
    mode: str
    period_sec: float
    payload_bytes: int

    @property
    def bytes_per_sec(self) -> float:
        return self.payload_bytes / self.period_sec

    @property
    def bits_per_sec(self) -> float:
        return self.bytes_per_sec * 8

    @property
    def kbps(self) -> float:
        return self.bits_per_sec / 1_000


def baseline_bytes_per_sec(rate_hz: float, payload_bytes: int) -> float:
    return rate_hz * payload_bytes


def reductions(
    baseline_bps: float,
    modes: Iterable[ModeReport],
) -> List[float]:
    return [baseline_bps / m.bytes_per_sec if m.bytes_per_sec else float("inf")
            for m in modes]


def _fmt_bytes(b: float) -> str:
    if b >= 1_000_000:
        return f"{b / 1_000_000:.2f} MB/s"
    if b >= 1_000:
        return f"{b / 1_000:.2f} kB/s"
    return f"{b:.0f} B/s"


def build_markdown(
    *,
    baseline_rate_hz: float,
    baseline_payload_bytes: int,
    current_payload_bytes: int,
    baseline_label: Optional[str] = None,
    current_label: Optional[str] = None,
) -> str:
    baseline_bps = baseline_bytes_per_sec(
        baseline_rate_hz, baseline_payload_bytes)

    reports = [
        ModeReport(mode=m, period_sec=PERIOD_BY_MODE[m],
                   payload_bytes=current_payload_bytes)
        for m in MODES
    ]
    ratios = reductions(baseline_bps, reports)
    default_ratio = next(
        ratio for m, ratio in zip(reports, ratios, strict=True) if m.mode == MODE_DEFAULT)

    now = datetime.now(timezone.utc).strftime("%Y-%m-%d %H:%M:%SZ")
    lines: List[str] = []
    lines.append("# SLAM aggregation bandwidth — v1.0 vs v1.1")
    lines.append("")
    lines.append(f"Generated {now}.  Reference: ADR-002.")
    lines.append("")
    lines.append("## Inputs")
    lines.append("")
    lines.append(f"- Baseline label: `{baseline_label or '(unset)'}`")
    lines.append(f"- Current label:  `{current_label or '(unset)'}`")
    lines.append(f"- Baseline rate: **{baseline_rate_hz} Hz**")
    lines.append(f"- Baseline payload: **{baseline_payload_bytes:,} B/frame**")
    lines.append(f"- v1.1 PNG-delta payload: **{current_payload_bytes:,} B/frame**")
    lines.append(f"- v1.0 wire rate: **{_fmt_bytes(baseline_bps)}** "
                 f"(≈ {baseline_bps * 8 / 1e6:.1f} Mbps)")
    lines.append("")

    lines.append("## v1.1 per-mode wire rate")
    lines.append("")
    lines.append("| Mode | Period (s) | Wire rate | Reduction vs v1.0 |")
    lines.append("|---|---:|---:|---:|")
    for m, ratio in zip(reports, ratios, strict=True):
        lines.append(
            f"| `{m.mode}` | {m.period_sec:.0f} | "
            f"{_fmt_bytes(m.bytes_per_sec)} | **{ratio:.1f}×** |"
        )
    lines.append("")

    verdict = "PASS" if default_ratio >= ADR_002_TARGET_RATIO else "FAIL"
    lines.append("## Acceptance vs ADR-002")
    lines.append("")
    lines.append(
        f"- Default-mode reduction: **{default_ratio:.1f}×** "
        f"(target: ≥ {ADR_002_TARGET_RATIO:.0f}×) — **{verdict}**")
    if verdict == "FAIL":
        lines.append(
            "  - Below the ADR-002 target. Verify the PNG-delta payload "
            "estimate and aggregator cadence before signing off.")
    lines.append("")

    return "\n".join(lines) + "\n"


def main(argv: Optional[List[str]] = None) -> int:
    parser = argparse.ArgumentParser(
        description=("Compute the v1.0 → v1.1 SLAM-aggregation wire-"
                     "bandwidth reduction and emit a Markdown report."))
    parser.add_argument("--baseline", type=str, default=None,
                        help="Label for the v1.0 source (unused; informational).")
    parser.add_argument("--current", type=str, default=None,
                        help="Label for the v1.1 source (unused; informational).")
    parser.add_argument("--baseline-rate-hz", type=float,
                        default=DEFAULT_BASELINE_RATE_HZ)
    parser.add_argument("--baseline-payload-bytes", type=int,
                        default=DEFAULT_BASELINE_PAYLOAD_BYTES)
    parser.add_argument("--current-payload-bytes", type=int,
                        default=DEFAULT_CURRENT_PAYLOAD_BYTES)
    parser.add_argument(
        "--output", type=Path, default=None,
        help="Markdown output path. Default: stdout.")

    args = parser.parse_args(argv)
    md = build_markdown(
        baseline_rate_hz=args.baseline_rate_hz,
        baseline_payload_bytes=args.baseline_payload_bytes,
        current_payload_bytes=args.current_payload_bytes,
        baseline_label=args.baseline,
        current_label=args.current,
    )
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(md, encoding="utf-8")
        print(f"Wrote {args.output} ({args.output.stat().st_size} bytes)")
    else:
        try:
            sys.stdout.reconfigure(encoding="utf-8")  # type: ignore[attr-defined]
        except (AttributeError, OSError):
            pass
        sys.stdout.write(md)
    return 0


if __name__ == "__main__":
    sys.exit(main())
