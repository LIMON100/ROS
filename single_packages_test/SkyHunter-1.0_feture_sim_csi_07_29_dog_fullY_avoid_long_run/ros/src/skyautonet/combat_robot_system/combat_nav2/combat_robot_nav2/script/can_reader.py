#!/usr/bin/env python3
import sys
import struct
import time
import threading
import queue
import math
# tkinter 는 GUI 모드에서만 필요 → 헤드리스(보드) 에서 python3-tk 없어도
# import 실패로 죽지 않도록 지연 로드한다. (--control / --listen-only 는 tk 불필요)
try:
    import tkinter as tk
    from tkinter import ttk
except ImportError:
    tk = None
    ttk = None

# ==========================================
# ROS 2 라이브러리
# ==========================================
import rclpy
from rclpy.node import Node
from nav_msgs.msg import Odometry
from geometry_msgs.msg import Twist, TransformStamped
from std_msgs.msg import Bool, UInt8
from tf2_ros import TransformBroadcaster

# python-can 임포트
try:
    import can
except ImportError as e:
    can = None
    print("python-can not available:", e)

# ==========================================
# 🌟 TinS-17 정밀 설정 (차량 스펙 및 CAN Protocol)
# ==========================================
WHEEL_RADIUS_M = 0.07
TRACK_WIDTH_M = 0.90
TRACK_SLIP_FACTOR = 1.2

CALIBRATION_SCALING = 35.714  

ENCODER_PPR = 1024         
GEAR_RATIO = 30.0          

TICKS_PER_WHEEL_REV = ENCODER_PPR * GEAR_RATIO
RAD_PER_TICK = ((2.0 * math.pi) / TICKS_PER_WHEEL_REV) * CALIBRATION_SCALING

CMD_CAN_ID = 0x201
FEEDBACK_CAN_ID = 0x181

CAN_SEND_FREQ_HZ = 30       
INTER_MSG_GAP_SEC = 0.0005  

DEAD_ZONE_STEER = 50        
DEAD_ZONE_SPEED = 50        

MAX_STEER = 2000  
MAX_SPEED = 5600  

