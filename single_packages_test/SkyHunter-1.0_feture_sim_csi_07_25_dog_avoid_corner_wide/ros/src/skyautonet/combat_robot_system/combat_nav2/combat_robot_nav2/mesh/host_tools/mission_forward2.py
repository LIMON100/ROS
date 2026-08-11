#!/usr/bin/env python3
# mission_forward2.py <rid> [dist_m] [yaw_deg]
#   근본원인 fix: LOAD 를 '팔로워 FSM 이 mesh 로 매칭될 때까지' 충분히 오래 반복 발행한 뒤 START.
#   (기존 mission_forward 는 LOAD 를 1.2s 만 쏘고 START → 늦게 매칭되는 팔로워 FSM 이 LOAD 를 놓침)
import sys, time, json, math, rclpy
from rclpy.node import Node
from rclpy.qos import QoSProfile, ReliabilityPolicy, HistoryPolicy, DurabilityPolicy
from sensor_msgs.msg import NavSatFix
from combat_robot_msgs.msg import SwarmPathCommand

CMD = {"start": 1, "stop": 2, "load": 5}

def main():
    rid = sys.argv[1]
    dist = float(sys.argv[2]) if len(sys.argv) > 2 else 3.0
    yaw = math.radians(float(sys.argv[3])) if len(sys.argv) > 3 else 0.0
    ns = f"s{rid}"
    rclpy.init()
    n = Node("mission_forward2", namespace=ns)

    fix = {}
    def cb(m):
        if m.latitude != 0.0 or m.longitude != 0.0:
            fix['lat'] = m.latitude; fix['lon'] = m.longitude
    qf = QoSProfile(depth=10); qf.reliability = ReliabilityPolicy.BEST_EFFORT
    n.create_subscription(NavSatFix, f"/{ns}/fix", cb, qf)

    t0 = time.time()
    while 'lat' not in fix and time.time() - t0 < 15:
        rclpy.spin_once(n, timeout_sec=0.2)
    if 'lat' not in fix:
        n.get_logger().error("fix 확보 실패"); return

    lat0, lon0 = fix['lat'], fix['lon']
    dEast = dist * math.cos(yaw); dNorth = dist * math.sin(yaw)
    dlat = dNorth / 111320.0
    dlon = dEast / (111320.0 * math.cos(math.radians(lat0)))
    wp = [{"lat": lat0, "lon": lon0}, {"lat": lat0 + dlat, "lon": lon0 + dlon}]
    pj = json.dumps({"waypoints": wp})
    n.get_logger().info(f"yaw={math.degrees(yaw):.1f}deg 전방 {dist}m nw={len(wp)}")

    q = QoSProfile(depth=10)
    q.reliability = ReliabilityPolicy.RELIABLE
    q.history = HistoryPolicy.KEEP_LAST
    q.durability = DurabilityPolicy.TRANSIENT_LOCAL   # 늦은 구독자도 마지막 LOAD 받도록 latch
    pub = n.create_publisher(SwarmPathCommand, '/swarm/path_command', q)

    def mk(cmd, path=""):
        m = SwarmPathCommand()
        m.header.stamp = n.get_clock().now().to_msg()
        m.command = CMD[cmd]
        m.num_waypoints = path.count('"lat"') if path else 0
        m.path_json = path
        return m

    # LOAD 를 '전 보드 FSM 이 discovery 될 때까지' 반복 발행(latch 유지).
    #   fresh sender 는 mesh discovery 가 느려 subs 가 천천히 오른다 → 목표 subs 도달
    #   또는 최대 20s 까지 유지. (기대 subs = 리더+팔로워 FSM 수. 인자 4번째로 조정)
    want = int(sys.argv[4]) if len(sys.argv) > 4 else 3
    load_msg = mk("load", pj)
    n.get_logger().info(f"LOAD 반복 발행 — 전 보드 FSM(subs>={want}) discovery 대기...")
    t0 = time.time()
    while time.time() - t0 < 20.0:
        pub.publish(load_msg)
        rclpy.spin_once(n, timeout_sec=0.05)
        time.sleep(0.25)
        if pub.get_subscription_count() >= want and time.time() - t0 > 3.0:
            break
    n.get_logger().info(f"LOAD 발행완료 subs={pub.get_subscription_count()} ({time.time()-t0:.1f}s)")

    # START 반복 발행 (2s)
    start_msg = mk("start")
    t0 = time.time()
    while time.time() - t0 < 2.0:
        pub.publish(start_msg)
        rclpy.spin_once(n, timeout_sec=0.05)
        time.sleep(0.25)
    n.get_logger().info(f"START 발행완료 subs={pub.get_subscription_count()}")
    time.sleep(0.5)
    n.destroy_node(); rclpy.shutdown()

if __name__ == "__main__":
    main()
