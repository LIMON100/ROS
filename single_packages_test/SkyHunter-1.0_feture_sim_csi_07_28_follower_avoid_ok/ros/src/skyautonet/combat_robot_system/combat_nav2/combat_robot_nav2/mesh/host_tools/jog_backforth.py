#!/usr/bin/env python3
# odom 폐루프 왕복 jog: 뒤로 <dist>m → 앞으로 <dist>m, <cycles>회 반복.
# 사용법: jog_backforth.py <ns> [dist=1.0] [cycles=3] [speed=0.15]
# /sN/odom 로 이동거리 피드백, /sN/cmd_vel 로 속도 지령. can_reader Enable Control 필요.
import sys, math, rclpy
from rclpy.node import Node
from geometry_msgs.msg import Twist
from nav_msgs.msg import Odometry
from rclpy.qos import QoSProfile, ReliabilityPolicy, HistoryPolicy

class Jog(Node):
    def __init__(self, ns, dist, cycles, speed):
        super().__init__("jog_backforth")
        self.dist=dist; self.cycles=cycles; self.speed=speed
        self.x0=None; self.x=None; self.y0=None; self.y=None
        q=QoSProfile(depth=10); q.reliability=ReliabilityPolicy.BEST_EFFORT; q.history=HistoryPolicy.KEEP_LAST
        self.sub=self.create_subscription(Odometry, f"/{ns}/odom", self.odom, q)
        self.pub=self.create_publisher(Twist, f"/{ns}/cmd_vel", 10)
        # 상태머신: 각 사이클 = back(뒤로 dist) 후 fwd(앞으로 dist)
        self.phase="wait"; self.cyc=0; self.leg_start=None
        self.timer=self.create_timer(0.05, self.tick)
        self.ns=ns

    def odom(self,m):
        p=m.pose.pose.position
        if self.x0 is None: self.x0=p.x; self.y0=p.y
        self.x=p.x; self.y=p.y

    def travelled(self):
        return math.hypot(self.x-self.leg_start[0], self.y-self.leg_start[1])

    def send(self,v):
        t=Twist(); t.linear.x=v; self.pub.publish(t)

    def tick(self):
        if self.x is None: return
        if self.phase=="wait":
            self.leg_start=(self.x,self.y); self.phase="back"; self.cyc=1
            self.get_logger().info(f"[{self.ns}] cycle {self.cyc}/{self.cycles} BACK 시작")
            return
        if self.phase=="back":
            if self.travelled() < self.dist: self.send(-self.speed)
            else:
                self.send(0.0); self.leg_start=(self.x,self.y); self.phase="fwd"
                self.get_logger().info(f"[{self.ns}] BACK 완료 {self.travelled():.2f}m → FWD")
        elif self.phase=="fwd":
            if self.travelled() < self.dist: self.send(self.speed)
            else:
                self.send(0.0)
                self.get_logger().info(f"[{self.ns}] FWD 완료 {self.travelled():.2f}m (cycle {self.cyc})")
                if self.cyc>=self.cycles:
                    self.phase="done"; self.get_logger().info(f"[{self.ns}] 전체 완료"); return
                self.cyc+=1; self.leg_start=(self.x,self.y); self.phase="back"
                self.get_logger().info(f"[{self.ns}] cycle {self.cyc}/{self.cycles} BACK 시작")
        elif self.phase=="done":
            self.send(0.0)

def main():
    ns=sys.argv[1]; dist=float(sys.argv[2]) if len(sys.argv)>2 else 1.0
    cycles=int(sys.argv[3]) if len(sys.argv)>3 else 3
    speed=float(sys.argv[4]) if len(sys.argv)>4 else 0.15
    rclpy.init(); n=Jog(ns,dist,cycles,speed)
    import time, signal
    stop={"f":False}
    def handler(sig,frm): stop["f"]=True
    signal.signal(signal.SIGTERM, handler); signal.signal(signal.SIGINT, handler)
    t0=time.time()
    # 실효속도 여유: leg당 최대 dist/speed*3 + 오버헤드
    hardcap = 15 + cycles*2*(dist/speed)*3
    try:
        while rclpy.ok() and n.phase!="done" and not stop["f"] and time.time()-t0 < hardcap:
            rclpy.spin_once(n, timeout_sec=0.05)
    finally:
        for _ in range(10): n.send(0.0); rclpy.spin_once(n, timeout_sec=0.02); time.sleep(0.02)
        n.get_logger().info(f"[{ns}] 정지 지령 발행 후 종료")
        n.destroy_node(); rclpy.shutdown()

if __name__=="__main__": main()
