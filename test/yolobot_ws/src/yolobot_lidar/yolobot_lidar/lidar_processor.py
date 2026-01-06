# #!/usr/bin/env python3
# import rclpy
# from rclpy.node import Node
# from sensor_msgs.msg import PointCloud2
# from std_msgs.msg import String # We'll publish a simple string message for now

# # ros2_numpy is a library for converting ROS messages to numpy arrays
# import ros2_numpy as rnp
# import numpy as np

# # We will use scikit-learn for a simple but effective clustering algorithm
# from sklearn.cluster import DBSCAN

# class LidarProcessorNode(Node):
#     def __init__(self):
#         super().__init__('lidar_processor')
        
        
#         self.ROI_X_MIN = 0.2   # Ignore points very close to the robot
#         self.ROI_X_MAX = 10.0  # Look further ahead
#         self.ROI_Y_MIN = -2.5  # Look further to the left
#         self.ROI_Y_MAX = 2.5   # Look further to the right

#         # self.ROI_Z_MIN = 0.05  # Keep this to ignore the ground
#         self.ROI_Z_MIN = -0.5  
#         self.ROI_Z_MAX = 2.0 
        
#         # DBSCAN clustering parameters
#         self.DBSCAN_EPS = 0.2  # Epsilon: Max distance between two samples for one to be considered as in the neighborhood of the other. (20cm)
#         self.DBSCAN_MIN_SAMPLES = 5 # The number of samples in a neighborhood for a point to be considered as a core point.

#         # --- Subscribers and Publishers ---
        
#         # Subscribe to the point cloud topic from Gazebo
#         self.subscription = self.create_subscription(
#             PointCloud2,
#             '/gazebo_ros_ray_sensor/out',  # IMPORTANT: This must match the topic name from 'ros2 topic list'
#             self.lidar_callback,
#             10)
        
#         # Publisher for the closest object's information
#         self.object_publisher = self.create_publisher(String, '/lidar/closest_object', 10)
        
#         self.get_logger().info('LIDAR Processor node has been started.')

#     def lidar_callback(self, msg):
#         """
#         This function is called every time a new PointCloud2 message is received.
#         """
#         # 1. Manually parse the PointCloud2 message. This is the most robust method.
#         #    We read the raw binary data and interpret it as floating-point numbers.
        
#         # Get the field offsets. The 'offset' tells us where each piece of data (x, y, z) starts
#         # inside a single point's data block.
#         offset_x = msg.fields[0].offset
#         offset_y = msg.fields[1].offset
#         offset_z = msg.fields[2].offset
#         point_step = msg.point_step  # This is the size of one point in bytes (e.g., 16)
        
#         # Convert the raw byte array to a NumPy array of floats
#         # We reshape it so that each row is a single point.
#         # We are only interested in the first 3 floats (x, y, z) if point_step is > 12.
#         num_points = msg.width * msg.height
#         try:
#             points_raw = np.frombuffer(msg.data, dtype=np.float32).reshape(-1, point_step // 4)
#             points = points_raw[:, [offset_x//4, offset_y//4, offset_z//4]]
#         except Exception as e:
#             self.get_logger().error(f"Failed to process PointCloud2 data buffer: {e}")
#             return

#         # 2. Filter the points to keep only those within our Region of Interest (ROI)
#         mask = np.where((points[:,0] > self.ROI_X_MIN) & (points[:,0] < self.ROI_X_MAX) &
#                         (points[:,1] > self.ROI_Y_MIN) & (points[:,1] < self.ROI_Y_MAX) &
#                         (points[:,2] > self.ROI_Z_MIN) & (points[:,2] < self.ROI_Z_MAX))
        
#         filtered_points = points[mask]

#         if len(filtered_points) == 0:
#             self.object_publisher.publish(String(data="No obstacles detected in ROI."))
#             return

#         # 3. Perform Clustering to group nearby points together
#         clustering = DBSCAN(eps=self.DBSCAN_EPS, min_samples=self.DBSCAN_MIN_SAMPLES).fit(filtered_points)
#         labels = clustering.labels_
        
#         unique_labels = set(labels)
#         if -1 in unique_labels:
#             unique_labels.remove(-1)
        
#         if not unique_labels:
#             self.object_publisher.publish(String(data="No clusters found, only noise."))
#             return
            
#         # 4. Find the closest object
#         closest_cluster_dist = float('inf')
#         closest_cluster_center = None

#         for label in unique_labels:
#             cluster_points = filtered_points[labels == label]
#             cluster_center = np.mean(cluster_points, axis=0)
#             distance = np.linalg.norm(cluster_center)
            
#             if distance < closest_cluster_dist:
#                 closest_cluster_dist = distance
#                 closest_cluster_center = cluster_center
        
#         # 5. Publish the result
#         if closest_cluster_center is not None:
#             result_msg = String()
#             result_msg.data = (f"Closest object detected at {closest_cluster_dist:.2f} meters. "
#                             f"Center (x,y,z): ({closest_cluster_center[0]:.2f}, "
#                             f"{closest_cluster_center[1]:.2f}, {closest_cluster_center[2]:.2f})")
#             self.object_publisher.publish(result_msg)