# ==========================================
# ROS 2 노드 클래스 (/odom 발행, /cmd_vel 수신)
# ==========================================
class VehicleROSNode(Node):
    def __init__(self, ui_app, node_name='combat_ctrl_ros_node', remote_ns=None):
        # remote_ns 가 있으면 '원격 GUI' 노드: CAN 대신 /<ns>/can/enable·estop·cmd_vel 발행.
        if remote_ns is not None:
            node_name = 'can_remote_%s' % (remote_ns or 'root')
        super().__init__(node_name)
        self.ui_app = ui_app
        self.remote_ns = remote_ns

        # 프레임명(네임스페이스 사용 시 sN/ 로 오버라이드)
        self.declare_parameter('odom_frame', 'odom')
        self.declare_parameter('base_frame', 'base_footprint')
        self.odom_frame = self.get_parameter('odom_frame').value
        self.base_frame = self.get_parameter('base_frame').value

        if remote_ns is not None:
            p = ('/%s' % remote_ns) if remote_ns else ''
            self.enable_pub = self.create_publisher(Bool, '%s/can/enable' % p, 10)
            self.estop_pub = self.create_publisher(Bool, '%s/can/estop' % p, 10)
            self.cmd_pub = self.create_publisher(Twist, '%s/cmd_vel' % p, 10)
            # 원격: 보드 /<ns>/odom(실제속도) + /<ns>/cmd_vel_nav(nav2 명령속도) 구독해 GUI 표시.
            self.remote_speed = 0.0   # 실제(odom)
            self.remote_wz = 0.0
            self.remote_cmd_v = 0.0   # nav2 명령(cmd_vel_nav)
            self.remote_cmd_wz = 0.0
            self.last_cmd_nav_t = 0.0  # nav2 cmd_vel_nav 마지막 수신시각(주행중 판정)
            self.create_subscription(Odometry, '%s/odom' % p, self._on_remote_odom, 10)
            self.create_subscription(Twist, '%s/cmd_vel_nav' % p, self._on_remote_cmd, 10)
        else:
            self.odom_pub = self.create_publisher(Odometry, '/odom', 10)
            self.tf_broadcaster = TransformBroadcaster(self)
            # 수신전용(listen-only) 모드에선 ui_app=None → cmd_vel 구독/제어 없음(RC 제어 유지)
            if ui_app is not None:
                self.cmd_sub = self.create_subscription(Twist, '/cmd_vel', self.cmd_vel_callback, 10)

        # 🔥 yaw 적분 제거 — EKF가 yaw 추정 담당
        # 그래도 UI 표시용으로 누적치 보관 (orientation 발행에는 사용 안 함)
        self.x = 0.0
        self.y = 0.0
        self.th = 0.0   # UI 표시용만
        self.last_time = self.get_clock().now()

    def publish_remote(self, enabled, speed_units, steer_units, nav2_active=False):
        """원격 GUI → 보드. ARM(/can/enable) 발행 + 수동조그 cmd_vel 발행.
        ★ 단, nav2 가 주행 중(cmd_vel_nav 최근수신)이면 cmd_vel 을 쏘지 않는다:
        nav2 가 /<ns>/cmd_vel 로 직접 주행하므로 겹치면 경합. nav2 유휴일 때만 슬라이더로
        수동주행(nav2 주행중엔 슬라이더가 nav2 명령 시각화)."""
        b = Bool(); b.data = bool(enabled); self.enable_pub.publish(b)
        if enabled and not nav2_active and (abs(speed_units) > DEAD_ZONE_SPEED or abs(steer_units) > DEAD_ZONE_STEER):
            t = Twist()
            t.linear.x = (float(speed_units) / MAX_SPEED) * 0.8
            t.angular.z = (float(steer_units) / MAX_STEER) * 1.0
            self.cmd_pub.publish(t)

    def send_estop(self):
        b = Bool(); b.data = True; self.estop_pub.publish(b)

    def _on_remote_odom(self, msg):
        # 보드 /<ns>/odom → 실제 속도(선/각). GUI 표시용.
        self.remote_speed = msg.twist.twist.linear.x
        self.remote_wz = msg.twist.twist.angular.z

    def _on_remote_cmd(self, msg):
        # /<ns>/cmd_vel_nav → nav2 명령 속도(DISARM 이어도 나옴).
        self.remote_cmd_v = msg.linear.x
        self.remote_cmd_wz = msg.angular.z
        self.last_cmd_nav_t = time.time()   # nav2 주행중 판정용
        # ★ nav2 명령을 슬라이더에 반영(로컬 can_reader 의 cmd_vel_callback 과 동일).
        #   → GUI 슬라이더가 nav2 가 지금 명령하는 speed/steer 를 실시간 표시.
        if self.ui_app is not None:
            self.cmd_vel_callback(msg)

    def cmd_vel_callback(self, msg):
        v_x = msg.linear.x
        v_yaw = msg.angular.z
        
        left_v = v_x - (v_yaw * TRACK_WIDTH_M / 2.0)
        right_v = v_x + (v_yaw * TRACK_WIDTH_M / 2.0)
        
        conv = RAD_PER_TICK * WHEEL_RADIUS_M
        left_line = left_v / conv
        right_line = right_v / conv
        
        speed = (left_line + right_line) / 2.0
        steer = (right_line - left_line) / 2.0
        
        self.ui_app.current_speed.set(max(-MAX_SPEED, min(MAX_SPEED, int(speed))))
        self.ui_app.current_steer.set(max(-MAX_STEER, min(MAX_STEER, int(steer))))

    def publish_odom(self, left_line_s, right_line_s):
        current_time = self.get_clock().now()
        dt = (current_time - self.last_time).nanoseconds / 1e9
        self.last_time = current_time

        # 정밀 파라미터 기반 선속도 계산
        left_v = left_line_s * RAD_PER_TICK * WHEEL_RADIUS_M
        right_v = right_line_s * RAD_PER_TICK * WHEEL_RADIUS_M

        v_x = (left_v + right_v) / 2.0
        ideal_v_yaw = (right_v - left_v) / TRACK_WIDTH_M
        v_yaw = ideal_v_yaw * TRACK_SLIP_FACTOR

        # 🔥 UI 표시용 누적치 (EKF에 전달 안 됨)
        self.th += v_yaw * dt
        self.x += (v_x * math.cos(self.th)) * dt
        self.y += (v_x * math.sin(self.th)) * dt

        # 🔥 Odometry 메시지 — orientation은 항상 identity (yaw 적분 제거)
        # EKF는 v_x만 사용 (config에서 그렇게 설정)
        odom_msg = Odometry()
        odom_msg.header.stamp = current_time.to_msg()
        odom_msg.header.frame_id = self.odom_frame
        odom_msg.child_frame_id = self.base_frame

        # 🔥 position과 orientation을 모두 0 (EKF가 v_x만 보게)
        odom_msg.pose.pose.position.x = 0.0
        odom_msg.pose.pose.position.y = 0.0
        odom_msg.pose.pose.position.z = 0.0
        odom_msg.pose.pose.orientation.x = 0.0
        odom_msg.pose.pose.orientation.y = 0.0
        odom_msg.pose.pose.orientation.z = 0.0
        odom_msg.pose.pose.orientation.w = 1.0

        odom_msg.twist.twist.linear.x = float(v_x)
        odom_msg.twist.twist.angular.z = float(v_yaw)

        # 🔥 공분산: pose는 무한대(EKF가 쓰지 않게), twist는 신뢰도 표현
        # EKF config에 따르면 v_x만 사용. v_x 공분산 = 0.05 (σ ≈ 0.22 m/s)
        pose_cov = [
            999.0, 0.0,   0.0,   0.0,   0.0,   0.0,
            0.0,   999.0, 0.0,   0.0,   0.0,   0.0,
            0.0,   0.0,   999.0, 0.0,   0.0,   0.0,
            0.0,   0.0,   0.0,   999.0, 0.0,   0.0,
            0.0,   0.0,   0.0,   0.0,   999.0, 0.0,
            0.0,   0.0,   0.0,   0.0,   0.0,   999.0,
        ]
        twist_cov = [
            0.05,  0.0,   0.0,   0.0,   0.0,   0.0,    # v_x σ ≈ 0.22 m/s
            0.0,   999.0, 0.0,   0.0,   0.0,   0.0,
            0.0,   0.0,   999.0, 0.0,   0.0,   0.0,
            0.0,   0.0,   0.0,   999.0, 0.0,   0.0,
            0.0,   0.0,   0.0,   0.0,   999.0, 0.0,
            0.0,   0.0,   0.0,   0.0,   0.0,   0.5,    # v_yaw σ ≈ 0.7 rad/s
        ]
        odom_msg.pose.covariance = pose_cov
        odom_msg.twist.covariance = twist_cov

        self.odom_pub.publish(odom_msg)

