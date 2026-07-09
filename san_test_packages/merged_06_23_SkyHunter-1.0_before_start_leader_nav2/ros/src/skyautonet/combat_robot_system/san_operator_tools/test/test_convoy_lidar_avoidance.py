# Copyright 2026 SkyAutoNet Inc.
#
# Proprietary and confidential. Unauthorized copying, distribution, or use
# of this file, via any medium, is strictly prohibited.

"""Lidar 장애물 회피 통합(기능) + 성능 검증 (PR #279 follow-up).

Go2 4D lidar 회피 파이프라인을 rclpy 없이 end-to-end 로 검증한다. 사슬:

    PointCloud2 → [인지] project_point → world_to_cell → (hits 누적/감쇠/임계)
                → cluster_obstacles → /convoy/detected_obstacles
                → [병합] coordinator.on_detected (중복 1.5m 제외)
                → [회피] front_obstacle → avoid_target

`_CostmapSim` / `_merge_detected` 는 각각 convoy_costmap 노드(on_cloud/publish)와
convoy_coordinator.on_detected 의 ROS 글루를 그대로 거울 반영한다(순수 함수 위임부는
실제 모듈을 호출). 파라미터/격자는 convoy_costmap·convoy_coordinator·convoy_demo
기본값과 동일.

기능 검증:
  - 경로상 lidar 장애물 → 검출 위치 정확 + 회피 타겟이 측면 ≥ lateral_min 확보
  - 경로 비껴난 장애물 → 검출되더라도 회피 미발동(불필요 회피 없음)
  - 후방(뒤따르는 UGV) / 지면·머리위(높이밴드 밖) 점 → 오검출 0 (전방·z 게이트)
  - 단일 프레임 검출의 decay 지속/소멸(이동 장애물 응답성)

성능 검증:
  - 조밀 클라우드(지면·원거리 노이즈 혼합)를 1.25Hz publish 주기(0.8s) 안에 처리
  - 처리량(points/s) 측정·리포트, 고부하에서도 검출 정확도 불변

standalone pytest 러너가 자동 수집(rclpy 미import).
"""

import struct
import time
from types import SimpleNamespace

import pytest
from san_operator_tools.convoy_avoidance import (
    avoid_target,
    front_obstacle,
    obstacle_on_path_ahead,
)
from san_operator_tools.convoy_perception import (
    cluster_obstacles,
    project_point,
    read_xyz,
    world_to_cell,
)

# ── convoy_costmap 격자/게이트 기본값 ──────────────────────────────────────
RES = 0.25
XMIN, XMAX, YMIN, YMAX = -6.0, 45.0, -8.0, 8.0
NX = int((XMAX - XMIN) / RES)  # 204
NY = int((YMAX - YMIN) / RES)  # 64
ZLO, ZHI = 0.15, 1.8
FWD_MIN, MAX_RANGE = 0.8, 12.0
OCC_THRESH, DECAY = 0.7, 0.9
LEADER_Z = 0.3
IDENT = ((1.0, 0.0, 0.0), (0.0, 1.0, 0.0), (0.0, 0.0, 1.0))
MT0 = (0.0, 0.0, 0.0)

# ── convoy_coordinator 회피 기본값 ─────────────────────────────────────────
WPS = [
    (0.0, 0.0),
    (4.0, 1.0),
    (8.0, 2.0),
    (12.0, 2.5),
    (16.0, 2.0),
    (20.0, 1.0),
    (24.0, 0.0),
    (28.0, -1.0),
    (32.0, -1.5),
    (36.0, -1.0),
    (40.0, 0.0),
]
LOOKAHEAD = 6.0
COLLISION_MARGIN = 0.8
LAT_MIN, LAT_MAX = 1.3, 4.0
TRACK_MARGIN = 0.5
WP_NEAR = 3
DEDUP_M = 1.5  # on_detected 중복 제외 거리


