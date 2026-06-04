# Copyright 2026 SkyAutoNet Inc.
#
# Proprietary and confidential. Unauthorized copying, distribution, or use
# of this file, via any medium, is strictly prohibited.

"""SAN v1.5 Phase 2-E Turn 11-12 — Perception rclpy node (PATCHED 2026-05-13).

Architecture:
    Camera (CompressedImage) ─┐
    Thermal (Image)            ├─→ PerceptionNode ─→ ~/detections
    Pose (PoseStamped)         ┘                  (DetectionArray)

The node wires together fusion + detection + NPU runner modules:
  1. On camera frame: run inference, post-process, emit detections
  2. Latest thermal frame is buffered and used for fusion
  3. Pose is buffered for future depth-aware projection

PATCH 2026-05-13 (san_perception deep-dive review):
  * C3 — main() now uses MultiThreadedExecutor explicitly so camera
    inference (~30 ms) doesn't block thermal / pose callbacks queued
    on the same single-thread executor.
  * C4 / C5 — threading.Lock guards every shared field touched from
    multiple callbacks: frame_count, inference_total_ms, latest_thermal_msg,
    latest_pose_xy. The GIL serializes single-instruction reads / writes
    but compound `+=` is bytecode-multistep and NOT atomic.
  * C6 — DetectionArray.header.stamp = camera_header.stamp (capture
    time), not get_clock().now() (publish time). The fire-auth side
    can now correctly age-check detections.
  * M13 — inference_time_ms reported as round() rather than int(),
    so a 0.6 ms inference no longer becomes 0.
  * M10 — StereoExtrinsic R now configurable via parameters as a
    flat 9-element row-major list (default identity).
  * M11 — thermal raw-to-Celsius scale/offset now configurable.
  * M12 — frame_count / inference_total_ms reset every health tick
    so they report a "last second" rolling average instead of an
    ever-growing total.
"""
from __future__ import annotations

import threading
import time

