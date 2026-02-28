import rclpy
from rclpy.node import Node
from skyhunter_msgs.msg import LeaderState, SwarmHeartbeat

class LeaderRelay(Node):
    def __init__(self):
        super().__init__('r1_virtual_relay')
        # Sub to Global R1, Pub to Virtual Swarm
        self.sub = self.create_subscription(LeaderState, '/leader_state', self.cb, 10)
        self.pub = self.create_publisher(LeaderState, '/swarm/virtual_leader/state', 10)
        self.hb_pub = self.create_publisher(SwarmHeartbeat, '/swarm/heartbeat', 10)
        self.timer = self.create_timer(0.5, self.hb_cb)

    def cb(self, msg):
        self.pub.publish(msg) # Forward R1 data to the swarm

    def hb_cb(self):
        hb = SwarmHeartbeat()
        hb.robot_id = "robot_01"
        hb.is_leader = True
        self.hb_pub.publish(hb) # Keep the swarm from taking over too early

def main():
    rclpy.init()
    rclpy.spin(LeaderRelay())
    rclpy.shutdown()