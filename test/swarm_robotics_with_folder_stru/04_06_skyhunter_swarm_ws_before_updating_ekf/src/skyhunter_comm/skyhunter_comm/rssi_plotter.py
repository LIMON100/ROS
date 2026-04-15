#!/usr/bin/env python3
"""
RSSI vs Distance plot generator for V&V 2.2
Collects samples from /mesh_metrics and generates validation plot
"""

import rclpy
from rclpy.node import Node
from skyhunter_msgs.msg import MeshMetrics
import matplotlib.pyplot as plt
import numpy as np
from collections import defaultdict
import math
import os


class RSSIPlotter(Node):
    def __init__(self, num_samples=100):
        super().__init__('rssi_plotter')
        self.num_samples = num_samples
        self.sample_count = 0
        
        # Store data: {distance: [rssi_values]}
        self.data = defaultdict(list)
        
        self.subscription = self.create_subscription(
            MeshMetrics,
            '/mesh_metrics',
            self.callback,
            10
        )
        self.get_logger().info(f'Collecting {num_samples} samples...')

    def callback(self, msg):
        for link in msg.links:
            if link.connected and link.distance_m > 0:
                # Round distance to 1 decimal for grouping
                d = round(link.distance_m, 1)
                self.data[d].append(link.rssi_dbm)
        
        self.sample_count += 1
        if self.sample_count % 10 == 0:
            self.get_logger().info(f'Collected {self.sample_count}/{self.num_samples} samples')
        
        if self.sample_count >= self.num_samples:
            self.generate_plot()
            rclpy.shutdown()

    def generate_plot(self):
        self.get_logger().info('Generating plot...')
        
        # Extract data
        distances = []
        rssi_means = []
        rssi_stds = []
        rssi_all_d = []
        rssi_all_r = []
        
        for d in sorted(self.data.keys()):
            values = self.data[d]
            distances.append(d)
            rssi_means.append(np.mean(values))
            rssi_stds.append(np.std(values))
            # All points for scatter
            for r in values:
                rssi_all_d.append(d)
                rssi_all_r.append(r)
        
        # Theoretical path loss curve
        d_theory = np.linspace(0.5, max(distances) + 1, 100)
        ref_rssi = -30.0
        n = 2.8
        rssi_theory = ref_rssi - 10 * n * np.log10(d_theory)
        
        # Create plot
        fig, ax = plt.subplots(figsize=(10, 6))
        
        # Scatter all points
        ax.scatter(rssi_all_d, rssi_all_r, alpha=0.3, label='Measured samples', c='blue', s=20)
        
        # Mean with error bars
        ax.errorbar(distances, rssi_means, yerr=rssi_stds, fmt='ro', capsize=5, 
                    label='Mean ± std', markersize=8)
        
        # Theoretical curve
        ax.plot(d_theory, rssi_theory, 'g--', linewidth=2, 
                label=f'Theoretical (n={n})')
        
        # Threshold line
        ax.axhline(y=-85, color='r', linestyle=':', label='Disconnect threshold (-85 dB)')
        
        ax.set_xlabel('Distance (m)', fontsize=12)
        ax.set_ylabel('RSSI (dBm)', fontsize=12)
        ax.set_title('WiFi6 Mesh Simulation: RSSI vs Distance\nV&V 2.2 - Packet Loss Model Validation', fontsize=14)
        ax.legend(loc='upper right')
        ax.grid(True, alpha=0.3)
        
        # Save plot
        plt.tight_layout()
        pkg_path = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        figs_path = os.path.join(pkg_path, 'docs/figures', 'rssi_vs_distance_vv22.png')
        plt.savefig(figs_path, dpi=150)
        self.get_logger().info(f'Plot saved: {figs_path}')
        
        # Print summary
        print('\n' + '='*50)
        print('V&V 2.2 - RSSI vs Distance Summary')
        print('='*50)
        print(f'{"Distance (m)":<15} {"Mean RSSI":<15} {"Std Dev":<15} {"Samples":<10}')
        print('-'*50)
        for d in sorted(self.data.keys()):
            values = self.data[d]
            print(f'{d:<15.1f} {np.mean(values):<15.2f} {np.std(values):<15.2f} {len(values):<10}')
        print('='*50)
        print(f'Path loss exponent (n): {n}')
        print(f'Reference RSSI at 1m: {ref_rssi} dBm')
        print(f'Disconnect threshold: -85 dBm')
        print('='*50)


def main():
    rclpy.init()
    node = RSSIPlotter(num_samples=100)
    rclpy.spin(node)


if __name__ == '__main__':
    main()