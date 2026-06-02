"""AhdCameraAdapter — Lab dev FHD camera (SDD §2.2, P2-4).

Drop-in replacement for IMX678Adapter during Phase B-D.
Cost: $30 AHD vs $300+ Sony IMX678.

Same camera_shm fan-out, same NV12 output, same SHM reaper pattern.
Selected via config: payload.camera_type = 'imx678' | 'ahd'.

Wire path:
  AHD camera → AHD-to-USB capture box (UVC) → /dev/video0 → V4L2
  Default: 1920x1080 @ 30 fps NV12
"""
from __future__ import annotations

import logging
import time
from typing import Optional

import numpy as np

from core.base_process import BaseProcess

try:
    import cv2  # V4L2 capture in dev; falls back to mock in tests/CI
    CV2_AVAILABLE = True
except ImportError:
    CV2_AVAILABLE = False

log = logging.getLogger(__name__)


class AhdCameraAdapter(BaseProcess):
    """AHD camera process for Lab development."""

    DEFAULT_DEVICE = "/dev/video0"
    DEFAULT_FPS = 30
    DEFAULT_RESOLUTION = "1080p"

    RESOLUTIONS = {
        "1080p": (1920, 1080),
        "720p":  (1280, 720),
        "480p":  (640, 480),
    }

    def __init__(self, queues, shutdown_event, config,
                 camera_shm=None, **diag):
        fps = float(config.get("payload", "ahd_fps",
                               default=self.DEFAULT_FPS))
        affinity = config.get("system", "cpu_affinity", "ahd_camera") or [3]
        super().__init__(
            name="AhdCamera",
            shutdown_event=shutdown_event,
            rate_hz=fps,
            cpu_affinity=affinity,
            **diag,
        )
        self.queues = queues
        self.cfg = config
        self.camera_shm = camera_shm
        self.device_path = config.get("payload", "ahd_device",
                                      default=self.DEFAULT_DEVICE)
        resolution = config.get("payload", "ahd_resolution",
                                default=self.DEFAULT_RESOLUTION)
        self.width, self.height = self.RESOLUTIONS.get(
            resolution, self.RESOLUTIONS["1080p"])
        self.fps = int(fps)
        self._capture: Optional[object] = None
        self._frame_count = 0
        self._error_count = 0

    def setup(self) -> None:
        if not CV2_AVAILABLE:
            log.warning("cv2 not available — AHD adapter in mock mode")
            return
        try:
            self._capture = cv2.VideoCapture(self.device_path)
            self._capture.set(cv2.CAP_PROP_FRAME_WIDTH, self.width)
            self._capture.set(cv2.CAP_PROP_FRAME_HEIGHT, self.height)
            self._capture.set(cv2.CAP_PROP_FPS, self.fps)
            if not self._capture.isOpened():
                log.error("failed to open %s", self.device_path)
                self._capture = None
        except Exception as e:
            log.error("setup error: %s", e)
            self._capture = None

        if self.camera_shm is not None and hasattr(self, "spawn_thread"):
            self.spawn_thread(self._shm_reaper_loop, name="AhdShmReaper")

    def step(self) -> None:
        if self._capture is None:
            return
        try:
            ret, frame = self._capture.read()
            if not ret or frame is None:
                self._error_count += 1
                if self._error_count > 10:
                    log.error("repeated capture failures — re-init")
                    self.setup()
                    self._error_count = 0
                return

            self._error_count = 0
            self._frame_count += 1
            nv12 = self._bgr_to_nv12(frame)
            if self.camera_shm is not None:
                self.camera_shm.publish(nv12.tobytes(), n_consumers=3)
        except Exception as e:
            log.error("step error: %s", e)
            self._error_count += 1

    def teardown(self) -> None:
        if self._capture is not None:
            try:
                self._capture.release()
            except Exception:
                pass
            self._capture = None

    def _shm_reaper_loop(self) -> None:
        period = max(0.5, getattr(self.camera_shm, "slot_timeout_s", 2.0) / 2.0)
        while self.is_running():
            time.sleep(period)
            try:
                forced = self.camera_shm.reap_orphans()
                if forced > 0:
                    log.warning("reaped %d orphan SHM slots", forced)
            except Exception as e:
                log.error("reaper error: %s", e)

    @staticmethod
    def _bgr_to_nv12(bgr: np.ndarray) -> np.ndarray:
        """Convert OpenCV BGR → I420/NV12 (Y plane + interleaved UV)."""
        if not CV2_AVAILABLE:
            h, w, _ = bgr.shape
            return np.zeros((h * 3 // 2, w), dtype=np.uint8)
        return cv2.cvtColor(bgr, cv2.COLOR_BGR2YUV_I420)

    def get_metrics(self) -> dict:
        """Adapter-specific metrics. Named to avoid clashing with
        BaseProcess.metrics (which holds a MetricsCollector instance)."""
        return {
            "frames_captured": self._frame_count,
            "error_count": self._error_count,
            "device": self.device_path,
            "resolution": f"{self.width}x{self.height}",
        }


def select_camera_adapter(config) -> str:
    """Camera dispatcher per config payload.camera_type."""
    cam_type = config.get("payload", "camera_type", default="imx678")
    return "ahd" if str(cam_type).lower() == "ahd" else "imx678"
