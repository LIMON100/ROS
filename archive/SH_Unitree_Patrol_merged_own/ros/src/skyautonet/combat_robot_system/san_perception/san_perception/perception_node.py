"""SAN v1.5 Phase 2-E Turn 11-12 — Perception rclpy node.

Architecture:
    Camera (CompressedImage) ─┐
    Thermal (Image)            ├─→ PerceptionNode ─→ ~/detections
    Pose (PoseStamped)         ┘                  (DetectionArray)

The node wires together fusion + detection + NPU runner modules:
  1. On camera frame: run inference, post-process, emit detections
  2. Latest thermal frame is buffered and used for fusion
  3. Pose is buffered for future depth-aware projection

Heavy decode/preprocess (H.265 → tensor) is left as a stub for the
real NPU integration turn; this node establishes the message flow.
"""
from __future__ import annotations

import time

import rclpy
from combat_robot_msgs.msg import Detection, DetectionArray
from geometry_msgs.msg import PoseStamped
from rclpy.node import Node
from rclpy.qos import (
    QoSDurabilityPolicy,
    QoSHistoryPolicy,
    QoSProfile,
    QoSReliabilityPolicy,
)
from sensor_msgs.msg import CompressedImage, Image

from san_perception.detection import post_process
from san_perception.fusion import (
    CameraCalib,
    StereoExtrinsic,
)
from san_perception.rknn_runner import make_runner


