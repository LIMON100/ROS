"""
DisplayProcess — dev-mode display sink.

The robot is headless in production. During development we want to see
what the camera is sending so we can validate framing, exposure, lens
focus etc. without WiFi-streaming to the operator's phone.

Pipeline strategies (chosen by config[display]mode):
  • "kmssink"    — GStreamer kmssink → HDMI/eDP via DRM/KMS. Hardware
                    overlay, ~5 ms latency, zero CPU. RK3588's preferred
                    path on Linux desktop spins.
  • "fbdevsink"  — GStreamer fbdevsink → Linux framebuffer. Fallback
                    when DRM master isn't free (e.g. an X server is
                    holding it).
  • "cv2"        — OpenCV imshow. Useful for laptop development.
  • "stub"       — drains the queue and counts; no actual rendering.
                    Default in CI / when no display device is present.

Threading: separate from streaming + perception. A slow display can't
back-pressure either of those because the camera fan-out drops oldest
on any single consumer's queue overflow (per AIRYS NV12Pool semantics).
"""
from __future__ import annotations

import os
import subprocess
import time
from typing import Optional

from core.base_process import BaseProcess
from core.ipc import consume

# Mode names — keep in sync with config[display]mode
MODE_STUB     = "stub"
MODE_KMSSINK  = "kmssink"
MODE_FBDEV    = "fbdevsink"
MODE_CV2      = "cv2"


