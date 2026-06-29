#!/usr/bin/env python3
# Convoy 로컬 코스트맵 — Go2 4D lidar(/unitree_lidar/points)로 장애물 occupancy 격자 구축.
#
# 콘보이 장애물 회피의 인지 입력. tf2 없이 리더 pose(/odom_gt)로 lidar 점을 odom 으로
# 변환(신뢰성), 높이 밴드/사거리 필터 후 격자에 누적·팽창(inflation). 점유 셀을 근접
# 클러스터링해 장애물(중심+반경)을 검출한다.
#   구독: /odom_gt(Go2 pose), /unitree_lidar/points(PointCloud2)
#   발행: /convoy/costmap(nav_msgs/OccupancyGrid, RViz Map),
#         /convoy/detected_obstacles(geometry_msgs/PoseArray — position=(x,y,r))
import math

import rclpy
from geometry_msgs.msg import Pose, PoseArray
from nav_msgs.msg import OccupancyGrid, Odometry
from rclpy.node import Node
from rclpy.qos import qos_profile_sensor_data
from sensor_msgs.msg import PointCloud2

from san_operator_tools import convoy_perception as cp


class ConvoyCostmap(Node):
    def __init__(self):
        super().__init__("convoy_costmap")
        self.frame = self.declare_parameter("frame_id", "odom").value
        self.res = self.declare_parameter("resolution_m", 0.25).value
        self.xmin = self.declare_parameter("x_min", -6.0).value
        self.xmax = self.declare_parameter("x_max", 45.0).value
        self.ymin = self.declare_parameter("y_min", -8.0).value
        self.ymax = self.declare_parameter("y_max", 8.0).value
        self.zlo = self.declare_parameter("obstacle_z_min", 0.15).value
        self.zhi = self.declare_parameter("obstacle_z_max", 1.8).value
        self.max_range = self.declare_parameter("max_range_m", 12.0).value
        # 전방만 채택(lidar +x = 전방). 뒤따르는 UGV 팔로워를 장애물로 오검출하지 않도록.
        self.fwd_min = self.declare_parameter("forward_min_m", 0.8).value
        self.inflate = self.declare_parameter("inflate_m", 0.5).value
        self.decay = self.declare_parameter(
            "decay", 0.9
        ).value  # 셀 신뢰 감쇠(이동 장애물 대응; ↑=원거리 희소점 누적 지속)
        # 점유 판정 임계(decay 적용 후 hit≥occ_threshold). 지속/밀도 필터: 1.5 로 두어 셀당 다수
        # 점(조밀 반사) 또는 다프레임 누적(decay 0.9 → 매프레임 재관측)이 있어야 점유로 등록.
        # 단발/산발 lidar 노이즈(전방 허위 장애물 → 불필요 AVOID)를 제거하되, 실제 장애물은
        # 접근하며 조밀·지속 관측되어 검출(정적 prior 가 추가 보강).
        self.occ_thresh = self.declare_parameter("occ_threshold", 2.5).value
        self.nx = max(1, int((self.xmax - self.xmin) / self.res))
        self.ny = max(1, int((self.ymax - self.ymin) / self.res))
        self.hits = [0.0] * (self.nx * self.ny)  # 셀 누적 hit(연속값)
        # 4D L1 lidar 장착 변환(URDF lidar_l1_joint). lidar 점 → base_link.
        mr = self.declare_parameter("lidar_mount_rpy", [2.879, 0.0, 1.5705]).value
        mt = self.declare_parameter("lidar_mount_xyz", [0.25, -0.038, -0.03]).value
        self.mR = cp.rpy_matrix([float(v) for v in mr])
        self.mt = (float(mt[0]), float(mt[1]), float(mt[2]))
        # 클러스터 최소 셀 수 — 이보다 작은(≈0.5 m 미만) 점유 클러스터는 희소 lidar 노이즈로
        # 보고 버림(산발 허위 장애물 제거). 실제 장애물(반경 0.5 m)은 다수 셀이라 영향 없음.
        self.min_cluster_cells = self.declare_parameter("min_cluster_cells", 3).value
        self.leader_z = 0.3
        self.leader = None
        self.leader_rp = (0.0, 0.0)  # 리더 몸체 (roll, pitch) — on_leader 에서 갱신
        self.create_subscription(
            Odometry, "/odom_gt", self.on_leader, qos_profile_sensor_data
        )
        self.create_subscription(
            PointCloud2, "/unitree_lidar/points", self.on_cloud, qos_profile_sensor_data
        )
        self.grid_pub = self.create_publisher(OccupancyGrid, "/convoy/costmap", 1)
        self.obs_pub = self.create_publisher(
            PoseArray, "/convoy/detected_obstacles", 10
        )
        self.create_timer(0.8, self.publish)  # ~1.25Hz (CPU↓ → gz RTF/gait 안정)
        self.get_logger().info(
            f"ConvoyCostmap UP grid={self.nx}x{self.ny}@{self.res}m frame={self.frame}"
        )

    def on_leader(self, m):
        self.leader_z = m.pose.pose.position.z
        self.leader = (
            m.pose.pose.position.x,
            m.pose.pose.position.y,
            cp.yaw_of(m.pose.pose.orientation),
        )
        # 몸체 기울기(roll/pitch) — gait 중 전방 지면점이 띄워져 허위검출되는 것 방지(투영 보상).
        self.leader_rp = cp.roll_pitch_of(m.pose.pose.orientation)

    def on_cloud(self, msg):
        if self.leader is None:
            return
        leader = self.leader
        for p in cp.read_xyz(msg):
            # 순수 로직 위임 — lidar 점 → odom (장착변환→리더 자세/위치), 전방·사거리 게이트.
            w = cp.project_point(
                p,
                self.mR,
                self.mt,
                leader,
                self.leader_z,
                self.fwd_min,
                self.max_range,
                lead_rp=self.leader_rp,
            )
            if w is None:
                continue
            wx, wy, wz = w
            if not (self.zlo <= wz <= self.zhi):  # 높이밴드 필터
                continue
            j = cp.world_to_cell(
                wx, wy, self.xmin, self.ymin, self.res, self.nx, self.ny
            )
            if j is not None:
                self.hits[j] = min(self.hits[j] + 1.0, 10.0)

    def publish(self):
        stamp = self.get_clock().now().to_msg()
        self.hits = [h * self.decay for h in self.hits]  # 감쇠를 publish(2Hz)로 — CPU↓
        occ = [c for c, h in enumerate(self.hits) if h >= self.occ_thresh]
        # 팽창(inflation): 점유 셀 주변 반경 inflate 를 비용으로
        infl = int(self.inflate / self.res)
        grid = [0] * (self.nx * self.ny)
        for j in occ:
            cy, cx = divmod(j, self.nx)
            for dy in range(-infl, infl + 1):
                for dx in range(-infl, infl + 1):
                    nx_, ny_ = cx + dx, cy + dy
                    if 0 <= nx_ < self.nx and 0 <= ny_ < self.ny:
                        d = math.hypot(dx, dy) * self.res
                        val = (
                            100
                            if d <= self.res
                            else max(0, int(100 * (1.0 - d / self.inflate)))
                        )
                        k = ny_ * self.nx + nx_
                        if val > grid[k]:
                            grid[k] = val
        og = OccupancyGrid()
        og.header.frame_id = self.frame
        og.header.stamp = stamp
        og.info.resolution = self.res
        og.info.width = self.nx
        og.info.height = self.ny
        og.info.origin.position.x = self.xmin
        og.info.origin.position.y = self.ymin
        og.info.origin.orientation.w = 1.0
        og.data = [min(100, v) for v in grid]
        self.grid_pub.publish(og)
        self._detect_and_publish(occ, stamp)

    def _detect_and_publish(self, occ, stamp):
        # 순수 로직 위임 — 점유 셀 8-이웃 클러스터링 → 장애물 (cx, cy, radius) 리스트.
        pa = PoseArray()
        pa.header.frame_id = self.frame
        pa.header.stamp = stamp
        for cx, cy, rad in cp.cluster_obstacles(
            occ,
            self.nx,
            self.ny,
            self.res,
            self.xmin,
            self.ymin,
            min_cells=self.min_cluster_cells,
        ):
            p = Pose()
            p.position.x = cx
            p.position.y = cy
            p.position.z = rad
            p.orientation.w = 1.0
            pa.poses.append(p)
        self.obs_pub.publish(pa)


def main():
    rclpy.init()
    rclpy.spin(ConvoyCostmap())
    rclpy.shutdown()


if __name__ == "__main__":
    main()