# def main(args=None):
#     rclpy.init(args=args)
#     node = LidarProcessorNode()
#     rclpy.spin(node)
#     node.destroy_node()
#     rclpy.shutdown()

# if __name__ == '__main__':
#     main()



#!/usr/bin/env python3
import rclpy
from rclpy.node import Node
from sensor_msgs.msg import PointCloud2
from std_msgs.msg import String
import math
import numpy as np
from sklearn.cluster import DBSCAN

class LidarProcessorNode(Node):
    def __init__(self):
        super().__init__('lidar_processor')
        
        # --- ROI SETTINGS ---
        self.ROI_X_MIN = 0.2   
        self.ROI_X_MAX = 10.0  
        self.ROI_Y_MIN = -2.5  
        self.ROI_Y_MAX = 2.5   
        # Z adjusted to include sensor plane (0.0)
        self.ROI_Z_MIN = -0.5  
        self.ROI_Z_MAX = 2.0   
        
        # DBSCAN clustering parameters
        self.DBSCAN_EPS = 0.2
        self.DBSCAN_MIN_SAMPLES = 5

        # Subscribers and Publishers
        self.subscription = self.create_subscription(
            PointCloud2,
            '/gazebo_ros_ray_sensor/out', 
            self.lidar_callback,
            10)
        
        self.object_publisher = self.create_publisher(String, '/lidar/closest_object', 10)
        
        self.get_logger().info('LIDAR Processor node started with Angle detection.')

    def get_angle(self, x, y):
        """
        Calculates angle from x (forward) and y (left) coordinates.
        Returns: (degrees, radians)
        """
        # atan2 takes (y, x) -> returns radians between -pi and pi
        angle_rad = math.atan2(y, x)
        angle_deg = math.degrees(angle_rad)
        return angle_deg, angle_rad

    def lidar_callback(self, msg):
        # 1. Parse PointCloud2
        offset_x = msg.fields[0].offset
        offset_y = msg.fields[1].offset
        offset_z = msg.fields[2].offset
        point_step = msg.point_step
        
        try:
            points_raw = np.frombuffer(msg.data, dtype=np.float32).reshape(-1, point_step // 4)
            points = points_raw[:, [offset_x//4, offset_y//4, offset_z//4]]
        except Exception as e:
            self.get_logger().error(f"Failed to process buffer: {e}")
            return

        # 2. Filter ROI
        mask = np.where((points[:,0] > self.ROI_X_MIN) & (points[:,0] < self.ROI_X_MAX) &
                        (points[:,1] > self.ROI_Y_MIN) & (points[:,1] < self.ROI_Y_MAX) &
                        (points[:,2] > self.ROI_Z_MIN) & (points[:,2] < self.ROI_Z_MAX))
        
        filtered_points = points[mask]

        if len(filtered_points) == 0:
            self.object_publisher.publish(String(data="No obstacles detected in ROI."))
            return

        # 3. Clustering
        clustering = DBSCAN(eps=self.DBSCAN_EPS, min_samples=self.DBSCAN_MIN_SAMPLES).fit(filtered_points)
        labels = clustering.labels_
        
        unique_labels = set(labels)
        if -1 in unique_labels:
            unique_labels.remove(-1)
        
        if not unique_labels:
            self.object_publisher.publish(String(data="No clusters found (noise only)."))
            return
            
        # 4. Find Closest Object
        closest_cluster_dist = float('inf')
        closest_cluster_center = None

        for label in unique_labels:
            cluster_points = filtered_points[labels == label]
            # Calculate the centroid (average x, y, z) of the cluster
            cluster_center = np.mean(cluster_points, axis=0)
            
            # Distance formula: sqrt(x^2 + y^2)
            distance = np.linalg.norm(cluster_center)
            
            if distance < closest_cluster_dist:
                closest_cluster_dist = distance
                closest_cluster_center = cluster_center
        
        # 5. Calculate Angle and Publish
        if closest_cluster_center is not None:
            cx = closest_cluster_center[0]
            cy = closest_cluster_center[1]
            cz = closest_cluster_center[2]
            
            # --- CALCULATE ANGLE HERE ---
            deg, rad = self.get_angle(cx, cy)
            
            # Determine direction text for easier reading
            direction = "FRONT"
            if deg > 5: direction = "LEFT"
            elif deg < -5: direction = "RIGHT"

            result_msg = String()
            result_msg.data = (f"Object Detected: {direction}\n"
                               f"  Dist: {closest_cluster_dist:.2f}m\n"
                               f"  Angle: {deg:.1f} degrees\n"
                               f"  Coords: ({cx:.2f}, {cy:.2f}, {cz:.2f})")
            
            self.object_publisher.publish(result_msg)
            # Also print to terminal for debugging
            print(result_msg.data)
            print("-" * 20)

def main(args=None):
    rclpy.init(args=args)
    node = LidarProcessorNode()
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()

if __name__ == '__main__':
    main()