# ==========================================
# CAN & Thread Workers
# ==========================================
class CanBusWrapper:
    def __init__(self):
        self.bus = None
        if can is not None:
            try:
                # 보드 내장 SocketCAN(can0). 비트레이트는 OS에서 `ip link`로 설정(250kbps).
                self.bus = can.interface.Bus(interface='socketcan', channel='can0')
                print("✅ SocketCAN Bus Initialized (can0, 250kbps)")
            except Exception as e:
                print("❌ CAN init error:", e)

    def send(self, msg):
        if self.bus:
            try: self.bus.send(msg)
            except: pass

    def recv(self, timeout=1.0):
        if self.bus is None:
            time.sleep(timeout)
            return None
        try: return self.bus.recv(timeout=timeout)
        except: return None

class TxWorker(threading.Thread):
    def __init__(self, can_wrapper, tx_queue, stop_event):
        super().__init__(daemon=True)
        self.can, self.tx_queue, self.stop_event = can_wrapper, tx_queue, stop_event

    def run(self):
        while not self.stop_event.is_set():
            try: arb_id, payload = self.tx_queue.get(timeout=0.1)
            except queue.Empty: continue
            self.can.send(can.Message(arbitration_id=arb_id, data=payload, is_extended_id=False))
            time.sleep(INTER_MSG_GAP_SEC)

class RxWorker(threading.Thread):
    def __init__(self, can_wrapper, ui_queue, stop_event):
        super().__init__(daemon=True)
        self.can, self.ui_queue, self.stop_event = can_wrapper, ui_queue, stop_event

    def run(self):
        while not self.stop_event.is_set():
            msg = self.can.recv(timeout=0.5)
            if msg: self.ui_queue.put(("rx", msg))

