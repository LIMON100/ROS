#!/usr/bin/env python3
# Convoy RViz 시각화 노드 — 콘보이 경로/궤적/로봇/장애물을 RViz 표시용으로 발행.
#
# convoy_coordinator/convoy_ugv 가 쓰는 토픽을 구독해 RViz 마커를 만든다(메시 비의존 →
# 공백경로 curl 이슈·per-robot robot_state_publisher 불필요). 모두 odom 프레임.
#   구독: /odom_gt(리더 truth), /convoy/report/r<id>(UGV 위치), /convoy/obstacles(장애물맵)
#   발행: /convoy/leader_track(nav_msgs/Path, 리더 실주행 궤적),
#         /convoy/markers(MarkerArray — 로봇 큐브+라벨 + 콘보이 체인 라인 + 장애물 실린더)
#   ※ 계획경로는 코디네이터의 /convoy/plan(Path)를 RViz 가 직접 표시.
import math

import rclpy
from geometry_msgs.msg import Point, PoseArray, PoseStamped
from nav_msgs.msg import Odometry, Path
from rclpy.node import Node
from rclpy.qos import qos_profile_sensor_data
from std_msgs.msg import ColorRGBA
from visualization_msgs.msg import Marker, MarkerArray

CHAIN = [3, 4, 5, 2]  # leader -> 3 -> 4 -> 5 -> 2
ROLE = {3: "deputy", 4: "follower", 5: "follower", 2: "hub"}
# 0 = leader(Go2)
COLOR = {
    0: (1.0, 0.85, 0.1),
    3: (0.2, 0.55, 0.9),
    4: (0.25, 0.8, 0.3),
    5: (0.9, 0.55, 0.15),
    2: (0.8, 0.3, 0.8),
}


class ConvoyViz(Node):
    def __init__(self):
        super().__init__("convoy_viz")
        self.frame = self.declare_parameter("frame_id", "odom").value
        self.max_track = self.declare_parameter("max_track_pts", 3000).value
        self.min_step = self.declare_parameter("track_step_m", 0.1).value
        self.leader = None
        self.ugv = {}
        self.obstacles = []
        self.track = []
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
        self.create_subscription(PoseArray, "/convoy/obstacles", self.on_obs, 10)
        self.mk_pub = self.create_publisher(MarkerArray, "/convoy/markers", 10)
        self.track_pub = self.create_publisher(Path, "/convoy/leader_track", 10)
        self.create_timer(0.2, self.publish)  # 5Hz
        self.get_logger().info(f"ConvoyViz UP (frame={self.frame})")

    def on_leader(self, m):
        x, y = m.pose.pose.position.x, m.pose.pose.position.y
        self.leader = (x, y)
        if (
            not self.track
            or math.hypot(x - self.track[-1][0], y - self.track[-1][1]) > self.min_step
        ):
            self.track.append((x, y))
            if len(self.track) > self.max_track:
                self.track.pop(0)

    def on_report(self, m, n):
        self.ugv[n] = (m.pose.pose.position.x, m.pose.pose.position.y)

    def on_obs(self, m):
        self.obstacles = [(p.position.x, p.position.y, p.position.z) for p in m.poses]

    def _stamp(self):
        return self.get_clock().now().to_msg()

    def publish(self):
        stamp = self._stamp()
        # 리더 실주행 궤적(Path)
        path = Path()
        path.header.frame_id = self.frame
        path.header.stamp = stamp
        for x, y in self.track:
            ps = PoseStamped()
            ps.header.frame_id = self.frame
            ps.pose.position.x = x
            ps.pose.position.y = y
            ps.pose.orientation.w = 1.0
            path.poses.append(ps)
        self.track_pub.publish(path)

        ma = MarkerArray()
        nodes = []
        if self.leader is not None:
            nodes.append((0, self.leader[0], self.leader[1], "leader (Go2)"))
        for n in CHAIN:
            if n in self.ugv:
                nodes.append(
                    (n, self.ugv[n][0], self.ugv[n][1], f"robot_{n} ({ROLE[n]})")
                )

        mid = 0
        for rid, x, y, label in nodes:
            c = COLOR[rid]
            mk = Marker()
            mk.header.frame_id = self.frame
            mk.header.stamp = stamp
            mk.ns = "robots"
            mk.id = mid
            mid += 1
            mk.type = Marker.SPHERE if rid == 0 else Marker.CUBE
            mk.action = Marker.ADD
            mk.pose.position.x = x
            mk.pose.position.y = y
            mk.pose.position.z = 0.25
            mk.pose.orientation.w = 1.0
            mk.scale.x = mk.scale.y = mk.scale.z = 0.5
            mk.color = ColorRGBA(r=c[0], g=c[1], b=c[2], a=0.95)
            ma.markers.append(mk)
            tx = Marker()
            tx.header.frame_id = self.frame
            tx.header.stamp = stamp
            tx.ns = "labels"
            tx.id = mid
            mid += 1
            tx.type = Marker.TEXT_VIEW_FACING
            tx.action = Marker.ADD
            tx.pose.position.x = x
            tx.pose.position.y = y
            tx.pose.position.z = 0.85
            tx.pose.orientation.w = 1.0
            tx.scale.z = 0.3
            tx.color = ColorRGBA(r=1.0, g=1.0, b=1.0, a=1.0)
            tx.text = label
            ma.markers.append(tx)

        # 콘보이 체인 라인(leader -> 3 -> 4 -> 5 -> 2)
        if len(nodes) >= 2:
            line = Marker()
            line.header.frame_id = self.frame
            line.header.stamp = stamp
            line.ns = "chain"
            line.id = mid
            mid += 1
            line.type = Marker.LINE_STRIP
            line.action = Marker.ADD
            line.scale.x = 0.06
            line.color = ColorRGBA(r=0.95, g=0.9, b=0.2, a=0.85)
            line.pose.orientation.w = 1.0
            for _rid, x, y, _label in nodes:
                pt = Point()
                pt.x = x
                pt.y = y
                pt.z = 0.25
                line.points.append(pt)
            ma.markers.append(line)

        # 장애물 실린더
        for ox, oy, r in self.obstacles:
            mk = Marker()
            mk.header.frame_id = self.frame
            mk.header.stamp = stamp
            mk.ns = "obstacles"
            mk.id = mid
            mid += 1
            mk.type = Marker.CYLINDER
            mk.action = Marker.ADD
            mk.pose.position.x = ox
            mk.pose.position.y = oy
            mk.pose.position.z = 1.0
            mk.pose.orientation.w = 1.0
            mk.scale.x = mk.scale.y = 2.0 * r
            mk.scale.z = 2.0
            mk.color = ColorRGBA(r=0.85, g=0.2, b=0.2, a=0.55)
            ma.markers.append(mk)

        self.mk_pub.publish(ma)


def main():
    rclpy.init()
    rclpy.spin(ConvoyViz())
    rclpy.shutdown()


if __name__ == "__main__":
    main()
