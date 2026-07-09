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


# ── 추종자 nav2 참조경로(DCN-2026-029 P1) ─────────────────────────────────────
# 리더(Go2)가 실제 주행한 breadcrumb 궤적 = 이미 장애물을 회피한 '안전 차선'. 단일종대
# 콘보이라 모든 로봇의 참조 차선은 동일한 리더 궤적이고, 슬롯(체인상 간격)은 각 로봇이
# 그 차선의 '어디에' 있는지만 정한다. 리더가 자기 궤적과 슬롯거리(arc)를 주면 추종자 nav2 가
# 그 점부터 앞쪽 차선을 global plan(FollowPath)으로 추종한다. ROS 무관 순수 기하.


def slot_index(trail, slot_arc):
    # trail: [(x,y),...] 오래된→최신(끝=리더 head). head 에서 뒤로 호장 slot_arc 만큼
    # 떨어진 점의 인덱스(추종자의 참조 슬롯)를 반환. 궤적 전체보다 slot_arc 가 길면 0
    # (전체 궤적)으로 클램프. slot_arc<=0 도 전체 궤적(head 앞에 추종자 없음).
    n = len(trail)
    if n < 2 or slot_arc <= 0.0:
        return 0
    s = 0.0
    for i in range(n - 1, 0, -1):
        s += math.hypot(trail[i][0] - trail[i - 1][0], trail[i][1] - trail[i - 1][1])
        if s >= slot_arc:
            return i - 1
    return 0


def ref_path_from_trail(trail, slot_arc):
    # 슬롯이 리더 head 뒤 slot_arc(m) 에 있는 추종자의 참조 차선 = 리더 breadcrumb 궤적의
    # 슬롯점부터 head 까지. 호출측이 (x,y) 들을 nav_msgs/Path 로 감싼다. 궤적이 2점 미만이면
    # 그대로 반환(아직 차선 미형성).
    if len(trail) < 2:
        return [(float(p[0]), float(p[1])) for p in trail]
    i = slot_index(trail, slot_arc)
    return [(float(p[0]), float(p[1])) for p in trail[i:]]


def should_resend_path(prev_head, new_head, move_tol):
    # P3 FollowPath 재전송 판정(순수): 참조경로는 @5Hz 갱신되나 매번 action goal 을 보내면
    # controller_server 가 끊임없이 preempt 된다. 직전 전송한 경로의 head(리더 최신점)에서
    # 새 head 가 move_tol(m) 이상 이동했을 때만 재전송 → 추종 안정 + 최신 차선 반영.
    # prev_head=None(첫 전송) 이면 항상 True.
    if prev_head is None:
        return True
    return (
        math.hypot(new_head[0] - prev_head[0], new_head[1] - prev_head[1]) >= move_tol
    )


def downsample_path(points, spacing):
    # P4 NavigateThroughPoses 용(순수): 조밀한 참조경로(breadcrumb, ~0.12 m 간격)를 호장
    # spacing 마다 1점으로 솎아 via-pose 리스트로 만든다. NavigateThroughPoses 는 pose 마다
    # 전역 플래너를 돌리므로 조밀 입력은 과부하 → 듬성한 경유점만. 첫 점과 끝 점(리더 head)은
    # 항상 보존(목적지=head). 2점 미만이면 그대로.
    if len(points) < 2:
        return [(float(p[0]), float(p[1])) for p in points]
    out = [(float(points[0][0]), float(points[0][1]))]
    acc = 0.0
    for i in range(1, len(points)):
        acc += math.hypot(
            points[i][0] - points[i - 1][0], points[i][1] - points[i - 1][1]
        )
        if acc >= spacing:
            out.append((float(points[i][0]), float(points[i][1])))
            acc = 0.0
    last = (float(points[-1][0]), float(points[-1][1]))
    if out[-1] != last:
        out.append(last)
    return out