# ==========================================
# 메인 GUI 앱
# ==========================================
class VehicleControl:
    def __init__(self, root, remote_ns=None):
        # remote_ns 가 None 이 아니면 '원격 GUI' 모드: can0 을 열지 않고 GUI 조작을
        #   /<ns>/can/enable(Bool) · /<ns>/can/estop(Bool) · /<ns>/cmd_vel(Twist) 로 발행.
        #   → 보드(can_reader --control)가 받아 실제 CAN 을 구동. 호스트에서 실행용.
        self.remote_ns = remote_ns
        self.root = root
        self.root.title(("TinS-17 REMOTE → /%s" % remote_ns) if remote_ns else ("TinS-17 ["+__import__('os').environ.get('CAN_LABEL','')+"] Control"))
        self.root.geometry("600x380")

        self.current_steer = tk.IntVar(value=0)
        self.current_speed = tk.IntVar(value=0)
        self.control_enabled = tk.BooleanVar(value=False)
        self.headlight_mode = tk.IntVar(value=2)  # 1, 2, 3 중 선택 (기본 2)

        self.filterd_steer, self.filterd_speed = 0.0, 0.0

        if remote_ns is None:
            self.can = CanBusWrapper()
            self.tx_queue, self.ui_queue = queue.Queue(), queue.Queue()
            self.stop_event = threading.Event()
            self.tx_worker = TxWorker(self.can, self.tx_queue, self.stop_event)
            self.rx_worker = RxWorker(self.can, self.ui_queue, self.stop_event)
        else:
            self.can = None
            self.tx_queue, self.ui_queue = queue.Queue(), queue.Queue()
            self.stop_event = threading.Event()
            self.tx_worker = self.rx_worker = None

        self.init_ui()

        rclpy.init(args=None)
        self.ros_node = VehicleROSNode(self, remote_ns=remote_ns)
        self.ros_thread = threading.Thread(target=rclpy.spin, args=(self.ros_node,), daemon=True)
        self.ros_thread.start()

        self.root.bind('<Up>', self.increase_speed)
        self.root.bind('<Down>', self.decrease_speed)
        self.root.bind('<Left>', self.increase_steer)
        self.root.bind('<Right>', self.decrease_steer)
        self.root.bind('<space>', self.reset_controls)

        if remote_ns is None:
            self.tx_worker.start()
            self.rx_worker.start()
            self.status_label.config(text="CAN: connected (socketcan can0 250k)" if self.can.bus else "CAN: interface down.")
        else:
            self.status_label.config(text="REMOTE → /%s (board CAN). Enable=ARM, 창닫기=ESTOP" % remote_ns)

        self.process_ui_queue()
        self.push_tx_loop()

    def init_ui(self):
        frame = ttk.Frame(self.root, padding="10")
        frame.pack(fill=tk.BOTH, expand=True)

        ctrl_frame = ttk.LabelFrame(frame, text="System Toggles")
        ctrl_frame.pack(fill=tk.X, pady=10, ipady=5)
        
        ttk.Checkbutton(ctrl_frame, text="✅ Enable Control (Must be ON to move)", variable=self.control_enabled).pack(side=tk.LEFT, padx=10)
        ttk.Label(ctrl_frame, text="Headlights:").pack(side=tk.LEFT, padx=(15, 2))
        for v in (1, 2, 3):
            ttk.Radiobutton(ctrl_frame, text=str(v), value=v, variable=self.headlight_mode).pack(side=tk.LEFT, padx=2)

        slider_frame = ttk.LabelFrame(frame, text="Speed & Steering Targets (cmd_vel / Manual)")
        slider_frame.pack(fill=tk.BOTH, expand=True, pady=10)

        ttk.Label(slider_frame, text="Steering:").grid(row=0, column=0, padx=5, pady=15, sticky="w")
        self.steer_slider = ttk.Scale(slider_frame, from_=MAX_STEER, to=-MAX_STEER, orient=tk.HORIZONTAL, variable=self.current_steer)
        self.steer_slider.grid(row=0, column=1, padx=5, sticky="ew")
        self.steer_val_lbl = ttk.Label(slider_frame, text="0", width=6)
        self.steer_val_lbl.grid(row=0, column=2, padx=5)

        ttk.Label(slider_frame, text="Speed:").grid(row=1, column=0, padx=5, pady=15, sticky="w")
        self.speed_slider = ttk.Scale(slider_frame, from_=MAX_SPEED, to=-MAX_SPEED, orient=tk.HORIZONTAL, variable=self.current_speed)
        self.speed_slider.grid(row=1, column=1, padx=5, sticky="ew")
        self.speed_val_lbl = ttk.Label(slider_frame, text="0", width=6)
        self.speed_val_lbl.grid(row=1, column=2, padx=5)

        slider_frame.columnconfigure(1, weight=1)
        self.status_label = ttk.Label(frame, text="Status: Ready", font=("Arial", 10, "bold"), foreground="blue")
        self.status_label.pack(side=tk.BOTTOM, fill=tk.X, pady=5)

        self.current_steer.trace_add('write', lambda *args: self.steer_val_lbl.config(text=str(self.current_steer.get())))
        self.current_speed.trace_add('write', lambda *args: self.speed_val_lbl.config(text=str(self.current_speed.get())))

    def increase_speed(self, e=None): self.current_speed.set(min(self.current_speed.get() + 200, MAX_SPEED))
    def decrease_speed(self, e=None): self.current_speed.set(max(self.current_speed.get() - 200, -MAX_SPEED))
    def increase_steer(self, e=None): self.current_steer.set(min(self.current_steer.get() + 100, MAX_STEER))
    def decrease_steer(self, e=None): self.current_steer.set(max(self.current_steer.get() - 100, -MAX_STEER))
    def reset_controls(self, e=None): self.current_speed.set(0); self.current_steer.set(0)

    def process_ui_queue(self):
        while not self.ui_queue.empty():
            msg_type, data = self.ui_queue.get()
            
            if msg_type == "rx":
                if data.arbitration_id == FEEDBACK_CAN_ID and len(data.data) >= 4:
                    left_act, right_act = struct.unpack("<hh", data.data[0:4])
                    
                    if abs(left_act) < 5: left_act = 0
                    if abs(right_act) < 5: right_act = 0

                    self.ros_node.publish_odom(left_act, right_act)
                    
                    odom_info = f"X: {self.ros_node.x:.2f}m | Y: {self.ros_node.y:.2f}m | Yaw: {math.degrees(self.ros_node.th):.1f}°"
                    self.status_label.config(text=f"Motor L: {left_act}, R: {right_act}  ||  {odom_info}")

        # 원격 모드: nav2 명령속도(cmd_vel_nav) + 실제속도(odom) 표시. CAN RX 없음.
        if self.remote_ns is not None:
            v = getattr(self.ros_node, 'remote_speed', 0.0)
            wz = getattr(self.ros_node, 'remote_wz', 0.0)
            cv = getattr(self.ros_node, 'remote_cmd_v', 0.0)
            cwz = getattr(self.ros_node, 'remote_cmd_wz', 0.0)
            armed = "ARMED" if self.control_enabled.get() else "DISARMED"
            self.status_label.config(
                text="REMOTE /%s [%s]  nav2cmd: v=%.2f w=%.2f  |  actual(odom): v=%.2f w=%.2f"
                     % (self.remote_ns, armed, cv, cwz, v, wz))

        self.root.after(33, self.process_ui_queue)

    def apply_deadzone(self, value, dz): return 0 if abs(value) <= dz else value

    def push_tx_loop(self):
        if self.remote_ns is not None:
            # 원격: Enable=ARM 발행 + (nav2 유휴시) 슬라이더 수동조그 cmd_vel 발행.
            #   nav2 주행중(cmd_vel_nav 0.5s내 수신)이면 슬라이더=nav2 시각화 → 조그 발행 안함(경합방지).
            nav2_active = (time.time() - getattr(self.ros_node, 'last_cmd_nav_t', 0.0)) < 0.5
            # nav2 방금 멈춤(active→idle): 슬라이더에 남은 nav2 값이 자동 조그로 나가지 않게 0 리셋.
            if getattr(self, '_nav2_was_active', False) and not nav2_active:
                self.current_speed.set(0); self.current_steer.set(0)
            self._nav2_was_active = nav2_active
            self.ros_node.publish_remote(self.control_enabled.get(),
                                         self.current_speed.get(), self.current_steer.get(),
                                         nav2_active)
            self.root.after(round(1000 / CAN_SEND_FREQ_HZ), self.push_tx_loop)
            return
        if not self.control_enabled.get():
            self.filterd_speed = 0.0
            self.filterd_steer = 0.0
            left_wheel = 0
            right_wheel = 0
            start_stop = 0x00
        else:
            steer = self.apply_deadzone(self.current_steer.get(), DEAD_ZONE_STEER)
            speed = self.apply_deadzone(self.current_speed.get(), DEAD_ZONE_SPEED)

            alpha = 0.3
            self.filterd_steer = (1 - alpha) * self.filterd_steer + alpha * steer
            self.filterd_speed = (1 - alpha) * self.filterd_speed + alpha * speed
            
            left_wheel = max(-MAX_SPEED, min(MAX_SPEED, int(self.filterd_speed + self.filterd_steer)))
            right_wheel = max(-MAX_SPEED, min(MAX_SPEED, int(self.filterd_speed - self.filterd_steer)))
            start_stop = 0x01

        light_ctrl = self.headlight_mode.get() & 0xFF  # 1, 2, 3 중 선택값
        payload = struct.pack("<hhBBBB", right_wheel, left_wheel, light_ctrl, 0x00, start_stop, 0x05)

        try: self.tx_queue.put_nowait((CMD_CAN_ID, payload))
        except queue.Full: pass

        self.root.after(round(1000 / CAN_SEND_FREQ_HZ), self.push_tx_loop)

    def on_closing(self):
        print("Shutting down...")
        if self.remote_ns is not None:
            try: self.ros_node.send_estop()   # 원격: 창 닫으면 보드에 ESTOP
            except Exception: pass
        else:
            try: self.tx_queue.put_nowait((CMD_CAN_ID, struct.pack("<hhBBBB", 0, 0, 0x01, 0x00, 0x00, 0x05)))
            except queue.Full: pass

        self.stop_event.set()
        if self.tx_worker: self.tx_worker.join(timeout=1.0)
        if self.rx_worker: self.rx_worker.join(timeout=1.0)

        rclpy.shutdown()
        self.ros_thread.join(timeout=1.0)

        self.root.destroy()
        sys.exit()

