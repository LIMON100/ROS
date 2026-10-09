#!/usr/bin/env python3
"""Go2 lidar cloud fixer — put the dog's L1 cloud into the body frame the C++
executor already expects, so the UGV avoidance works for the dog too.

swarm_path_executor.cpp::on_lidar() (~:2445) reads the cloud RAW and assumes
    x = forward, y = left, z = up, ground at z~0 (obstacle_z_min_m = 0.0)
It applies no rotation. True for the UGV, NOT for the Go2: the L1 is mounted
rpy="2.879 0.0 1.5705", so every point is on the wrong axis -> s1 logs
`dist=-1.00m 실폭=0.0m` (blind) while pure_pursuit_leader.py, which rotates,
sees front=1.1.

Chain after this node:
gz bridge --/s1/rslidar_points_raw--> THIS --/s1/rslidar_points-->
    swarm_lidar_filter --/s1/rslidar_points_filtered--> swarm_path_executor

so the executor's cluster + persistence + real-width(실폭) -> wide-obstacle
planner detour + narrow swerve + hard stop all start working for the dog,
with ZERO C++ changes.

Run:  python3 scripts/go2_cloud_fixer.py      (no build needed)
"""
import math

import numpy as np
import rclpy
from rclpy.node import Node
from rclpy.qos import qos_profile_sensor_data
from sensor_msgs.msg import Imu, PointCloud2
from sensor_msgs_py import point_cloud2 as pc2
import bisect
from collections import deque

# Go2 L1 mount, from unitree_go2_description/urdf/lidar_4D_lidar.xacro
LIDAR_RPY = (2.879, 0.0, 1.5705)

# Lidar origin relative to base_footprint. Matches the static TF already in
# go2_leader.launch.py ('0.2 0 0.4'). Z is the one that matters: it puts the
# ground near z=0, which is what obstacle_z_min_m = 0.0 expects.
# TUNING: if s1 swerves at ground returns on clear road, lower Z_OFF by 0.05
#         (or raise formation.obstacle_z_min_m to ~0.10).
X_OFF, Y_OFF, Z_OFF = 0.2, 0.0, 0.4

IN_TOPIC  = 'rslidar_points_raw'   # from the gz bridge (sensor frame)
OUT_TOPIC = 'rslidar_points'       # what swarm_lidar_filter already subscribes to
OUT_FRAME = 'base_footprint'       # -> 's1/base_footprint'


def _rot_rpy(r, p, y):
    cr, sr = math.cos(r), math.sin(r)
    cp, sp = math.cos(p), math.sin(p)
    cy, sy = math.cos(y), math.sin(y)
    Rx = np.array([[1, 0, 0], [0, cr, -sr], [0, sr, cr]])
    Ry = np.array([[cp, 0, sp], [0, 1, 0], [-sp, 0, cp]])
    Rz = np.array([[cy, -sy, 0], [sy, cy, 0], [0, 0, 1]])
    return Rz @ Ry @ Rx 
    
    
R_LIDAR = _rot_rpy(*LIDAR_RPY)

