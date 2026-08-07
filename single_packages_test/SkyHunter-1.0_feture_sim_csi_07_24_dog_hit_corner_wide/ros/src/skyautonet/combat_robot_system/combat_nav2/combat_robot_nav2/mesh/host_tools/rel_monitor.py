#!/usr/bin/env python3
# 통합 TF에서 s1/s2 base_footprint 위치를 s1/map 기준으로 읽어 차간 벡터/거리 로깅.
# 주행(왕복) 중 상대위치(=calibration) 유지 여부 판정용.
# 실행: 호스트 domain0 + rviz_host.xml 프로파일 + tf relay/bridge 떠 있어야 함.
import math, rclpy
from rclpy.node import Node
import tf2_ros
from rclpy.time import Time
from rclpy.duration import Duration

class Mon(Node):
    def __init__(self):
        super().__init__("rel_monitor")
        self.buf=tf2_ros.Buffer(); self.tl=tf2_ros.TransformListener(self.buf,self)
        self.timer=self.create_timer(0.5,self.tick)
        self.d0=None; self.samples=[]
    def pos(self, child):
        t=self.buf.lookup_transform("s1/map", child, Time(), Duration(seconds=0.2))
        return t.transform.translation.x, t.transform.translation.y
    def tick(self):
        try:
            x1,y1=self.pos("s1/base_footprint"); x2,y2=self.pos("s2/base_footprint")
        except Exception as e:
            self.get_logger().warn(f"tf: {e}"); return
        dx,dy=x2-x1,y2-y1; d=math.hypot(dx,dy)
        if self.d0 is None:
            self.d0=d; self.dx0,self.dy0=dx,dy
            print(f"[BASELINE] 차간거리={d:.3f}m 벡터=({dx:+.3f},{dy:+.3f})", flush=True)
        dd=d-self.d0
        self.samples.append(d)
        print(f"s1=({x1:+.2f},{y1:+.2f}) s2=({x2:+.2f},{y2:+.2f}) "
              f"차간={d:.3f}m Δ={dd:+.3f}m 벡터=({dx:+.3f},{dy:+.3f})", flush=True)
    def summary(self):
        if len(self.samples)>1:
            mn=min(self.samples); mx=max(self.samples); rng=mx-mn
            import statistics as st
            print(f"\n[요약] n={len(self.samples)} 차간 평균={st.mean(self.samples):.3f} "
                  f"min={mn:.3f} max={mx:.3f} 변동폭={rng*100:.1f}cm "
                  f"std={st.pstdev(self.samples)*100:.1f}cm", flush=True)

def main():
    rclpy.init(); n=Mon()
    import time,sys
    dur=float(sys.argv[1]) if len(sys.argv)>1 else 60.0
    t0=time.time()
    try:
        while rclpy.ok() and time.time()-t0<dur:
            rclpy.spin_once(n,timeout_sec=0.1)
    except KeyboardInterrupt: pass
    finally:
        n.summary(); n.destroy_node(); rclpy.shutdown()

if __name__=="__main__": main()