def run_listen_only():
    """수신전용(headless): CAN RX 0x181 → /odom 발행만. TX(0x201) 없음.

    can_reader 가 0x201 명령프레임을 계속 쏘면 모터컨트롤러가 외부(CAN)제어
    모드로 전환돼 조종기(RC)를 무시한다. 이 모드는 TX 를 전혀 하지 않으므로
    RC 로 수동주행하면서 휠 오도메트리(/odom)만 뽑아 EKF(odom)에 넣을 수 있다.
    tkinter GUI 도 없어 headless(보드/ssh)에서 그대로 동작한다.
    """
    rclpy.init(args=None)
    node = VehicleROSNode(None, node_name='can_reader_listen')
    can_bus = CanBusWrapper()
    stop_event = threading.Event()
    ui_queue = queue.Queue()
    rx_worker = RxWorker(can_bus, ui_queue, stop_event)
    rx_worker.start()
    node.get_logger().info(
        'can_reader LISTEN-ONLY: RX 0x181 -> /odom (frame %s->%s), TX 없음(RC 제어 유지)'
        % (node.odom_frame, node.base_frame)
    )
    ros_thread = threading.Thread(target=rclpy.spin, args=(node,), daemon=True)
    ros_thread.start()
    try:
        while rclpy.ok():
            try:
                _tag, data = ui_queue.get(timeout=0.5)
            except queue.Empty:
                continue
            if data.arbitration_id == FEEDBACK_CAN_ID and len(data.data) >= 4:
                left_act, right_act = struct.unpack("<hh", data.data[0:4])
                if abs(left_act) < 5:
                    left_act = 0
                if abs(right_act) < 5:
                    right_act = 0
                node.publish_odom(left_act, right_act)
    except KeyboardInterrupt:
        pass
    finally:
        stop_event.set()
        rx_worker.join(timeout=1.0)
        if rclpy.ok():
            rclpy.shutdown()


