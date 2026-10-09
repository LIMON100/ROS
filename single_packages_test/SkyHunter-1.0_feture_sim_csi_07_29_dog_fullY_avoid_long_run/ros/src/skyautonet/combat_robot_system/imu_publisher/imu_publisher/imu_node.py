#!/usr/bin/env python3
"""
wt901_node.py – Dual BLE IMU (WT901 & WTS) with random-address connect 토픽
  /imu/gun_angle   geometry_msgs/Vector3Stamped  (WT901  roll-pitch-yaw deg)
  /imu/car_angle   geometry_msgs/Vector3Stamped  (WTS    roll-pitch-yaw deg)
  /imu/difference  geometry_msgs/Vector3Stamped  (gun – car deg)

MAC 주소·BLE 동글(HCI) 번호는
  share/wt901_publisher/config/imu_mac.yaml 에서 읽습니다.
"""

import math, struct, threading, time, yaml, os, sys
import rclpy
from rclpy.node        import Node
from geometry_msgs.msg import Vector3Stamped
from combat_robot_msgs.msg import IMUState
from ament_index_python.packages import get_package_share_directory
from bluepy import btle

# ---------- BLE UUID 접미사 ----------
NOTIFY_SUF = ("ffe4", "fff1", "fff3")
WRITE_SUF  = ("ffe9", "fff2", "fff5")
i16x3      = lambda b: struct.unpack("<hhh", b)

# ---------- GATT handle 검색 ----------
def find_handles(periph, log, tag):
    notif = write = None
    for ch in periph.getCharacteristics():
        uid, prop = str(ch.uuid).lower(), ch.propertiesToString().lower()
        if any(s in uid for s in NOTIFY_SUF) and "notify" in prop:
            notif = ch.getHandle()
        if any(s in uid for s in WRITE_SUF)  and "write"  in prop:
            write = ch.getHandle()
    if notif is None:
        log.error(f"[{tag}] ❌ notify handle not found – GATT table dump:")
        for ch in periph.getCharacteristics():
            log.info(f"[{tag}]   0x{ch.getHandle():04X} {ch.uuid} {ch.propertiesToString()}")
    return notif, write

# ---------- 디바이스 스레드 ----------
class IMUDevice(threading.Thread):
    def __init__(self, node, mac, topic, key, hci=0):
        super().__init__(daemon=True)
        self.node, self.mac, self.key, self.hci = node, mac, key, hci
        self.pub = node.create_publisher(IMUState, topic, 10)
        self.last = None
        self.lock = threading.Lock()
        self.is_connected = False
        self.start()

    def run(self):
        log = self.node.get_logger()
        while rclpy.ok():
            try:
                log.info(f"[{self.key}] 🔗 connect {self.mac} (random addr)")
                periph = btle.Peripheral(self.mac,
                                         addrType=btle.ADDR_TYPE_RANDOM,
                                         iface=self.hci)
                self.is_connected = True
                log.info(f"[{self.key}]   ✓ connected")
                notif, write = find_handles(periph, log, self.key)
                if notif is None:
                    raise RuntimeError("notify handle missing")

                periph.writeCharacteristic(notif + 1, b"\x01\x00", False)
                log.info(f"[{self.key}]   ✓ notify enabled")

                periph.withDelegate(self._Delegate(self))
                next_mag = time.time() + 1

                while rclpy.ok():
                    periph.waitForNotifications(1)
                    if write and time.time() >= next_mag:
                        try:
                            periph.writeCharacteristic(write, b"\xFF\xAA\x27\x3A\x00")
                        except btle.BTLEException:
                            pass
                        next_mag = time.time() + 1

            except Exception as e:
                log.warning(f"[{self.key}] BLE error: {e}")
                self.is_connected = False
                # Publish disconnected state
                msg = IMUState()
                msg.header.stamp = self.node.get_clock().now().to_msg()
                msg.is_connected = False
                msg.device_id = self.key
                self.pub.publish(msg)
            finally:
                try:
                    periph.disconnect()
                except Exception:
                    pass
                time.sleep(1)

    # ── 알림 처리 ──
    class _Delegate(btle.DefaultDelegate):
        def __init__(s, ctx): super().__init__(); s.ctx = ctx
        def handleNotification(s, h, data):
            for i in range(0, len(data), 20):
                pkt = data[i:i+20]
                if len(pkt) == 20 and pkt[0] == 0x55 and pkt[1] == 0x61:
                    ang = [v / 32768 * 180 for v in i16x3(pkt[14:20])]
                    s.ctx._publish_angle(ang)

    def _publish_angle(self, ang):
        msg = IMUState()
        msg.header.stamp = self.node.get_clock().now().to_msg()
        msg.angle.x, msg.angle.y, msg.angle.z = ang
        msg.is_connected = self.is_connected
        msg.device_id = self.key
        self.pub.publish(msg)
        with self.lock:
            self.last = ang
        self.node._try_publish_diff()

# ---------- 메인 노드 ----------
class DualIMU(Node):
    def __init__(self):
        super().__init__("imu_ble_node")
        cfg = self._load_yaml()
        hci = cfg.get("hci", 0)

        self.devs = {
            "gun": IMUDevice(self, cfg["gun"], "/imu/gun_angle", "gun", hci),
            "car": IMUDevice(self, cfg["car"], "/imu/car_angle", "car", hci),
        }
        self.diff_pub = self.create_publisher(Vector3Stamped,
                                              "/imu/difference", 10)

    def _load_yaml(self):
        path = os.getenv("IMU_MAC_YAML") or os.path.join(
            get_package_share_directory("imu_publisher"),
            "config", "imu_mac.yaml")
        if not os.path.isfile(path):
            self.get_logger().fatal(f"YAML not found: {path}"); sys.exit(1)
        with open(path, "r") as f:
            return yaml.safe_load(f)

    def _try_publish_diff(self):
        g = self.devs["gun"].last
        c = self.devs["car"].last
        if g is None or c is None:
            return
        diff = [gi - ci for gi, ci in zip(g, c)]
        msg = Vector3Stamped()
        msg.header.stamp = self.get_clock().now().to_msg()
        msg.vector.x, msg.vector.y, msg.vector.z = diff
        self.diff_pub.publish(msg)

# ---------- main ----------
def main(args=None):
    rclpy.init(args=args)
    node = DualIMU()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    rclpy.shutdown()

if __name__ == "__main__":
    main()