import numpy as np
import rclpy
from combat_robot_msgs.msg import Detection, DetectionArray
from geometry_msgs.msg import PoseStamped
from rclpy.executors import MultiThreadedExecutor
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
        # ★ PATCH 2026-05-13 (C1 surface): operators can override the
        # post_process drop_stub policy if they explicitly want stub
        # output (e.g. for end-to-end message-flow tests).
        self.declare_parameter("drop_stub_detections", True)
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
        # ★ PATCH 2026-05-13 (M10): RGB→thermal rotation as a flat
        # 9-element row-major list. Default = identity.
        self.declare_parameter(
            "stereo_R_row_major",
            [1.0, 0.0, 0.0,
             0.0, 1.0, 0.0,
             0.0, 0.0, 1.0],
        )
        # ★ PATCH 2026-05-13 (M11): thermal raw→°C calibration.
        self.declare_parameter("thermal_celsius_scale",  0.01)
        self.declare_parameter("thermal_celsius_offset", -273.15)

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
        self._drop_stub = bool(
            self.get_parameter("drop_stub_detections").value)
        self._th_scale = float(
            self.get_parameter("thermal_celsius_scale").value)
        self._th_offset = float(
            self.get_parameter("thermal_celsius_offset").value)

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
        self._ext.t = np.array(
            [float(self.get_parameter("stereo_baseline_m").value),
             0.0, 0.0],
            dtype=np.float64,
        )
        # ★ PATCH 2026-05-13 (M10): parse R from parameters.
        r_flat = list(self.get_parameter("stereo_R_row_major").value)
        if len(r_flat) != 9:
            self.get_logger().warn(
                f"stereo_R_row_major must be length 9 (got {len(r_flat)}); "
                f"falling back to identity")
            r_flat = [1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0]
        self._ext.R = np.array(r_flat, dtype=np.float64).reshape(3, 3)

        # ─── NPU runner ────────────────────────────────────────────
        self._runner = make_runner(
            backend=backend, model_path=model_path,
            input_width=in_w, input_height=in_h,
            stub_on_no_npu=stub_fallback,
        )

        # ─── State buffers (★ PATCH 2026-05-13 C4/C5: lock-guarded) ───
        self._state_lock = threading.Lock()
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
            f"drop_stub={self._drop_stub} "
            f"npu_ready={self._runner.is_ready()}")

    # ─── Callbacks ─────────────────────────────────────────────────

    def _on_camera(self, msg: CompressedImage) -> None:
        if not self._runner.is_ready():
            return
        t0 = time.time()
        raw = self._runner.infer(
            bytes(msg.data), self._rgb_cal.width, self._rgb_cal.height)
        elapsed_ms = (time.time() - t0) * 1000.0

        kept = post_process(
            raw,
            image_width=self._rgb_cal.width,
            image_height=self._rgb_cal.height,
            min_confidence=self._min_conf,
            iou_threshold=self._iou_thr,
            drop_stub=self._drop_stub,    # ★ PATCH 2026-05-13 (C1)
        )

        # ★ PATCH 2026-05-13 (C4/C5): atomic counter update.
        with self._state_lock:
            self._inference_total_ms += elapsed_ms
            self._frame_count += 1

        self._publish_detections(kept, elapsed_ms, msg.header)

    def _on_thermal(self, msg: Image) -> None:
        # ★ PATCH 2026-05-13 (C5): atomic reference swap.
        with self._state_lock:
            self._latest_thermal_msg = msg

    def _on_pose(self, msg: PoseStamped) -> None:
        # ★ PATCH 2026-05-13 (C5): atomic reference swap.
        with self._state_lock:
            self._latest_pose_xy = (
                msg.pose.position.x, msg.pose.position.y)

    # ─── Publishing ────────────────────────────────────────────────

    def _publish_detections(
        self,
        kept: list,
        inference_ms: float,
        camera_header,
    ) -> None:
        out = DetectionArray()
        # ★ PATCH 2026-05-13 (C6): capture time, not publish time.
        # The fire-auth side ages detections against this stamp; a
        # publish-time stamp hides the inference latency from the
        # staleness check.
        out.header.stamp    = camera_header.stamp
        out.header.frame_id = "perception"
        out.source_width    = self._rgb_cal.width
        out.source_height   = self._rgb_cal.height
        out.source_frame_id = camera_header.frame_id
        # ★ PATCH 2026-05-13 (M13): round() so sub-ms inference still
        # shows up. 0.6 ms → 1, not 0.
        out.inference_time_ms = int(round(inference_ms))
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
            out.detections.append(det)

        self._det_pub.publish(out)

    # ─── Health ────────────────────────────────────────────────────

    def _on_health_tick(self) -> None:
        # ★ PATCH 2026-05-13 (C4/M12): snapshot + reset under lock.
        # Reset makes the average a "last-second" rolling number
        # rather than a lifetime cumulative.
        with self._state_lock:
            frames = self._frame_count
            total_ms = self._inference_total_ms
            has_thermal = self._latest_thermal_msg is not None
            has_pose = self._latest_pose_xy is not None
            self._frame_count = 0
            self._inference_total_ms = 0.0

        avg_ms = total_ms / frames if frames > 0 else 0.0
        self.get_logger().info(
            f"perception frames={frames} "
            f"avg_inf={avg_ms:.1f}ms thermal={'y' if has_thermal else 'n'} "
            f"pose={'y' if has_pose else 'n'}")


def main(args=None):
    rclpy.init(args=args)
    node = None
    try:
        node = PerceptionNode()
        # ★ PATCH 2026-05-13 (C3/M14): MultiThreadedExecutor explicit.
        # Camera inference (~30 ms) on the same thread as thermal/pose
        # would back up the queue and break sensor sync. Using num_threads=4
        # gives a thread for each of: camera, thermal, pose, timer/spare.
        executor = MultiThreadedExecutor(num_threads=4)
        executor.add_node(node)
        try:
            executor.spin()
        finally:
            executor.shutdown()
    except KeyboardInterrupt:
        pass
    except Exception as e:
        import sys
        print(f"PerceptionNode aborted: {e}", file=sys.stderr)
        raise
    finally:
        if node is not None:
            node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":
    main()