# ── PointCloud2 빌더 ───────────────────────────────────────────────────────
def _cloud(points):
    # (x,y,z) float32 ×N → convoy_costmap 가 읽는 최소 PointCloud2 형태(point_step=12).
    data = b"".join(struct.pack("<fff", *p) for p in points)
    fields = [
        SimpleNamespace(name="x", offset=0),
        SimpleNamespace(name="y", offset=4),
        SimpleNamespace(name="z", offset=8),
    ]
    return SimpleNamespace(fields=fields, point_step=12, data=data)


def _frange(lo, hi, step):
    out, x, i = [], lo, 0
    while x <= hi + 1e-9:
        out.append(x)
        i += 1
        x = lo + i * step
    return out


def _obstacle_points_odom(ocx, ocy, leader, half=0.35, zc=0.6, step=0.12):
    # odom 의 장애물(중심 ocx,ocy, 반폭 half, 높이 zc) 를 채우는 lidar-프레임 점들.
    # 전제: IDENT 장착 + leader yaw=0 → lidar 점 = (odom - leader)(x,y), pz = zc - leader_z.
    lx, ly, lyaw = leader
    assert abs(lyaw) < 1e-9, "헬퍼는 yaw=0 리더 가정(인지 변환은 별도 단위테스트)"
    pts = []
    for wx in _frange(ocx - half, ocx + half, step):
        for wy in _frange(ocy - half, ocy + half, step):
            pts.append((wx - lx, wy - ly, zc - LEADER_Z))
    return pts


# ── convoy_costmap 노드 글루 거울(on_cloud + publish) ──────────────────────
class _CostmapSim:
    """convoy_costmap 의 ROS 글루를 거울 반영 — 순수 위임부는 실제 모듈 호출."""

    def __init__(self, mount_rot=IDENT, mount_t=MT0):
        self.hits = [0.0] * (NX * NY)
        self.mount_rot = mount_rot
        self.mount_t = mount_t
        self.leader_z = LEADER_Z

    def ingest(self, cloud_msg, leader):
        # convoy_costmap.on_cloud: 투영 → 전방/사거리 게이트 → 높이밴드 → 셀 누적(cap 10).
        for p in read_xyz(cloud_msg):
            w = project_point(
                p,
                self.mount_rot,
                self.mount_t,
                leader,
                self.leader_z,
                FWD_MIN,
                MAX_RANGE,
            )
            if w is None:
                continue
            wx, wy, wz = w
            if not (ZLO <= wz <= ZHI):
                continue
            j = world_to_cell(wx, wy, XMIN, YMIN, RES, NX, NY)
            if j is not None:
                self.hits[j] = min(self.hits[j] + 1.0, 10.0)

    def detect(self):
        # convoy_costmap.publish: 감쇠 → 임계 → 8-이웃 클러스터링 → (cx,cy,r) 리스트.
        self.hits = [h * DECAY for h in self.hits]
        occ = [c for c, h in enumerate(self.hits) if h >= OCC_THRESH]
        return cluster_obstacles(occ, NX, NY, RES, XMIN, YMIN)


def _merge_detected(base, detected):
    # convoy_coordinator.on_detected: 알려진 장애물 유지 + 신규(중복 1.5m 제외)만 추가.
    merged = list(base)
    for dx, dy, dr in detected:
        dr = max(0.3, dr)
        if all(
            ((dx - ox) ** 2 + (dy - oy) ** 2) ** 0.5 > DEDUP_M for ox, oy, _ in merged
        ):
            merged.append((dx, dy, dr))
    return merged


def _pipeline(cloud_pts, leader, base=None, cycles=1):
    # 전체 사슬 1회 실행: 클라우드 → 인지 → 병합 → (회피 입력) obstacles 리스트 반환.
    sim = _CostmapSim()
    detected = []
    for _ in range(cycles):
        sim.ingest(_cloud(cloud_pts), leader)
        detected = sim.detect()
    return _merge_detected(base or [], detected), detected


