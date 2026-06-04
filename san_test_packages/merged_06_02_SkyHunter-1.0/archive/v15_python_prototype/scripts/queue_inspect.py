#!/usr/bin/env python3
"""
Queue depth inspector — peek at IPC queue states without disturbing them.

Approach: this tool reads the same `/tmp/patrol-metrics.json` file that
main.py publishes. We embed per-queue depths into the metrics snapshot
under a special process name "_ipc". To enable, set in main.py:

    queues_meta = QueueMetrics(queues, metrics_dict)
    queues_meta.start()

(See diag.py for the QueueMetrics impl. This script is a passive reader.)

Usage:
    python3 scripts/queue_inspect.py             # current depths
    python3 scripts/queue_inspect.py --watch     # live refresh
    python3 scripts/queue_inspect.py --high      # show only queues > 50% full
"""
from __future__ import annotations

import argparse
import json
import sys
import time
from pathlib import Path


def render(snapshot: dict, only_high: bool, color: bool) -> str:
    ipc = snapshot.get("_ipc", {})
    if not ipc:
        return "(no _ipc entry — main.py may not have QueueMetrics enabled)"
    counters = ipc.get("counters", {}) or {}
    # Counters of form 'depth:<queue_name>' = current size, 'cap:<name>' = capacity
    rows = []
    seen_names = set()
    for k, v in counters.items():
        if k.startswith("depth:"):
            qname = k.split(":", 1)[1]
            seen_names.add(qname)
            depth = v
            cap = counters.get(f"cap:{qname}", 0)
            full_pct = (100.0 * depth / cap) if cap > 0 else 0.0
            if only_high and full_pct < 50.0:
                continue
            rows.append((qname, depth, cap, full_pct))
    rows.sort(key=lambda r: -r[3])      # most-full first
    if not rows:
        return "(all queues idle)"

    out = [f"{'QUEUE':<22} {'DEPTH':>7} {'CAP':>5} {'FULL':>6}"]
    out.append("─" * 50)
    for name, d, c, p in rows:
        col = "32" if p < 50 else "33" if p < 80 else "31"
        line = f"{name:<22} {d:>7} {c:>5} {p:>5.0f}%"
        if color:
            line = f"\033[{col}m{line}\033[0m"
        out.append(line)
    return "\n".join(out)


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--source", default="/tmp/patrol-metrics.json")
    p.add_argument("--watch", action="store_true")
    p.add_argument("--high", action="store_true",
                   help="Only show queues that are > 50% full")
    p.add_argument("--no-color", action="store_true")
    args = p.parse_args()

    color = (not args.no_color) and sys.stdout.isatty()
    src = Path(args.source)

    def render_once() -> None:
        try:
            data = json.loads(src.read_text())
        except (FileNotFoundError, json.JSONDecodeError):
            print(f"No metrics at {src}", file=sys.stderr)
            sys.exit(1)
        print(render(data, args.high, color))

    if not args.watch:
        render_once()
        return 0

    try:
        while True:
            print("\033[H\033[J", end="")
            print(f"queue_inspect  ·  {time.strftime('%H:%M:%S')}\n")
            render_once()
            time.sleep(1.0)
    except KeyboardInterrupt:
        return 0


if __name__ == "__main__":
    sys.exit(main())
