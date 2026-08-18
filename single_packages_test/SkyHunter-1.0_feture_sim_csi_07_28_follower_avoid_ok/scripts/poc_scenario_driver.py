#!/usr/bin/env python3
"""POC stop-and-go 시나리오 드라이버 (SkyHunter-1.0 feat/poc-mission-on-csi 포팅).

활주로 월드(poc_runway_world.sdf, 동쪽 +x 주행)에서 단계별 미션:
    LINE(0→40m) → COLUMN(→230m, 장애물 x[80,230] 관통) → ENEMY_STOP(→250m)
    → ASSAULT 160° 원호(→320m)
매 단계 = STOP → 대형 변경 → 리더 현재 위치 앞에서 새 경로 LOAD → START.
정지 상태에서만 대형을 바꾸고(검증된 메커니즘), 경로는 항상 로봇 앞에서 시작
(뒤로 끌림 없음).

combatrobot_1 적응:
  · 기본 --via direct = executor 직접입력 /sN/mission/path_command (sim FSM 은
    LOAD→START 로 IDLE 을 못 벗어나는 문제가 있어 우회. 실보드 체인은 FSM 경유).
  · 대형은 /sN/mission/control_command 로 5Hz 연속 assert — FSM 이 IDLE 에서
    formation_type=NONE 을 덮어쓰므로 one-shot 은 유지 안 됨(formation_test.py 와 동일).
  · 리더 진행도 = /s1/odometry/global (EKF map 프레임; datum=리더 스폰(0,0)이라
    map x == 월드 x). gz DiffDrive /odom 은 드리프트가 있어 쓰지 않음.

실행(사전: swarm_sim.launch.py world:=poc_runway_world.sdf path_axis:=east 기동):
  source ros/install/setup.bash; export ROS_DOMAIN_ID=96
  python3 scripts/poc_scenario_driver.py --robots 5
"""
import argparse
import json
import threading
import time

import rclpy
from combat_robot_msgs.msg import SwarmControlCommand, SwarmPathCommand
from nav_msgs.msg import Odometry
from rclpy.node import Node
from rclpy.qos import (QoSDurabilityPolicy, QoSHistoryPolicy, QoSProfile,
                       QoSReliabilityPolicy)

RELIABLE_TL = QoSProfile(history=QoSHistoryPolicy.KEEP_LAST, depth=10,
                         reliability=QoSReliabilityPolicy.RELIABLE,
                         durability=QoSDurabilityPolicy.TRANSIENT_LOCAL)

DATUM_LAT = 36.61002559225        # poc_runway_world.sdf spherical_coordinates 와 동일
DATUM_LON = 127.28772570583
import math
MLON = 1.0 / (111320.0 * math.cos(math.radians(DATUM_LAT)))   # 1m 동쪽당 deg lon

# SwarmControlCommand.formation_type(작전모드) → geometryForMode:
#   0 NONE→ABREAST(LINE) / 1 RECON→COLUMN / 2 PROTECT→DIAMOND / 3 ASSAULT→WEDGE
#   / 4 ARC(POC 돌격 원호)
# 단계: (이름, formation, end_x, 정지 유지 s) — 리더 x ≥ end_x 까지 해당 대형 주행
STAGES = [
    ("LINE",       0,  200.0, 3.0),
    ("COLUMN",     1,  220.0, 3.0),   # 장애물 구간 관통(회피는 nav2 가 담당)
    ("ENEMY_STOP", 1,  240.0, 5.0),   # 장애물 지나 평지에서 정지(적 발견 모사)
    ("ASSAULT",    4,  260.0, 3.0),   # 평지에서 원호 전개
]
MARGIN = 15.0    # 단계 끝점보다 여유 있게 LOAD(목표가 경계 위에 오지 않게)
# ★mid-drive 대형전환(rolling merge): 설정되면 LINE 으로 START 후 x>=MIDCHANGE_X 에서 정지
#   없이 target 대형으로 전환. --formation 시 main 에서 세팅.
MIDCHANGE_X = None
MIDCHANGE_MODE = None