# ════════════════════════════════════════════════════════════════════════════
# 기능(통합) 테스트
# ════════════════════════════════════════════════════════════════════════════
def test_on_path_obstacle_detected_and_avoided():
    # 경로상 (8,2) lidar 장애물 → 검출(위치 정확) → 회피 타겟 측면 ≥ lateral_min.
    leader = (4.0, 1.0, 0.0)  # 경로 (4,1) 접근 중
    pts = _obstacle_points_odom(8.0, 2.0, leader)
    obstacles, detected = _pipeline(pts, leader, base=[])

    assert len(detected) == 1, f"단일 장애물 1개 클러스터 기대, got {detected}"
    dcx, dcy, drad = detected[0]
    assert dcx == pytest.approx(8.0, abs=0.3)
    assert dcy == pytest.approx(2.0, abs=0.3)
    assert drad >= 0.3

    # lidar 검출만으로 회피 발동 + 측면 안전거리 확보(정적맵 base 없음)
    assert obstacle_on_path_ahead(
        obstacles, WPS, leader[0], leader[1], LOOKAHEAD, COLLISION_MARGIN
    )
    tx, ty = avoid_target(
        obstacles,
        WPS,
        leader[0],
        leader[1],
        1,
        WP_NEAR,
        LOOKAHEAD,
        COLLISION_MARGIN,
        LAT_MIN,
        LAT_MAX,
        TRACK_MARGIN,
    )
    assert abs(ty - dcy) >= LAT_MIN, "회피 타겟이 장애물에서 lateral_min 미만"
    assert tx >= leader[0], "회피 타겟 x 가 리더 뒤 → 정체/후진 위험"


def test_off_path_obstacle_detected_but_not_avoided():
    # 경로 비껴난 (8,6) — 검출은 되지만(인지 정상) 충돌 예상 아님 → 회피 미발동.
    leader = (4.0, 1.0, 0.0)
    pts = _obstacle_points_odom(8.0, 6.0, leader)
    obstacles, detected = _pipeline(pts, leader, base=[])

    assert len(detected) == 1
    assert detected[0][1] == pytest.approx(6.0, abs=0.3)
    # 경로 y(8)=2.0, |2-6|=4 > r+margin → 안전 → 회피 안 함(불필요 회피 0)
    assert not obstacle_on_path_ahead(
        obstacles, WPS, leader[0], leader[1], LOOKAHEAD, COLLISION_MARGIN
    )
    assert (
        front_obstacle(
            obstacles, WPS, leader[0], leader[1], LOOKAHEAD, COLLISION_MARGIN
        )
        is None
    )


def test_rear_points_not_detected():
    # 뒤따르는 UGV 모사(리더 바로 뒤·옆) → 전방 게이트(bx<0.8) 로 오검출 0.
    leader = (4.0, 1.0, 0.0)
    rear = _obstacle_points_odom(4.2, 1.0, leader, half=0.3)  # px=0.2 < fwd_min
    _obstacles, detected = _pipeline(rear, leader, base=[])
    assert detected == [], f"후방 점이 장애물로 오검출됨: {detected}"


def test_ground_and_overhead_points_filtered():
    # 지면(z≈0) + 머리위(z>1.8) 점만 → 높이밴드 밖 → 검출 0(코스트맵 z 필터).
    leader = (4.0, 1.0, 0.0)
    ground = _obstacle_points_odom(8.0, 2.0, leader, zc=0.0)  # wz=0.0 < zlo
    overhead = _obstacle_points_odom(9.0, 2.0, leader, zc=2.3)  # wz=2.3 > zhi
    _obstacles, detected = _pipeline(ground + overhead, leader, base=[])
    assert detected == [], f"높이밴드 밖 점이 검출됨: {detected}"