class PerceptionNode(Node):
    def __init__(self):
        super().__init__("perception_node")

        # ─── Parameters ────────────────────────────────────────────
        self.declare_parameter("npu_backend",          "stub")
        self.declare_parameter("model_path",           "")
        self.declare_parameter("input_width",          640)
        self.declare_parameter("input_height",         640)
        self.declare_parameter("min_confidence",       0.4)
        self.declare_parameter("iou_threshold",        0.5)
        self.declare_parameter("stub_on_no_npu",       True)
        self.declare_parameter("rgb_fx",               1500.0)
        self.declare_parameter("rgb_fy",               1500.0)
        self.declare_parameter("rgb_cx",               1920.0)
        self.declare_parameter("rgb_cy",               1080.0)
        self.declare_parameter("rgb_width",            3840)
        self.declare_parameter("rgb_height",           2160)
        self.declare_parameter("thermal_fx",           300.0)
        self.declare_parameter("thermal_fy",           300.0)
        self.declare_parameter("thermal_cx",           320.0)
        self.declare_parameter("thermal_cy",           256.0)
        self.declare_parameter("thermal_width",        640)
        self.declare_parameter("thermal_height",       512)
        self.declare_parameter("stereo_baseline_m",    0.10)

        backend       = str(self.get_parameter("npu_backend").value)
        model_path    = str(self.get_parameter("model_path").value)
        in_w          = int(self.get_parameter("input_width").value)
        in_h          = int(self.get_parameter("input_height").value)
        self._min_conf = float(
            self.get_parameter("min_confidence").value)
        self._iou_thr  = float(
            self.get_parameter("iou_threshold").value)
        stub_fallback  = bool(
            self.get_parameter("stub_on_no_npu").value)

        # ─── Calibration ───────────────────────────────────────────
        self._rgb_cal = CameraCalib(
            width=int(self.get_parameter("rgb_width").value),
            height=int(self.get_parameter("rgb_height").value),
            fx=float(self.get_parameter("rgb_fx").value),
            fy=float(self.get_parameter("rgb_fy").value),
            cx=float(self.get_parameter("rgb_cx").value),
            cy=float(self.get_parameter("rgb_cy").value),
        )
        self._th_cal = CameraCalib(
            width=int(self.get_parameter("thermal_width").value),
            height=int(self.get_parameter("thermal_height").value),
            fx=float(self.get_parameter("thermal_fx").value),
            fy=float(self.get_parameter("thermal_fy").value),
            cx=float(self.get_parameter("thermal_cx").value),
            cy=float(self.get_parameter("thermal_cy").value),
        )
        self._ext = StereoExtrinsic()
        import numpy as np
        self._ext.t = np.array(
            [float(self.get_parameter("stereo_baseline_m").value),
             0.0, 0.0],
            dtype=np.float64,
        )

        # ─── NPU runner ────────────────────────────────────────────
        self._runner = make_runner(
            backend=backend, model_path=model_path,
            input_width=in_w, input_height=in_h,
            stub_on_no_npu=stub_fallback,
        )

        # ─── State buffers ─────────────────────────────────────────
        self._latest_thermal_msg = None      # type: Image | None
        self._latest_pose_xy     = None
        self._frame_count        = 0
        self._inference_total_ms = 0.0

        # ─── QoS ────────────────────────────────────────────────────
        sensor_qos = QoSProfile(
            history=QoSHistoryPolicy.KEEP_LAST, depth=5,
            reliability=QoSReliabilityPolicy.BEST_EFFORT,
            durability=QoSDurabilityPolicy.VOLATILE,
        )
        reliable_qos = QoSProfile(
            history=QoSHistoryPolicy.KEEP_LAST, depth=10,
            reliability=QoSReliabilityPolicy.RELIABLE,
            durability=QoSDurabilityPolicy.VOLATILE,
        )

        # ─── Subscriptions ─────────────────────────────────────────
        self._cam_sub = self.create_subscription(
            CompressedImage, "camera_compressed",
            self._on_camera, sensor_qos)
        self._thermal_sub = self.create_subscription(
            Image, "thermal_image",
            self._on_thermal, sensor_qos)
        self._pose_sub = self.create_subscription(
            PoseStamped, "pose",
            self._on_pose, sensor_qos)

        # ─── Publisher ─────────────────────────────────────────────
        self._det_pub = self.create_publisher(
            DetectionArray, "~/detections", reliable_qos)

        # Health timer
        self._health_timer = self.create_timer(1.0, self._on_health_tick)

        self.get_logger().info(
            f"PerceptionNode UP: backend={backend} "
            f"min_conf={self._min_conf} iou={self._iou_thr} "
            f"npu_ready={self._runner.is_ready()}")

    # ─── Callbacks ─────────────────────────────────────────────────

    def _on_camera(self, msg: CompressedImage) -> None:
        if not self._runner.is_ready():
            return
        t0 = time.time()
        raw = self._runner.infer(
            bytes(msg.data), self._rgb_cal.width, self._rgb_cal.height)
        elapsed_ms = (time.time() - t0) * 1000.0
        self._inference_total_ms += elapsed_ms

        kept = post_process(
            raw,
            image_width=self._rgb_cal.width,
            image_height=self._rgb_cal.height,
            min_confidence=self._min_conf,
            iou_threshold=self._iou_thr,
        )
        self._publish_detections(kept, int(elapsed_ms), msg.header)
        self._frame_count += 1

    def _on_thermal(self, msg: Image) -> None:
        self._latest_thermal_msg = msg

    def _on_pose(self, msg: PoseStamped) -> None:
        self._latest_pose_xy = (
            msg.pose.position.x, msg.pose.position.y)

    # ─── Publishing ────────────────────────────────────────────────

    def _publish_detections(
        self,
        kept: list,
        inference_ms: int,
        camera_header,
    ) -> None:
        out = DetectionArray()
        out.header.stamp    = self.get_clock().now().to_msg()
        out.header.frame_id = "perception"
        out.source_width    = self._rgb_cal.width
        out.source_height   = self._rgb_cal.height
        out.source_frame_id = camera_header.frame_id
        out.inference_time_ms = int(inference_ms)
        out.cycle_timestamp_ms = int(
            self.get_clock().now().nanoseconds // 1_000_000)

        for d in kept:
            det = Detection()
            det.class_id          = d.class_id
            det.confidence        = float(d.confidence)
            det.bbox_x1           = int(d.x1)
            det.bbox_y1           = int(d.y1)
            det.bbox_x2           = int(d.x2)
            det.bbox_y2           = int(d.y2)
            det.estimated_depth_m = 0.0   # TODO Turn 11-12.5: LRF/SLAM
            det.thermal_avg_temp_c = float("nan")
            det.thermal_max_temp_c = float("nan")
            det.has_thermal_signature = False

            # If we have a thermal frame + plausible depth, project
            # bbox into thermal and compute stats. Skipping in stub
            # mode since depth is unknown.
            # (Real depth source comes in Turn 11-12.5.)
            out.detections.append(det)

        self._det_pub.publish(out)

    # ─── Health ────────────────────────────────────────────────────

    def _on_health_tick(self) -> None:
        avg_ms = (
            self._inference_total_ms / self._frame_count
            if self._frame_count > 0 else 0.0
        )
        self.get_logger().info(
            f"perception frames={self._frame_count} "
            f"avg_inf={avg_ms:.1f}ms thermal={'y' if self._latest_thermal_msg else 'n'} "
            f"pose={'y' if self._latest_pose_xy else 'n'}")


def main(args=None):
    rclpy.init(args=args)
    try:
        node = PerceptionNode()
        rclpy.spin(node)
    except Exception as e:
        import sys
        print(f"PerceptionNode aborted: {e}", file=sys.stderr)
        rclpy.shutdown()
        sys.exit(1)
    rclpy.shutdown()


if __name__ == "__main__":
    main()
