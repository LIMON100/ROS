import math
import rclpy
from rclpy.node import Node
from rclpy.action import ActionClient
from rclpy.qos import QoSProfile, QoSReliabilityPolicy
from rclpy.time import Time

import tf2_ros
from std_msgs.msg import Header
from geometry_msgs.msg import PointStamped, PoseStamped
from nav2_msgs.action import NavigateThroughPoses
from combat_robot_msgs.msg import ThreatAlert


class ThreatToEncircle(Node):
    def __init__(self):
        super().__init__("threat_to_encircle")
        self.declare_parameter("leader_robot_id", 1)          # int (matches -p ...:=1)
        self.declare_parameter("leader_base_frame", "base_footprint")
        self.declare_parameter("map_frame", "map")
        self.declare_parameter("person_threat_type", 99)
        self.declare_parameter("min_range_m", 0.5)
        self.declare_parameter("max_range_m", 30.0)
        self.declare_parameter("publish_rate_hz", 5.0)
        self.declare_parameter("target_hold_s", 1.5)
        self.declare_parameter("smoothing", 0.3)
        self.declare_parameter("combat_standoff_m", 6.0)      # leader stops this far from person
        self.declare_parameter("nav_action", "navigate_through_poses")
        
        self.leader_id = str(self.get_parameter("leader_robot_id").value)
        self.base_frame = str(self.get_parameter("leader_base_frame").value)
        self.map_frame = str(self.get_parameter("map_frame").value)
        self.person_type = int(self.get_parameter("person_threat_type").value)
        self.min_r = float(self.get_parameter("min_range_m").value)
        self.max_r = float(self.get_parameter("max_range_m").value)
        self.hold_s = float(self.get_parameter("target_hold_s").value)
        self.alpha = float(self.get_parameter("smoothing").value)
        self.standoff = float(self.get_parameter("combat_standoff_m").value)
        
        self.tf_buffer = tf2_ros.Buffer()
        self.tf_listener = tf2_ros.TransformListener(self.tf_buffer, self)
        self._nav = ActionClient(
            self, NavigateThroughPoses,
            str(self.get_parameter("nav_action").value))
            
        self.target = None
        self.last_seen = None
        self.engaged = False     # latch: leader sent to standoff once
        
        qos = QoSProfile(depth=10)
        qos.reliability = QoSReliabilityPolicy.RELIABLE
        self.create_subscription(
            ThreatAlert, "/swarm/threat_alert_raw", self._on_threat, qos)
        self.pub = self.create_publisher(
            PointStamped, "/swarm/encircle_target", qos)
        self.create_timer(
            1.0 / float(self.get_parameter("publish_rate_hz").value), self._tick)
            
        self.get_logger().info(
            f"ThreatToEncircle (engagement) UP: leader={self.leader_id} "
            f"standoff={self.standoff}m person_type={self.person_type}")
            
    def _on_threat(self, msg: ThreatAlert):
        if msg.threat_type != self.person_type or not msg.has_position:
            return
        if str(msg.source_robot_id) != self.leader_id:
            return
        if not (self.min_r <= msg.range_m <= self.max_r):
            return
        try:
            tf = self.tf_buffer.lookup_transform(
                self.map_frame, self.base_frame, Time())
        except Exception:
            return
        lx = tf.transform.translation.x
        ly = tf.transform.translation.y
        b = math.radians(msg.bearing_deg)            # world-frame
        tx = lx + msg.range_m * math.cos(b)
        ty = ly + msg.range_m * math.sin(b)
        if self.target is None: 
            self.target = (tx, ty)
        else:
            self.target = (self.target[0] * (1 - self.alpha) + tx * self.alpha,
                            self.target[1] * (1 - self.alpha) + ty * self.alpha)
        self.last_seen = self.get_clock().now()
        
        # First confirmed person -> advance the leader to the standoff (once).
        if not self.engaged:
            self._engage_leader(lx, ly, self.target[0], self.target[1])
            
    def _engage_leader(self, lx, ly, tx, ty):
        ang = math.atan2(ly - ty, lx - tx)           # target -> leader
        cx = tx + self.standoff * math.cos(ang)
        cy = ty + self.standoff * math.sin(ang)
        yaw = ang + math.pi                          # face the target
        if not self._nav.wait_for_server(timeout_sec=2.0):
            self.get_logger().error("Nav2 unavailable — cannot send leader standoff goal")
            return
        ps = PoseStamped()
        ps.header.frame_id = self.map_frame
        ps.header.stamp = self.get_clock().now().to_msg()
        ps.pose.position.x = cx
        ps.pose.position.y = cy
        ps.pose.orientation.z = math.sin(yaw / 2.0)
        ps.pose.orientation.w = math.cos(yaw / 2.0)
        goal = NavigateThroughPoses.Goal()
        goal.poses = [ps]
        self._nav.send_goal_async(goal)
        self.engaged = True
        self.get_logger().warn(
            f"ENGAGE: leader -> standoff ({cx:.1f},{cy:.1f}) facing person; "
            f"followers encircling")
            
    def _tick(self):
        if self.target is None:
            return
        # Pre-engage: drop a target that went stale before we committed.
        # Post-engage: LATCH — keep broadcasting so followers hold the ring
        # even when detection flickers (skyhunter atomic combat state).
        if (not self.engaged and self.last_seen is not None and
                (self.get_clock().now() - self.last_seen).nanoseconds * 1e-9 > self.hold_s):
            self.target = None
            return
        m = PointStamped()
        m.header = Header(frame_id=self.map_frame)
        m.header.stamp = self.get_clock().now().to_msg()
        m.point.x, m.point.y, m.point.z = float(self.target[0]), float(self.target[1]), 0.0
        self.pub.publish(m)
        # Visible heartbeat — proves publishing without racing `topic hz`.
        self._pub_n = getattr(self, "_pub_n", 0) + 1
        if self._pub_n % 25 == 1:                      # ~every 5 s at 5 Hz
            self.get_logger().info(
                f"ENCIRCLE pub -> ({self.target[0]:.1f}, {self.target[1]:.1f}) engaged={self.engaged}")



def main(args=None):
    rclpy.init(args=args)
    node = ThreatToEncircle()
    try: 
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()
        
        
if __name__ == "__main__":
    main()