# ==========================================
# 헤드리스 제어 노드 (--control): 하트비트를 GUI 와 분리
# ==========================================
#   문제: 기존 GUI 모드는 하트비트(0x201 TX)가 tkinter 메인루프(push_tx_loop)에
#     종속 → X11/wifi 스톨 시 하트비트가 끊겨 모터컨트롤러가 fail-safe 정지.
#   해결: 이 모드는 보드에서 헤드리스로 돌며 하트비트 30Hz 를 "전용 스레드" 에서
#     항상 발행(통신/GUI 무관). 모터값은 /cmd_vel(nav2 로컬), 제어권/정지는
#     /can/enable · /can/estop(호스트 can_panel GUI 가 발행) 로 받는다.
#   통신 끊겨도 하트비트 유지 → 오정지 없음. /cmd_vel 이 cmd_timeout 넘게 끊기면
#     모터만 0(안전), 하트비트는 계속. estop 또는 disarm 이면 모터 0.
class CanControlNode(VehicleROSNode):
    def __init__(self):
        super().__init__(None, node_name='can_control')
        self.declare_parameter('cmd_timeout', 0.5)   # /cmd_vel 끊김 안전정지(초)
        self.declare_parameter('headlight', 2)
        # ★ cmd_vel → CAN 모터값 매핑: 각 바퀴속도(m/s)를 odom(피드백)과 동일한 물리
        #   보정 스케일(conv=RAD_PER_TICK*WHEEL_RADIUS_M)로 매핑 → 명령↔피드백 일치.
        #   차량 모터는 ±100 미만이면 명령 들어가도 안 굴러가므로(데드밴드), 움직일 땐
        #   최소 motor_min(100)으로 floor, 안전 상한은 motor_max. 데드밴드 이하 속도는 0.
        #   (이전엔 전체 범위를 [100,800]으로 압축해 최고속도가 깎이고 odom 스케일과
        #    3배 어긋났음 → 물리 conv 매핑으로 복원)
        self.declare_parameter('motor_min', 100)
        self.declare_parameter('motor_max', 5600)     # 안전 상한(CAN units, ≈2.86m/s)
        self.declare_parameter('max_lin', 0.8)        # 정격 선속도[m/s] (참고용)
        self.declare_parameter('max_ang', 1.0)        # 정격 각속도[rad/s]
        self.declare_parameter('cmd_deadband', 0.02)  # 이 이하 바퀴속도는 0
        self.cmd_timeout = float(self.get_parameter('cmd_timeout').value)
        self.headlight = int(self.get_parameter('headlight').value)
        self.motor_min = int(self.get_parameter('motor_min').value)
        self.motor_max = int(self.get_parameter('motor_max').value)
        self.max_lin = float(self.get_parameter('max_lin').value)
        self.max_ang = float(self.get_parameter('max_ang').value)
        self.cmd_deadband = float(self.get_parameter('cmd_deadband').value)
        self.max_wheel_v = self.max_lin + self.max_ang * TRACK_WIDTH_M / 2.0
        self.target_lin = 0.0      # m/s (원본 cmd_vel)
        self.target_ang = 0.0      # rad/s
        self.armed = False         # /can/enable
        self.estop = False         # /can/estop
        self.last_cmd = 0.0
        self.lock = threading.Lock()
        self.create_subscription(Twist, '/cmd_vel', self._on_cmd, 10)
        # 라이트 상태(executor /sN/headlight): 3=하이빔(주행) 2=하향등 1=소등. CAN light 바이트 반영.
        self.create_subscription(UInt8, '/headlight', self._on_light, 10)
        self.create_subscription(Bool, '/can/enable', self._on_enable, 10)
        self.create_subscription(Bool, '/can/estop', self._on_estop, 10)

    def _on_cmd(self, msg):
        # 원본 cmd_vel(m/s, rad/s) 그대로 보관 → tx 스레드가 바퀴별 100~800 매핑.
        with self.lock:
            self.target_lin = msg.linear.x
            self.target_ang = msg.angular.z
            self.last_cmd = time.time()

    def map_wheel(self, v):
        """바퀴 선속도 v[m/s] → CAN 모터값. 데드밴드 이하는 0. 아니면 odom(피드백)과
        동일한 물리 보정 스케일(conv=RAD_PER_TICK*WHEEL_RADIUS_M, ≈1956 units/(m/s))로
        매핑 → 명령↔피드백 일치. motor_min 으로 저속 크리핑 floor, motor_max 안전 상한."""
        if abs(v) < self.cmd_deadband:
            return 0
        conv = RAD_PER_TICK * WHEEL_RADIUS_M          # m/s per CAN unit (odom 과 동일)
        mag = min(abs(v) / conv, float(self.motor_max))
        mag = max(mag, float(self.motor_min))
        return int(math.copysign(mag, v))

    def _on_enable(self, msg):
        self.armed = bool(msg.data)
        if not self.armed:
            self.estop = False       # disarm 이 estop 도 해제
        self.get_logger().info('ARMED=%s' % self.armed)

    def _on_light(self, msg):
        # executor 라이트 상태 → CAN light 바이트(0x201 byte5). 1=소등 2=하향등 3=하이빔.
        self.headlight = int(msg.data) & 0xFF

    def _on_estop(self, msg):
        if bool(msg.data):
            self.estop = True
            self.armed = False
            self.get_logger().warn('ESTOP → 모터 0 (하트비트 유지)')


