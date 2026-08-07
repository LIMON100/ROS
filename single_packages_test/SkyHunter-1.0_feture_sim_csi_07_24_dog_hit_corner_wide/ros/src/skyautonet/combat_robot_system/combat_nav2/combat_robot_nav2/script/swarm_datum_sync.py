#!/usr/bin/env python3
"""Leader-broadcast GNSS datum sync for the swarm.

각 로봇의 navsat_transform 은 wait_for_datum:false 면 자기 첫 GPS fix 를 datum 으로
자동 설정한다 → 로봇마다 datum(=ENU world 원점)이 달라 /swarm/obstacles·formation 의
world 좌표 비교가 깨진다. 이 노드는 datum 을 한 점으로 통일한다:

  · 리더(robot_id==leader_robot_id): 첫 유효 fix 를 datum 으로 결정해
    /swarm/datum (latched) 으로 브로드캐스트.
  · 리더·팔로워 전원: /swarm/datum 을 받아 자기 navsat 의 SetDatum 서비스('datum')
    를 호출 → 모두 동일 datum 으로 ENU 좌표계 정렬.

DatumStr 가 latched(transient_local)라 늦게 뜬 팔로워도 마지막 datum 을 받는다.
datum 은 한 번만 설정(이후 무시)해 좌표계가 주행 중 흔들리지 않게 한다.
"""
import rclpy
from rclpy.node import Node
from rclpy.qos import QoSProfile, DurabilityPolicy, ReliabilityPolicy
from sensor_msgs.msg import NavSatFix
from geographic_msgs.msg import GeoPose
from robot_localization.srv import SetDatum


class SwarmDatumSync(Node):
    def __init__(self):
        super().__init__('swarm_datum_sync')
        robot_id = int(self.declare_parameter('robot_id', 1).value)
        leader_id = int(self.declare_parameter('leader_robot_id', 1).value)
        self.is_leader = (robot_id == leader_id)
        self.datum_set = False
        self.published = False

        latched = QoSProfile(depth=1)
        latched.durability = DurabilityPolicy.TRANSIENT_LOCAL
        latched.reliability = ReliabilityPolicy.RELIABLE

        # 공유 datum 버스 — 절대 토픽(전 로봇 공통), latched
        self.datum_pub = self.create_publisher(GeoPose, '/swarm/datum', latched)
        self.datum_sub = self.create_subscription(
            GeoPose, '/swarm/datum', self.on_datum, latched)
        # navsat SetDatum 서비스 (네임스페이스 상대 → /sN/datum)
        self.cli = self.create_client(SetDatum, 'datum')

        if self.is_leader:
            self.fix_sub = self.create_subscription(
                NavSatFix, 'fix', self.on_fix, 10)
            self.get_logger().info(
                'datum sync: LEADER — 첫 유효 fix 를 datum 으로 브로드캐스트')
        else:
            self.get_logger().info(
                'datum sync: FOLLOWER — /swarm/datum 대기')

    def on_fix(self, msg: NavSatFix):
        if self.published:
            return
        if msg.status.status < 0:                 # NavSatStatus.STATUS_NO_FIX
            return
        gp = GeoPose()
        gp.position.latitude = float(msg.latitude)
        gp.position.longitude = float(msg.longitude)
        gp.position.altitude = 0.0
        gp.orientation.w = 1.0                    # datum heading = ENU(yaw 0)
        self.datum_pub.publish(gp)
        self.published = True
        self.get_logger().info(
            '리더 datum 브로드캐스트: lat=%.7f lon=%.7f'
            % (msg.latitude, msg.longitude))

    def on_datum(self, gp: GeoPose):
        if self.datum_set:
            return
        if not self.cli.service_is_ready():
            if not self.cli.wait_for_service(timeout_sec=2.0):
                self.get_logger().warn('navsat datum 서비스 미발견 — 다음 수신 때 재시도')
                return
        req = SetDatum.Request()
        req.geo_pose = gp
        self.cli.call_async(req)
        self.datum_set = True
        self.get_logger().info(
            'navsat datum 설정 완료: lat=%.7f lon=%.7f'
            % (gp.position.latitude, gp.position.longitude))


def main():
    rclpy.init()
    node = SwarmDatumSync()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        if rclpy.ok():
            rclpy.shutdown()


if __name__ == '__main__':
    main()
