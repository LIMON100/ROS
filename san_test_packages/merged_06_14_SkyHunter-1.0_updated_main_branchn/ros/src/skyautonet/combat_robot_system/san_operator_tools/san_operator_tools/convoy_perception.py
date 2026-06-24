#!/usr/bin/env python3
# Convoy 로컬 코스트맵 순수 로직 — rclpy/ROS 무관. Go2 4D lidar 점을 odom 격자로 투영하고
# 점유 셀을 클러스터링해 장애물(중심+반경)을 검출하는 기하/격자 연산의 단일 소스(seam).
# 순수 함수라 하드웨어/ROS 그래프 없이 단위테스트 가능(standalone pytest 러너가 자동 수집).
#
# convoy_costmap(rclpy Node)가 이 모듈에 위임한다(인지 입력 → /convoy/detected_obstacles).
import math
import struct


def yaw_of(q):
    # 쿼터니언(x,y,z,w 속성) → yaw(z축 회전). geometry_msgs Quaternion 류 객체를 받음.
    return math.atan2(
        2.0 * (q.w * q.z + q.x * q.y), 1.0 - 2.0 * (q.y * q.y + q.z * q.z)
    )


def roll_pitch_of(q):
    # 쿼터니언 → (roll, pitch). 지면 투영 보상용 — Go2 4족 gait 중 몸체 기울기(pitch/roll)를
    # 반영해야 전방 지면 반사점이 odom z≈0 으로 투영되어 높이밴드에서 걸러진다(허위검출 방지).
    sinr = 2.0 * (q.w * q.x + q.y * q.z)
    cosr = 1.0 - 2.0 * (q.x * q.x + q.y * q.y)
    roll = math.atan2(sinr, cosr)
    sinp = 2.0 * (q.w * q.y - q.z * q.x)
    sinp = max(-1.0, min(1.0, sinp))
    pitch = math.asin(sinp)
    return (roll, pitch)


def rpy_matrix(rpy):
    # Rz(yaw) @ Ry(pitch) @ Rx(roll) — lidar 장착 회전(URDF joint rpy). 직교·det=1.
    r, p, y = rpy
    cr, sr = math.cos(r), math.sin(r)
    cp, sp = math.cos(p), math.sin(p)
    cy, sy = math.cos(y), math.sin(y)
    return (
        (cy * cp, cy * sp * sr - sy * cr, cy * sp * cr + sy * sr),
        (sy * cp, sy * sp * sr + cy * cr, sy * sp * cr - cy * sr),
        (-sp, cp * sr, cp * cr),
    )


def read_xyz(msg):
    # PointCloud2 → (x,y,z) generator. float32 x/y/z at their declared offsets.
    # msg: .fields(name/offset 속성 리스트) / .point_step / .data 를 가진 객체(메시지 무관).
    fmt = {f.name: f.offset for f in msg.fields}
    ox, oy, oz = fmt.get("x"), fmt.get("y"), fmt.get("z")
    if ox is None or oy is None or oz is None:
        return
    step, data = msg.point_step, msg.data
    n = len(data) // step if step else 0
    for i in range(n):
        b = i * step
        yield (
            struct.unpack_from("<f", data, b + ox)[0],
            struct.unpack_from("<f", data, b + oy)[0],
            struct.unpack_from("<f", data, b + oz)[0],
        )


def project_point(
    p,
    mount_rot,
    mount_t,
    leader,
    leader_z,
    fwd_min,
    max_range,
    min_range=0.3,
    lead_rp=(0.0, 0.0),
):
    # lidar 점 1개 → odom (x,y,z). 장착 회전/이동(base_link) → 리더 자세 회전/평행이동(odom).
    # 전방(bx≥fwd_min, 뒤따르는 UGV 제외) + 사거리(min_range..max_range) 게이트 통과만 반환,
    # 아니면 None. 높이밴드(z) 필터는 호출측이 적용(반환 wz 로 판정).
    # lead_rp=(roll, pitch): 리더(Go2) 몸체 기울기. gait 중 pitch/roll 을 반영해 전방 지면점이
    # odom z≈0 으로 투영되도록(yaw-only 투영은 지면을 높이밴드로 띄워 허위 장애물 유발).
    px, py, pz = p
    (r00, r01, r02), (r10, r11, r12), (r20, r21, r22) = mount_rot
    tx, ty, tz = mount_t
    bx = r00 * px + r01 * py + r02 * pz + tx
    by = r10 * px + r11 * py + r12 * pz + ty
    bz = r20 * px + r21 * py + r22 * pz + tz
    rng = math.hypot(bx, by)
    if rng < min_range or rng > max_range:
        return None
    if bx < fwd_min:  # 전방만(뒤따르는 UGV 팔로워 제외)
        return None
    lx, ly, lyaw = leader
    lroll, lpitch = lead_rp
    # 전체 자세(roll/pitch/yaw) 회전(body→odom). lead_rp=(0,0) 이면 기존 yaw-only 와 동일.
    (m00, m01, m02), (m10, m11, m12), (m20, m21, m22) = rpy_matrix(
        (lroll, lpitch, lyaw)
    )
    wx = lx + m00 * bx + m01 * by + m02 * bz
    wy = ly + m10 * bx + m11 * by + m12 * bz
    wz = leader_z + m20 * bx + m21 * by + m22 * bz
    return (wx, wy, wz)


def world_to_cell(wx, wy, xmin, ymin, res, nx, ny):
    # odom (wx,wy) → 격자 셀 인덱스(row-major, cy*nx+cx). 격자 밖이면 None.
    cx = int((wx - xmin) / res)
    cy = int((wy - ymin) / res)
    if 0 <= cx < nx and 0 <= cy < ny:
        return cy * nx + cx
    return None


def cell_center(cell, nx, res, xmin, ymin):
    # 셀 인덱스 → 셀 중심 (wx,wy)(셀 중앙 = +res/2).
    cx = cell % nx
    cy = cell // nx
    return (cx * res + xmin + res / 2, cy * res + ymin + res / 2)


def cluster_obstacles(occ, nx, ny, res, xmin, ymin, min_radius=0.3, min_cells=1):
    # 점유 셀(occ, 인덱스 리스트)을 8-이웃 연결요소로 클러스터링 → 각 장애물 (cx, cy, radius).
    # radius = max(min_radius, 중심에서 가장 먼 셀 거리 + res). convoy_costmap._detect 와 동일.
    # min_cells: 이보다 작은 클러스터는 희소 lidar 노이즈로 보고 버림(허위 장애물 제거).
    cells = set(occ)
    seen = set()
    clusters = []
    for j in cells:
        if j in seen:
            continue
        stack, comp = [j], []
        seen.add(j)
        while stack:
            cur = stack.pop()
            comp.append(cur)
            cy, cx = divmod(cur, nx)
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    nb = (cy + dy) * nx + (cx + dx)
                    if (
                        0 <= cx + dx < nx
                        and 0 <= cy + dy < ny
                        and nb in cells
                        and nb not in seen
                    ):
                        seen.add(nb)
                        stack.append(nb)
        clusters.append(comp)
    out = []
    for comp in clusters:
        if len(comp) < min_cells:  # 희소 노이즈 클러스터 제거
            continue
        pts = [cell_center(c, nx, res, xmin, ymin) for c in comp]
        cx = sum(x for x, _ in pts) / len(pts)
        cy = sum(y for _, y in pts) / len(pts)
        rad = max(min_radius, max(math.hypot(x - cx, y - cy) for x, y in pts) + res)
        out.append((cx, cy, rad))
    return out