def _heartbeat_tx_loop(node, can_bus, stop_event):
    """0x201 하트비트를 30Hz 로 항상 발행(전용 스레드). GUI/통신과 완전 분리."""
    period = 1.0 / CAN_SEND_FREQ_HZ
    filt_left, filt_right = 0.0, 0.0          # 출력 LPF 상태(툭툭 끊김 방지)
    while not stop_event.is_set():
        t0 = time.time()
        with node.lock:
            armed, estop = node.armed, node.estop
            lin, ang, lc = node.target_lin, node.target_ang, node.last_cmd
        fresh = (time.time() - lc) < node.cmd_timeout
        if estop or not armed:
            left = right = 0
            start_stop = 0x00
            filt_left = filt_right = 0.0       # 안전: 즉시 0 (LPF 램프 없이 정지)
        else:
            if not fresh:
                lin = ang = 0.0
            # 차동구동: 각 바퀴 선속도[m/s] → 물리 conv 매핑(floor motor_min, cap motor_max)
            left_v = lin - ang * TRACK_WIDTH_M / 2.0
            right_v = lin + ang * TRACK_WIDTH_M / 2.0
            tgt_left = max(-node.motor_max, min(node.motor_max, node.map_wheel(left_v)))
            tgt_right = max(-node.motor_max, min(node.motor_max, node.map_wheel(right_v)))
            # LPF 스무딩(alpha=0.3, 30Hz): raw 매핑을 부드럽게 → 툭툭 끊김 방지(옛 동작 복원)
            alpha = 0.3
            filt_left = (1 - alpha) * filt_left + alpha * tgt_left
            filt_right = (1 - alpha) * filt_right + alpha * tgt_right
            left = int(filt_left)
            right = int(filt_right)
            start_stop = 0x01
        payload = struct.pack("<hhBBBB", right, left, node.headlight & 0xFF, 0x00, start_stop, 0x05)
        if can is not None:
            can_bus.send(can.Message(arbitration_id=CMD_CAN_ID, data=payload, is_extended_id=False))
        dt = period - (time.time() - t0)
        if dt > 0:
            time.sleep(dt)


