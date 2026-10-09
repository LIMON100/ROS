#!/usr/bin/env python3
# 이동 중 GPS 진행방향(course) vs dual-antenna heading 비교 → 안테나 오프셋/반대 판정
import sys, math
import rclpy
from rclpy.node import Node
from geometry_msgs.msg import TwistStamped
from std_msgs.msg import Float64

NS = sys.argv[1] if len(sys.argv) > 1 else 's1'

def norm360(a): return a % 360.0
def norm180(a):
    a = (a + 180.0) % 360.0 - 180.0
    return a

class HChk(Node):
    def __init__(self):
        super().__init__('heading_check')
        self.edge = None
        self.create_subscription(Float64, f'/{NS}/edge_heading', self.on_edge, 10)
        self.create_subscription(TwistStamped, f'/{NS}/vel', self.on_vel, 10)
        self.create_timer(0.5, self.tick)
        self.last = None
    def on_edge(self, m): self.edge = m.data
    def on_vel(self, m):
        vx, vy = m.twist.linear.x, m.twist.linear.y   # x=East, y=North (ENU)
        spd = math.hypot(vx, vy)
        course = norm360(math.degrees(math.atan2(vx, vy)))  # compass: 0=N,90=E
        self.last = (spd, course)
    def tick(self):
        if self.last is None or self.edge is None:
            print(f'[{NS}] waiting data...', flush=True); return
        spd, course = self.last
        if spd < 0.4:
            print(f'[{NS}] STOPPED spd={spd:.2f} (move faster) | edge_heading={self.edge:.1f}', flush=True)
            return
        off = norm180(self.edge - course)   # 안테나-진행방향 = 오프셋(정상이면 일정), ~180이면 반대
        verdict = 'ANTENNA REVERSED (~180)!' if abs(abs(off)-180) < 40 else ('OK-ish offset' if abs(off) < 40 else f'offset {off:.0f}')
        print(f'[{NS}] spd={spd:.2f} course={course:.1f} edge_heading={self.edge:.1f} diff(edge-course)={off:+.1f}  => {verdict}', flush=True)

def main():
    rclpy.init(); n = HChk()
    try: rclpy.spin(n)
    except KeyboardInterrupt: pass

main()
