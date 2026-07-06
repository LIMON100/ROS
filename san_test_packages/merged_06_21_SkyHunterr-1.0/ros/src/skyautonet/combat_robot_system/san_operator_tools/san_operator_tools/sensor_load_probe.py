#!/usr/bin/env python3
# Q2 S2 — sensor_load_probe: ROS-native (no shell-out, ADR-006) per-robot sensor
# rate / jitter / drop probe for the sensors-under-load test (S3).
# Rates measured in WALL time (low RTF shows as low Hz); RTF from /clock vs wall.
import csv as csvmod
import time

import rclpy
from nav_msgs.msg import Odometry
from rclpy.node import Node
from rclpy.qos import qos_profile_sensor_data
from rosgraph_msgs.msg import Clock
from sensor_msgs.msg import Image, Imu, NavSatFix, PointCloud2

# (sensor, topic suffix, msg type, nominal Hz)  — pass = hz >= 0.9*nominal
SENSORS = [
    ("lidar", "scan/points", PointCloud2, 5.0),
    ("odom", "odom", Odometry, 50.0),
    ("imu", "imu", Imu, 100.0),                  # bridge not added yet -> 0 Hz
    ("gnss", "fix", NavSatFix, 5.0),             # bridge not added yet -> 0 Hz
    ("camera", "front_camera/image", Image, 15.0),  # stripped -> 0 Hz
]   


class Stat:
    def __init__(self):
        self.count = 0
        self.last = None
        self.gaps = []

    def tick(self, t):
        self.count += 1
        if self.last is not None:
            self.gaps.append(t - self.last)
        self.last = t
        
        
class SensorLoadProbe(Node):
    def __init__(self):
        super().__init__("sensor_load_probe")
        ids = self.declare_parameter("robot_ids", [3, 4]).value
        self.window = float(self.declare_parameter("window_sec", 30.0).value)
        self.csv_path = self.declare_parameter("csv_path", "/tmp/sensor_load.csv").value
        self.nom = {n: h for n, _, _, h in SENSORS}
        self.stats = {}
        for rid in ids:
            for name, suffix, mtype, _ in SENSORS:
                key = (rid, name)
                self.stats[key] = Stat()
                self.create_subscription(
                    mtype, f"/robot_{rid}/{suffix}",
                    lambda m, k=key: self.stats[k].tick(time.monotonic()),
                    qos_profile_sensor_data)
        self._c0 = self._w0 = self._clast = None
        self.create_subscription(Clock, "/clock", self._on_clock, qos_profile_sensor_data)
        self.t_start = time.monotonic() 
        self.get_logger().info(
            f"probe up: robots={ids} window={self.window}s -> {self.csv_path}")
        self.create_timer(self.window, self._report_once)
        
    def _on_clock(self, m):
        t = time.monotonic()
        sim = m.clock.sec + m.clock.nanosec * 1e-9
        if self._c0 is None:
            self._c0, self._w0 = sim, t
        self._clast = (sim, t)
        
    def _rtf(self):
        if self._c0 is None or self._clast is None:
            return 0.0
        wall_d = self._clast[1] - self._w0
        return (self._clast[0] - self._c0) / wall_d if wall_d > 1e-6 else 0.0
        
    def _report_once(self):
        dur = time.monotonic() - self.t_start
        rtf = self._rtf()
        rows = []
        for (rid, name), st in self.stats.items():
            n = self.nom[name]
            hz = st.count / dur if dur > 0 else 0.0
            jit = 0.0
            if len(st.gaps) > 1:
                mean = sum(st.gaps) / len(st.gaps)
                jit = (sum((g - mean) ** 2 for g in st.gaps) / len(st.gaps)) ** 0.5 * 1000.0
            drop_rate = max(0.0, 1.0 - hz / n) * 100.0 if n > 0 else 0.0
            period = 1.0 / n if n > 0 else 0.0 
            drop_gap = sum(1 for g in st.gaps if g > 1.5 * period)
            passed = "PASS" if (hz >= 0.9 * n and drop_rate < 1.0) else "FAIL"
            rows.append(dict(
                robot=rid, sensor=name, count=st.count, window_s=round(dur, 1),
                hz=round(hz, 2), hz_nominal=n, jitter_ms=round(jit, 1),
                drop_pct_rate=round(drop_rate, 1), drop_events_gap=drop_gap, result=passed))
        self.get_logger().info(
            f"=== SENSOR LOAD REPORT  RTF={rtf:.2f} ({'PASS' if rtf >= 0.8 else 'FAIL'}) ===")
        for r in rows:
            self.get_logger().info(
                f"r{r['robot']:>2} {r['sensor']:<7} {r['hz']:>6.2f}Hz/{r['hz_nominal']:>5.0f}"
                f"  jit={r['jitter_ms']:>5.1f}ms  drop={r['drop_pct_rate']:>4.1f}%  {r['result']}")
        with open(self.csv_path, "w", newline="") as f:
            w = csvmod.DictWriter(f, fieldnames=list(rows[0].keys()))
            w.writeheader()
            w.writerows(rows)
            f.write(f"# RTF,{rtf:.3f},{'PASS' if rtf >= 0.8 else 'FAIL'}\n")
        self.get_logger().info(f"wrote {self.csv_path}")
        rclpy.shutdown()
        
def main():
      rclpy.init()
      rclpy.spin(SensorLoadProbe())
      
      
if __name__ == "__main__":
    main()