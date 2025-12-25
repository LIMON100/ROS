import rclpy
from rclpy.node import Node
from geometry_msgs.msg import Point, Twist
from nav_msgs.msg import Odometry
# ... (your DWA logic would be imported or included here)

class RobotDriverNode(Node):
    def __init__(self, robot_id):
        # The node name must be unique
        super().__init__(f'robot_driver_{robot_id}')
        self.robot_id = robot_id
        
        self.get_logger().info(f"Driver for Robot {self.robot_id} has started.")
        
        self.goal = None
        self.current_odom = None
        
        # Subscribe to this robot's specific goal topic
        self.goal_sub = self.create_subscription(
            Point, f'/robot_{self.robot_id}/goal', self.goal_callback, 10)
        
        # Subscribe to this robot's specific odom topic
        self.odom_sub = self.create_subscription(
            Odometry, f'/robot_{self.robot_id}/odom', self.odom_callback, 10)
            
        # Publish to this robot's specific velocity topic
        self.cmd_vel_pub = self.create_publisher(Twist, f'/robot_{self.robot_id}/cmd_vel', 10)
        
        self.timer = self.create_timer(0.05, self.run_dwa)

    def goal_callback(self, msg):
        self.goal = msg
        
    def odom_callback(self, msg):
        self.current_odom = msg

    def run_dwa(self):
        if self.goal is None or self.current_odom is None:
            return
            
        # --- Your DWA logic from formation.py goes here ---
        # It calculates best_vx, best_vy based on self.goal and self.current_odom
        # For this example, let's just make it move simply towards the goal
        dx = self.goal.x - self.current_odom.pose.pose.position.x
        dy = self.goal.y - self.current_odom.pose.pose.position.y
        dist = (dx**2 + dy**2)**0.5
        
        best_vx = 0.0
        best_vy = 0.0
        if dist > 0.1: # A small threshold
            speed = 1.0
            best_vx = (dx / dist) * speed
            best_vy = (dy / dist) * speed
        # ----------------------------------------------------

        cmd_msg = Twist()
        cmd_msg.linear.x = best_vx
        cmd_msg.linear.y = best_vy
        self.cmd_vel_pub.publish(cmd_msg)

# --- Main Functions for Each Robot ---

def run_driver(robot_id):
    rclpy.init()
    node = RobotDriverNode(robot_id)
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()

def main_robot0(args=None): run_driver(0)
def main_robot1(args=None): run_driver(1)
def main_robot2(args=None): run_driver(2)
def main_robot3(args=None): run_driver(3)
def main_robot4(args=None): run_driver(4)