 #!/usr/bin/env python3
"""POC stop-and-go scenario driver for CSI.

Runs the staged mission using ONLY the proven start-in-formation mechanism —
NEVER the broken mid-drive reshape. Each stage:
    STOP -> set new formation -> LOAD a fresh path from the leader's CURRENT
    position forward -> START.
So every reform happens from rest (works), and every path starts ahead of the
robots (no backward pull). Drives EAST (+lon) along the runway.

Run (after the sim is up):
source ros/install/setup.bash; export ROS_DOMAIN_ID=96
python3 scripts/poc_scenario_driver.py --robots 5
"""
import argparse, json, math, time
import rclpy
from rclpy.node import Node
from nav_msgs.msg import Odometry
from combat_robot_msgs.msg import SwarmPathCommand, SwarmControlCommand

DATUM_LAT = 36.61002559225
DATUM_LON = 127.28772570583
MLON = 1.0 / (111320.0 * math.cos(math.radians(DATUM_LAT)))   # deg lon per metre east

# CSI formation_type: 0 line, 1 column, 2 diamond, 3 wedge
# stages: (name, formation, end_x)  — drive in <formation> until leader x >= end_x
# STAGES = [
#     ("LINE",    0, 40.0),
#     ("COLUMN",  1, 110.0),
#     ("WATCH",   2, 150.0),   # diamond (sub for double-column)
#     ("ASSAULT", 3, 190.0),   # wedge placeholder until the 160-arc (later)
# ]   

# STAGES = [
#     ("LINE",       0, 20.0,  3.0),
#     ("COLUMN",     1, 40.0,  3.0),
#     ("ENEMY_STOP", 1, 60.0, 5.0),   # ★ coordinated 5 s freeze at enemy contact
#     ("ASSAULT",    4, 80.0, 3.0),   # column for now; real 160° arc later
# ]

STAGES = [
    ("COLUMN",  1, 20.0,  3.0),
    ("ASSAULT", 4, 40.0, 3.0),
]

MARGIN = 15.0   # load the path a bit past the stage end so the goal isn't at the box

class StopGoDriver(Node):
    def __init__(self, n):
        super().__init__("poc_scenario_driver")
        self.n = n
        self.leader_x = 0.0
        self.seg = 0
        self.phase = "INIT"
        self.t0 = time.time()
        self.path_pubs = [self.create_publisher(SwarmPathCommand, f"/s{i}/swarm/path_command", 10)
                        for i in range(1, n + 1)]
        self.ctrl_pubs = [self.create_publisher(SwarmControlCommand, f"/s{i}/mission/control_command", 10)
                          for i in range(1, n + 1)]
        self.create_subscription(Odometry, "/s1/odom", self._odom, 10)
        self.create_timer(0.2, self._tick)   # 5 Hz state machine
        self.get_logger().info(f"stop-and-go driver up — {n} robots")
        
    def _odom(self, m):
        self.leader_x = m.pose.pose.position.x
        
    def _elapsed(self):
        return time.time() - self.t0
        
    def _set_phase(self, p):
        self.phase = p
        self.t0 = time.time()
        
    def _formation(self, ft):
        for pub in self.ctrl_pubs:
            m = SwarmControlCommand(); m.formation_type = ft; m.formation_number = 1
            for _ in range(3): pub.publish(m)

    def _path_cmd(self, cmd, num=0, path_json=""):
          for pub in self.path_pubs:
              m = SwarmPathCommand(); m.command = cmd; m.num_waypoints = num; m.path_json = path_json
              for _ in range(3): pub.publish(m)
              
    def _load_from(self, x_start, x_end):
        xs = [x_start + 3.0, (x_start + x_end) / 2.0, x_end]
        wps = [{"lat": DATUM_LAT, "lon": round(DATUM_LON + xx * MLON, 7)} for xx in xs]
        self._path_cmd(5, 3, json.dumps({"waypoints": wps}))   # 5 = LOAD_PATH
        
    def _tick(self):
        # name, form, end_x = STAGES[self.seg]
        name, form, end_x, hold_s = STAGES[self.seg]
        e = self._elapsed()
        if self.phase == "INIT":
            self.get_logger().info(f"[{name}] form + LOAD 0->{end_x+MARGIN:.0f}")
            self._formation(form); self._load_from(0.0, end_x + MARGIN)
            self._set_phase("START_WAIT")
        elif self.phase == "START_WAIT" and e > 3.0:
            self._path_cmd(1)                                   # 1 = START
            self.get_logger().info(f"[{name}] START — driving to x={end_x:.0f}")
            self._set_phase("DRIVING")
        elif self.phase == "DRIVING":
            if self.leader_x >= end_x:
                self.get_logger().info(f"[{name}] reached x={self.leader_x:.1f} -> STOP")
                self._path_cmd(2)                               # 2 = STOP
                self._set_phase("STOP_WAIT")   
        # elif self.phase == "STOP_WAIT" and e > 3.0:                 
        elif self.phase == "STOP_WAIT" and e > hold_s:            
            if self.seg + 1 >= len(STAGES):
                self.get_logger().info("MISSION COMPLETE — all stages done"); self._set_phase("DONE"); return
            self.seg += 1
            # nm, nf, nx = STAGES[self.seg]
            nm, nf, nx, nh = STAGES[self.seg]
            self.get_logger().info(f"[{nm}] reform (formation={nf}) + LOAD {self.leader_x:.0f}->{nx+MARGIN:.0f}")
            self._formation(nf)
            self._load_from(self.leader_x, nx + MARGIN)
            self._set_phase("START_WAIT")
            
            
def main(): 
    ap = argparse.ArgumentParser(); ap.add_argument("--robots", type=int, default=5)
    a = ap.parse_args()
    rclpy.init(); node = StopGoDriver(a.robots)
    try: rclpy.spin(node)
    except KeyboardInterrupt: pass
    finally: node.destroy_node(); rclpy.shutdown()
    
    
if __name__ == "__main__":
    main()
