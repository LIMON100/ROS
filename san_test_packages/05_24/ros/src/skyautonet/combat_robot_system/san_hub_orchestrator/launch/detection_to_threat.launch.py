# SAN v1.5.2 DCN-2026-010 D-028 — DetectionToThreatNode standalone launch.
#
# Squadron-level launch wires this node into squadron.launch.py (D-029).
# This standalone launch file is for unit / smoke-test usage.

from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description():
    return LaunchDescription([
        Node(
            package="san_hub_orchestrator",
            executable="detection_to_threat_node",
            name="detection_to_threat_node",
            output="screen",
            parameters=[{
                "fused_topic":              "/perception_node/detections_fused",
                "rgb_topic":                "/human_detector_node/detections_rgb",
                "output_topic":             "/swarm/threat_alert_raw",
                "source_robot_id":          "perception",
                "confidence_threshold":      0.9,    # Scenario B B2: ≥ 90%
                "rgb_confidence_threshold":  0.8,
                "fused_fallback_window_s":   1.0,    # ≤ 1 s dropout → RGB
            }],
        ),
    ])
