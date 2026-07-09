#!/usr/bin/env python3
# Convoy 데이터 통신 정책 — Leader(로봇개/Go2) 측 코디네이터.
#
# Go2 SITL(외부 unitree_go2_ros2) 리더 + UGV×4 단일종대 콘보이 데모의 리더 측 노드.
# 검증: Ubuntu 24.04 / ROS 2 Jazzy / Gazebo Harmonic 8.14 (SITL). 리더가 계획경로를
# 8/8 waypoint 완주(dx≈15 m, 전복 0), UGV 4대 gap≈3.5 m 종대 유지, 장애물(≥2.25 m) 회피.
#
#   정책:
#    - Leader 는 계획 경로(waypoints)를 pure-pursuit 로 주행(급회전 억제→전복 방지, 장애물 우회).
#    - UGV→Leader @2Hz: 각 UGV 가 /convoy/report/<id> 로 현재 world 위치 보고.
#    - Leader→UGV @2Hz: 각 UGV 의 '앞선 로봇' 현재위치+속도를 /convoy/target/<id> 로 제공
#      (UGV 가 이 속도로 1초 후 목표를 산출). pred 0 = Leader(Go2).
#    - Leader 속도 = 첫 UGV(robot_3) 간격 반영(뒤처지면 감속 → 콘보이 유지). 평시 최고속도.
#    - 지도 표시: /convoy/plan(nav_msgs/Path) 발행(+Gazebo 마커는 런처가 spawn).
import math
import time
from collections import deque

import rclpy
from geometry_msgs.msg import Pose, PoseArray, PoseStamped, Twist
from nav_msgs.msg import Odometry, Path
from rclpy.node import Node
from rclpy.qos import qos_profile_sensor_data

from san_operator_tools import convoy_avoidance as av

CHAIN = [3, 4, 5, 2]  # leader -> 3 -> 4 -> 5 -> 2
PRED = {3: 0, 4: 3, 5: 4, 2: 5}  # 0 = leader(Go2)
# 체인상 슬롯 번호(리더 바로 뒤=1). 추종자 nav2 참조경로(DCN-2026-029 P1)의 슬롯거리 =
# SLOT[n] * slot_gap (리더 breadcrumb head 뒤 호장). 단일종대라 차선은 공통, 슬롯만 다름.
SLOT = {n: i + 1 for i, n in enumerate(CHAIN)}  # {3:1, 4:2, 5:3, 2:4}


def yaw_of(q):
    return math.atan2(
        2.0 * (q.w * q.z + q.x * q.y), 1.0 - 2.0 * (q.y * q.y + q.z * q.z)
    )


def wrap(a):
    return math.atan2(math.sin(a), math.cos(a))


