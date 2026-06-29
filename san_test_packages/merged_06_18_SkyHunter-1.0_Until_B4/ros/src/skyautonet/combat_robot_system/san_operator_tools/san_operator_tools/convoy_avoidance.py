#!/usr/bin/env python3
# Convoy 회피 순수 기하 로직 — rclpy/ROS 무관. 리더(convoy_coordinator)와 UGV 측면 결정의
# 단일 소스(seam). 순수 함수라 하드웨어/ROS 그래프 없이 단위테스트 가능(standalone pytest
# 러너가 자동 수집).
#
# 전략: 주행 경로상 충돌이 예상되는 경우(전방 lookahead 이내 경로 통과 y 가 장애물 중심에서
# r+collision_margin 이내)에만, 결정론적 측면(장애물 부근 계획경로 국소형상)으로
# [lateral_min+track_margin, lateral_max] 측면 안전거리를 두고 우회한다. 측면이 리더 실시간
# 위치에 비의존이라 런마다 일관 → 종대(UGV) 가 리더와 같은 쪽을 추종(표류/추종실패 방지).
import math


def parse_triples(seq):
    # 평탄 [x,y,r,x,y,r,...] → [(x,y,r),...]. None/빈 입력 안전(obstacles:=[] lidar 단독,
    # 정적맵 없음 → ROS2 가 None 전달) + 3 의 배수 아닌 꼬리 무시(IndexError 방지).
    seq = seq or []
    return [
        (float(seq[i]), float(seq[i + 1]), float(seq[i + 2]))
        for i in range(0, len(seq) - 2, 3)
    ]


def path_y_at(wps, x):
    # 계획경로(waypoints) 에서 x 위치의 y 보간(장애물 통과 쪽 판정용). 범위 밖이면 마지막 y.
    for i in range(len(wps) - 1):
        x0, y0 = wps[i]
        x1, y1 = wps[i + 1]
        if (x0 <= x <= x1) or (x1 <= x <= x0):
            if abs(x1 - x0) < 1e-6:
                return 0.5 * (y0 + y1)
            t = (x - x0) / (x1 - x0)
            return y0 + t * (y1 - y0)
    return wps[-1][1]


def front_obstacle(obstacles, wps, lx, ly, lookahead, collision_margin):
    # 회피 대상: 전방 lookahead 이내 + 리더보다 앞(ox>=lx-0.5) + 계획경로가 충돌예상
    # (통과 y 가 중심에서 r+collision_margin 이내)인 것 중 가장 가까운 (ox, oy, r, dist).
    # 충돌 예상 장애물이 없으면(안전) None → 평시 waypoint 추종.
    best = None
    for ox, oy, r in obstacles:
        if math.hypot(ox - lx, oy - ly) > lookahead:
            continue
        if ox < lx - 0.5:  # 이미 지나친 장애물 제외
            continue
        if abs(path_y_at(wps, ox) - oy) < r + collision_margin:
            d = ox - lx
            if best is None or d < best[3]:
                best = (ox, oy, r, d)
    return best


def avoid_side(wps, ox, oy):
    # 결정론적 회피 측면(리더 위치 비의존 → 런마다 일관): 장애물 부근 계획경로 국소형상 —
    # 정점(bump)/하강 → 아래(-1, 짧은 저궤적 우회), 골(valley)/상승 → 위(+1).
    by = path_y_at(wps, ox - 3.0)
    ay = path_y_at(wps, ox + 3.0)
    if by < oy and ay < oy:
        return -1.0
    if by > oy and ay > oy:
        return 1.0
    return -1.0 if ay <= by else 1.0


def avoid_target(
    obstacles,
    wps,
    lx,
    ly,
    wp_idx,
    wp_near,
    lookahead,
    collision_margin,
    lateral_min,
    lateral_max,
    track_margin,
):
    # 전방 충돌 장애물을 결정론적 측면으로 [lateral_min+track_margin, lateral_max] 측면거리
    # 우회. 충돌 없으면 앞 wp_near 번째 waypoint(평시 추종). 타겟 x = max(장애물 x, 리더+1.5):
    # 접근 중엔 장애물 x 겨냥(최근접서 full offset → 추종지연에도 clearance 확보), 옆/통과 시엔
    # 항상 앞을 겨냥(타겟이 뒤로 가 정체/후진 방지).
    near_i = min(wp_idx + wp_near, len(wps) - 1)
    obs = front_obstacle(obstacles, wps, lx, ly, lookahead, collision_margin)
    if obs is None:
        return wps[near_i]
    ox, oy = obs[0], obs[1]
    side = avoid_side(wps, ox, oy)
    clear = min(lateral_max, lateral_min + track_margin)
    return max(ox, lx + 1.5), oy + side * clear


def obstacle_on_path_ahead(obstacles, wps, lx, ly, lookahead, collision_margin):
    # 충돌 예상(회피 필요) 장애물이 하나라도 있으면 True(없으면 안전 → waypoint 추종).
    return (
        front_obstacle(obstacles, wps, lx, ly, lookahead, collision_margin) is not None
    )
