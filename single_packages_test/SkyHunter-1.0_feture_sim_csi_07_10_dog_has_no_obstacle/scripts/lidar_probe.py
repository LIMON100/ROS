#!/usr/bin/env python3
import rclpy, numpy as np
from rclpy.node import Node
from sensor_msgs.msg import PointCloud2
from sensor_msgs_py import point_cloud2 as pc2
from rclpy.qos import qos_profile_sensor_data

class Probe(Node):
    def __init__(self):
        super().__init__('lidar_probe')
        self.create_subscription(PointCloud2, '/s1/rslidar_points', self.cb, qos_profile_sensor_data)
        self.done = False
    def cb(self, msg):
        if self.done: return
        self.done = True
        p = pc2.read_points_numpy(msg, field_names=('x','y','z'), skip_nans=True)
        x, y, z = p[:,0], p[:,1], p[:,2]
        print('frame:', msg.header.frame_id, ' N=', len(x))
        for n, a in (('x',x),('y',y),('z',z)):
            print(f'{n}: min={a.min():.2f} max={a.max():.2f} mean={a.mean():.2f}')
        rh = np.hypot(x, y)
        print('nearest 12 points (x, y, z, rh):')
        for i in np.argsort(rh)[:12]:
            print(f'  {x[i]:+.2f} {y[i]:+.2f} {z[i]:+.2f}  rh={rh[i]:.2f}')


def main(): 
    rclpy.init(); n = Probe()
    while rclpy.ok() and not n.done:
        rclpy.spin_once(n, timeout_sec=0.5)
    n.destroy_node(); rclpy.shutdown()
main()    