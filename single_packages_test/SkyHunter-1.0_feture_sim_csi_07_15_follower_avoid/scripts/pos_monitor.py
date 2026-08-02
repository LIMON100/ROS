#!/usr/bin/env python3
"""Log per-robot pose/speed from /sN/odometry/global every 2 s.

Prints one CSV-ish line per sample: t, s1=(x,y|v) s2=(...) s3=(...).
Used to score the wall-gap test: did every robot thread the gap (min_y<=-6
in x in [90,105]) and reach x~160 without freezing (v~0 at x<95)?
"""
import math
import rclpy
from rclpy.node import Node
from rclpy.qos import QoSProfile, QoSReliabilityPolicy, QoSHistoryPolicy
from nav_msgs.msg import Odometry


class PosMon(Node):
    def __init__(self, n):
        super().__init__("pos_monitor")
        self.rids = list(range(1, n + 1))
        self.last = {r: None for r in self.rids}
        qos = QoSProfile(depth=10,
                         reliability=QoSReliabilityPolicy.BEST_EFFORT,
                         history=QoSHistoryPolicy.KEEP_LAST)
        for r in self.rids:
            self.create_subscription(
                Odometry, f"/s{r}/odometry/global",
                lambda m, rr=r: self._odom(rr, m), qos)
        self.t0 = None
        self.create_timer(2.0, self._tick)

    def _odom(self, r, m):
        p = m.pose.pose.position
        v = m.twist.twist.linear
        self.last[r] = (p.x, p.y, math.hypot(v.x, v.y))

    def _tick(self):
        if self.t0 is None:
            self.t0 = self.get_clock().now()
        t = (self.get_clock().now() - self.t0).nanoseconds / 1e9
        cells = []
        for r in self.rids:
            d = self.last[r]
            if d is None:
                cells.append(f"s{r}=(--)")
            else:
                cells.append(f"s{r}=({d[0]:6.1f},{d[1]:6.1f}|{d[2]:4.2f})")
        print(f"t={t:6.1f}  " + "  ".join(cells), flush=True)


def main():
    import sys
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 3
    rclpy.init()
    node = PosMon(n)
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":
    main()
