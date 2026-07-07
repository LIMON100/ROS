#!/usr/bin/env python3
# Convoy 데이터 통신 정책 — UGV 측 체인 추종 노드.
#
# Go2 SITL 리더 + UGV×4 단일종대 콘보이 데모의 UGV 측 노드(robot_2/3/4/5 인스턴스).
# 리더 측은 convoy_coordinator. 검증 결과는 그 모듈 헤더 참조.
#
#   - 자기 /robot_<id>/odom(gz-sim-odometry-publisher = world 지상진실) 사용.
#   - UGV→Leader @2Hz: /convoy/report/<id> 로 현재 위치 보고.
#   - Leader→UGV: /convoy/target/<id> 로 '앞선 로봇' 현재위치(pose)+속도(twist) 수신.
#       → 1초 후 예측 = pose + vel*horizon, 슬롯 = 예측 - gap*(진행방향).
#   - 슬롯 추종: 최고속도 지향(kp·dist→vmax) + 앞 로봇과 충돌회피(근접 시 감속/정지).
import math
import time
from collections import deque

import rclpy
from geometry_msgs.msg import PoseArray, Twist
from nav_msgs.msg import Odometry
from rclpy.node import Node
from rclpy.qos import qos_profile_sensor_data


def wrap(a):
    return math.atan2(math.sin(a), math.cos(a))


def yaw_of(q):
    return math.atan2(
        2.0 * (q.w * q.z + q.x * q.y), 1.0 - 2.0 * (q.y * q.y + q.z * q.z)
    )


