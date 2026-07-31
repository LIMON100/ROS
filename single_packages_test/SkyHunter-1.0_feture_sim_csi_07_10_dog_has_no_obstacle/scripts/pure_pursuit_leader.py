#!/usr/bin/env python3
"""Pure-pursuit leader for the Go2 (s1) — now WITH frontal-lidar avoidance.

Drives the dog from /s1/odom + the LOADed path (no nav2). Reads /s1/rslidar_points
directly (in the sensor frame, no TF needed) to slow / steer / stop around obstacles.
"""
import json, math
import rclpy
from rclpy.node import Node
from nav_msgs.msg import Odometry
from geometry_msgs.msg import Twist
from sensor_msgs.msg import PointCloud2
from combat_robot_msgs.msg import SwarmPathCommand
from rclpy.qos import qos_profile_sensor_data
try:
    import numpy as np
    from sensor_msgs_py import point_cloud2 as pc2
    HAVE_PC = True
except Exception:
    HAVE_PC = False
    
DATUM_LAT = 36.61002559225 
DATUM_LON = 127.28772570583
M_PER_DEG_LAT = 111320.0
M_PER_DEG_LON = 111320.0 * math.cos(math.radians(DATUM_LAT))

MAX_V, MAX_W = 0.5, 0.5
LOOKAHEAD, GOAL_TOL, K_W = 2.5, 1.0, 1.5

# --- lidar avoidance (tune GROUND_Z from the z-range you measure) ---
DETECT_RANGE = 5.0      # m: look this far ahead
CORRIDOR_HALF = 0.6     # m: body half-width for the STOP/SLOW trigger
WIDE_HALF = 2.0         # m: side band used to pick the clearer side
GROUND_Z = 0.30        # m (sensor frame): drop points below this (ground). TUNE.
TOP_Z = 2.0             # m: drop points above this
STOP_DIST = 1.0         # m: full stop + rotate to clear side
SLOW_DIST = 3.0         # m: start slowing + steering away
AVOID_W = 0.6           # rad/s steering bias when avoiding
TURN_W = 0.4            # rad/s in-place turn when stopped
SELF_R = 1.2 
K_CONE = 0.27          # measured: the dog's self/ground return cone is z ≈ 0.27*rh
CONE_MARGIN = 0.4      # m: a real obstacle must rise this far ABOVE that cone
MIN_V     = 0.20

def yaw_of(q):
    return math.atan2(2.0 * (q.w * q.z + q.x * q.y),
                    1.0 - 2.0 * (q.y * q.y + q.z * q.z))
                    
                    