class Go2CloudFixer(Node):
    def __init__(self, ns='s1'):
        super().__init__('go2_cloud_fixer')
        self.ns = ns
        # self.roll = self.pitch = 0.0
        self.imu_buf = deque(maxlen=400)
        self.imu_t = 0.0
        self.n_in = 0
        self._last = (0.0, 0.0, 0.0, 0.0)
        self.n_drop = 0
        
        self.pub = self.create_publisher(
            PointCloud2, f'/{ns}/{OUT_TOPIC}', qos_profile_sensor_data)
        self.create_subscription(
            PointCloud2, f'/{ns}/{IN_TOPIC}', self._cloud, qos_profile_sensor_data)
        self.create_subscription(
            Imu, f'/{ns}/imu/data', self._imu, qos_profile_sensor_data)
        self.create_timer(5.0, self._diag)
        
        self.get_logger().info(
            f'go2 cloud fixer up: /{ns}/{IN_TOPIC} -> /{ns}/{OUT_TOPIC} '
            f'(frame {ns}/{OUT_FRAME}, rpy={LIDAR_RPY}, z_off={Z_OFF})')
            
    def _imu(self, m):
        q = m.orientation
        roll = math.atan2(2.0 * (q.w*q.x + q.y*q.z),
                            1.0 - 2.0 * (q.x*q.x + q.y*q.y))
        sp = max(-1.0, min(1.0, 2.0 * (q.w*q.y - q.z*q.x)))
        t = m.header.stamp.sec + m.header.stamp.nanosec * 1e-9
        self.imu_buf.append((t, roll, math.asin(sp)))

    def _tilt_at(self, t):
        if not self.imu_buf:
            return 0.0, 0.0
        ts = [s[0] for s in self.imu_buf]
        i = bisect.bisect_left(ts, t)
        if i == 0:       return self.imu_buf[0][1],  self.imu_buf[0][2]
        if i >= len(ts): return self.imu_buf[-1][1], self.imu_buf[-1][2]
        t0, r0, p0 = self.imu_buf[i-1]
        t1, r1, p1 = self.imu_buf[i]
        a = 0.0 if t1 == t0 else (t - t0) / (t1 - t0)
        return r0 + a*(r1-r0), p0 + a*(p1-p0)

    def _cloud(self, msg):
        t_cloud = msg.header.stamp.sec + msg.header.stamp.nanosec * 1e-9
        try:
            pts = pc2.read_points_numpy(
                msg, field_names=('x', 'y', 'z'), skip_nans=True)
        except Exception:
            return
        if pts.size == 0:
            return
            
        x, y, z = pts[:, 0], pts[:, 1], pts[:, 2]
        keep = np.isfinite(x) & np.isfinite(y) & np.isfinite(z)
        if not keep.any():
            return
        xyz = np.vstack((x[keep], y[keep], z[keep]))
        
        # 1) sensor -> trunk axes (fixed URDF mount)
        # 2) undo trunk roll/pitch so the ground stops swinging with the gait
        t_cloud = msg.header.stamp.sec + msg.header.stamp.nanosec * 1e-9
        roll, pitch = self._tilt_at(t_cloud)
        R = _rot_rpy(roll, pitch, 0.0) @ R_LIDAR

        bx, by, bz = R @ xyz
        
        # 3) lidar origin -> base_footprint origin (ground ~ z=0)
        bx = bx + X_OFF
        by = by + Y_OFF
        bz = bz + Z_OFF

        rh = np.hypot(bx, by)
        near = (rh > 3.0) & (rh < 15.0)
        if near.sum() > 50:
            zc = bz[near]
            gnd = zc <= np.percentile(zc, 20)          # lowest 20% = ground
            gx, gy, gz = bx[near][gnd], by[near][gnd], zc[gnd]
            if gx.size > 30:
                A = np.column_stack((gx, gy, np.ones_like(gx)))
                try:
                    a, b, _ = np.linalg.lstsq(A, gz, rcond=None)[0]   # z = a*x + b*y + c
                    a = float(np.clip(a, -0.25, 0.25))                # ±14 deg guard
                    b = float(np.clip(b, -0.25, 0.25))
                    bx, by, bz = _rot_rpy(-b, a, 0.0) @ np.vstack((bx, by, bz))
                except Exception:
                    pass


        out = pc2.create_cloud_xyz32(
            msg.header, np.column_stack((bx, by, bz)).tolist())
        out.header.frame_id = f'{self.ns}/{OUT_FRAME}'
        self.pub.publish(out) 

        self.n_in += 1
        self._last = (float(bx.min()), float(bx.max()),
                    float(bz.min()), float(bz.max()))
    
    def _diag(self): 
        if self.n_in == 0:
            self.get_logger().warn(
                f'no cloud on /{self.ns}/{IN_TOPIC} — is go2_leader.launch.py '
                f'remapped to *_raw?')
            return
        x0, x1, z0, z1 = self._last
        self.get_logger().info(
            f'[fixer] {self.n_in} clouds/5s | BODY x[{x0:.1f},{x1:.1f}] '
            f'z[{z0:.2f},{z1:.2f}]  (ground should sit near z=0)')
        self.n_in = 0


def main():
    import sys
    ns = sys.argv[1] if len(sys.argv) > 1 else 's1'
    rclpy.init()
    node = Go2CloudFixer(ns)
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()
          
        
if __name__ == '__main__':
    main()