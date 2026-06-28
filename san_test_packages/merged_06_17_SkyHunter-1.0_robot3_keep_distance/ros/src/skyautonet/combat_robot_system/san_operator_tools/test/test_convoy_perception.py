# Copyright 2026 SkyAutoNet Inc.
#
# Proprietary and confidential. Unauthorized copying, distribution, or use
# of this file, via any medium, is strictly prohibited.

"""Convoy lidar 코스트맵 순수 로직 단위테스트.

convoy_perception 은 rclpy 무관 순수 함수 — Go2 4D lidar 점을 odom 격자로 투영하고
점유 셀을 클러스터링해 장애물(중심+반경)을 검출하는 인지 로직의 단일 소스. 검증 항목:
  - rpy_matrix: 영회전→단위행렬, 임의 장착회전의 직교성(R·Rᵀ=I, det=1)
  - yaw_of: 단위/축회전 쿼터니언 → yaw
  - read_xyz: PointCloud2 raw 바이트 → (x,y,z) 디코드 / 필드 누락 시 빈 출력
  - project_point: 장착·리더 변환 + 전방/사거리 게이트(뒤·근·원 거부)
  - world_to_cell / cell_center: 격자 인덱싱 왕복(범위 밖 None)
  - cluster_obstacles: 8-이웃 연결요소 → 중심/반경, 분리 클러스터 분할, 빈 입력

standalone pytest 러너가 자동 수집(rclpy 미import). convoy_costmap 파라미터 기본값과 동일.
"""

import math
import struct
from types import SimpleNamespace

import pytest
from san_operator_tools.convoy_perception import (
    cell_center,
    cluster_obstacles,
    project_point,
    read_xyz,
    rpy_matrix,
    world_to_cell,
    yaw_of,
)

IDENT = ((1.0, 0.0, 0.0), (0.0, 1.0, 0.0), (0.0, 0.0, 1.0))
# convoy_costmap 격자 기본값
XMIN, YMIN, RES, NX, NY = -6.0, -8.0, 0.25, 204, 64
# Go2 L1 lidar 실제 장착(URDF lidar_l1_joint)
MOUNT_RPY = [2.879, 0.0, 1.5705]


# ── rpy_matrix ────────────────────────────────────────────────────────────
def test_rpy_matrix_zero_is_identity():
    rot = rpy_matrix([0.0, 0.0, 0.0])
    for i in range(3):
        for j in range(3):
            assert rot[i][j] == pytest.approx(1.0 if i == j else 0.0, abs=1e-9)


def test_rpy_matrix_is_orthonormal():
    # 회전행렬은 직교(R·Rᵀ=I) + det=+1 — 실제 기울어진 장착각으로 검증
    rot = rpy_matrix(MOUNT_RPY)
    for i in range(3):
        for j in range(3):
            dot = sum(rot[i][k] * rot[j][k] for k in range(3))
            assert dot == pytest.approx(1.0 if i == j else 0.0, abs=1e-9)
    det = (
        rot[0][0] * (rot[1][1] * rot[2][2] - rot[1][2] * rot[2][1])
        - rot[0][1] * (rot[1][0] * rot[2][2] - rot[1][2] * rot[2][0])
        + rot[0][2] * (rot[1][0] * rot[2][1] - rot[1][1] * rot[2][0])
    )
    assert det == pytest.approx(1.0, abs=1e-9)


def test_rpy_matrix_yaw_90_rotates_x_to_y():
    # yaw=+90° → x축(1,0,0) 이 +y(0,1,0) 로
    rot = rpy_matrix([0.0, 0.0, math.pi / 2])
    vx = (rot[0][0], rot[1][0], rot[2][0])
    assert vx[0] == pytest.approx(0.0, abs=1e-9)
    assert vx[1] == pytest.approx(1.0, abs=1e-9)


# ── yaw_of ────────────────────────────────────────────────────────────────
def test_yaw_of_identity_is_zero():
    assert yaw_of(SimpleNamespace(x=0.0, y=0.0, z=0.0, w=1.0)) == pytest.approx(0.0)


def test_yaw_of_90deg_about_z():
    h = math.sqrt(0.5)  # z회전 90° 쿼터니언 (0,0,sin45,cos45)
    assert yaw_of(SimpleNamespace(x=0.0, y=0.0, z=h, w=h)) == pytest.approx(math.pi / 2)


# ── read_xyz ──────────────────────────────────────────────────────────────
def _cloud(points, step=12):
    data = b"".join(struct.pack("<fff", *p) for p in points)
    fields = [
        SimpleNamespace(name="x", offset=0),
        SimpleNamespace(name="y", offset=4),
        SimpleNamespace(name="z", offset=8),
    ]
    return SimpleNamespace(fields=fields, point_step=step, data=data)


def test_read_xyz_decodes_points():
    pts = [(1.0, 2.0, 3.0), (-1.5, 0.0, 0.25)]
    out = list(read_xyz(_cloud(pts)))
    assert len(out) == 2
    for got, exp in zip(out, pts):
        assert got == pytest.approx(exp)


