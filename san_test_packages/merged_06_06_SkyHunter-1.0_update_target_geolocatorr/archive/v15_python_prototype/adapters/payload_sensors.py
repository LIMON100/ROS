"""
Payload sensors on RK3588 — adapters running as Python processes.

  • IMX678Adapter        — Sony IMX678 (4K Starvis-2) RGB camera over MIPI CSI-2
  • ThermalCameraAdapter — typically FLIR Boson / Hti via USB-UVC, 9–60 Hz mono16
  • LrfAdapter           — laser range finder (single point, e.g. Lightware LW20)
  • ExternalImuAdapter   — high-rate IMU on the payload (vs Go2's internal IMU)

All adapters share two patterns:
  • They probe for the device on setup(); if absent → STUB mode that emits
    plausible synthetic data at the right rate.
  • Large frames go through SHM (CameraFrameRef-style refs on the queue);
    small messages go directly through mp.Queue.

This file is intentionally cohesive — every payload sensor is a thin
hardware bridge with the same shape. Splitting them into 4 files would
just duplicate boilerplate.
"""
from __future__ import annotations

import logging
import math
import time
from typing import Optional

import numpy as np

from core.base_process import BaseProcess
from core.ipc import consume, publish
from core.messages import (
    CameraFrameRef,
    Header,
    ImuData,
    LrfReading,
    ThermalFrameRef,
)

log = logging.getLogger(__name__)


