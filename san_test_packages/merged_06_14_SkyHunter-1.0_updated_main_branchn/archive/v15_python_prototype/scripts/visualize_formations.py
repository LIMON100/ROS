"""Render PNG previews of all 9 formation types (P2-1).

Usage:
    python scripts/visualize_formations.py [--out tests/data] [--n 6] [--d 5.0]

Saves one PNG per FormationType to <out>/formation_<name>.png. Body frame:
+x is leader heading, +y is left of leader. Leader is at origin (red star).
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

import matplotlib.pyplot as plt

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from mission.formation_library import FormationLibrary, FormationType  # noqa: E402


def render(formation_type: FormationType, n: int, d: float,
           out_dir: Path) -> Path:
    offsets = FormationLibrary.compute(formation_type, n_followers=n, d_m=d)
    fig, ax = plt.subplots(figsize=(6, 6))
    ax.plot(0, 0, "r*", markersize=20, label="Leader")
    for i, (x, y) in enumerate(offsets):
        ax.plot(x, y, "bo", markersize=10)
        ax.annotate(f"F{i+1}", (x, y), textcoords="offset points",
                    xytext=(6, 6))
    ax.set_aspect("equal")
    ax.set_title(f"{formation_type.value}  (n={n}, d={d} m)")
    ax.set_xlabel("x_body (m) — forward")
    ax.set_ylabel("y_body (m) — left")
    ax.grid(True, alpha=0.3)
    ax.axhline(0, color="gray", lw=0.5)
    ax.axvline(0, color="gray", lw=0.5)
    ax.legend(loc="upper right")
    out_path = out_dir / f"formation_{formation_type.value}.png"
    fig.savefig(out_path, dpi=120, bbox_inches="tight")
    plt.close(fig)
    return out_path


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("--out", default="tests/data",
                   help="Output directory (created if missing)")
    p.add_argument("--n", type=int, default=6, help="Number of followers")
    p.add_argument("--d", type=float, default=5.0, help="Spacing in meters")
    args = p.parse_args()

    out_dir = Path(args.out)
    out_dir.mkdir(parents=True, exist_ok=True)

    for ft in FormationType:
        path = render(ft, args.n, args.d, out_dir)
        print(f"  {ft.value:<14}  -> {path}")


if __name__ == "__main__":
    main()