class FormationAsserter:
    """대형을 5Hz 연속 발행(백그라운드). FSM 의 IDLE NONE 덮어쓰기 대응."""

    def __init__(self, rids, via):
        self.node = rclpy.create_node("poc_formation_assert")
        q = QoSProfile(depth=10, reliability=QoSReliabilityPolicy.RELIABLE)
        topic = ("mission/control_command" if via == "direct"
                 else "swarm/control_command")
        self.pubs = [self.node.create_publisher(
            SwarmControlCommand, f"/s{r}/{topic}", q) for r in rids]
        self.mode = 0
        self._stop = threading.Event()
        self._t = threading.Thread(target=self._run, daemon=True)
        self._t.start()

    def _run(self):
        while not self._stop.is_set():
            m = SwarmControlCommand()
            m.formation_type = self.mode
            m.formation_number = 0
            m.header.stamp = self.node.get_clock().now().to_msg()
            for p in self.pubs:
                p.publish(m)
            time.sleep(0.2)

    def stop(self):
        self._stop.set()


class StopGoDriver(Node):
    def __init__(self, n_robots, via):
        super().__init__("poc_scenario_driver")
        self.rids = list(range(1, n_robots + 1))
        path_topic = ("mission/path_command" if via == "direct"
                      else "swarm/path_command")
        self.path_pubs = [self.create_publisher(
            SwarmPathCommand, f"/s{r}/{path_topic}", RELIABLE_TL) for r in self.rids]
        self.asserter = FormationAsserter(self.rids, via)
        self.leader_x = None
        self.seg = 0
        self.phase = "WAIT_POSE"
        self.t0 = time.time()
        self.create_subscription(Odometry, "/s1/odometry/global", self._odom, 10)
        self.create_timer(0.2, self._tick)     # 5Hz 상태기계
        self.get_logger().info(
            f"stop-and-go driver up — {n_robots} robots, via={via}")

    def _odom(self, m):
        self.leader_x = m.pose.pose.position.x

    def _set_phase(self, p):
        self.phase = p
        self.t0 = time.time()

    def _path_cmd(self, cmd, num=0, path_json=""):
        m = SwarmPathCommand()
        m.header.stamp = self.get_clock().now().to_msg()
        m.command = cmd
        m.num_waypoints = num
        m.path_json = path_json
        for pub in self.path_pubs:
            for _ in range(3):
                pub.publish(m)

    def _load_from(self, x_start, x_end):
        xs = [x_start + 3.0, (x_start + x_end) / 2.0, x_end]
        wps = [{"lat": DATUM_LAT, "lon": round(DATUM_LON + xx * MLON, 7)}
               for xx in xs]
        self._path_cmd(5, 3, json.dumps({"waypoints": wps}))   # 5 = LOAD_PATH

    def _tick(self):
        name, form, end_x, hold_s = STAGES[self.seg]
        e = time.time() - self.t0
        if self.phase == "WAIT_POSE":
            if self.leader_x is not None:
                self.get_logger().info(f"leader pose ok (x={self.leader_x:.1f})")
                self._set_phase("WAIT_SUBS")
        elif self.phase == "WAIT_SUBS":
            # ★필수: 전 로봇 executor 가 path_command 구독을 매칭할 때까지 대기.
            # executor 구독은 volatile 이라 transient_local 히스토리를 못 받는다 —
            # 매칭 전에 발행한 LOAD/START 는 조용히 유실(과부하 sim/실보드 discovery
            # 지연에서 실측). mission_local.py 와 동일한 subs>0 대기 패턴.
            unmatched = [i + 1 for i, p in enumerate(self.path_pubs)
                         if p.get_subscription_count() < 1]
            if not unmatched:
                self.get_logger().info("전 로봇 path_command 구독 매칭 완료")
                self._path_cmd(2)          # STOP: 이전 실행 잔여 상태 리셋
                self._set_phase("INIT")
            elif e > 120.0:
                self.get_logger().error(f"s{unmatched} 구독 매칭 120s 초과 — 중단")
                self._set_phase("DONE")
            elif int(e) % 5 == 0 and e - int(e) < 0.2:
                self.get_logger().info(f"구독 매칭 대기: s{unmatched}")
        elif self.phase == "INIT":
            # ★mid-drive 대형전환(--formation): LINE 으로 START 후 주행 중 target 대형으로 무정지
            #   전환(rolling merge). 스테이지 사이 STOP 이 없어 리더가 계속 주행 → 팔로워 후진 없음.
            start_form = 0 if MIDCHANGE_X is not None else form
            self.get_logger().info(f"[{name}] form={start_form} + LOAD 0->{end_x + MARGIN:.0f}")
            self.asserter.mode = start_form
            self._load_from(max(self.leader_x, 0.0), end_x + MARGIN)
            self._set_phase("START_WAIT")
        elif self.phase == "START_WAIT" and e > 3.0:
            self._path_cmd(1)                                   # 1 = START
            self.get_logger().info(f"[{name}] START — x={end_x:.0f} 까지 주행")
            self._set_phase("DRIVING")
        elif self.phase == "DRIVING":
            if int(e) % 10 == 0 and e - int(e) < 0.2:
                self.get_logger().info(f"[{name}] x={self.leader_x:.1f}/{end_x:.0f}")
            # ★주행 중 무정지 대형전환(rolling merge): x>=MIDCHANGE_X 에서 asserter 모드만 바꿈.
            if (MIDCHANGE_X is not None and self.leader_x >= MIDCHANGE_X
                    and self.asserter.mode != MIDCHANGE_MODE):
                self.asserter.mode = MIDCHANGE_MODE
                self.get_logger().info(
                    f"[{name}] 주행 중 대형전환 form={MIDCHANGE_MODE} @x={self.leader_x:.1f} "
                    f"(정지 없음, rolling merge)")
            if self.leader_x >= end_x:
                self.get_logger().info(f"[{name}] 도달 x={self.leader_x:.1f} -> STOP")
                self._path_cmd(2)                               # 2 = STOP
                self._set_phase("STOP_WAIT")
        elif self.phase == "STOP_WAIT" and e > hold_s:
            if self.seg + 1 >= len(STAGES):
                self.get_logger().info("MISSION COMPLETE — 전 단계 완료")
                self._set_phase("DONE")
                return
            self.seg += 1
            nm, nf, nx, _ = STAGES[self.seg]
            self.get_logger().info(
                f"[{nm}] 재편성(form={nf}) + LOAD {self.leader_x:.0f}->{nx + MARGIN:.0f}")
            self.asserter.mode = nf
            self._load_from(self.leader_x, nx + MARGIN)
            self._set_phase("START_WAIT")