# ════════════════════════════════════════════════════════════════════════
# IMX678 (4K RGB)
# ════════════════════════════════════════════════════════════════════════
class IMX678Adapter(BaseProcess):
    """Sony IMX678 over MIPI CSI-2 with 3-way fan-out.

    Real driver: V4L2 (`/dev/video0`) → libcamera/GStreamer pipeline. Each
    captured frame is written *once* to a multi-consumer SHM pool and the
    same ref is published to all subscribed consumer queues. Consumers
    each call `release()` when done; the last release returns the slot to
    the free pool. This is the AIRYS NV12Pool pattern adapted for
    Python multiprocessing.

    Subscribers (per user req §4):
      • AI       — always subscribed (perception → RKNN inference)
      • STREAM   — subscribed when FSM ≥ STREAMING (Wi-Fi up + pipeline running)
      • DISPLAY  — subscribed when config.system.dev_display is true

    The adapter doesn't query the FSM directly; instead it watches a
    `subscribers_state` queue that the orchestrator updates whenever the
    set of active consumers changes. Default is {AI} — minimal load.

    Production replacement: V4L2 DMABUF + dma_fd shared via Unix sockets,
    so the encoder + RKNN + display all import the same buffer with zero
    memcpy. The wire abstraction (publish ref to N queues) stays the same.
    """

    def __init__(self, queues, shutdown_event, config, camera_shm,
                  fanout_pool=None, **diag):
        super().__init__(
            name="IMX678Adapter",
            shutdown_event=shutdown_event,
            rate_hz=config.get("imx678", "fps", default=15.0),
            cpu_affinity=config.get("system", "cpu_affinity", "imx678") or [],
            **diag,
        )
        self.queues = queues
        self.cfg = config
        self.shm = camera_shm
        # When fanout_pool is supplied, publish to the 3-way fan-out queues
        # (camera_ai_ref / camera_stream_ref / camera_display_ref).
        # When None, fall back to legacy single-queue path on imx678_ref —
        # this preserves backward compat with tests/main.py during the
        # rollout.
        self.fanout_pool = fanout_pool
        self._stub = False
        self._seq = 0
        self._lock = None
        # Default subscribers: AI only. Orchestrator will add STREAM /
        # DISPLAY by publishing on `camera_subscribers` queue.
        self._subscribers = {"ai"}

    def setup(self) -> None:
        import threading
        self._lock = threading.Lock()
        device = self.cfg.get("imx678", "device", default="/dev/video0")
        try:
            import os
            if not os.path.exists(device):
                raise FileNotFoundError(device)
            self._device = device
            log.info(f"IMX678 device available at {device}")
        except Exception as e:
            log.warning(f"IMX678 unavailable ({e}) → STUB mode")
            self._stub = True

        # Initial subscriber set from config (dev mode may add display)
        if self.cfg.get("system", "dev_display", default=False):
            self._subscribers.add("display")
        # Stream subscription is dynamic — orchestrator toggles it.

        if self.fanout_pool is not None:
            self.spawn_thread(self._subscriber_state_loop, name="ImxSubs")
            # Reaper thread — release SHM slots stranded by consumer crashes
            # (slot_timeout_s 보다 오래 outstanding 인 slot 강제 release)
            self.spawn_thread(self._shm_reaper_loop, name="ShmReaper")

    def step(self) -> None:
        if self._stub:
            payload = _stub_h265_payload()
        else:
            payload = self._capture_h265_real()
            if payload is None:
                return
        if self.fanout_pool is not None:
            self._publish_fanout(payload)
        else:
            self._publish_legacy(payload)

    # ───────── New 3-way fan-out path ─────────
    def _publish_fanout(self, payload: bytes) -> None:
        with self._lock:
            subs = set(self._subscribers)        # snapshot
        n = len(subs)
        if n == 0:
            return       # nobody listening — drop the frame at the producer

        shm_name = self.fanout_pool.publish(payload, n_consumers=n)
        if shm_name is None:
            return            # pool exhausted

        ref = CameraFrameRef(
            header=Header.now(frame_id="imx678", seq=self._seq),
            shm_name=shm_name, nbytes=len(payload),
            width=3840, height=2160, encoding="h265",
        )
        self._seq += 1

        target_qs = []
        if "ai" in subs:
            target_qs.append(self.queues.camera_ai_ref)
        if "stream" in subs:
            target_qs.append(self.queues.camera_stream_ref)
        if "display" in subs:
            target_qs.append(self.queues.camera_display_ref)

        delivered = 0
        for q in target_qs:
            if publish(q, ref):
                delivered += 1
            else:
                # Queue full — consumer can't keep up. Decrement so the
                # slot still frees correctly when the others release.
                self.fanout_pool.release(shm_name)
        # If nobody got it, force-release immediately
        if delivered == 0:
            for _ in range(n - 1):       # we already released once per drop
                self.fanout_pool.release(shm_name)

    # ───────── Legacy single-consumer path (back-compat) ─────────
    def _publish_legacy(self, payload: bytes) -> None:
        shm_name = self.shm.write(payload)
        if shm_name is None:
            return
        ref = CameraFrameRef(
            header=Header.now(frame_id="imx678", seq=self._seq),
            shm_name=shm_name, nbytes=len(payload),
            width=3840, height=2160, encoding="h265",
        )
        self._seq += 1
        publish(self.queues.imx678_ref, ref)

    # ───────── Subscriber-state listener ─────────
    def _subscriber_state_loop(self):
        """Orchestrator publishes on `camera_subscribers` whenever the active
        consumer set changes (e.g. STREAM added when Wi-Fi pipeline starts)."""
        while self.is_running():
            msg = consume(self.queues.camera_subscribers, timeout=0.2)
            if msg is None:
                continue
            action = msg.get("action")
            who    = msg.get("consumer")
            if action == "add" and who:
                with self._lock:
                    self._subscribers.add(who)
                log.info(f"IMX678: + subscriber {who} → {self._subscribers}")
            elif action == "remove" and who:
                with self._lock:
                    self._subscribers.discard(who)
                log.info(f"IMX678: – subscriber {who} → {self._subscribers}")

    # ───────── SHM orphan reaper ─────────
    def _shm_reaper_loop(self) -> None:
        """Sweep stuck SHM slots periodically.

        Without this, a consumer process crash strands its slot reference
        and the camera fan-out exhausts after `n_slots / fps` seconds
        (eg. 16 slots / 15 fps ≈ 1 sec to deadlock the producer).
        """
        period = max(0.5, self.fanout_pool.slot_timeout_s / 2.0)
        while self.is_running():
            # Sleep in 50 ms slices so a shutdown is acted on quickly —
            # otherwise pool.teardown() in tests would tear down the
            # Manager while a reaper is mid-sleep, and the next iteration
            # would hit a dead Manager proxy. With slicing, shutdown
            # latency is ≤ 50 ms regardless of `period`.
            slept = 0.0
            while slept < period and self.is_running():
                time.sleep(0.05)
                slept += 0.05
            if not self.is_running():
                break
            try:
                forced = self.fanout_pool.reap_orphans()
                if forced > 0:
                    log.warning(
                        f"reaped {forced} orphan SHM slots "
                        f"(consumer crash recovery)")
                    if self.metrics:
                        self.metrics.counter("shm_reaped", forced)
            except Exception as e:
                log.error(f"reaper error: {e}")

    def _capture_h265_real(self) -> Optional[bytes]:
        # Real V4L2/GStreamer capture not implemented in PoC.
        return None


