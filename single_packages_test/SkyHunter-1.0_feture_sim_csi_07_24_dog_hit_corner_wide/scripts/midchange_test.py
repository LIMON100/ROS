#!/usr/bin/env python3
"""Mid-drive formation-change probe: ONE continuous drive, change formation at
several x-thresholds (rolling, no stop). See if the 2nd/3rd change sticks.
Stays before the wall (goal x=150) to isolate the transition logic."""
import json, math, threading, time, argparse
import rclpy 
from combat_robot_msgs.msg import SwarmControlCommand, SwarmPathCommand
from nav_msgs.msg import Odometry
from rclpy.node import Node
from rclpy.qos import (QoSDurabilityPolicy, QoSHistoryPolicy, QoSProfile,
                        QoSReliabilityPolicy)
RELIABLE_TL = QoSProfile(history=QoSHistoryPolicy.KEEP_LAST, depth=10,
                        reliability=QoSReliabilityPolicy.RELIABLE,
                        durability=QoSDurabilityPolicy.TRANSIENT_LOCAL)
DATUM_LAT = 36.61002559225
DATUM_LON = 127.28772570583
MLON = 1.0 / (111320.0 * math.cos(math.radians(DATUM_LAT)))
GOAL_X = 150.0
CHANGES = [(40.0,1,"COLUMN"), (70.0,3,"WEDGE"), (90.0,2,"DIAMOND"), (120.0,1,"COLUMN")]
class Asserter:
    def __init__(self, rids):
        self.node = rclpy.create_node("midchange_assert")
        q = QoSProfile(depth=10, reliability=QoSReliabilityPolicy.RELIABLE)
        self.pubs = [self.node.create_publisher(
            SwarmControlCommand, f"/s{r}/mission/control_command", q) for r in rids]
        self.mode = 0; self._stop = threading.Event()
        threading.Thread(target=self._run, daemon=True).start()
    def _run(self):
        while not self._stop.is_set(): 
            m = SwarmControlCommand(); m.formation_type = self.mode
            m.header.stamp = self.node.get_clock().now().to_msg()
            for p in self.pubs: p.publish(m)
            time.sleep(0.2)
    def stop(self): self._stop.set()

class Probe(Node):
    def __init__(self, n):
        super().__init__("midchange_test"); self.rids = list(range(1, n+1))
        self.path_pubs = [self.create_publisher(
            SwarmPathCommand, f"/s{r}/mission/path_command", RELIABLE_TL) for r in self.rids]
        self.asserter = Asserter(self.rids); self.leader_x = None
        self.phase = "WAIT_POSE"; self.t0 = time.time(); self.done = set()
        self.create_subscription(Odometry, "/s1/odometry/global", self._odom, 10)
        self.create_timer(0.2, self._tick)
        self.get_logger().info(f"midchange probe up — {n} robots")
    def _odom(self, m): self.leader_x = m.pose.pose.position.x
    def _ph(self, p): self.phase = p; self.t0 = time.time()
    def _cmd(self, c, num=0, pj=""):
        m = SwarmPathCommand(); m.header.stamp = self.get_clock().now().to_msg()
        m.command = c; m.num_waypoints = num; m.path_json = pj
        for pub in self.path_pubs:
            for _ in range(3): pub.publish(m)
    def _load(self, x0, x1):
        xs = [x0+3.0, (x0+x1)/2.0, x1]
        wps = [{"lat": DATUM_LAT, "lon": round(DATUM_LON + xx*MLON, 7)} for xx in xs]
        self._cmd(5, 3, json.dumps({"waypoints": wps}))
    def _tick(self): 
        e = time.time() - self.t0
        if self.phase == "WAIT_POSE":
            if self.leader_x is not None:
                self.get_logger().info(f"leader pose ok x={self.leader_x:.1f}"); self._ph("WAIT_SUBS")
        elif self.phase == "WAIT_SUBS":
            un = [i+1 for i,p in enumerate(self.path_pubs) if p.get_subscription_count() < 1]
            if not un:
                self.get_logger().info("subs matched"); self._cmd(2); self._ph("INIT")
            elif e > 120: self.get_logger().error("subs timeout"); self._ph("DONE")
        elif self.phase == "INIT":
            self.asserter.mode = 0
            self.get_logger().info(f"[LINE] LOAD 0->{GOAL_X+15:.0f}")
            self._load(max(self.leader_x,0.0), GOAL_X+15); self._ph("START_WAIT")
        elif self.phase == "START_WAIT" and e > 3.0:
              self._cmd(1); self.get_logger().info("START"); self._ph("DRIVING")
        elif self.phase == "DRIVING":
            if int(e) % 5 == 0 and e-int(e) < 0.2:
                self.get_logger().info(f"x={self.leader_x:.1f}/{GOAL_X:.0f} form={self.asserter.mode}")
            for i,(xt,md,nm) in enumerate(CHANGES):
                if i not in self.done and self.leader_x >= xt:
                    self.done.add(i); self.asserter.mode = md
                    self.get_logger().warn(f">>> CHANGE #{i+1} @x={self.leader_x:.1f} -> {nm}(form={md})")
            if self.leader_x >= GOAL_X:
                self.get_logger().info(f"REACHED x={self.leader_x:.1f} -> STOP"); self._cmd(2); self._ph("DONE")
def main():
    ap = argparse.ArgumentParser(); ap.add_argument("--robots", type=int, default=3)
    a = ap.parse_args(); rclpy.init(); node = Probe(a.robots)
    try: rclpy.spin(node)
    except KeyboardInterrupt: pass
    finally: node.asserter.stop(); node.destroy_node(); rclpy.shutdown()
if __name__ == "__main__": main()