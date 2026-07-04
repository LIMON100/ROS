#!/usr/bin/env python3
# Convoy nav2 tf shim (DCN-2026-029 P2, O-2) — ground-truth odom → tf 브로드캐스터.
#
# convoy 는 tf2-free 로 /robot_<id>/odom(gz-sim-odometry-publisher 지상진실) 토픽만 쓴다.
# nav2 는 map→odom→base_link tf 트리를 요구한다(DCN-2026-029 §9 O-2). 실 하드웨어는
# san_localization 의 dual-EKF 가 이 tf 를 만들지만, sim 은 지상진실이라 EKF(IMU+휠) 가
# 과하다 → 지상진실 odom 토픽을 그대로 parent→child tf 로 중계한다. map→odom 은 런처의
# static_transform_publisher(지상진실 identity)가 담당 → 합쳐 map→odom→base_footprint 완성.
#
# 네임스페이스(PushRosNamespace robot_<id>)로 인스턴스화하고 parent_frame/child_frame 을
# robot_<id>/odom, robot_<id>/base_footprint 로 주입한다. 입력 메시지의 frame_id 는 무시하고
# 파라미터 frame 으로 브로드캐스트(sim odom frame 명명과 nav2 기대 frame 명명을 분리).
import rclpy
from geometry_msgs.msg import TransformStamped
from nav_msgs.msg import Odometry
from rclpy.node import Node
from rclpy.qos import qos_profile_sensor_data
from tf2_ros import TransformBroadcaster


class ConvoyOdomTf(Node):
    def __init__(self):
        super().__init__("convoy_odom_tf")
        # odom_topic 은 상대명(기본 "odom") → 네임스페이스 하에서 /robot_<id>/odom 으로 해석.
        self.odom_topic = self.declare_parameter("odom_topic", "odom").value
        # nav2 가 기대하는 tf frame. 런처가 네임스페이스 prefix 를 넣어 주입.
        self.parent_frame = self.declare_parameter("parent_frame", "odom").value
        self.child_frame = self.declare_parameter("child_frame", "base_footprint").value
        self.bc = TransformBroadcaster(self)
        self.create_subscription(
            Odometry, self.odom_topic, self.on_odom, qos_profile_sensor_data
        )
        self.get_logger().info(
            f"convoy_odom_tf: {self.odom_topic} → tf({self.parent_frame}"
            f"→{self.child_frame})"
        )

    def on_odom(self, m):
        t = TransformStamped()
        # 입력 stamp 보존(sim time). frame 은 파라미터(메시지 frame_id 비의존).
        t.header.stamp = m.header.stamp
        t.header.frame_id = self.parent_frame
        t.child_frame_id = self.child_frame
        t.transform.translation.x = m.pose.pose.position.x
        t.transform.translation.y = m.pose.pose.position.y
        t.transform.translation.z = m.pose.pose.position.z
        t.transform.rotation = m.pose.pose.orientation
        self.bc.sendTransform(t)


def main():
    rclpy.init()
    rclpy.spin(ConvoyOdomTf())
    rclpy.shutdown()


if __name__ == "__main__":
    main()