def test_in_band_detected_when_mixed_with_ground():
    # 실제 상황: 지면·머리위 점이 섞여도 밴드 내(장애물 몸통) 점은 검출.
    leader = (4.0, 1.0, 0.0)
    body = _obstacle_points_odom(8.0, 2.0, leader, zc=0.6)
    ground = _obstacle_points_odom(8.0, 2.0, leader, zc=0.0)
    _obstacles, detected = _pipeline(body + ground, leader, base=[])
    assert len(detected) == 1
    assert detected[0][0] == pytest.approx(8.0, abs=0.3)


def test_noise_hit_decays_fast():
    # 단발 노이즈(고립 셀 1점) → decay 로 수 cycle 내 소멸(스퍼리어스 거부 응답성).
    # hits 1.0: 0.9, 0.81, 0.729 (≥0.7) → 0.656 (<0.7) 에서 소멸 = cycle 4.
    leader = (4.0, 1.0, 0.0)
    sim = _CostmapSim()
    sim.ingest(_cloud([(4.0, 1.0, 0.3)]), leader)  # odom (8,2,0.6) 단일점

    assert len(sim.detect()) == 1, "단일 hit 검출 실패"
    cleared_at = next((k for k in range(2, 10) if not sim.detect()), None)
    assert cleared_at is not None and cleared_at <= 5, (
        f"단발 노이즈 소멸이 느림(cycle {cleared_at})"
    )


def test_saturated_obstacle_eventually_clears():
    # 포화 관측(매 cycle 입력으로 cap 10 도달)된 실장애물도 입력 끊기면 결국 소멸 —
    # 영구 잔상(ghost) 없음. cap 10 → 10·0.9^n<0.7 → n≈26 (1.25Hz 에서 ~21s).
    leader = (4.0, 1.0, 0.0)
    pts = _obstacle_points_odom(8.0, 2.0, leader)
    sim = _CostmapSim()
    for _ in range(15):  # 충분히 관측 → cap 포화
        sim.ingest(_cloud(pts), leader)
        sim.detect()
    assert sim.detect(), "관측 중 검출 실패"

    # 입력 끊김 — 감쇠가 단조로 검출을 소멸시킴(잔류 영구화 없음)
    cleared_at = next((k for k in range(1, 40) if not sim.detect()), None)
    print(f"\n[behavior] saturated obstacle cleared after {cleared_at} idle cycles")
    assert cleared_at is not None, "포화 장애물이 영구 잔류(감쇠 미동작)"
    assert cleared_at <= 28, f"소멸 cycle {cleared_at} — cap 이론한계(26~27) 초과"


def test_sustained_obstacle_persists():
    # 장애물이 계속 보이면(매 cycle 입력) 검출 안정 유지(깜빡임 없음).
    leader = (4.0, 1.0, 0.0)
    pts = _obstacle_points_odom(8.0, 2.0, leader)
    sim = _CostmapSim()
    for _ in range(6):
        sim.ingest(_cloud(pts), leader)
        det = sim.detect()
        assert len(det) == 1, "지속 관측 중 검출 깜빡임 발생"


def test_two_separated_obstacles_distinct():
    # 떨어진 두 장애물 → 2 클러스터(병합 안 됨), 둘 다 보존.
    leader = (0.0, 0.0, 0.0)
    a = _obstacle_points_odom(6.0, 0.0, leader)
    b = _obstacle_points_odom(6.0, 5.0, leader)  # 5m 이격
    _obstacles, detected = _pipeline(a + b, leader, base=[])
    assert len(detected) == 2


def test_lidar_detection_dedup_against_known_obstacle():
    # 알려진(param) 장애물 근처 lidar 검출은 중복 제외(1.5m) → 이중 등록 안 함.
    leader = (4.0, 1.0, 0.0)
    pts = _obstacle_points_odom(8.0, 2.0, leader)
    base = [(8.1, 2.0, 0.5)]  # 알려진 장애물(검출과 ~0.1m)
    obstacles, detected = _pipeline(pts, leader, base=base)
    assert len(detected) == 1
    assert len(obstacles) == 1, "중복 장애물이 이중 등록됨"