class ConvoyUGV(Node):
    def __init__(self):
        super().__init__("convoy_ugv")
        self.id = self.declare_parameter("robot_id", 3).value
        self.ox = self.declare_parameter("spawn_x", -3.0).value
        self.oy = self.declare_parameter("spawn_y", 0.0).value
        self.gap = self.declare_parameter("gap_m", 3.0).value
        self.vmax = self.declare_parameter("max_linear_mps", 0.6).value
        self.wmax = self.declare_parameter("max_angular_rps", 1.0).value
        self.kp_lin = self.declare_parameter("kp_lin", 0.8).value
        # 각속 게인 하향(2.0→1.2) + 각가속(slew) 제한 → 헤딩 진동(ZigZag) 억제.
        self.kp_ang = self.declare_parameter("kp_ang", 1.2).value
        self.ang_slew = self.declare_parameter("ang_slew_rps2", 2.0).value
        # 간격 회귀 게인: 속도 = 선행 pace + kp_gap·(arc-gap) 로 경로상 간격을 gap 으로 능동
        # 유지(좁혀지면 감속, 멀어지면 가속). 선행 추월/충돌은 min_gap/slow_gap 이 별도 보호.
        self.kp_gap = self.declare_parameter("kp_gap", 0.6).value
        # 경로추종 look-ahead 호장: 자기 최근접 trail 점에서 이만큼 앞선 점을 향함(작을수록
        # 경로 밀착, 너무 작으면 jitter). 선행이 '지나간 좌표'를 정밀 되밟기 위함.
        self.lookahead = self.declare_parameter("path_lookahead_m", 0.5).value
        self.horizon = self.declare_parameter("predict_horizon_s", 1.0).value
        self.min_gap = self.declare_parameter(
            "min_gap_m", 2.0
        ).value  # 앞로봇 충돌 정지거리
        self.slow_gap = self.declare_parameter(
            "slow_gap_m", 3.2
        ).value  # 앞로봇 감속 시작거리
        self.timeout = self.declare_parameter("target_timeout_s", 3.0).value
        # ★측면 안전거리: 선행 path 참조하되 장애물 x 영향권에서 중심선(장애물 y)으로부터
        #   회피측 편차 ≥ lateral_min 확보한 새 waypoint 생성. (UGV폭/2+1m = 2.25m, 사용자 지정)
        self.lateral_min = self.declare_parameter("lateral_min_m", 1.5).value
        self.lateral_max = self.declare_parameter("lateral_max_m", 3.0).value
        # 장애물 회피 휴면 비상망(순수 조향). 평시 미작동(전복/stuck 없음).
        self.react = self.declare_parameter("obs_react_m", 0.1).value
        self.clear = self.declare_parameter("obs_clear_m", 0.5).value
        self.avoid_cone = self.declare_parameter("obs_cone_rad", 1.4).value
        self.obstacles = []  # [(x,y,r), ...]
        self.x = self.y = self.yaw = 0.0
        self.have_own = False
        self.px = self.py = self.pyaw = 0.0
        self.pvx = self.pvy = 0.0
        self.t_stamp = 0.0
        self.have_t = False
        # breadcrumb: 앞 로봇의 실제 궤적 누적 → gap 만큼 뒤 점을 추종(코너컷 없음=단일종대)
        self.trail = deque(maxlen=3000)
        self.min_step = self.declare_parameter("trail_step_m", 0.12).value
        self.last_v = 0.0
        self.last_w = 0.0  # 각가속 제한용(이전 tick 각속)
        # 자기 위치: /robot_<id>/odom = gz-sim-odometry-publisher(지상진실, 이미 world 좌표)
        self.create_subscription(Odometry, "odom", self.on_gt, qos_profile_sensor_data)
        self.create_subscription(
            Odometry, f"/convoy/target/r{self.id}", self.on_target, 10
        )
        self.create_subscription(PoseArray, "/convoy/obstacles", self.on_obstacles, 10)
        self.report = self.create_publisher(Odometry, f"/convoy/report/r{self.id}", 10)
        self.cmd = self.create_publisher(Twist, "cmd_vel", 10)
        # POC 스펙: 전로봇→리더 위치보고 주기 0.2 s(5 Hz). DCN delta — convoy 통신율 상향.
        comm_period = self.declare_parameter("comm_period_s", 0.2).value
        self.create_timer(0.05, self.drive)  # 20Hz control
        self.create_timer(comm_period, self.pub_report)  # 5Hz(0.2s) UGV->Leader report
        self.create_timer(2.0, self.diag)
        self.get_logger().info(
            f"ConvoyUGV id={self.id} spawn=({self.ox:.1f},{self.oy:.1f}) "
            f"gap={self.gap:.1f} vmax={self.vmax:.2f}"
        )

    def on_gt(self, m):
        self.x = m.pose.pose.position.x  # 지상진실 world pose (offset 불필요)
        self.y = m.pose.pose.position.y
        self.yaw = yaw_of(m.pose.pose.orientation)
        self.have_own = True

    def on_target(self, m):
        self.px = m.pose.pose.position.x
        self.py = m.pose.pose.position.y
        self.pyaw = yaw_of(m.pose.pose.orientation)
        self.pvx = m.twist.twist.linear.x
        self.pvy = m.twist.twist.linear.y
        self.t_stamp = time.monotonic()
        self.have_t = True
        # 궤적 누적. 최초엔 앞 로봇→내 위치 방향으로 후방 연장 seed(시작 시 gap 붕괴 방지).
        if not self.trail:
            if self.have_own:
                dx, dy = self.px - self.x, self.py - self.y
                d = math.hypot(dx, dy)
                if d > 0.2:
                    ux, uy = dx / d, dy / d
                    n = int((self.gap + 1.5) / 0.2)
                    for i in range(n, -1, -1):
                        self.trail.append(
                            (self.px - ux * 0.2 * i, self.py - uy * 0.2 * i)
                        )
                else:
                    self.trail.append((self.px, self.py))
            else:
                self.trail.append((self.px, self.py))
        else:
            lx, ly = self.trail[-1]
            if math.hypot(self.px - lx, self.py - ly) > self.min_step:
                self.trail.append((self.px, self.py))

    def _nearest_idx(self):
        # 자기 위치에서 가장 가까운 trail 점 인덱스 = 되밟을 경로상 '현재 위치'.
        pts = self.trail
        best_i, best_d = 0, float("inf")
        for i in range(len(pts)):
            d = math.hypot(pts[i][0] - self.x, pts[i][1] - self.y)
            if d < best_d:
                best_d, best_i = d, i
        return best_i

    def _path_target(self, i0):
        # nearest 점에서 lookahead 호장만큼 앞선 trail 점(보간). 선행이 '지나간 좌표'를
        # 그대로 되밟음 → 선행의 현재 방향/위치가 아닌 경로 자체를 정밀 추종(코너컷 없음).
        pts = self.trail
        rem = self.lookahead
        for i in range(i0, len(pts) - 1):
            ax, ay = pts[i]
            bx, by = pts[i + 1]
            seg = math.hypot(bx - ax, by - ay)
            if seg >= rem:
                t = rem / seg if seg > 1e-6 else 0.0
                return (ax + t * (bx - ax), ay + t * (by - ay))
            rem -= seg
        return pts[-1]

    def _arc_to_dog(self, i0):
        # nearest 점 → trail 끝(선행 최신 위치)까지 호장 = 경로상 선행 간격(직선거리 아님).
        pts = self.trail
        s = 0.0
        for i in range(i0, len(pts) - 1):
            s += math.hypot(pts[i + 1][0] - pts[i][0], pts[i + 1][1] - pts[i][1])
        return s

    def follow_target(self, i0):
        # 경로추종 타겟 = 선행이 지나간 trail 의 되밟기 점 그대로(측면 보정 없음).
        # 선행 경로 자체가 이미 장애물을 회피한 안전 경로이므로, 그 좌표를 정확히 되밟으면
        # 동일 클리어런스가 보장된다(별도 측면 오프셋은 retrace 를 왜곡 → 제거). 돌발 장애물은
        # avoid_override 안전망이 처리.
        return self._path_target(i0)

    def on_obstacles(self, m):
        # (x, y, radius, side) — side=리더 경로가 가는 회피 쪽(코디네이터 제공)
        self.obstacles = [
            (
                p.position.x,
                p.position.y,
                p.position.z,
                1.0 if p.orientation.z >= 0 else -1.0,
            )
            for p in m.poses
        ]

    def avoid_override(self, v, w):
        # 장애물 회피(최우선): 진행방향 cone 내 가장 위협적인 장애물 회피.
        worst = None
        for ox, oy, r, oside in self.obstacles:
            dx, dy = ox - self.x, oy - self.y
            do = math.hypot(dx, dy)
            surf = do - r
            if surf > r + self.react:
                continue
            bearing = wrap(math.atan2(dy, dx) - self.yaw)
            if abs(bearing) > self.avoid_cone:
                continue
            if worst is None or surf < worst[0]:
                worst = (surf, oside, r)
        if worst is None:
            return v, w, False
        surf, oside, r = worst
        # 코디네이터 회피쪽으로 '순수 조향'(속도 불변 → stuck 방지). 가까울수록 강하게.
        # 앞 로봇 충돌은 drive()의 min_gap 이 별도 처리. breadcrumb 이 주 회피, 이건 안전망.
        gain = max(0.0, min(1.0, (r + self.react - surf) / self.react))
        turn = oside * min(self.wmax, 0.55) * (0.4 + 0.6 * gain)
        return v, turn, True

    def pub_report(self):
        if not self.have_own:
            return
        o = Odometry()
        o.header.frame_id = "odom"
        o.header.stamp = self.get_clock().now().to_msg()
        o.pose.pose.position.x = self.x
        o.pose.pose.position.y = self.y
        o.pose.pose.orientation.z = math.sin(self.yaw / 2.0)
        o.pose.pose.orientation.w = math.cos(self.yaw / 2.0)
        self.report.publish(o)

    def drive(self):
        cmd = Twist()
        if (
            not self.have_own
            or not self.have_t
            or (time.monotonic() - self.t_stamp) > self.timeout
            or len(self.trail) < 2
        ):
            self.cmd.publish(cmd)
            return
        # ★경로추종: 선행이 '지나간 좌표(trail)'를 되밟는다. 자기 최근접 trail 점(i0)에서
        #   lookahead 앞선 점을 향함 — 선행의 현재 위치/방향이 아닌 경로 자체를 정밀 추종.
        i0 = self._nearest_idx()
        tx, ty = self.follow_target(i0)
        dx, dy = tx - self.x, ty - self.y
        des = math.atan2(dy, dx)
        yerr = math.atan2(math.sin(des - self.yaw), math.cos(des - self.yaw))
        turn = max(0.0, 1.0 - abs(yerr) / math.pi)
        # 간격 유지: 경로상 선행 간격(arc, 호장)을 gap 으로 회귀 + 선행 pace feed-forward.
        #   arc>gap → 가속(낙오 방지), arc<gap → 감속(좁혀지면 속도↓). 직선거리(dpred)가 아닌
        #   호장 회귀라 곡선에서도 정상상태 offset 없이 ~gap 수렴.
        arc = self._arc_to_dog(i0)
        pace = math.hypot(self.pvx, self.pvy)
        v = pace + self.kp_gap * (arc - self.gap)
        v = min(max(0.0, v), self.vmax) * turn
        dpred = math.hypot(self.px - self.x, self.py - self.y)  # 직선 충돌거리(보호용)
        if dpred < self.min_gap:  # 앞 로봇 충돌회피(정지)
            v = 0.0
        elif dpred < self.slow_gap:  # 근접 감속
            v *= (dpred - self.min_gap) / (self.slow_gap - self.min_gap)
        # 조향: 각속 게인 + 각가속(slew) 제한으로 헤딩 진동(ZigZag) 억제(20Hz, dt=0.05).
        w_des = max(-self.wmax, min(self.wmax, self.kp_ang * yerr))
        dw = self.ang_slew * 0.05
        w = max(self.last_w - dw, min(self.last_w + dw, w_des))
        v, w, _ = self.avoid_override(v, w)  # 장애물 회피 최우선(안전망)
        self.last_w = w
        cmd.linear.x = max(0.0, v)
        cmd.angular.z = w
        self.last_v = cmd.linear.x
        self.cmd.publish(cmd)

    def diag(self):
        if self.have_t and len(self.trail) >= 2:
            i0 = self._nearest_idx()
            tx, ty = self.follow_target(i0)
            arc = self._arc_to_dog(i0)
            dpred = math.hypot(self.px - self.x, self.py - self.y)
            self.get_logger().info(
                f"id={self.id} own=({self.x:.2f},{self.y:.2f}) "
                f"tgt=({tx:.2f},{ty:.2f}) arc={arc:.2f} dpred={dpred:.2f} "
                f"trail={len(self.trail)} v={self.last_v:.2f}"
            )
        else:
            self.get_logger().info(
                f"id={self.id} own=({self.x:.2f},{self.y:.2f}) "
                f"no target(have_own={self.have_own})"
            )


def main():
    rclpy.init()
    rclpy.spin(ConvoyUGV())
    rclpy.shutdown()


if __name__ == "__main__":
    main()
