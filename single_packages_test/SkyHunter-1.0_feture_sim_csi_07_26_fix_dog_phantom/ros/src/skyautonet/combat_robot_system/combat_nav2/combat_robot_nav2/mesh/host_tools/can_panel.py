#!/usr/bin/env python3
"""can_panel — 호스트 GUI 제어패널 (CAN/하트비트 없음).

보드의 headless `can_reader.py --control` 노드에 **제어신호만** 발행한다:
  · ARM/DISARM  → /<ns>/can/enable (std_msgs/Bool)
  · STOP        → /<ns>/can/estop  (std_msgs/Bool True)
  · 수동 조그   → /<ns>/cmd_vel     (geometry_msgs/Twist)  ※nav2 안 띄웠을 때만 사용

하트비트는 보드 can_control 노드가 전용 스레드로 항상 발행하므로, 이 패널이나
wifi 가 끊겨도 모터컨트롤러는 fail-safe 정지하지 않는다(오정지 방지). 의도적
정지는 STOP 버튼(또는 물리 E-stop/RC).

사용법:  ros2 run 대신 직접 실행 →  python3 can_panel.py <ns>   (예: s2)
   또는  python3 can_panel.py s2 --max-lin 0.6 --max-ang 0.8
"""
import sys
import argparse
import tkinter as tk
from tkinter import ttk

import rclpy
from rclpy.node import Node
from std_msgs.msg import Bool
from geometry_msgs.msg import Twist


class PanelNode(Node):
    def __init__(self, ns):
        super().__init__('can_panel_%s' % (ns or 'root'))
        p = ('/%s' % ns) if ns else ''
        self.enable_pub = self.create_publisher(Bool, '%s/can/enable' % p, 10)
        self.estop_pub = self.create_publisher(Bool, '%s/can/estop' % p, 10)
        self.cmd_pub = self.create_publisher(Twist, '%s/cmd_vel' % p, 10)

    def set_enable(self, v):
        self.enable_pub.publish(Bool(data=bool(v)))

    def estop(self):
        self.estop_pub.publish(Bool(data=True))

    def jog(self, lin, ang):
        t = Twist()
        t.linear.x = float(lin)
        t.angular.z = float(ang)
        self.cmd_pub.publish(t)


class Panel:
    def __init__(self, root, node, max_lin, max_ang):
        self.root, self.node = root, node
        self.max_lin, self.max_ang = max_lin, max_ang
        self.armed = tk.BooleanVar(value=False)
        root.title('can_panel [%s]' % node.get_name())
        root.geometry('460x300')

        top = ttk.Frame(root, padding=10)
        top.pack(fill=tk.BOTH, expand=True)

        arm = ttk.Checkbutton(top, text='ARM (Enable Control)', variable=self.armed,
                              command=self._on_arm)
        arm.pack(anchor='w', pady=4)

        stop = tk.Button(top, text='■ STOP (E-STOP)', bg='#c0392b', fg='white',
                         font=('Arial', 16, 'bold'), height=2, command=self._on_stop)
        stop.pack(fill=tk.X, pady=8)

        jog = ttk.LabelFrame(top, text='수동 조그 (nav2 미기동 시) — 화살표키 / 슬라이더')
        jog.pack(fill=tk.BOTH, expand=True, pady=6)
        ttk.Label(jog, text='linear').grid(row=0, column=0, padx=4, pady=4)
        self.lin = tk.DoubleVar(value=0.0)
        ttk.Scale(jog, from_=-max_lin, to=max_lin, variable=self.lin, orient=tk.HORIZONTAL,
                  length=260, command=lambda _=None: self._send()).grid(row=0, column=1)
        ttk.Label(jog, text='angular').grid(row=1, column=0, padx=4, pady=4)
        self.ang = tk.DoubleVar(value=0.0)
        ttk.Scale(jog, from_=max_ang, to=-max_ang, variable=self.ang, orient=tk.HORIZONTAL,
                  length=260, command=lambda _=None: self._send()).grid(row=1, column=1)

        self.status = ttk.Label(top, text='DISARMED', font=('Arial', 11, 'bold'), foreground='gray')
        self.status.pack(side=tk.BOTTOM, pady=4)

        root.bind('<Up>', lambda e: self._nudge(0.1, 0))
        root.bind('<Down>', lambda e: self._nudge(-0.1, 0))
        root.bind('<Left>', lambda e: self._nudge(0, 0.1))
        root.bind('<Right>', lambda e: self._nudge(0, -0.1))
        root.bind('<space>', lambda e: self._zero())
        self._tick()

    def _on_arm(self):
        self.node.set_enable(self.armed.get())
        self.status.config(text='ARMED' if self.armed.get() else 'DISARMED',
                           foreground='#27ae60' if self.armed.get() else 'gray')

    def _on_stop(self):
        self.armed.set(False)
        self._zero()
        self.node.estop()
        self.status.config(text='ESTOP', foreground='#c0392b')

    def _nudge(self, dl, da):
        self.lin.set(max(-self.max_lin, min(self.max_lin, self.lin.get() + dl)))
        self.ang.set(max(-self.max_ang, min(self.max_ang, self.ang.get() + da)))
        self._send()

    def _zero(self):
        self.lin.set(0.0)
        self.ang.set(0.0)
        self._send()

    def _send(self):
        self.node.jog(self.lin.get(), self.ang.get())

    def _tick(self):
        rclpy.spin_once(self.node, timeout_sec=0)
        # ★ ARM(enable) 상태일 때만 /cmd_vel 을 매 tick(~33Hz) 연속 발행 → "set-and-go"
        #   (슬라이더/키로 속도 설정 → 손 떼도 계속 주행). space/STOP/DISARM 으로 정지.
        #   DISARM 시엔 발행 안 함 → 실수로 ARM 하는 순간 급발진 방지 + /cmd_vel 스팸 없음.
        #   패널이 죽으면 발행이 끊겨 보드 can_control 이 0.5s 후 자동정지(안전).
        if self.armed.get():
            self._send()
        self.root.after(30, self._tick)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('ns', nargs='?', default='', help='namespace (예: s2). 생략시 루트')
    ap.add_argument('--max-lin', type=float, default=0.6)
    ap.add_argument('--max-ang', type=float, default=0.8)
    args = ap.parse_args()
    rclpy.init()
    node = PanelNode(args.ns)
    root = tk.Tk()
    Panel(root, node, args.max_lin, args.max_ang)
    try:
        root.mainloop()
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
