#!/usr/bin/env python3
"""
mock_board.py — 실차 보드(mesh_board1/2) 없이 보드가 내보내는 데이터를 그대로 흉내내는 시뮬레이터.

보드 1대 = 이 노드 1개. namespace(ns)만 s1/s2로 바꿔 두 번 실행하면 두 보드를 흉내낸다.
호스트 rviz(rviz_both.sh)·swarm 조율·command_server·데이터 송수신 테스트를 하드웨어 없이 할 수 있다.

발행 토픽(실보드와 동일 이름/타입/대략적 rate):
  /{ns}/fix                sensor_msgs/NavSatFix     20Hz  (RTK, status=2)
  /{ns}/imu/data           sensor_msgs/Imu           50Hz
  /{ns}/edge_heading       std_msgs/Float64          20Hz  (dual-antenna heading, deg 0=N cw)
  /{ns}/vel                geometry_msgs/TwistStamped 20Hz (ENU 속도: x=E, y=N)
  /{ns}/odom               nav_msgs/Odometry         20Hz  (휠 오도, base_footprint)
  /{ns}/odometry/global    nav_msgs/Odometry         20Hz  (EKF map pose)
  /{ns}/local_costmap/costmap  nav_msgs/OccupancyGrid 2Hz  (10m 빈 롤링맵)
  TF   {ns}/map->{ns}/odom, {ns}/odom->{ns}/base_footprint       20Hz
  TF_static {ns}/base_footprint->{ns}/base_link                  once

파라미터:
  ns            (str, 's1')      네임스페이스
  datum_lat/lon (float)          GPS datum (기본 인천 37.5934/126.6232)
  spawn_x/y     (float, 0/0)     map 내 시작 위치 [m] (ENU)
  heading_deg   (float, 45.0)    시작 heading [deg, ENU 0=E,90=N]
  antenna_rev   (bool, False)    True면 edge_heading을 180° 반대로(안테나 반대 상황 재현)
  move          (bool, False)    True면 heading 방향으로 전진(course/heading 테스트용)
  speed         (float, 0.6)     move 속도 [m/s]

실행 예:
  python3 mock_board.py --ros-args -p ns:=s1
  python3 mock_board.py --ros-args -p ns:=s2 -p spawn_x:=-2.0 -p heading_deg:=45 -p move:=true
"""
import math
import rclpy
from rclpy.node import Node
from rclpy.qos import QoSProfile, QoSDurabilityPolicy, QoSReliabilityPolicy
from builtin_interfaces.msg import Time as TimeMsg
from std_msgs.msg import Float64
from sensor_msgs.msg import NavSatFix, NavSatStatus, Imu
from geometry_msgs.msg import TwistStamped, TransformStamped, Quaternion
from nav_msgs.msg import Odometry, OccupancyGrid
from tf2_msgs.msg import TFMessage

EARTH = 111320.0  # m/deg (위도)


def yaw_to_quat(yaw):
    q = Quaternion()
    q.z = math.sin(yaw / 2.0)
    q.w = math.cos(yaw / 2.0)
    return q


def enu_to_compass(enu_deg):
    # ENU(0=E,90=N,ccw) -> compass(0=N,90=E,cw)
    return (90.0 - enu_deg) % 360.0