# ════════════════════════════════════════════════════════════════════════
# Thermal camera
# ════════════════════════════════════════════════════════════════════════
class ThermalCameraAdapter(BaseProcess):
    """USB-UVC thermal (FLIR Boson 640 etc.). Streams mono16 at ~9 Hz."""

    def __init__(self, queues, shutdown_event, config, camera_shm, **diag):
        super().__init__(
            name="ThermalCameraAdapter",
            shutdown_event=shutdown_event,
            rate_hz=config.get("thermal", "fps", default=9.0),
            cpu_affinity=config.get("system", "cpu_affinity", "thermal") or [],
            **diag,
        )
        self.queues = queues
        self.cfg = config
        self.shm = camera_shm
        self._stub = False
        self._seq = 0

    def setup(self) -> None:
        device = self.cfg.get("thermal", "device", default="/dev/video2")
        try:
            import os
            if not os.path.exists(device):
                raise FileNotFoundError(device)
            self._device = device
            log.info(f"Thermal device available at {device}")
        except Exception as e:
            log.warning(f"Thermal unavailable ({e}) → STUB mode")
            self._stub = True

    def step(self) -> None:
        w, h = 640, 512
        if self._stub:
            # 2 KB synthetic frame (highly compressed mono16 stand-in)
            payload = _stub_thermal_payload(w, h)
        else:
            payload = self._capture_real(w, h)
            if payload is None:
                return
        shm_name = self.shm.write(payload)
        if shm_name is None:
            return
        ref = ThermalFrameRef(
            header=Header.now(frame_id="thermal", seq=self._seq),
            shm_name=shm_name, nbytes=len(payload),
            width=w, height=h, encoding="mono16",
            min_temp_c=-20.0, max_temp_c=120.0,
        )
        self._seq += 1
        publish(self.queues.thermal_ref, ref)

    def _capture_real(self, w: int, h: int) -> Optional[bytes]:
        return None     # PoC stub