# ════════════════════════════════════════════════════════════════════════════
# 성능 검증
# ════════════════════════════════════════════════════════════════════════════
def _dense_scene(leader, n_target):
    # 장애물(몸통) + 지면 노이즈 + 원거리(사거리 밖) 노이즈 혼합 ~n_target 점.
    lx, ly, _ = leader
    obstacle = _obstacle_points_odom(8.0, 2.0, leader, half=0.45, step=0.08)
    pts = list(obstacle)
    # 지면 노이즈(높이밴드 밖, 격자 채움 — 게이트 부하 가중)
    gx = _frange(2.0, 11.0, 0.25)
    gy = _frange(-4.0, 4.0, 0.25)
    for wx in gx:
        for wy in gy:
            pts.append((wx - lx, wy - ly, 0.0 - LEADER_Z))  # wz=0 → 거부
            if len(pts) >= n_target:
                return pts
    # 원거리 노이즈(range>max_range → 거부)
    while len(pts) < n_target:
        pts.append((20.0, float(len(pts) % 7), 0.4))
    return pts


@pytest.mark.parametrize("n_points", [2000, 8000])
def test_ingest_within_publish_budget(n_points):
    # 조밀 클라우드 1프레임 인지(투영+게이트+격자)가 1.25Hz publish 주기(0.8s) 안에 처리.
    leader = (4.0, 1.0, 0.0)
    scene = _dense_scene(leader, n_points)
    msg = _cloud(scene)
    sim = _CostmapSim()

    t0 = time.perf_counter()
    sim.ingest(msg, leader)
    elapsed = time.perf_counter() - t0

    pps = len(scene) / elapsed if elapsed > 0 else float("inf")
    print(
        f"\n[perf] ingest {len(scene)} pts in {elapsed * 1e3:.1f} ms ({pps:,.0f} pts/s)"
    )
    # publish 주기 0.8s — 인지는 그 절반(0.4s) 안에 끝나야 detect/publish 여유 확보.
    assert elapsed < 0.4, f"인지 {len(scene)}점이 {elapsed:.3f}s — 0.4s 예산 초과"
    # 고부하에서도 장애물 검출은 정확(성능≠정확도 희생)
    det = sim.detect()
    onpath = [d for d in det if abs(d[0] - 8.0) < 0.6 and abs(d[1] - 2.0) < 0.6]
    assert onpath, "고부하 시 경로상 장애물 검출 실패"


def test_detect_within_publish_budget():
    # 격자 전체(204×64) 감쇠 + 클러스터링(점유 다수)이 publish 주기 안에 처리.
    leader = (4.0, 1.0, 0.0)
    sim = _CostmapSim()
    # 넓은 점유 영역 누적(클러스터링 부하 가중)
    wall = []
    for wx in _frange(6.0, 11.0, 0.1):
        for wy in _frange(-3.0, 3.0, 0.1):
            wall.append((wx - leader[0], wy - leader[1], 0.6 - LEADER_Z))
    sim.ingest(_cloud(wall), leader)

    t0 = time.perf_counter()
    det = sim.detect()
    elapsed = time.perf_counter() - t0
    print(f"\n[perf] detect ({len(det)} clusters) in {elapsed * 1e3:.1f} ms")
    assert elapsed < 0.4, f"검출(감쇠+클러스터)이 {elapsed:.3f}s — 0.4s 예산 초과"


def test_full_cycle_within_budget():
    # 인지+검출 1 cycle 합산이 1.25Hz publish 주기(0.8s) 안에.
    leader = (4.0, 1.0, 0.0)
    scene = _dense_scene(leader, 8000)
    msg = _cloud(scene)
    sim = _CostmapSim()

    t0 = time.perf_counter()
    sim.ingest(msg, leader)
    sim.detect()
    elapsed = time.perf_counter() - t0
    print(f"\n[perf] full cycle (8000 pts) in {elapsed * 1e3:.1f} ms")
    assert elapsed < 0.8, f"full cycle {elapsed:.3f}s — publish 주기(0.8s) 초과"