def main():
    global STAGES, MIDCHANGE_X, MIDCHANGE_MODE
    ap = argparse.ArgumentParser()
    ap.add_argument("--robots", type=int, default=5)
    ap.add_argument("--via", choices=["direct", "fsm"], default="direct",
                    help="direct=executor 직접입력(sim 기본) / fsm=FSM 경유(실보드 체인)")
    # ★벽 회피 재현(poc_wall_world): 한 대형으로 LINE 정렬 후 그 대형으로 벽까지 주행.
    #   조기종대(obs_convoy)+IDM 검증용. 미지정(poc)이면 상단 기본 STAGES(활주로 4단계).
    ap.add_argument("--formation",
                    choices=["column", "diamond", "wedge", "line", "arc", "poc"],
                    default="poc",
                    help="벽 시나리오 대형(column/diamond/wedge/line/arc) 또는 poc(활주로 4단계)")
    ap.add_argument("--dist", type=float, default=160.0,
                    help="벽 시나리오 주행 종료 x(m)")
    a = ap.parse_args()
    _mode = {"line": 0, "column": 1, "diamond": 2, "wedge": 3, "arc": 4}
    if a.formation != "poc":
        # ★단일 연속 주행: LINE 으로 START → x=15 에서 정지 없이 target 대형으로 전환(rolling
        #   merge) → dist 까지 주행(벽 x=100 통과). 스테이지 사이 STOP 없음 = 리더 계속 주행.
        STAGES = [(a.formation.upper(), _mode[a.formation], a.dist, 3.0)]
        MIDCHANGE_X = 15.0
        MIDCHANGE_MODE = _mode[a.formation]
    rclpy.init()
    node = StopGoDriver(a.robots, a.via)
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.asserter.stop()
        node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":
    main()
