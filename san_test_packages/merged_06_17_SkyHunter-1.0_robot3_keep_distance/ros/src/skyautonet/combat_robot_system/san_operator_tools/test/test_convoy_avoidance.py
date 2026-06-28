# Copyright 2026 SkyAutoNet Inc.
#
# Proprietary and confidential. Unauthorized copying, distribution, or use
# of this file, via any medium, is strictly prohibited.

"""Convoy 회피 순수 기하 로직 단위테스트 (PR #279 hardening).

convoy_avoidance 는 rclpy 무관 순수 함수 — 리더(convoy_coordinator)와 UGV 측면 결정의
단일 소스. 검증 항목:
  - path_y_at 보간(정확/범위 밖)
  - 결정론적 회피 측면(avoid_side): apex 정점→아래, 6번째 슬로프→아래, 입력 동일→출력 동일
  - 충돌 예상 시에만 회피(front_obstacle): 경로상 장애물만 검출, 안전·후방·원거리는 None
  - 회피 타겟이 측면 안전거리 [lateral_min+margin] 유지 + x 전진(정체/후진 방지)
  - 안전 시 waypoint 추종

standalone pytest 러너가 자동 수집(rclpy 미import). 좌표·파라미터는 convoy_coordinator
및 convoy_demo.launch.py 기본값과 동일.
"""

import pytest
from san_operator_tools.convoy_avoidance import (
    avoid_side,
    avoid_target,
    front_obstacle,
    obstacle_on_path_ahead,
    parse_triples,
    path_y_at,
)

# 데모 40m 완만 S-curve 경로(코디네이터 기본 waypoints)
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
LAT_MIN = 1.3
LAT_MAX = 4.0
TRACK_MARGIN = 0.5
WP_NEAR = 3

APEX = (12.0, 2.5, 0.5)  # 4번째 waypoint 정점 위 → 충돌 → 회피
SIXTH = (20.0, 1.0, 0.5)  # 6번째 waypoint 위 → 충돌 → 회피
SAFE = (28.0, -3.0, 0.5)  # 경로(x=28,y=-1)서 2m 비껴 → 안전 → 회피X


def test_parse_triples_basic():
    assert parse_triples([12.0, 2.5, 0.5, 20.0, 1.0, 0.5]) == [
        (12.0, 2.5, 0.5),
        (20.0, 1.0, 0.5),
    ]


def test_parse_triples_none_and_empty_are_safe():
    # obstacles:=[] (lidar 단독, 정적맵 없음) → ROS2 가 None 전달 → 빈 리스트(크래시 없음)
    assert parse_triples(None) == []
    assert parse_triples([]) == []


def test_parse_triples_ragged_tail_ignored():
    # 3 의 배수 아닌 꼬리는 무시(IndexError 방지)
    assert parse_triples([1.0, 2.0, 0.5, 9.0, 9.0]) == [(1.0, 2.0, 0.5)]


def test_path_y_at_interpolates():
    assert path_y_at(WPS, 0.0) == pytest.approx(0.0)
    assert path_y_at(WPS, 12.0) == pytest.approx(2.5)
    assert path_y_at(WPS, 10.0) == pytest.approx(2.25)  # (8,2)-(12,2.5) 중간
    assert path_y_at(WPS, 22.0) == pytest.approx(0.5)  # (20,1)-(24,0) 중간


def test_path_y_at_out_of_range_returns_last():
    assert path_y_at(WPS, 100.0) == pytest.approx(0.0)  # 마지막 waypoint y


def test_avoid_side_apex_is_down():
    # apex 는 경로 국소 정점(양 이웃 낮음) → 아래(-1) 회피(짧은 저궤적·일관)
    assert avoid_side(WPS, APEX[0], APEX[1]) == -1.0


def test_avoid_side_sixth_is_down():
    # (20,1) 은 하강 슬로프(이전 1.5 > 이후 0.5) → 아래(-1)
    assert avoid_side(WPS, SIXTH[0], SIXTH[1]) == -1.0


def test_avoid_side_deterministic():
    # 리더 실시간 위치 비의존 — 같은 입력은 항상 같은 측면(런마다 일관 / UGV oside 일치 근거)
    for _ in range(5):
        assert avoid_side(WPS, APEX[0], APEX[1]) == -1.0


def test_avoid_side_valley_is_up():
    # 인공 골(valley) 경로: 양 이웃이 모두 높음 → 위(+1)
    valley = [(0.0, 2.0), (5.0, 0.0), (10.0, 2.0)]
    assert avoid_side(valley, 5.0, 0.0) == 1.0