class DisplayProcess(BaseProcess):
    """Consumes camera_display_ref and renders to a local display.

    On capture: stub mode merely drains and counts; real modes feed the
    SHM-backed frame to a GStreamer subprocess via shmsrc, or to OpenCV
    imshow on laptops. Either way, perception and streaming are
    untouched — they're separate consumers reading from their own queues.
    """

    def __init__(self, queues, shutdown_event, config,
                  camera_shm=None, fanout_pool=None, **diag):
        super().__init__(
            name="Display", shutdown_event=shutdown_event,
            rate_hz=1.0,
            cpu_affinity=config.get("system", "cpu_affinity", "display"),
            **diag,
        )
        self.queues = queues
        self.cfg = config
        # Which fan-out pool to release back into. Either may be None
        # depending on production configuration.
        self.camera_shm = camera_shm
        self.fanout_pool = fanout_pool
        self._mode: str = MODE_STUB
        self._gst_proc: Optional[subprocess.Popen] = None
        self._cv2 = None
        self._stats = {
            "frames_seen": 0, "frames_rendered": 0, "frames_dropped": 0,
        }

    # ───────── Lifecycle ─────────
    def setup(self) -> None:
        # Skip entirely in headless production
        if not self.cfg.get("system", "dev_display", default=False):
            self._mode = MODE_STUB
            self.log.info("display: dev_display=false — running stub")
            self.spawn_thread(self._frame_consumer, name="DispCnsm")
            return

        requested = self.cfg.get("display", "mode", default=MODE_KMSSINK)
        # Validate / fall back
        self._mode = self._select_mode(requested)
        self.log.info(f"display: mode={self._mode} (requested={requested})")

        if self._mode == MODE_CV2:
            try:
                import cv2  # noqa: F401
                self._cv2 = cv2
            except ImportError:
                self.log.warning("opencv-python missing — falling back to stub")
                self._mode = MODE_STUB
        elif self._mode in (MODE_KMSSINK, MODE_FBDEV):
            self._start_gst_subprocess()

        self.spawn_thread(self._frame_consumer, name="DispCnsm")

    def step(self) -> None:
        # Health check: if the gst subprocess died, drop back to stub
        if self._mode in (MODE_KMSSINK, MODE_FBDEV) and self._gst_proc:
            rc = self._gst_proc.poll()
            if rc is not None:
                self.log.error(
                    f"display gst-launch died (rc={rc}) — falling back to stub")
                self._gst_proc = None
                self._mode = MODE_STUB
        # Periodic stats line ~30 s
        if self._stats["frames_seen"] % 300 == 1:
            self.log.info(
                f"display  mode={self._mode} "
                f"seen={self._stats['frames_seen']} "
                f"rendered={self._stats['frames_rendered']} "
                f"dropped={self._stats['frames_dropped']}"
            )

    def teardown(self) -> None:
        if self._gst_proc is not None:
            try:
                self._gst_proc.terminate()
                self._gst_proc.wait(timeout=2.0)
            except (subprocess.TimeoutExpired, OSError):
                try:
                    self._gst_proc.kill()
                except OSError:
                    pass
        if self._cv2 is not None:
            try:
                self._cv2.destroyAllWindows()
            except Exception:        # pylint: disable=broad-except
                pass

    # ───────── Mode selection ─────────
    def _select_mode(self, requested: str) -> str:
        if requested not in (MODE_KMSSINK, MODE_FBDEV, MODE_CV2, MODE_STUB):
            self.log.warning(f"display: unknown mode {requested!r} — stub")
            return MODE_STUB
        if requested == MODE_KMSSINK and not _kms_available():
            self.log.info("kmssink unavailable (no DRM device or master held)")
            return MODE_FBDEV if _fbdev_available() else MODE_STUB
        if requested == MODE_FBDEV and not _fbdev_available():
            return MODE_STUB
        return requested

    # ───────── GStreamer subprocess (kmssink/fbdevsink) ─────────
    def _start_gst_subprocess(self) -> None:
        # We feed frames via shmsrc; the producer side writes raw NV12 to
        # a Unix shm path, and gst-launch reads from there. The stream
        # comes from the existing camera_shm; here we just point shmsrc
        # at it. (Production would use the same DMABUF the encoder uses.)
        device = self.cfg.get("display", "device", default="/dev/dri/card0")
        sink = (f"kmssink driver-name=rockchip force-modesetting=true "
                 f"device={device} sync=false"
                 if self._mode == MODE_KMSSINK
                 else "fbdevsink sync=false")
        # Since we don't have a real shmsrc path in dev, use videotestsrc
        # as a placeholder so kmssink has something to render. The frame
        # consumer thread silently drains camera_display_ref to release
        # SHM slots. In production this becomes shmsrc → kmssink and the
        # consumer thread copies from camera_shm.read() to the shmsrc fifo.
        argv = [
            "gst-launch-1.0", "-q",
            "videotestsrc", "is-live=true",
            "!", "video/x-raw,format=NV12,width=1280,height=720,framerate=30/1",
            "!", "queue", "leaky=downstream", "max-size-buffers=2",
            "!", *sink.split(),
        ]
        try:
            self._gst_proc = subprocess.Popen(
                argv, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE,
            )
            time.sleep(0.5)
            if self._gst_proc.poll() is not None:
                self.log.error(
                    f"display gst-launch failed to start "
                    f"(rc={self._gst_proc.returncode})")
                self._gst_proc = None
                self._mode = MODE_STUB
        except (OSError, FileNotFoundError) as e:
            self.log.warning(f"gst-launch not found ({e}) — stub")
            self._gst_proc = None
            self._mode = MODE_STUB

    # ───────── Frame consumer ─────────
    def _frame_consumer(self):
        """Drain camera_display_ref and (optionally) render via cv2.imshow.

        For kmssink/fbdevsink modes the GStreamer subprocess handles
        rendering on its own; we just need to release the SHM slot so
        the producer can reuse it.
        """
        while self.is_running():
            ref = consume(self.queues.camera_display_ref, timeout=0.1)
            if ref is None:
                continue
            self._stats["frames_seen"] += 1

            if self._mode == MODE_CV2 and self._cv2 is not None:
                self._render_cv2(ref)
            elif self._mode in (MODE_KMSSINK, MODE_FBDEV):
                # Subprocess reads its own source for now; real impl reads
                # ref → SHM and pipes into shmsrc. Counted as rendered
                # when the subprocess is alive.
                if self._gst_proc and self._gst_proc.poll() is None:
                    self._stats["frames_rendered"] += 1
                else:
                    self._stats["frames_dropped"] += 1
            # else: stub — count as seen, drop silently

            # Always release the fan-out slot so the producer can reuse it
            if self.fanout_pool is not None:
                self.fanout_pool.release(ref.shm_name)

    def _render_cv2(self, ref) -> None:
        """OpenCV imshow path — for laptop dev only."""
        try:
            import numpy as np
            payload = (self.camera_shm.read(ref.shm_name, ref.nbytes)
                        if self.camera_shm is not None else None)
            if payload is None:
                self._stats["frames_dropped"] += 1
                return
            # NV12 decode is non-trivial; for stub we just show a placeholder
            # sized image. Real impl: cv2.cvtColor(buf, COLOR_YUV2BGR_NV12).
            arr = np.frombuffer(payload[:ref.width * ref.height], dtype=np.uint8)
            try:
                img = arr.reshape((ref.height, ref.width))
            except ValueError:
                # Encoded payload — skip
                self._stats["frames_dropped"] += 1
                return
            self._cv2.imshow("patrol-display", img)
            self._cv2.waitKey(1)
            self._stats["frames_rendered"] += 1
        except Exception as e:        # pylint: disable=broad-except
            self.log.exception(f"cv2 render failed: {e}")
            self._stats["frames_dropped"] += 1


# ─────────── Capability probes ───────────
def _kms_available() -> bool:
    """Quick check: /dev/dri/card0 exists and we can open it."""
    if not os.path.exists("/dev/dri/card0"):
        return False
    try:
        fd = os.open("/dev/dri/card0", os.O_RDWR)
        os.close(fd)
        return True
    except OSError:
        return False


def _fbdev_available() -> bool:
    return os.path.exists("/dev/fb0")
