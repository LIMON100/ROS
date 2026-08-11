#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
pan_tilt_fixed_corners.py
- '의도 타깃'을 고정된 네 꼭짓점으로 설정하고, 절대 center를 갱신하지 않음
- 코너 좌표를 시작 시 콘솔에 출력 + 모든 로그 라인에 기록
- 한 축씩만 이동(팬→틸트→팬→틸트), 코너에서 dwell 가능
- CSV: ~/logs/pan_tilt_log_YYYYmmdd_HHMMSS.csv
#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""

import os, sys, csv, select, tty, termios, math
from datetime import datetime

import rclpy
from rclpy.node import Node
from rclpy.qos import QoSProfile, ReliabilityPolicy, HistoryPolicy
from std_msgs.msg import Header
from combat_robot_msgs.msg import PanTiltControlCommand, PanTiltState

CONTROL_BRAKE   = 0
CONTROL_HOR_POS = 1
CONTROL_VER_POS = 2
CONTROL_DIR     = 3  # (미사용)

class PanTiltBestRectNode(Node):
    def __init__(self):
        super().__init__('pan_tilt_best_rect_node')

        # ===== 사용자 파라미터 =====
        self.timer_hz         = 20.0
        self.speed            = 20
        self.tol_deg          = 0.05   # 현실 최소 허용오차 권장값 (0.0은 노이즈/분해능 때문에 비권장)
        self.snap_th          = 0.05   # 이 이하면 최종 스냅(동일 좌표 재명령) 후 도달로 인정
        self.dwell_sec        = 1.0
        self.use_initial_center = False
        self.initial_pan_offset = -5.0  # 시작 시 왼쪽 살짝 보내 중심 고정
        # center를 직접 박고 싶으면 아래에 값 지정
        self.center_pan_param  = 0.0
        self.center_tilt_param = 0.0

        self.delta_pan   = 5.0  # 가로 반폭
        self.delta_tilt  = 2.5   # 세로 반폭

        # 재전송 주기(축 대기 중 명령 주기적 재전송)
        self.resend_period_s = 0.3

        # ===== ROS I/O =====
        state_qos = QoSProfile(reliability=ReliabilityPolicy.BEST_EFFORT,
                               history=HistoryPolicy.KEEP_LAST, depth=10)
        cmd_qos   = QoSProfile(reliability=ReliabilityPolicy.RELIABLE,
                               history=HistoryPolicy.KEEP_LAST, depth=10)
        self.pub = self.create_publisher(PanTiltControlCommand, '/pan_tilt_control_command', cmd_qos)
        self.sub = self.create_subscription(PanTiltState, '/current_actuator_state_info', self.state_cb, state_qos)

        # ===== 내부 상태 =====
        self.have_state   = False
        self.current_pan  = 0.0
        self.current_tilt = 0.0

        self.center_pan   = None
        self.center_tilt  = None

        self.corners = []        # [(pan,tilt) * 4] 고정 의도 타깃
        self.corner_idx = 0      # 현재 목표 코너 인덱스 0~3

        self.cmd_target_pan  = 0.0   # 의도 타깃(항상 corner)
        self.cmd_target_tilt = 0.0

        self.state = 'WAIT_FIRST_STATE'
        self.axis_phase = None        # 'pan' or 'tilt' (한 축 정렬 중)
        self.dwell_deadline = None

        # 초기 보정용 '고정' 타깃
        self.init_pan_target  = None
        self.init_tilt_target = None

        # Zero-cross(지남) 검출용: 현재 축에서 이전 에러 저장
        self.prev_err = None
        self.last_send_time = 0.0

        # ===== CSV 로깅 =====
        log_dir = os.path.expanduser('~/logs')
        os.makedirs(log_dir, exist_ok=True)
        ts = datetime.now().strftime('%Y%m%d_%H%M%S')
        self.log_path = os.path.join(log_dir, f'pan_tilt_log_{ts}.csv')
        self.log_file = open(self.log_path, 'w', newline='')
        self.csv = csv.writer(self.log_file)
        self.csv.writerow([
            'time_sec','event','fsm_state',
            'current_pan','current_tilt',
            'cmd_target_pan','cmd_target_tilt',
            'corner_index',
            'corner0_pan','corner0_tilt','corner1_pan','corner1_tilt',
            'corner2_pan','corner2_tilt','corner3_pan','corner3_tilt',
            'err_pan','err_tilt'
        ])
        self.get_logger().info(f'CSV logging → {self.log_path}')
        self.get_logger().info("PanTiltBestRectNode started. 'q'로 (0,0) 복귀 후 종료.")

        # ===== 타이머 & 키보드 =====
        self.timer = self.create_timer(1.0/self.timer_hz, self.loop)
        try:
            self.stdin_fd = sys.stdin.fileno()
            self.stdin_old = termios.tcgetattr(self.stdin_fd)
            tty.setcbreak(self.stdin_fd)
            self.keyboard_enabled = True
        except Exception:
            self.keyboard_enabled = False
            self.get_logger().warn("STDIN 사용 불가 → 'q' 종료 비활성화(launch 환경이면 정상)")

    # ---------- ROS 콜백 ----------
    def state_cb(self, msg: PanTiltState):
        self.current_pan  = float(msg.horizontal_angle)
        self.current_tilt = float(msg.vertical_angle)
        self.have_state   = True

    # ---------- 유틸 ----------
    def now_s(self):
        return self.get_clock().now().nanoseconds / 1e9

    def key_pressed(self):
        if not self.keyboard_enabled:
            return None
        dr, _, _ = select.select([sys.stdin], [], [], 0)
        return sys.stdin.read(1) if dr else None

    def send_pan(self, pan_target):
        cmd = PanTiltControlCommand()
        cmd.header = Header(); cmd.header.stamp = self.get_clock().now().to_msg()
        cmd.control_mode = CONTROL_HOR_POS
        cmd.horizontal_angle = float(pan_target)
        cmd.vertical_angle   = float(self.current_tilt)  # 틸트 유지
        cmd.pan_speed = self.speed; cmd.tilt_speed = self.speed
        cmd.pan_dir = 0; cmd.tilt_dir = 0
        self.pub.publish(cmd)

    def send_tilt(self, tilt_target):
        cmd = PanTiltControlCommand()
        cmd.header = Header(); cmd.header.stamp = self.get_clock().now().to_msg()
        cmd.control_mode = CONTROL_VER_POS
        cmd.horizontal_angle = float(self.current_pan)   # 팬 유지
        cmd.vertical_angle   = float(tilt_target)
        cmd.pan_speed = self.speed; cmd.tilt_speed = self.speed
        cmd.pan_dir = 0; cmd.tilt_dir = 0
        self.pub.publish(cmd)

    def reached(self, pan_tgt, tilt_tgt):
        # 현실적 도달: 작은 오차면 최종 스냅 후 True
        pan_err  = abs(self.current_pan  - pan_tgt)
        tilt_err = abs(self.current_tilt - tilt_tgt)
        if pan_err < self.snap_th and tilt_err < self.snap_th:
            # 최종 스냅(두 축 모두 동일 좌표로 명령)
            self.send_pan(pan_tgt); self.send_tilt(tilt_tgt)
            return True
        return (pan_err <= self.tol_deg) and (tilt_err <= self.tol_deg)

    def write_log(self, event='NORMAL'):
        t = self.now_s()
        err_pan  = self.cmd_target_pan  - self.current_pan
        err_tilt = self.cmd_target_tilt - self.current_tilt
        c0 = self.corners[0] if self.corners else ("","")
        c1 = self.corners[1] if self.corners else ("","")
        c2 = self.corners[2] if self.corners else ("","")
        c3 = self.corners[3] if self.corners else ("","")
        self.csv.writerow([
            f'{t:.3f}', event, self.state,
            f'{self.current_pan:.3f}', f'{self.current_tilt:.3f}',
            f'{self.cmd_target_pan:.3f}', f'{self.cmd_target_tilt:.3f}',
            self.corner_idx,
            f'{c0[0]}', f'{c0[1]}', f'{c1[0]}', f'{c1[1]}',
            f'{c2[0]}', f'{c2[1]}', f'{c3[0]}', f'{c3[1]}',
            f'{err_pan:.3f}', f'{err_tilt:.3f}',
        ])

    def print_corners(self):
        self.get_logger().info("=== Fixed Corners (pan°, tilt°) ===")
        for i,(px,py) in enumerate(self.corners):
            self.get_logger().info(f"corner[{i}]: ({px:.3f}, {py:.3f})")
        self.get_logger().info("Order: 0 → 1 → 2 → 3 → 0 ... (팬/틸트 한 축씩 이동)")

    # ---------- 메인 루프 ----------
    def loop(self):
        # q 종료
        if self.key_pressed() == 'q':
            self.get_logger().info("Exit requested → (0°,0°) 복귀 중…")
            self.send_pan(0.0);  self._block_until(lambda: abs(self.current_pan)  <= self.tol_deg, 3.0)
            self.send_tilt(0.0); self._block_until(lambda: abs(self.current_tilt) <= self.tol_deg, 3.0)
            self._shutdown()
            return

        # FSM
        if self.state == 'WAIT_FIRST_STATE':
            if self.have_state:
                if self.center_pan_param is not None and self.center_tilt_param is not None:
                    self.center_pan  = float(self.center_pan_param)
                    self.center_tilt = float(self.center_tilt_param)
                    self._build_corners_and_start()
                elif self.use_initial_center:
                    # 초기 보정용 고정 타깃 생성 후 이동 시작
                    self.init_pan_target  = float(self.current_pan + self.initial_pan_offset)
                    self.init_tilt_target = float(self.current_tilt)
                    self.send_pan(self.init_pan_target)
                    self.last_send_time = self.now_s()
                    self.state = 'INIT_MOVE_LEFT'
                else:
                    self.center_pan  = self.current_pan
                    self.center_tilt = self.current_tilt
                    self._build_corners_and_start()

        elif self.state == 'INIT_MOVE_LEFT':
            # 고정 타깃에 대한 도달 판정 + 주기 재전송
            if self.reached(self.init_pan_target, self.init_tilt_target):
                # 여기서의 '현재 위치'를 최종 center로 고정
                self.center_pan  = self.current_pan
                self.center_tilt = self.current_tilt
                self._build_corners_and_start()
            else:
                if self.now_s() - self.last_send_time > self.resend_period_s:
                    self.send_pan(self.init_pan_target)
                    self.last_send_time = self.now_s()

        elif self.state == 'ISSUE_CORNER':
            tgt_pan, tgt_tilt = self.corners[self.corner_idx]
            self.cmd_target_pan, self.cmd_target_tilt = tgt_pan, tgt_tilt
            # 먼저 팬 정렬 → 그 다음 틸트
            if abs(self.current_pan - tgt_pan) > self.tol_deg:
                self.send_pan(tgt_pan)
                self.axis_phase = 'pan'
                self.prev_err = tgt_pan - self.current_pan
                self.last_send_time = self.now_s()
                self.state = 'WAIT_AXIS_REACH'
            elif abs(self.current_tilt - tgt_tilt) > self.tol_deg:
                self.send_tilt(tgt_tilt)
                self.axis_phase = 'tilt'
                self.prev_err = tgt_tilt - self.current_tilt
                self.last_send_time = self.now_s()
                self.state = 'WAIT_AXIS_REACH'
            else:
                # 이미 corner 안에 있음 → dwell
                self._enter_dwell_or_next()

        elif self.state == 'WAIT_AXIS_REACH':
            tgt_pan, tgt_tilt = self.corners[self.corner_idx]
            self.cmd_target_pan, self.cmd_target_tilt = tgt_pan, tgt_tilt

            if self.axis_phase == 'pan':
                # 지남(zerocross) 기록
                curr_err = tgt_pan - self.current_pan
                if self.prev_err is not None and (self.prev_err * curr_err) <= 0:
                    self.write_log(event='ZEROCROSS')
                self.prev_err = curr_err

                # 주기 재전송
                if self.now_s() - self.last_send_time > self.resend_period_s:
                    self.send_pan(tgt_pan); self.last_send_time = self.now_s()

                # 도달하면 틸트 정렬로
                if self.reached(tgt_pan, self.current_tilt):
                    if abs(self.current_tilt - tgt_tilt) > self.tol_deg:
                        self.send_tilt(tgt_tilt)
                        self.axis_phase = 'tilt'
                        self.prev_err = tgt_tilt - self.current_tilt
                        self.last_send_time = self.now_s()
                    else:
                        # 최종 스냅 후 코너 처리
                        self.send_pan(tgt_pan); self.send_tilt(tgt_tilt)
                        self._enter_dwell_or_next()

            else:  # 'tilt'
                curr_err = tgt_tilt - self.current_tilt
                if self.prev_err is not None and (self.prev_err * curr_err) <= 0:
                    self.write_log(event='ZEROCROSS')
                self.prev_err = curr_err

                if self.now_s() - self.last_send_time > self.resend_period_s:
                    self.send_tilt(tgt_tilt); self.last_send_time = self.now_s()

                if self.reached(self.current_pan, tgt_tilt):
                    # 최종 스냅 후 코너 처리
                    self.send_pan(tgt_pan); self.send_tilt(tgt_tilt)
                    self._enter_dwell_or_next()

        elif self.state == 'DWELL':
            if self.now_s() >= self.dwell_deadline:
                self.corner_idx = (self.corner_idx + 1) % 4
                self.state = 'ISSUE_CORNER'

        else:
            self.get_logger().warn(f'Unknown state: {self.state}')
            self.state = 'WAIT_FIRST_STATE'

        # 기본 로깅
        self.write_log('NORMAL' if self.state != 'DWELL' else 'DWELL')

    # ---------- 보조 ----------
    def _build_corners_and_start(self):
        # 고정 코너 4개: 위→우상→우하→좌하 (시계 방향)
        cp, ct = self.center_pan, self.center_tilt
        dp, dt = self.delta_pan, self.delta_tilt
        self.corners = [
            (cp-dp,  ct+dt),  # 0
            (cp+dp,  ct+dt),  # 1
            (cp+dp,  ct-dt),  # 2
            (cp-dp,  ct-dt),  # 3
        ]
        self.corner_idx = 0
        self.print_corners()
        self.state = 'ISSUE_CORNER'

    def _enter_dwell_or_next(self):
        if self.dwell_sec > 0.0:
            self.dwell_deadline = self.now_s() + self.dwell_sec
            self.state = 'DWELL'
        else:
            self.corner_idx = (self.corner_idx + 1) % 4
            self.state = 'ISSUE_CORNER'

    def _block_until(self, predicate, timeout_s):
        end = self.now_s() + timeout_s
        while self.now_s() < end and rclpy.ok():
            if predicate():
                return True
            rclpy.spin_once(self, timeout_sec=0.02)
        return False

    def _shutdown(self):
        try:
            self.log_file.flush()
            self.log_file.close()
        except Exception:
            pass
        try:
            if self.keyboard_enabled:
                termios.tcsetattr(self.stdin_fd, termios.TCSADRAIN, self.stdin_old)
        except Exception:
            pass
        self.get_logger().info("Shutting down.")
        rclpy.shutdown()

def main():
    rclpy.init()
    node = PanTiltBestRectNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        node._shutdown()

if __name__ == '__main__':
    main()