def test_front_obstacle_collision_detected():
    # apex 는 경로 통과 y(2.5)=중심 → 충돌 → 검출
    obs = front_obstacle([APEX], WPS, 8.0, 2.0, LOOKAHEAD, COLLISION_MARGIN)
    assert obs is not None
    assert obs[0] == APEX[0] and obs[1] == APEX[1]


def test_front_obstacle_safe_is_ignored():
    # (28,-3) 은 경로(y=-1)서 |(-1)-(-3)|=2 > r+margin(1.3) → 안전 → None
    assert front_obstacle([SAFE], WPS, 26.0, -1.0, LOOKAHEAD, COLLISION_MARGIN) is None


def test_front_obstacle_beyond_lookahead_is_none():
    # apex 까지 12m > lookahead 6m → None
    assert front_obstacle([APEX], WPS, 0.0, 0.0, LOOKAHEAD, COLLISION_MARGIN) is None


def test_front_obstacle_behind_is_none():
    # 이미 지나친(ox < lx-0.5) 장애물 제외
    assert front_obstacle([APEX], WPS, 14.0, 2.0, LOOKAHEAD, COLLISION_MARGIN) is None


def test_front_obstacle_nearest_selected():
    # 두 충돌 장애물 중 더 가까운 것 선택
    obs = front_obstacle([SIXTH, APEX], WPS, 9.0, 2.0, LOOKAHEAD, COLLISION_MARGIN)
    assert obs[0] == APEX[0]  # apex(12) 가 (20) 보다 가까움


def test_avoid_target_maintains_lateral_clearance():
    # apex 회피 타겟의 측면거리(중심~중심) = lateral_min+track_margin = 1.8 ≥ 1.3, 아래쪽
    _tx, ty = avoid_target(
        [APEX],
        WPS,
        8.0,
        2.0,
        2,
        WP_NEAR,
        LOOKAHEAD,
        COLLISION_MARGIN,
        LAT_MIN,
        LAT_MAX,
        TRACK_MARGIN,
    )
    assert abs(ty - APEX[1]) == pytest.approx(LAT_MIN + TRACK_MARGIN)
    assert abs(ty - APEX[1]) >= LAT_MIN  # 스펙 floor 충족
    assert ty < APEX[1]  # 아래로(-1) 회피


def test_avoid_target_clearance_capped_at_max():
    # track_margin 이 커도 측면거리는 lateral_max 이내
    _tx, ty = avoid_target(
        [APEX],
        WPS,
        8.0,
        2.0,
        2,
        WP_NEAR,
        LOOKAHEAD,
        COLLISION_MARGIN,
        LAT_MIN,
        LAT_MAX,
        10.0,
    )
    assert abs(ty - APEX[1]) <= LAT_MAX


def test_avoid_target_x_aims_at_obstacle_then_ahead():
    # 접근 중(lx=8): tx=max(ox,lx+1.5)=max(12,9.5)=12 (장애물 x 겨냥 → 최근접서 full offset)
    tx, _ty = avoid_target(
        [APEX],
        WPS,
        8.0,
        2.0,
        2,
        WP_NEAR,
        LOOKAHEAD,
        COLLISION_MARGIN,
        LAT_MIN,
        LAT_MAX,
        TRACK_MARGIN,
    )
    assert tx == pytest.approx(12.0)
    # 옆(lx=11.5): tx=max(12,13)=13 (전진 유지 → 정체/후진 방지)
    tx2, _ = avoid_target(
        [APEX],
        WPS,
        11.5,
        1.0,
        2,
        WP_NEAR,
        LOOKAHEAD,
        COLLISION_MARGIN,
        LAT_MIN,
        LAT_MAX,
        TRACK_MARGIN,
    )
    assert tx2 == pytest.approx(13.0)


def test_avoid_target_follows_waypoint_when_safe():
    # 충돌 장애물 없으면 앞 wp_near 번째 waypoint 추종(평시)
    tgt = avoid_target(
        [SAFE],
        WPS,
        4.0,
        1.0,
        1,
        WP_NEAR,
        LOOKAHEAD,
        COLLISION_MARGIN,
        LAT_MIN,
        LAT_MAX,
        TRACK_MARGIN,
    )
    assert tgt == WPS[min(1 + WP_NEAR, len(WPS) - 1)]


def test_obstacle_on_path_ahead_collision_only():
    assert obstacle_on_path_ahead([APEX], WPS, 8.0, 2.0, LOOKAHEAD, COLLISION_MARGIN)
    assert not obstacle_on_path_ahead(
        [SAFE], WPS, 26.0, -1.0, LOOKAHEAD, COLLISION_MARGIN
    )
