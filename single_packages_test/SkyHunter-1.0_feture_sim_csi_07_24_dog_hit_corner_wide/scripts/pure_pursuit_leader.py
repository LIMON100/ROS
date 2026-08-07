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
import numpy as np
from sensor_msgs.msg import PointCloud2, Imu

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

MAX_V, MAX_W = 0.5, 0.8
LOOKAHEAD, GOAL_TOL, K_W = 2.5, 1.0, 1.5

LIDAR_RPY = (2.879, 0.0, 1.5705)

# --- lidar avoidance (tune GROUND_Z from the z-range you measure) ---
DETECT_RANGE = 25.0 #5.0      # m: look this far ahead
CORRIDOR_HALF = 1.0 #0.6     # m: body half-width for the STOP/SLOW trigger
WIDE_HALF = 12.0 #2.0         # m: side band used to pick the clearer side
GROUND_Z = -0.20 #0.30        # m (sensor frame): drop points below this (ground). TUNE.
GROUND_SLOPE = 0.12
TOP_Z = 2.0             # m: drop points above this
STOP_DIST = 1.2 #1.0         # m: full stop + rotate to clear side
SLOW_DIST = 4.0 #3.0         # m: start slowing + steering away
AVOID_W = 0.6           # rad/s steering bias when avoiding
TURN_W = 0.4            # rad/s in-place turn when stopped
SELF_R = 2.0 #1.2
K_CONE = 0.27          # measured: the dog's self/ground return cone is z ≈ 0.27*rh
CONE_MARGIN = 0.4      # m: a real obstacle must rise this far ABOVE that cone
MIN_V     = 0.20
SWERVE_HOLD_TICKS = 40
STRAFE_V  = 0.35      
PASS_DIST = 6.0

AVOID_ANGLE = 0.5      # rad (~29°): heading offset to hold while passing
MIN_LATCH   = SELF_R + 0.3


def yaw_of(q):
    return math.atan2(2.0 * (q.w * q.z + q.x * q.y),
                    1.0 - 2.0 * (q.y * q.y + q.z * q.z))

def _rot_rpy(r, p, y):
    cr, sr = math.cos(r), math.sin(r)
    cp, sp = math.cos(p), math.sin(p)
    cy, sy = math.cos(y), math.sin(y)
    Rx = np.array([[1,0,0],[0,cr,-sr],[0,sr,cr]])
    Ry = np.array([[cp,0,sp],[0,1,0],[-sp,0,cp]])
    Rz = np.array([[cy,-sy,0],[sy,cy,0],[0,0,1]])
    return Rz @ Ry @ Rx
    