class Coordinator(Node):
    def __init__(self):
        super().__init__("convoy_coordinator")
        # 장애물 x=8(초기 경로 y=0 위, 출발에서 멀어 완만 등반 가능). 완만 사전우회로 (8,0)
        # 옆 ~2.5m 통과 → 중심선 편차 ≥2.25m 충족(+마진). 등반 ≤22° → Go2 전복 없음.
        # 긴 거리(~40m) 완만 S-curve. 급회전 없어 저RTF gait 안정.
        wp = self.declare_parameter(
            "waypoints",
            [
                0.0,
                0.0,
                4.0,
                1.0,
                8.0,
                2.0,
                12.0,
                2.5,
                16.0,
                2.0,
                20.0,
                1.0,
                24.0,
                0.0,
                28.0,
                -1.0,
                32.0,
                -1.5,
                36.0,
                -1.0,
                40.0,
                0.0,
            ],
        ).value
        self.wps = [(float(wp[i]), float(wp[i + 1])) for i in range(0, len(wp), 2)]
        self.vmax = self.declare_parameter("leader_vmax", 0.6).value
        self.wmax = self.declare_parameter(
            "leader_wmax", 0.22
        ).value  # 회전 매우 완만(저RTF 전복방지)
        self.kp_ang = self.declare_parameter("kp_ang", 0.7).value
        self.goal_tol = self.declare_parameter("goal_tol_m", 0.5).value
        self.match_gap = self.declare_parameter("match_gap_m", 3.5).value
        self.match_k = self.declare_parameter("match_slow_gain", 0.4).value
        self.ang_slew = self.declare_parameter("ang_slew_rps2", 1.2).value
        so = self.declare_parameter(
            "spawn_offsets", [3.0, -3.0, 4.0, -6.0, 5.0, -9.0, 2.0, -12.0]
        ).value
        self.spawn = {int(so[i]): float(so[i + 1]) for i in range(0, len(so), 2)}
        # 장애물 맵(world x,y,radius). 리더가 보유·브로드캐스트(데이터 통신 정책: 리더가 정보 제공).
        # obstacles:=[] (lidar 단독, 정적맵 없음) 시 ROS2 가 빈 배열을 None 으로 주므로 순수
        # 파서(av.parse_triples)가 None/빈/ragged 를 안전 처리 → base_obstacles 가 비고 회피는
        # 전적으로 lidar 검출(/convoy/detected_obstacles)에 의존.
        self.obstacles = av.parse_triples(
            self.declare_parameter("obstacles", [8.0, 0.0, 0.5]).value
        )
        # 알려진 장애물(데모 spawn 위치) — lidar 미검출로 인한 누락/충돌 방지용 항상 유지.
        self.base_obstacles = list(self.obstacles)
        # 클리어런스는 완만 경로(waypoint)로 확보. override 는 휴면 비상망(react 0.1)
        # — 급조향이 Go2 전복 유발하므로 평시 미작동, 표면 0.6m 이내 드리프트 시에만.
        self.obs_react = self.declare_parameter("obs_react_m", 0.1).value
        self.obs_cone = self.declare_parameter("obs_cone_rad", 1.3).value
        # 회피 측면 안전거리(회전 방향, 로봇 중심~장애물 중심): [최소, 최대] 유지.
        self.lateral_min = self.declare_parameter("lateral_min_m", 1.3).value
        self.lateral_max = self.declare_parameter("lateral_max_m", 4.0).value
        # 추종 지연 보상: 최근접 순간 PID 가 측면 목표를 ~0.1~0.2 m 미달 → 명령 클리어런스
        # floor 를 lateral_min+margin 으로 올려(밴드 내) 실제 clearance 가 1.3 m 를 확실히
        # 넘게. 명령은 [lateral_min+margin, lateral_max] ⊂ [1.3,4] 유지.
        self.track_margin = self.declare_parameter("avoid_track_margin_m", 0.5).value
        # 충돌 예상 판정: 계획 경로 통과 y 가 장애물 중심에서 r+collision_margin 이내면 충돌
        # 예상 → 회피. 그보다 멀면 안전 → waypoint 추종(불필요한 회피 안 함).
        self.collision_margin = self.declare_parameter("collision_margin_m", 0.8).value
        # 전방 6m 이내 경로상 장애물은 반드시 회피(이 거리 안에 들어오면 타겟 측면 보정).
        self.obs_lookahead = self.declare_parameter("obs_lookahead_m", 6.0).value
        # 적응형 look-ahead: 기본 앞 wp_near 번째 waypoint 방향 curve; 회전이 너무 급하면 wp_far.
        self.wp_near = self.declare_parameter("wp_lookahead_near", 3).value
        self.wp_far = self.declare_parameter("wp_lookahead_far", 6).value
        self.sharp_yaw = self.declare_parameter("sharp_yaw_rad", 0.7).value
        # 선회 각도(헤딩 오차)에 따른 감속: yerr=turn_slow_rad 에서 turn_min_factor 까지 선형 감속.
        self.turn_slow_rad = self.declare_parameter("turn_slow_rad", 1.4).value
        # 4족 gait 는 선회 중에도 전진속도가 있어야 안정(제자리 선회=횡전복). floor 0.45 로
        # 충분한 전진 유지 + 완만한 wmax 로 넓은 호를 그려 회피(저RTF 전복 방지).
        self.turn_min_factor = self.declare_parameter("turn_min_factor", 0.45).value
        # 소프트스타트: 구동 시작 후 v 를 0→1 로 점증(저RTF CHAMP gait 가 첫 보행에서 급가속
        # 시 시작 직후 전복하던 stochastic 실패 완화). 기본 60틱=3s(20Hz).
        self.soft_start_ticks = self.declare_parameter("soft_start_ticks", 80).value
        # 추종자 nav2 참조경로(DCN-2026-029 P1): 리더 breadcrumb 궤적(이미 회피 완료한 안전
        # 차선)을 슬롯 오프셋해 /convoy/ref_path/r{n} 로 발행. slot_gap = 슬롯 간 along-path
        # 간격(UGV gap_m 와 정합 3.0). trail_step = 궤적 누적 최소 이동(코너 보존·중복 억제).
        self.slot_gap = self.declare_parameter("slot_gap_m", 3.0).value
        self.trail_step = self.declare_parameter("trail_step_m", 0.12).value
        self.trail = deque(maxlen=3000)  # 리더 breadcrumb (오래된→최신, 끝=리더 head)
        self.drive_ticks = 0
        self.leader = None
        self.lvel = (0.0, 0.0)
        self._llast = None
        self.ugv = {}
        self.last_w = 0.0
        self.wp_idx = 1
        self.throttle = 1.0
        self.last_mode = (
            "FOLLOW"  # 평가용: 현재 모드(FOLLOW=waypoint 추종 / AVOID=회피)
        )
        self.last_target = None  # 평가용: 코디네이터가 조준하는 회피 계획 타겟
        self.create_subscription(
            Odometry, "/odom_gt", self.on_leader, qos_profile_sensor_data
        )
        for n in CHAIN:
            self.create_subscription(
                Odometry,
                f"/convoy/report/r{n}",
                lambda m, nn=n: self.on_report(m, nn),
                10,
            )
        # Go2 lidar costmap 이 검출한 장애물(인지 기반) — 정적 param 을 대체.
        self.create_subscription(
            PoseArray, "/convoy/detected_obstacles", self.on_detected, 10
        )
        self.cmd = self.create_publisher(Twist, "/cmd_vel", 10)
        self.tpub = {
            n: self.create_publisher(Odometry, f"/convoy/target/r{n}", 10)
            for n in CHAIN
        }
        self.path_pub = self.create_publisher(Path, "/convoy/plan", 1)
        # 추종자별 nav2 참조경로(DCN-2026-029 P1) — 리더 차선의 슬롯점부터 head 까지.
        self.ref_pub = {
            n: self.create_publisher(Path, f"/convoy/ref_path/r{n}", 1) for n in CHAIN
        }
        self.obs_pub = self.create_publisher(PoseArray, "/convoy/obstacles", 10)
        # POC 스펙: 리더→전로봇 경로/타겟 제공 주기 0.2 s(5 Hz). 파라미터화로 domain_bridge
        # 부하시험 시 가변 가능. DCN delta — convoy 위 통신율 2 Hz→5 Hz 상향(병렬 구현 아님).
        comm_period = self.declare_parameter("comm_period_s", 0.2).value
        self.create_timer(0.05, self.drive)  # 20Hz leader control
        self.create_timer(comm_period, self.broker)  # 5Hz(0.2s) Leader->UGV
        self.create_timer(1.0, self.pub_path)
        self.create_timer(2.0, self.diag)
        self.get_logger().info(
            f"Coordinator UP: {len(self.wps)} waypoints, vmax={self.vmax:.2f}"
        )

    def on_leader(self, m):
        x, y = m.pose.pose.position.x, m.pose.pose.position.y
        yaw = yaw_of(m.pose.pose.orientation)
        t = time.monotonic()
        if self._llast is not None:
            dt = t - self._llast[2]
            if dt > 1e-2:
                a = 0.4
                self.lvel = (
                    a * (x - self._llast[0]) / dt + (1 - a) * self.lvel[0],
                    a * (y - self._llast[1]) / dt + (1 - a) * self.lvel[1],
                )
        self._llast = (x, y, t)
        self.leader = (x, y, yaw)
        # breadcrumb 누적(추종자 참조경로 차선). 최소 이동(trail_step) 이상일 때만 추가 →
        # 정지 시 중복 점 억제, 코너 형상 보존.
        if (
            not self.trail
            or math.hypot(x - self.trail[-1][0], y - self.trail[-1][1])
            > self.trail_step
        ):
            self.trail.append((x, y))

    def on_report(self, m, n):
        x, y = m.pose.pose.position.x, m.pose.pose.position.y
        yaw = yaw_of(m.pose.pose.orientation)
        t = time.monotonic()
        prev = self.ugv.get(n)
        vx = vy = 0.0
        if prev is not None:
            dt = t - prev["t"]
            if dt > 1e-2:
                vx, vy = (x - prev["x"]) / dt, (y - prev["y"]) / dt
        self.ugv[n] = dict(x=x, y=y, yaw=yaw, vx=vx, vy=vy, t=t)

    def on_detected(self, m):
        # 알려진(param) 장애물은 항상 유지 + lidar 신규 검출(중복 제외)만 추가. lidar 가 알려진
        # 장애물을 일시 놓쳐도(미검출) 누락→충돌이 없도록. costmap 은 인지/시각화 + 신규 발견.
        merged = list(self.base_obstacles)
        for p in m.poses:
            dx, dy, dr = p.position.x, p.position.y, max(0.3, p.position.z)
            if all(math.hypot(dx - ox, dy - oy) > 1.5 for ox, oy, _ in merged):
                merged.append((dx, dy, dr))
        self.obstacles = merged

    def _front_obstacle(self, lx, ly):
        # 순수 로직 위임(convoy_avoidance) — 전방 충돌예상 장애물 중 최근접 (ox,oy,r,dist)|None.
        return av.front_obstacle(
            self.obstacles, self.wps, lx, ly, self.obs_lookahead, self.collision_margin
        )

    def _avoid_side(self, ox, oy):
        # 순수 로직 위임 — 결정론적 회피 측면(±1, 경로 국소형상). UGV oside 브로드캐스트와 동일.
        return av.avoid_side(self.wps, ox, oy)

    def _avoid_target(self, lx, ly):
        # 순수 로직 위임 — 충돌 시 [lateral_min+margin, lateral_max] 측면거리 우회 타겟,
        # 안전 시 앞 wp_near 번째 waypoint.
        return av.avoid_target(
            self.obstacles,
            self.wps,
            lx,
            ly,
            self.wp_idx,
            self.wp_near,
            self.obs_lookahead,
            self.collision_margin,
            self.lateral_min,
            self.lateral_max,
            self.track_margin,
        )

    def _obstacle_on_path_ahead(self, lx, ly):
        # 순수 로직 위임 — 전방 충돌예상 장애물 유무.
        return av.obstacle_on_path_ahead(
            self.obstacles, self.wps, lx, ly, self.obs_lookahead, self.collision_margin
        )

    def drive(self):
        cmd = Twist()
        if self.leader is None:
            self.cmd.publish(cmd)
            return
        self.drive_ticks += 1  # 소프트스타트 램프용(구동 시작 후 경과 틱)
        lx, ly, lyaw = self.leader
        # 전진 판정: along-track(x 진행)이 waypoint 를 지나거나 근접하면 advance. 회피로 측면
        # 이탈(off-path)해도 x 가 진행하면 전진 → 회피 중 wp 정체 방지(경로 x-단조 가정).
        while self.wp_idx < len(self.wps) - 1:
            wx, wy = self.wps[self.wp_idx]
            if lx >= wx - self.goal_tol or math.hypot(wx - lx, wy - ly) < self.goal_tol:
                self.wp_idx += 1
            else:
                break
        # 평시엔 다음 waypoint 추종. 전방 6m 이내 경로상 장애물이 있으면 → 앞 3번째 waypoint
        # 방향으로 선회 + 그 방향 측면을 장애물과 [lateral_min, lateral_max] 안전거리로 우회
        # (3번째 방향이 너무 급하면 6번째로 더 완만하게).
        if self._obstacle_on_path_ahead(lx, ly):
            self.last_mode = "AVOID"
            tx, ty = self._avoid_target(lx, ly)
            des = math.atan2(ty - ly, tx - lx)
            yerr = math.atan2(math.sin(des - lyaw), math.cos(des - lyaw))
        else:
            self.last_mode = "FOLLOW"
            tx, ty = self.wps[self.wp_idx]
            des = math.atan2(ty - ly, tx - lx)
            yerr = math.atan2(math.sin(des - lyaw), math.cos(des - lyaw))
        self.last_target = (tx, ty)
        dist = math.hypot(tx - lx, ty - ly)
        # ★선회 각도(헤딩 오차)에 따라 감속 — 급선회일수록 느리게(저RTF 비틀거림/전복 방지).
        v = self.vmax * max(self.turn_min_factor, 1.0 - abs(yerr) / self.turn_slow_rad)
        self.throttle = 1.0
        if 3 in self.ugv:  # 첫 UGV 간격 반영(데드락 방지 floor 0.5)
            gap = math.hypot(lx - self.ugv[3]["x"], ly - self.ugv[3]["y"])
            if gap > self.match_gap:
                self.throttle = max(0.5, 1.0 - self.match_k * (gap - self.match_gap))
                v *= self.throttle
        if self.wp_idx >= len(self.wps) - 1 and dist < self.goal_tol:
            v = 0.0
        w = max(-self.wmax, min(self.wmax, self.kp_ang * yerr))
        v, w = self.obs_override(lx, ly, lyaw, v, w)  # 장애물 회피 최우선
        mx = self.ang_slew * 0.05
        w = max(self.last_w - mx, min(self.last_w + mx, w))  # 슬루(전복 방지)
        # 소프트스타트: 시작 후 soft_start_ticks 동안 v·w 를 0→1 선형 램프. v 만 램프하면 시작
        # 시 v 미세한데 w 가 커져 '제자리 선회'→4족 횡전복하던 문제 → w 도 함께 램프(전진 우선).
        if self.drive_ticks < self.soft_start_ticks:
            ramp = self.drive_ticks / self.soft_start_ticks
            v *= ramp
            w *= ramp
        self.last_w = w
        cmd.linear.x = max(0.0, v)
        cmd.angular.z = w
        self.cmd.publish(cmd)

    def obs_override(self, x, y, yaw, v, w):
        # 장애물 회피(최우선): 전방 cone 내 가장 가까운 장애물 반대로 '순수 조향'.
        # 속도는 waypoint 제어 그대로 유지 → 리더가 장애물에 걸려 멈추는 일이 없음.
        worst = None
        for ox, oy, r in self.obstacles:
            surf = math.hypot(ox - x, oy - y) - r
            if surf > r + self.obs_react:
                continue
            bearing = wrap(math.atan2(oy - y, ox - x) - yaw)
            if abs(bearing) > self.obs_cone:
                continue
            if worst is None or surf < worst[0]:
                worst = (surf, bearing, r)
        if worst is None:
            return v, w
        surf, bearing, r = worst
        gain = max(
            0.0, min(1.0, (r + self.obs_react - surf) / self.obs_react)
        )  # 가까울수록 1
        # 백스톱: 평시엔 매끄러운 [1.3,4] 회피로 미발동(중심 1.1m 내 진입 없음). 발동해도
        # 급선회 전복 않도록 강도 축소(저RTF gait). 최대 0.6*wmax.
        mag = self.wmax * (0.2 + 0.4 * gain)
        turn = -math.copysign(mag, bearing if abs(bearing) > 0.05 else 1.0)
        return v, turn  # v 불변(stuck 방지)

    def path_y_at(self, x):
        # 순수 로직 위임 — 계획경로(waypoints) x 위치 y 보간.
        return av.path_y_at(self.wps, x)

    def pub_obstacles(self):
        pa = PoseArray()
        pa.header.frame_id = "odom"
        pa.header.stamp = self.get_clock().now().to_msg()
        for ox, oy, r in self.obstacles:
            p = Pose()
            p.position.x = ox
            p.position.y = oy
            p.position.z = r
            # 회피 측면을 리더와 동일한 결정론적 _avoid_side 로 제공 → UGV 가 리더와 같은 쪽으로
            # 우회(종대 일관). (옛 path_y_at>=oy 규칙은 리더 _avoid_side 와 어긋나 첫 UGV 가
            # 반대쪽으로 갈라져 추종 실패하던 버그 → 통일.)
            p.orientation.z = self._avoid_side(ox, oy)
            pa.poses.append(p)
        self.obs_pub.publish(pa)

    def pub_ref_paths(self):
        # 추종자 nav2 참조경로(DCN-2026-029 P1) @5Hz. 각 로봇 차선 = 리더 breadcrumb 궤적의
        # 슬롯점(head 뒤 SLOT[n]*slot_gap 호장)부터 head 까지. nav2 FollowPath 의 global plan
        # 입력. 궤적 미형성(2점 미만) 시 발행 생략. /convoy/target/r{n} 은 backward-compat 유지.
        if len(self.trail) < 2:
            return
        trail = list(self.trail)
        stamp = self.get_clock().now().to_msg()
        for n in CHAIN:
            pts = av.ref_path_from_trail(trail, SLOT[n] * self.slot_gap)
            path = Path()
            path.header.frame_id = "odom"
            path.header.stamp = stamp
            for px, py in pts:
                ps = PoseStamped()
                ps.header.frame_id = "odom"
                ps.pose.position.x = px
                ps.pose.position.y = py
                ps.pose.orientation.w = 1.0
                path.poses.append(ps)
            self.ref_pub[n].publish(path)

    def broker(self):
        self.pub_obstacles()  # 장애물 맵 @5Hz 브로드캐스트(Leader→UGV)
        self.pub_ref_paths()  # 추종자별 nav2 참조경로 @5Hz(DCN-2026-029 P1)
        for n in CHAIN:
            p = PRED[n]
            if p == 0:
                if self.leader is None:
                    continue
                x, y, yaw = self.leader
                vx, vy = self.lvel
            else:
                if p not in self.ugv:
                    continue
                u = self.ugv[p]
                x, y, yaw, vx, vy = u["x"], u["y"], u["yaw"], u["vx"], u["vy"]
            o = Odometry()
            o.header.frame_id = "odom"
            o.header.stamp = self.get_clock().now().to_msg()
            o.pose.pose.position.x = x  # 앞선 로봇 현재 위치
            o.pose.pose.position.y = y
            o.pose.pose.orientation.z = math.sin(yaw / 2.0)
            o.pose.pose.orientation.w = math.cos(yaw / 2.0)
            o.twist.twist.linear.x = vx  # 속도(UGV 가 1초 후 목표 산출)
            o.twist.twist.linear.y = vy
            self.tpub[n].publish(o)

    def pub_path(self):
        path = Path()
        path.header.frame_id = "odom"
        path.header.stamp = self.get_clock().now().to_msg()
        for x, y in self.wps:
            ps = PoseStamped()
            ps.header.frame_id = "odom"
            ps.pose.position.x = x
            ps.pose.position.y = y
            ps.pose.orientation.w = 1.0
            path.poses.append(ps)
        self.path_pub.publish(path)

    def diag(self):
        if self.leader is None:
            self.get_logger().info("waiting /odom_gt ...")
            return
        g3 = "-"
        if 3 in self.ugv:
            g3 = "{:.2f}".format(
                math.hypot(
                    self.leader[0] - self.ugv[3]["x"], self.leader[1] - self.ugv[3]["y"]
                )
            )
        tgt = self.last_target if self.last_target is not None else (0.0, 0.0)
        self.get_logger().info(
            f"leader=({self.leader[0]:.2f},{self.leader[1]:.2f}) "
            f"wp={self.wp_idx}/{len(self.wps) - 1} reports={len(self.ugv)} "
            f"gap3={g3} throttle={self.throttle:.2f} "
            f"mode={self.last_mode} target=({tgt[0]:.2f},{tgt[1]:.2f}) "
            f"trail={len(self.trail)}"
        )


def main():
    rclpy.init()
    rclpy.spin(Coordinator())
    rclpy.shutdown()


if __name__ == "__main__":
    main()
