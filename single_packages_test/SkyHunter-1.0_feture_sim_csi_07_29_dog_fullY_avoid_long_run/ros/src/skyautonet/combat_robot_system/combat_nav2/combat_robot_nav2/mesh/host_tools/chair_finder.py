#!/usr/bin/env python3
# 전방 공통 장애물(의자) 중심을 sN/map 좌표로 추출.
# 사용법: chair_finder.py <ns>   예: chair_finder.py s1
# 라이다 클라우드를 base_footprint로 변환→전방 박스 필터→중심→map으로 변환.
import sys, math, numpy as np, rclpy
from rclpy.node import Node
from rclpy.qos import QoSProfile, ReliabilityPolicy, HistoryPolicy, DurabilityPolicy
from sensor_msgs.msg import PointCloud2
from sensor_msgs_py import point_cloud2 as pc2
import tf2_ros
from rclpy.time import Time
from rclpy.duration import Duration

def quat_to_R(x, y, z, w):
    n = math.sqrt(x*x+y*y+z*z+w*w) or 1.0
    x,y,z,w = x/n,y/n,z/n,w/n
    return np.array([
        [1-2*(y*y+z*z), 2*(x*y-z*w),   2*(x*z+y*w)],
        [2*(x*y+z*w),   1-2*(x*x+z*z), 2*(y*z-x*w)],
        [2*(x*z-y*w),   2*(y*z+x*w),   1-2*(x*x+y*y)]])

def T_from_tf(t):
    tr = t.transform.translation; q = t.transform.rotation
    R = quat_to_R(q.x,q.y,q.z,q.w)
    return R, np.array([tr.x,tr.y,tr.z])

class Finder(Node):
    def __init__(self, ns):
        super().__init__("chair_finder")
        self.ns = ns
        self.base = f"{ns}/base_footprint"
        self.mapf = f"{ns}/map"
        self.buf = tf2_ros.Buffer()
        self.tl = tf2_ros.TransformListener(self.buf, self)
        q = QoSProfile(depth=5)
        q.reliability = ReliabilityPolicy.BEST_EFFORT
        q.history = HistoryPolicy.KEEP_LAST
        q.durability = DurabilityPolicy.VOLATILE
        self.sub = self.create_subscription(PointCloud2, f"/{ns}/rslidar_points", self.cb, q)
        self.done = False
        # 전방 필터 박스(base_footprint): x 전방, |y| 좁게, z 의자 높이
        self.xmin, self.xmax = 0.3, 3.5
        self.ymin, self.ymax = -2.5, 2.5
        self.zmin, self.zmax = -0.3, 1.5

    def cb(self, msg):
        if self.done:
            return
        try:
            tb = self.buf.lookup_transform(self.base, msg.header.frame_id, Time(), Duration(seconds=0.3))
            tm = self.buf.lookup_transform(self.mapf, self.base, Time(), Duration(seconds=0.3))
        except Exception as e:
            self.get_logger().warn(f"tf 대기: {e}")
            return
        pts = np.array([[p[0],p[1],p[2]] for p in pc2.read_points(
            msg, field_names=("x","y","z"), skip_nans=True)], dtype=float)
        if pts.size == 0:
            return
        Rb, tbv = T_from_tf(tb)
        pb = pts @ Rb.T + tbv   # base_footprint 좌표
        m = ((pb[:,0]>=self.xmin)&(pb[:,0]<=self.xmax)&
             (pb[:,1]>=self.ymin)&(pb[:,1]<=self.ymax)&
             (pb[:,2]>=self.zmin)&(pb[:,2]<=self.zmax))
        fp = pb[m]
        if fp.shape[0] < 10:
            self.get_logger().warn(f"전방 점 {fp.shape[0]}개 — 의자 미검출, 재시도")
            return
        # 가장 가까운 x 군집만: 최소 x 근처 0.4m 슬랩
        xmin = fp[:,0].min()
        near = fp[fp[:,0] <= xmin+0.4]
        c_base = near.mean(axis=0)
        Rm, tmv = T_from_tf(tm)
        c_map = Rm @ c_base + tmv
        print(f"RESULT ns={self.ns} n={near.shape[0]} "
              f"base=({c_base[0]:.3f},{c_base[1]:.3f},{c_base[2]:.3f}) "
              f"map=({c_map[0]:.4f},{c_map[1]:.4f},{c_map[2]:.4f})", flush=True)
        self.done = True

def main():
    ns = sys.argv[1]
    rclpy.init()
    n = Finder(ns)
    import time
    t0 = time.time()
    while rclpy.ok() and not n.done and time.time()-t0 < 15:
        rclpy.spin_once(n, timeout_sec=0.2)
    if not n.done:
        print(f"RESULT ns={ns} FAIL 검출실패", flush=True)
    n.destroy_node(); rclpy.shutdown()

if __name__ == "__main__":
    main()
