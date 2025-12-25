# In swarm_control/sim_engine_node.py
# (This is a simplified physics engine. Gazebo is the real version)
import rclpy
from rclpy.node import Node
from geometry_msgs.msg import Twist, Pose
from nav_msgs.msg import Odometry

class SimEngineNode(Node):
    def __init__(self):
        super().__init__('sim_engine')
        self.num_robots = 5 # Change as needed
        self.poses = [Pose() for _ in range(self.num_robots)]
        # Initialize starting positions
        # ... (set initial x, y for each robot)

        self.subs = []
        self.pubs = []

        for i in range(self.num_robots):
            # Listen for velocity commands
            self.subs.append(self.create_subscription(
                Twist, f'/robot_{i}/cmd_vel', 
                lambda msg, robot_id=i: self.cmd_vel_callback(msg, robot_id), 10))
            
            # Publish odometry
            self.pubs.append(self.create_publisher(Odometry, f'/robot_{i}/odom', 10))
            
        self.timer = self.create_timer(0.05, self.update_and_publish) # 20 Hz

    def cmd_vel_callback(self, msg, robot_id):
        # Apply physics (simplified)
        self.poses[robot_id].position.x += msg.linear.x * 0.05
        self.poses[robot_id].position.y += msg.linear.y * 0.05
        # ... (handle angular velocity and orientation later)

    def update_and_publish(self):
        for i in range(self.num_robots):
            odom_msg = Odometry()
            odom_msg.header.stamp = self.get_clock().now().to_msg()
            odom_msg.header.frame_id = 'odom'
            odom_msg.child_frame_id = f'robot_{i}/base_link'
            odom_msg.pose.pose = self.poses[i]
            self.pubs[i].publish(odom_msg)