#!/usr/bin/env python3
"""
Live debugging dashboard — terminal-based system overview.

Reads the metrics shared dict that main.py sets up, renders a refreshing
table of every process: rate, latency p50/p95, exception count, thread
liveness, custom counters.

Usage (terminal-attached, while main.py runs):
    python3 scripts/debug_dashboard.py

For CI/log usage:
    python3 scripts/debug_dashboard.py --once --json

Wire it up by setting `--metrics-socket /tmp/patrol-metrics.json` and
having main.py write its snapshot dict there every second. We use a JSON
file because Manager dicts only work within a single Python process tree;
a JSON file works across SSH sessions too.
"""
from __future__ import annotations

import argparse
import json
import sys
import time
from pathlib import Path
from typing import Any, Dict, List


def _ansi(code: str, text: str, color: bool) -> str:
    return f"\033[{code}m{text}\033[0m" if color else text


def _format_age(now_mono: float, last: float) -> str:
    if last <= 0:
        return "—"
    age = now_mono - last
    if age < 1.0:
        return f"{age*1000:.0f}ms"
    if age < 60.0:
        return f"{age:.1f}s"
    if age < 3600.0:
        return f"{age/60:.1f}m"
    return f"{age/3600:.1f}h"


def render(metrics: Dict[str, Any], color: bool = True) -> str:
    out: List[str] = []
    out.append(_ansi("1;36",
        f"{'PROCESS':<22} {'PID':>6} {'STEPS':>8} {'P50':>6} {'P95':>6} "
        f"{'EXC':>4} {'LAST_STEP':>10} {'THREADS':>8}", color))
    out.append("─" * 78)

    # The shared dict is mp.Manager().dict — convert to plain dict
    items = sorted(metrics.items())
    now_mono = time.monotonic()
    for name, m in items:
        if not isinstance(m, dict):
            continue
        steps      = m.get("step_count", 0)
        latencies  = m.get("latency_samples_ms", []) or []
        if latencies:
            s = sorted(latencies)
            p50 = s[len(s) // 2]
            p95 = s[int(len(s) * 0.95)] if len(s) > 1 else s[0]
        else:
            p50 = p95 = 0.0
        exc       = m.get("exception_count", 0)
        last_step = m.get("last_step_at", 0)
        threads   = m.get("threads", {}) or {}
        last_step_age = _format_age(now_mono, last_step)
        # Color exceptions red, healthy green, unhealthy yellow
        if exc > 0:
            row_color = "31"
        elif last_step > 0 and (now_mono - last_step) > 5.0:
            row_color = "33"
        else:
            row_color = "32"
        line = (f"{name:<22} {m.get('pid', 0):>6} {steps:>8} "
                f"{p50:>5.1f}m {p95:>5.1f}m {exc:>4} "
                f"{last_step_age:>10} {len(threads):>8}")
        out.append(_ansi(row_color, line, color))

        # Last exception (one line, indented)
        last_exc = m.get("last_exception", "")
        if last_exc:
            out.append(_ansi("31",
                f"   ⮕ {last_exc[:74]}", color))
        # Custom counters (joined inline if any)
        counters = m.get("counters", {}) or {}
        if counters:
            cs = "   "
            cs += "  ".join(f"{k}={v}" for k, v in
                            sorted(counters.items())[:6])
            out.append(_ansi("90", cs, color))
    return "\n".join(out)


def watch(path: Path, interval: float, color: bool) -> None:
    """Refresh in-place every `interval` seconds."""
    print("\033[?1049h\033[H", end="")    # alt screen
    try:
        while True:
            try:
                metrics = json.loads(path.read_text())
            except (FileNotFoundError, json.JSONDecodeError):
                metrics = {}
            print("\033[H\033[J", end="")    # cursor home + clear
            print(_ansi("1", f"patrol live  ·  {time.strftime('%H:%M:%S')}  ·  "
                             f"source={path}",
                        color))
            print()
            print(render(metrics, color=color))
            print()
            print(_ansi("90", "Ctrl-C to exit", color))
            time.sleep(interval)
    except KeyboardInterrupt:
        pass
    finally:
        print("\033[?1049l", end="")        # leave alt screen


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--source", default="/tmp/patrol-metrics.json",
                   help="JSON file written by main.py with the metrics snapshot")
    p.add_argument("--interval", type=float, default=1.0,
                   help="Refresh interval in seconds")
    p.add_argument("--once", action="store_true",
                   help="Render once and exit (CI-friendly)")
    p.add_argument("--json", action="store_true",
                   help="Output the raw metrics JSON instead of a table")
    p.add_argument("--no-color", action="store_true",
                   help="Disable ANSI colors")
    args = p.parse_args()

    src = Path(args.source)
    color = (not args.no_color) and sys.stdout.isatty()

    if args.once:
        try:
            metrics = json.loads(src.read_text())
        except (FileNotFoundError, json.JSONDecodeError):
            print(f"No metrics found at {src}", file=sys.stderr)
            return 1
        if args.json:
            print(json.dumps(metrics, indent=2, default=str))
        else:
            print(render(metrics, color=color))
        return 0

    watch(src, args.interval, color)
    return 0


if __name__ == "__main__":
    sys.exit(main())