R_LIDAR = _rot_rpy(*LIDAR_RPY) if HAVE_PC else None
                
                    
class PPLeader(Node): 
    def __init__(self, ns='s1'):
        super().__init__('pure_pursuit_leader')
        self.x = self.y = self.yaw = 0.0
        self.have_odom = False
        self.path, self.idx, self.active = [], 0, False
        self.front = float('inf'); self.left_min = float('inf'); self.right_min = float('inf')
        
        self.create_subscription(Odometry, f'/{ns}/odom', self._odom, 10)
        
        # --- SUBSCRIBE TO BOTH TOPICS SO IT WORKS WITH --via direct AND --via fsm ---
        self.create_subscription(SwarmPathCommand, f'/{ns}/swarm/path_command', self._cmd, 10)
        self.create_subscription(SwarmPathCommand, f'/{ns}/mission/path_command', self._cmd, 10)
        
        if HAVE_PC:                 
              self.create_subscription(PointCloud2, f'/{ns}/rslidar_points_raw', self._lidar,
                                      qos_profile_sensor_data)
        else:                        
            self.get_logger().warn('sensor_msgs_py/numpy missing — avoidance DISABLED')
        self.pub = self.create_publisher(Twist, f'/{ns}/cmd_vel_leader', 10)
        self.create_timer(0.05, self._tick)
        self.create_timer(1.0, self._diag)
        self.get_logger().info(f'pure-pursuit leader up -> /{ns}/cmd_vel_leader (avoid={HAVE_PC})')

        self.roll = self.pitch = 0.0
        self.avoid_side = 0
        self.avoid_hold = 0
        self.obs_y_min = 0.0
        self.obs_y_max = 0.0
        self.avoid_side = 0
        self.avoid_x0 = 0.0
        self.avoid_yaw0 = 0.0


        self.create_subscription(Imu, f'/{ns}/imu/data', self._imu, qos_profile_sensor_data)
    

    def _odom(self, m):
        self.x = m.pose.pose.position.x
        self.y = m.pose.pose.position.y
        self.yaw = yaw_of(m.pose.pose.orientation)
        self.have_odom = True

    def _imu(self, m):
        q = m.orientation
        self.roll = math.atan2(2.0 * (q.w*q.x + q.y*q.z),
                                1.0 - 2.0 * (q.x*q.x + q.y*q.y))
        sp = max(-1.0, min(1.0, 2.0 * (q.w*q.y - q.z*q.x)))
        self.pitch = math.asin(sp)
    
    def _lidar(self, msg):
        try:
            pts = pc2.read_points_numpy(msg, field_names=('x', 'y', 'z'), skip_nans=True)
        except Exception:
            return
        if pts.size == 0:
            return
        
        x, y, z = pts[:, 0], pts[:, 1], pts[:, 2]

        # 1. First, strictly filter out any NaNs or Inf values BEFORE doing math
        finite_mask = np.isfinite(x) & np.isfinite(y) & np.isfinite(z)
        if not finite_mask.any():
            return
            
        x = x[finite_mask]
        y = y[finite_mask]
        z = z[finite_mask]

        self.get_logger().info(f'RAW x[{x.min():.1f},{x.max():.1f}] y[{y.min():.1f},{y.max():.1f}] z[{z.min():.1f},{z.max():.1f}]')

        xb, yb, zb = R_LIDAR @ np.vstack((x, y, z))
        # level: undo trunk roll/pitch so +z is TRUE up regardless of gait
        x, y, z = _rot_rpy(self.roll, self.pitch, 0.0) @ np.vstack((xb, yb, zb))

        self.get_logger().info(f'BODY x[{x.min():.1f},{x.max():.1f}] y[{y.min():.1f},{y.max():.1f}] z[{z.min():.1f},{z.max():.1f}]')

        # 2. Now do the math safely
        rh = np.hypot(x, y)
        # dz = z - K_CONE * rh                       # height above the self/ground cone

        m = ((x > 0.0) & (rh > SELF_R) & (rh < DETECT_RANGE)
               & (z > GROUND_Z + GROUND_SLOPE * rh) & (z < TOP_Z))
        self.get_logger().info(f'HITS n={int(m.sum())} front={float(x[m].min()) if m.any() else -1:.1f}')
        
        # 3. Apply the filtering masks (including SELF_R)
        # m = (x > 0.0) & (rh > SELF_R) & (rh < DETECT_RANGE) & (dz > CONE_MARGIN)
        
        xf, yf = x[m], y[m]
        tight = np.abs(yf) < CORRIDOR_HALF
        
        # 4. Safely calculate distances
        self.front = float(xf[tight].min()) if tight.any() else float('inf')

        self.get_logger().info(
            f'HITS n={int(m.sum())} front={self.front:.1f} z[{z.min():.1f},{z.max():.1f}]')
        
        left = (yf > 0.2) & (yf < WIDE_HALF)
        right = (yf < -0.2) & (yf > -WIDE_HALF)
        self.left_min = float(xf[left].min()) if left.any() else float('inf')
        self.right_min = float(xf[right].min()) if right.any() else float('inf')

        if np.isfinite(self.front):
            near = (xf > self.front - 0.5) & (xf < self.front + 1.5)   # ★ slab, not "everything nearer"
            if near.any():
                self.obs_y_min = float(yf[near].min())
                self.obs_y_max = float(yf[near].max())
        else:   
            self.obs_y_min = self.obs_y_max = 0.0

    
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
            self.pub.publish(Twist())
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
        if (self.front < SLOW_DIST and self.front > MIN_LATCH
                and self.avoid_side == 0):
            left_room  = WIDE_HALF - self.obs_y_max
            right_room = self.obs_y_min + WIDE_HALF
            self.avoid_side = 1 if left_room >= right_room else -1
            self.avoid_x0, self.avoid_y0 = self.x, self.y
            self.avoid_yaw_t = self.yaw + AVOID_ANGLE * self.avoid_side
            self.get_logger().warn(
                f'[avoid] LATCH side={"L" if self.avoid_side>0 else "R"} '
                f'front={self.front:.1f} obs_y=[{self.obs_y_min:.1f},{self.obs_y_max:.1f}] '
                f'roomL={left_room:.1f} roomR={right_room:.1f}')
                
        if self.avoid_side != 0:
            travelled = math.hypot(self.x - self.avoid_x0, self.y - self.avoid_y0)
            if travelled > PASS_DIST and not np.isfinite(self.front):
                self.avoid_side = 0
                self.get_logger().info(f'[avoid] CLEAR after {travelled:.1f}m')
            else:
                # hold ONE offset heading — turn out, walk past, then release
                yerr = math.atan2(math.sin(self.avoid_yaw_t - self.yaw),
                                math.cos(self.avoid_yaw_t - self.yaw))
                t.angular.z = max(-MAX_W, min(MAX_W, 1.2 * yerr))
                v = max(MIN_V, min(v, MAX_V * 0.6))
                if self.front < STOP_DIST:
                    v = 0.0
                    
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