class PPLeader(Node): 
    def __init__(self, ns='s1'):
        super().__init__('pure_pursuit_leader')
        self.x = self.y = self.yaw = 0.0
        self.have_odom = False
        self.path, self.idx, self.active = [], 0, False
        self.front = float('inf'); self.left_min = float('inf'); self.right_min = float('inf')
        self.create_subscription(Odometry, f'/{ns}/odom', self._odom, 10)
        self.create_subscription(SwarmPathCommand, f'/{ns}/swarm/path_command', self._cmd, 10)
        if HAVE_PC:
            self.create_subscription(PointCloud2, f'/{ns}/rslidar_points', self._lidar,
                                    qos_profile_sensor_data)
        else:                        
            self.get_logger().warn('sensor_msgs_py/numpy missing — avoidance DISABLED')
        self.pub = self.create_publisher(Twist, f'/{ns}/cmd_vel_leader', 10)
        self.create_timer(0.05, self._tick)
        self.create_timer(1.0, self._diag)
        self.get_logger().info(f'pure-pursuit leader up -> /{ns}/cmd_vel_leader (avoid={HAVE_PC})')
    

    def _odom(self, m):
        self.x = m.pose.pose.position.x
        self.y = m.pose.pose.position.y
        self.yaw = yaw_of(m.pose.pose.orientation)
        self.have_odom = True
    
    def _lidar(self, msg):
        try:
            pts = pc2.read_points_numpy(msg, field_names=('x', 'y', 'z'), skip_nans=True)
        except Exception:
            return
        if pts.size == 0:
            return
        x, y, z = pts[:, 0], pts[:, 1], pts[:, 2]
        rh = np.hypot(x, y)
        dz = z - K_CONE * rh                       # height above the self/ground cone
        finite = np.isfinite(x) & np.isfinite(y) & np.isfinite(z)
        m = finite & (x > 0.0) & (rh < DETECT_RANGE) & (dz > CONE_MARGIN)
        xf, yf = x[m], y[m]
        tight = np.abs(yf) < CORRIDOR_HALF
        self.front = float(xf[tight].min()) if tight.any() else float('inf')
        left = (yf > 0.2) & (yf < WIDE_HALF)
        right = (yf < -0.2) & (yf > -WIDE_HALF)
        self.left_min = float(xf[left].min()) if left.any() else float('inf')
        self.right_min = float(xf[right].min()) if right.any() else float('inf')
    
    def _diag(self):
        if self.active and HAVE_PC:
            self.get_logger().info(
                f'[lidar] front={self.front:.1f} L={self.left_min:.1f} R={self.right_min:.1f}')
                
    def _cmd(self, m):
        if m.command == 5 and m.path_json:
            try:
                wps = json.loads(m.path_json).get('waypoints', [])
            except Exception:
                return
            self.path = [((w['lon'] - DATUM_LON) * M_PER_DEG_LON,
                        (w['lat'] - DATUM_LAT) * M_PER_DEG_LAT) for w in wps]
            self.idx = 0  
            if self.path: 
                self.get_logger().info(f'LOAD {len(self.path)} wps, goal x={self.path[-1][0]:.1f}')
        elif m.command == 1:
            self.active = True; self.get_logger().info('START')
        elif m.command in (2, 3):
            self.active = False; self.pub.publish(Twist()); self.get_logger().info('STOP')
        elif m.command == 4:
            self.active = True

    
    def _tick(self):
        if not (self.active and self.have_odom and self.path):
            return
        while self.idx < len(self.path) - 1:
            tx, ty = self.path[self.idx]
            if math.hypot(tx - self.x, ty - self.y) < LOOKAHEAD:
                self.idx += 1
            else:
                break
        tx, ty = self.path[self.idx]
        dx, dy = tx - self.x, ty - self.y
        dist = math.hypot(dx, dy)
        if self.idx == len(self.path) - 1 and dist < GOAL_TOL:
            self.active = False; self.pub.publish(Twist())
            self.get_logger().info('goal reached'); return
        ex = math.cos(self.yaw) * dx + math.sin(self.yaw) * dy
        ey = -math.sin(self.yaw) * dx + math.cos(self.yaw) * dy
        err = math.atan2(ey, ex) 
        t = Twist()
        t.angular.z = max(-MAX_W, min(MAX_W, K_W * err))
        v = MAX_V * max(0.0, math.cos(err))
        if self.idx == len(self.path) - 1:
            v = min(v, MAX_V * min(1.0, dist / 2.0))
        # ---- lidar avoidance overlay ----
        if self.front < STOP_DIST:
            # imminent contact: stop and pivot toward the clear side
            v = 0.0
            t.angular.z = TURN_W * (1.0 if self.left_min > self.right_min else -1.0)
        elif self.front < SLOW_DIST:
            # steer AROUND the box while STILL advancing (arc, don't stall and fight the path)
            v = max(MIN_V, min(v, MAX_V * 0.5))
            bias = AVOID_W * (1.0 if self.left_min > self.right_min else -1.0)
            t.angular.z = max(-MAX_W, min(MAX_W, t.angular.z + bias))

        t.linear.x = v
        self.pub.publish(t)
        
        
def main():
    rclpy.init()
    node = PPLeader()
    try: 
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node(); rclpy.shutdown()
        
        
if __name__ == '__main__':
    main() 