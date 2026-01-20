#!/usr/bin/env python3
import rclpy
from rclpy.node import Node
from sensor_msgs.msg import PointCloud2
from std_msgs.msg import String
import math
import numpy as np
from sklearn.cluster import DBSCAN
from rclpy.qos import QoSProfile, QoSReliabilityPolicy, QoSHistoryPolicy

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
        # self.subscription = self.create_subscription(
        #     PointCloud2,
        #     '/lidar/points',
        #     self.lidar_callback,
        #     10)

        qos_profile = QoSProfile(
        reliability=QoSReliabilityPolicy.BEST_EFFORT,
        history=QoSHistoryPolicy.KEEP_LAST,
        depth=5  # Keep the last 5 messages
    )

        self.subscription = self.create_subscription(
            PointCloud2,
            '/lidar/points',
            self.lidar_callback,
            qos_profile) # <-- Use the new QoS profile

        
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