# ════════════════════════════════════════════════════════════════════════
# LRF — single-point laser range finder
# ════════════════════════════════════════════════════════════════════════
class LrfAdapter(BaseProcess):
    """Single-point LRF, e.g. Lightware LW20 over UART. Typ. 1–10 Hz.

    Used for narrow-beam range queries (e.g., obstacle clearance check
    before a turn) that LiDAR can't resolve at distance.
    """

    def __init__(self, queues, shutdown_event, config, **diag):
        super().__init__(
            name="LrfAdapter",
            shutdown_event=shutdown_event,
            rate_hz=config.get("lrf", "rate_hz", default=5.0),
            cpu_affinity=config.get("system", "cpu_affinity", "lrf") or [],
            **diag,
        )
        self.queues = queues
        self.cfg = config
        self._serial = None
        self._stub = False
        self._seq = 0

    def setup(self) -> None:
        device = self.cfg.get("lrf", "device", default="/dev/ttyUSB0")
        baud   = self.cfg.get("lrf", "baud",   default=115200)
        try:
            import serial
            self._serial = serial.Serial(device, baud, timeout=0.1)
            log.info(f"LRF serial open: {device}")
        except Exception as e:
            log.warning(f"LRF unavailable ({e}) → STUB mode")
            self._stub = True

    def step(self) -> None:
        if self._stub:
            # Plausible 5 m fluctuating reading
            r = 5.0 + 0.3 * math.sin(time.monotonic())
            self._publish(r, strength=0.85, valid=True)
            return
        try:
            line = self._serial.readline().decode("ascii", errors="ignore").strip()
        except Exception as e:
            log.error(f"LRF read error: {e}")
            return
        if not line:
            return
        try:
            r = float(line.split(",")[0])
            self._publish(r, strength=0.9, valid=(0.05 < r < 100.0))
        except ValueError:
            return

    def teardown(self) -> None:
        if self._serial is not None:
            try:
                self._serial.close()
            except Exception:
                pass

    def _publish(self, range_m: float, strength: float, valid: bool) -> None:
        msg = LrfReading(
            header=Header.now(frame_id="lrf", seq=self._seq),
            range_m=range_m, return_strength=strength, valid=valid,
        )
        self._seq += 1
        publish(self.queues.lrf, msg)


# ════════════════════════════════════════════════════════════════════════
# External IMU on payload (separate from Go2's internal IMU)
# ════════════════════════════════════════════════════════════════════════
class ExternalImuAdapter(BaseProcess):
    """Higher-quality IMU on the RK3588 payload (e.g., Bosch BMI088,
    VectorNav VN-100). Used as primary odometry prior; Go2 internal IMU
    is consumed separately on `queues.imu`."""

    def __init__(self, queues, shutdown_event, config, **diag):
        super().__init__(
            name="ExternalImuAdapter",
            shutdown_event=shutdown_event,
            rate_hz=config.get("ext_imu", "rate_hz", default=200.0),
            cpu_affinity=config.get("system", "cpu_affinity", "ext_imu") or [],
            rt_priority=config.get("system", "rt_priority", "ext_imu") or 0,
            **diag,
        )
        self.queues = queues
        self.cfg = config
        self._serial = None
        self._stub = False
        self._seq = 0

    def setup(self) -> None:
        device = self.cfg.get("ext_imu", "device", default="/dev/ttyACM1")
        try:
            import serial
            self._serial = serial.Serial(device,
                                         self.cfg.get("ext_imu", "baud", default=921600),
                                         timeout=0.01)
            log.info(f"External IMU serial open: {device}")
        except Exception as e:
            log.warning(f"External IMU unavailable ({e}) → STUB mode")
            self._stub = True

    def step(self) -> None:
        if self._stub:
            # Static-platform noise model
            acc = np.array([0.0, 0.0, 9.81], dtype=np.float32) + \
                  np.random.normal(0, 0.05, 3).astype(np.float32)
            gyro = np.random.normal(0, 0.005, 3).astype(np.float32)
        else:
            data = self._read_packet()
            if data is None:
                return
            acc, gyro = data
        msg = ImuData(
            header=Header.now(frame_id="payload_imu", seq=self._seq),
            linear_acc=acc, angular_vel=gyro,
            orientation=np.array([0, 0, 0, 1], dtype=np.float32),
        )
        self._seq += 1
        publish(self.queues.imu_external, msg)

    def teardown(self) -> None:
        if self._serial is not None:
            try:
                self._serial.close()
            except Exception:
                pass

    def _read_packet(self):
        return None     # PoC stub


# ════════════════════════════════════════════════════════════════════════
# Stub payload helpers
# ════════════════════════════════════════════════════════════════════════
def _stub_h265_payload(seq: int = 0) -> bytes:
    # Minimal NAL-unit-shaped placeholder for SHM round-trip exercise
    return b"\x00\x00\x00\x01\x40\x01" + bytes(2046)


def _stub_thermal_payload(w: int, h: int) -> bytes:
    # Compressed-form placeholder; in production this is mono16 raw (w*h*2 bytes)
    return b"\xff" * 2048