class MockBoard(Node):
    def __init__(self):
        super().__init__('mock_board')
        self.ns = self.declare_parameter('ns', 's1').value
        self.datum_lat = self.declare_parameter('datum_lat', 37.5934).value
        self.datum_lon = self.declare_parameter('datum_lon', 126.6232).value
        self.spawn_x = self.declare_parameter('spawn_x', 0.0).value
        self.spawn_y = self.declare_parameter('spawn_y', 0.0).value
        self.heading = math.radians(self.declare_parameter('heading_deg', 45.0).value)  # ENU rad
        self.antenna_rev = self.declare_parameter('antenna_rev', False).value
        self.move = self.declare_parameter('move', False).value
        self.speed = self.declare_parameter('speed', 0.6).value

        ns = self.ns
        latch = QoSProfile(depth=1, durability=QoSDurabilityPolicy.TRANSIENT_LOCAL,
                           reliability=QoSReliabilityPolicy.RELIABLE)
        self.pub_fix = self.create_publisher(NavSatFix, f'/{ns}/fix', 10)
        self.pub_imu = self.create_publisher(Imu, f'/{ns}/imu/data', 20)
        self.pub_edge = self.create_publisher(Float64, f'/{ns}/edge_heading', 10)
        self.pub_vel = self.create_publisher(TwistStamped, f'/{ns}/vel', 10)
        self.pub_odom = self.create_publisher(Odometry, f'/{ns}/odom', 10)
        self.pub_glob = self.create_publisher(Odometry, f'/{ns}/odometry/global', 10)
        self.pub_cm = self.create_publisher(OccupancyGrid, f'/{ns}/local_costmap/costmap', 1)
        self.pub_tf = self.create_publisher(TFMessage, f'/{ns}/tf', 20)
        self.pub_tfs = self.create_publisher(TFMessage, f'/{ns}/tf_static', latch)

        # map 내 현재 위치(ENU) — spawn에서 시작
        self.x = float(self.spawn_x)
        self.y = float(self.spawn_y)

        self.publish_tf_static()
        self.publish_costmap()  # 초기 1회
        self.create_timer(0.05, self.tick_fast)     # 20Hz: fix/vel/odom/global/tf/edge
        self.create_timer(0.02, self.tick_imu)      # 50Hz: imu
        self.create_timer(0.5, self.publish_costmap)  # 2Hz: costmap
        self.get_logger().info(
            f'mock_board ns={ns} spawn=({self.x:.1f},{self.y:.1f}) '
            f'heading={math.degrees(self.heading):.0f}° antenna_rev={self.antenna_rev} move={self.move}')

    def now_msg(self):
        return self.get_clock().now().to_msg()

    def tick_imu(self):
        t = self.now_msg()
        imu = Imu()
        imu.header.stamp = t
        imu.header.frame_id = f'{self.ns}/imu_link'
        imu.orientation = yaw_to_quat(self.heading)
        imu.orientation_covariance[0] = 0.01
        imu.angular_velocity_covariance[0] = 0.001
        imu.linear_acceleration_covariance[0] = 0.01
        imu.linear_acceleration.z = 9.81
        self.pub_imu.publish(imu)

    def tick_fast(self):
        t = self.now_msg()
        # 이동
        vx = vy = 0.0
        if self.move:
            vx = self.speed * math.cos(self.heading)  # East
            vy = self.speed * math.sin(self.heading)  # North
            self.x += vx * 0.05
            self.y += vy * 0.05

        # fix (datum + ENU offset -> lat/lon)
        fix = NavSatFix()
        fix.header.stamp = t
        fix.header.frame_id = 'gps'
        fix.status.status = NavSatStatus.STATUS_SBAS_FIX
        fix.status.service = NavSatStatus.SERVICE_GPS
        coslat = math.cos(math.radians(self.datum_lat))
        fix.latitude = self.datum_lat + self.y / EARTH
        fix.longitude = self.datum_lon + self.x / (EARTH * coslat)
        fix.altitude = 15.0
        fix.position_covariance = [0.01, 0, 0, 0, 0.01, 0, 0, 0, 0.02]
        fix.position_covariance_type = 2
        self.pub_fix.publish(fix)

        # edge_heading (dual-antenna, compass deg). antenna_rev면 180° 반대
        edge = enu_to_compass(math.degrees(self.heading))
        if self.antenna_rev:
            edge = (edge + 180.0) % 360.0
        self.pub_edge.publish(Float64(data=edge))

        # vel (ENU)
        vel = TwistStamped()
        vel.header.stamp = t
        vel.header.frame_id = 'gps'
        vel.twist.linear.x = vx
        vel.twist.linear.y = vy
        self.pub_vel.publish(vel)

        # odom (휠, odom frame)
        od = Odometry()
        od.header.stamp = t
        od.header.frame_id = f'{self.ns}/odom'
        od.child_frame_id = f'{self.ns}/base_footprint'
        od.pose.pose.position.x = self.x
        od.pose.pose.position.y = self.y
        od.pose.pose.orientation = yaw_to_quat(self.heading)
        od.twist.twist.linear.x = self.speed if self.move else 0.0
        self.pub_odom.publish(od)

        # odometry/global (EKF, map frame)
        og = Odometry()
        og.header.stamp = t
        og.header.frame_id = f'{self.ns}/map'
        og.child_frame_id = f'{self.ns}/base_footprint'
        og.pose.pose.position.x = self.x
        og.pose.pose.position.y = self.y
        og.pose.pose.orientation = yaw_to_quat(self.heading)
        self.pub_glob.publish(og)

        # TF: map->odom (identity), odom->base_footprint (pose)
        tfm = TFMessage()
        m2o = TransformStamped()
        m2o.header.stamp = t
        m2o.header.frame_id = f'{self.ns}/map'
        m2o.child_frame_id = f'{self.ns}/odom'
        tfm.transforms.append(m2o)
        o2b = TransformStamped()
        o2b.header.stamp = t
        o2b.header.frame_id = f'{self.ns}/odom'
        o2b.child_frame_id = f'{self.ns}/base_footprint'
        o2b.transform.translation.x = self.x
        o2b.transform.translation.y = self.y
        o2b.transform.rotation = yaw_to_quat(self.heading)
        tfm.transforms.append(o2b)
        self.pub_tf.publish(tfm)

    def publish_tf_static(self):
        t = self.now_msg()
        tfm = TFMessage()
        b2b = TransformStamped()
        b2b.header.stamp = t
        b2b.header.frame_id = f'{self.ns}/base_footprint'
        b2b.child_frame_id = f'{self.ns}/base_link'
        b2b.transform.translation.z = 0.38
        b2b.transform.rotation.w = 1.0
        tfm.transforms.append(b2b)
        self.pub_tfs.publish(tfm)

    def publish_costmap(self):
        og = OccupancyGrid()
        og.header.stamp = self.now_msg()
        og.header.frame_id = f'{self.ns}/odom'
        res = 0.05
        n = 200  # 10m
        og.info.resolution = res
        og.info.width = n
        og.info.height = n
        og.info.origin.position.x = self.x - n * res / 2.0
        og.info.origin.position.y = self.y - n * res / 2.0
        og.info.origin.orientation.w = 1.0
        og.data = [0] * (n * n)  # 빈 맵(free)
        self.pub_cm.publish(og)


def main():
    rclpy.init()
    node = MockBoard()
    try:
        rclpy.spin(node)
    except (KeyboardInterrupt, Exception):
        pass
    finally:
        try:
            rclpy.shutdown()
        except Exception:
            pass


if __name__ == '__main__':
    main()