def test_read_xyz_missing_field_yields_nothing():
    msg = _cloud([(1.0, 2.0, 3.0)])
    msg.fields = [f for f in msg.fields if f.name != "z"]  # z 누락
    assert list(read_xyz(msg)) == []


# ── project_point ─────────────────────────────────────────────────────────
LEADER0 = (0.0, 0.0, 0.0)  # 원점, yaw=0
MT0 = (0.0, 0.0, 0.0)
LZ = 0.3
FWD_MIN, MAX_RANGE = 0.8, 12.0


def test_project_point_ahead_passes():
    # 전방 2 m 점 → odom (2,0, leader_z+0.5)
    w = project_point((2.0, 0.0, 0.5), IDENT, MT0, LEADER0, LZ, FWD_MIN, MAX_RANGE)
    assert w is not None
    assert w[0] == pytest.approx(2.0)
    assert w[1] == pytest.approx(0.0)
    assert w[2] == pytest.approx(LZ + 0.5)


def test_project_point_behind_rejected():
    # bx < fwd_min(0.8) → 뒤따르는 UGV 오검출 방지로 None
    assert (
        project_point((0.5, 0.0, 0.0), IDENT, MT0, LEADER0, LZ, FWD_MIN, MAX_RANGE)
        is None
    )


def test_project_point_too_far_rejected():
    assert (
        project_point((15.0, 0.0, 0.0), IDENT, MT0, LEADER0, LZ, FWD_MIN, MAX_RANGE)
        is None
    )


def test_project_point_too_near_rejected():
    assert (
        project_point((0.2, 0.0, 0.0), IDENT, MT0, LEADER0, LZ, FWD_MIN, MAX_RANGE)
        is None
    )


def test_project_point_applies_leader_yaw():
    # 리더 (1,1) yaw=+90°: base 전방 x=2 → odom +y 방향 → (1, 3)
    leader = (1.0, 1.0, math.pi / 2)
    w = project_point((2.0, 0.0, 0.0), IDENT, MT0, leader, LZ, FWD_MIN, MAX_RANGE)
    assert w[0] == pytest.approx(1.0, abs=1e-9)
    assert w[1] == pytest.approx(3.0, abs=1e-9)


# ── world_to_cell / cell_center ───────────────────────────────────────────
def test_world_to_cell_in_range():
    j = world_to_cell(4.0, 0.0, XMIN, YMIN, RES, NX, NY)
    assert j == 32 * NX + 40  # cx=40, cy=32


def test_world_to_cell_out_of_range_is_none():
    assert world_to_cell(100.0, 0.0, XMIN, YMIN, RES, NX, NY) is None
    assert world_to_cell(-10.0, 0.0, XMIN, YMIN, RES, NX, NY) is None


def test_cell_center_roundtrip():
    # 셀 인덱스 → 중심 (cx*res+xmin+res/2). cx=40,cy=32
    cx, cy = cell_center(32 * NX + 40, NX, RES, XMIN, YMIN)
    assert cx == pytest.approx(40 * RES + XMIN + RES / 2)  # = 4.125
    assert cy == pytest.approx(32 * RES + YMIN + RES / 2)  # = 0.125
    # 중심을 다시 셀로 → 동일 셀
    assert world_to_cell(cx, cy, XMIN, YMIN, RES, NX, NY) == 32 * NX + 40


# ── cluster_obstacles ─────────────────────────────────────────────────────
# 작은 격자(10×10, res=0.5, 원점 0,0)로 클러스터링 검증
CNX, CNY, CRES, CX0, CY0 = 10, 10, 0.5, 0.0, 0.0


def _cell(cx, cy):
    return cy * CNX + cx


def test_cluster_single_block():
    # (3,3)(4,3)(3,4)(4,4) 2×2 블록(8-이웃 연결) → 1 클러스터, 중심 ≈ (2.0, 2.0)
    occ = [_cell(3, 3), _cell(4, 3), _cell(3, 4), _cell(4, 4)]
    out = cluster_obstacles(occ, CNX, CNY, CRES, CX0, CY0)
    assert len(out) == 1
    cx, cy, rad = out[0]
    assert cx == pytest.approx(2.0)  # (1.75+2.25)/2
    assert cy == pytest.approx(2.0)
    assert rad >= 0.3
    assert rad == pytest.approx(math.hypot(0.25, 0.25) + CRES)  # ≈0.854


def test_cluster_two_separated():
    # 2×2 블록 + 멀리 떨어진 단일 셀 → 2 클러스터
    occ = [_cell(3, 3), _cell(4, 3), _cell(3, 4), _cell(4, 4), _cell(8, 8)]
    out = cluster_obstacles(occ, CNX, CNY, CRES, CX0, CY0)
    assert len(out) == 2


def test_cluster_singleton_radius_floor():
    # 단일 셀 → radius = max(0.3, 0+res) = 0.5
    out = cluster_obstacles([_cell(5, 5)], CNX, CNY, CRES, CX0, CY0)
    assert len(out) == 1
    assert out[0][2] == pytest.approx(0.5)


def test_cluster_empty():
    assert cluster_obstacles([], CNX, CNY, CRES, CX0, CY0) == []
