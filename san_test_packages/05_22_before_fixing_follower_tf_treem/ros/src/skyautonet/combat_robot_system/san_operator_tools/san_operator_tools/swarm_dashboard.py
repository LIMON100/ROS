"""SAN v1.5 — Swarm Dashboard (Tk GUI).

Adapted from skyhunter_nav_tools::swarm_dashboard (Limon code).
Subscribes to per-robot RobotStatus + FormationStatus + TierStatusChange
and displays a live table of swarm state in a Tkinter window. Useful
for PDR demonstrations.

Usage:
    ros2 run san_operator_tools swarm_dashboard
"""
import threading
from collections import defaultdict
from typing import Dict

import rclpy
from rclpy.node import Node

from combat_robot_msgs.msg import (
    RobotStatus,
    FormationStatus,
    TierStatusChange,
)


TIER_NAMES = {
    0: "T0_PREDICTIVE",
    1: "T1_NORMAL",
    2: "T1.5_REROUTE",
    3: "T2_CATCHUP",
    4: "T3_HARD_CATCH",
    5: "T4_BREADCRUMB",
}


class SwarmDashboard(Node):
    def __init__(self):
        super().__init__("swarm_dashboard")
        # robot_id → state dict
        self.state: Dict[int, dict] = defaultdict(lambda: {
            "pose": (0.0, 0.0),
            "battery": None,
            "tier": "—",
            "leader": False,
            "limp": False,
        })
        # Formation summary
        self.formation_summary = {
            "avg_alignment_m": None,
            "epoch": None,
            "phase": None,
        }

        self.create_subscription(
            RobotStatus, "/robot_status_collected",
            self._on_status, 10)
        self.create_subscription(
            FormationStatus, "/swarm/formation/status",
            self._on_formation, 5)
        self.create_subscription(
            TierStatusChange, "/swarm/tier_status_change",
            self._on_tier, 20)
        # Also subscribe per-robot namespace pattern (best-effort)
        for rid in range(1, 9):
            try:
                self.create_subscription(
                    RobotStatus, f"/robot_{rid}/robot_status",
                    self._on_status, 10)
                self.create_subscription(
                    TierStatusChange,
                    f"/robot_{rid}/tier_node/tier_status_change",
                    self._on_tier, 10)
            except Exception:
                pass

    def _on_status(self, msg: RobotStatus):
        rid = int(msg.robot_id)
        self.state[rid]["pose"] = (
            float(msg.pose.position.x),
            float(msg.pose.position.y),
        )
        try:
            self.state[rid]["battery"] = float(msg.battery_percent)
        except AttributeError:
            pass
        try:
            self.state[rid]["leader"] = bool(msg.is_leader_role_active)
        except AttributeError:
            pass
        try:
            self.state[rid]["limp"] = bool(msg.in_limp_mode)
        except AttributeError:
            pass

    def _on_formation(self, msg: FormationStatus):
        try:
            self.formation_summary["avg_alignment_m"] = float(
                msg.avg_alignment_error_m)
            self.formation_summary["epoch"] = int(msg.formation_epoch)
        except AttributeError:
            pass

    def _on_tier(self, msg: TierStatusChange):
        rid = int(msg.robot_id)
        self.state[rid]["tier"] = TIER_NAMES.get(
            int(msg.current_tier), f"T{msg.current_tier}")


def _gui_main(node: SwarmDashboard):
    """Tk GUI thread."""
    import tkinter as tk
    from tkinter import ttk

    root = tk.Tk()
    root.title("SAN v1.5 — Swarm Dashboard")
    root.geometry("700x420")

    # Formation summary label
    summary_var = tk.StringVar(value="Formation: —")
    ttk.Label(root, textvariable=summary_var, font=("Arial", 11, "bold")
        ).pack(pady=4)

    # Robot table
    columns = ("rid", "role", "x", "y", "battery", "tier", "limp")
    tree = ttk.Treeview(root, columns=columns, show="headings", height=10)
    for col, label, width in [
        ("rid",     "ID",         60),
        ("role",    "Role",       80),
        ("x",       "X (m)",      80),
        ("y",       "Y (m)",      80),
        ("battery", "Batt %",     70),
        ("tier",    "Tier",      150),
        ("limp",    "Limp",       60),
    ]:
        tree.heading(col, text=label)
        tree.column(col, width=width, anchor="center")
    tree.pack(fill="both", expand=True, padx=10, pady=6)

    def refresh():
        # Update summary
        fs = node.formation_summary
        if fs["avg_alignment_m"] is not None:
            summary_var.set(
                f"Formation epoch={fs['epoch']}  "
                f"avg alignment={fs['avg_alignment_m']:.2f} m  "
                f"(KPP-1 ≤ 2.0)")
        # Update rows
        for item in tree.get_children():
            tree.delete(item)
        for rid in sorted(node.state):
            st = node.state[rid]
            tree.insert("", "end", values=(
                rid,
                "LEADER" if st["leader"] else "FOLLOWER",
                f"{st['pose'][0]:.2f}",
                f"{st['pose'][1]:.2f}",
                f"{st['battery']:.0f}" if st["battery"] is not None else "—",
                st["tier"],
                "LIMP" if st["limp"] else "OK",
            ))
        root.after(500, refresh)

    refresh()
    root.mainloop()


def main(args=None):
    rclpy.init(args=args)
    node = SwarmDashboard()
    spin_thread = threading.Thread(target=rclpy.spin, args=(node,), daemon=True)
    spin_thread.start()
    try:
        _gui_main(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":
    main()