def run_headless_control():
    rclpy.init(args=None)
    node = CanControlNode()
    can_bus = CanBusWrapper()
    stop_event = threading.Event()
    ui_queue = queue.Queue()
    rx_worker = RxWorker(can_bus, ui_queue, stop_event)
    rx_worker.start()
    tx_thread = threading.Thread(target=_heartbeat_tx_loop, args=(node, can_bus, stop_event), daemon=True)
    tx_thread.start()
    node.get_logger().info(
        'can_control HEADLESS: RX 0x181->/odom (frame %s->%s), TX 0x201 하트비트@%dHz(전용스레드), '
        '/cmd_vel 모터, /can/enable /can/estop (기본 DISARMED)'
        % (node.odom_frame, node.base_frame, CAN_SEND_FREQ_HZ)
    )
    ros_thread = threading.Thread(target=rclpy.spin, args=(node,), daemon=True)
    ros_thread.start()
    try:
        while rclpy.ok():
            try:
                _tag, data = ui_queue.get(timeout=0.5)
            except queue.Empty:
                continue
            if data.arbitration_id == FEEDBACK_CAN_ID and len(data.data) >= 4:
                left_act, right_act = struct.unpack("<hh", data.data[0:4])
                if abs(left_act) < 5:
                    left_act = 0
                if abs(right_act) < 5:
                    right_act = 0
                node.publish_odom(left_act, right_act)
    except KeyboardInterrupt:
        pass
    finally:
        stop_event.set()
        rx_worker.join(timeout=1.0)
        tx_thread.join(timeout=1.0)
        # 종료 시 정지 프레임 몇 개 발행
        for _ in range(5):
            if can is not None:
                can_bus.send(can.Message(
                    arbitration_id=CMD_CAN_ID,
                    data=struct.pack("<hhBBBB", 0, 0, node.headlight & 0xFF, 0x00, 0x00, 0x05),
                    is_extended_id=False))
            time.sleep(0.02)
        if rclpy.ok():
            rclpy.shutdown()


if __name__ == "__main__":
    # 모드 선택(ROS args 앞): --control(헤드리스 자율제어) / --listen-only(RX만)
    #   / --remote <ns>(호스트 원격 GUI, CAN 대신 /<ns>/can/enable·estop·cmd_vel 발행)
    #   / 기본 GUI(로컬 can0)
    _argv = rclpy.utilities.remove_ros_args(sys.argv)
    if '--control' in _argv:
        run_headless_control()
    elif '--listen-only' in _argv:
        run_listen_only()
    elif '--remote' in _argv:
        # --remote <ns> : ns 는 다음 인자(예: s4). 생략/‘-’ 시작이면 루트('').
        i = _argv.index('--remote')
        ns = _argv[i + 1] if (i + 1 < len(_argv) and not _argv[i + 1].startswith('-')) else ''
        if tk is None:
            print("❌ 원격 GUI 에도 python3-tk 필요")
            sys.exit(1)
        root = tk.Tk()
        app = VehicleControl(root, remote_ns=ns)
        root.protocol("WM_DELETE_WINDOW", app.on_closing)
        root.mainloop()
    else:
        if tk is None:
            print("❌ GUI 모드엔 python3-tk 필요. 헤드리스는 --control 또는 --listen-only 사용")
            sys.exit(1)
        root = tk.Tk()
        app = VehicleControl(root)
        root.protocol("WM_DELETE_WINDOW", app.on_closing)
        root.mainloop()