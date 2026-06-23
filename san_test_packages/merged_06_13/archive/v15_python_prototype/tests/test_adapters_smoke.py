"""
Smoke tests for adapters in STUB mode.

Each adapter falls back to STUB mode when its hardware is absent. This
verifies the STUB path actually publishes plausible messages — not just
that import succeeds.

Strategy: instantiate the adapter without spawning, manually call
setup() + step() against a fake queue, inspect what was published.
"""
from __future__ import annotations

import multiprocessing as mp
from queue import Queue as ThreadQueue

import pytest

from core.messages import (
    LTE_REGISTERED_HOME,
    RTK_FIX_FIXED,
    RTK_FIX_GPS,
    CameraFrameRef,
    ImuData,
    LrfReading,
    LteStatus,
    RtkFix,
    ThermalFrameRef,
)


# ──────────── Test scaffolding ────────────
class _FakeShm:
    """Minimal shm pool: write returns a fake name."""
    def __init__(self):
        self.writes = 0
    def write(self, payload: bytes):
        self.writes += 1
        return f"shm_{self.writes}"


class _FakeQueues:
    """Drop-in for TopicQueues — every attribute is a thread-local Queue."""
    def __init__(self):
        # All the queue attrs we might publish to
        for name in ("rtk", "lte_status", "imu_external", "lrf",
                     "imx678_ref", "thermal_ref", "camera_ref",
                     "lidar_ref", "imu", "pose", "cumulative_update",
                     "shared_map_out"):
            setattr(self, name, ThreadQueue(maxsize=10))


class _FakeConfig:
    """Minimal config supporting cfg.get('a', 'b', 'c', default=...).

    `overrides` lets a test inject specific key-tuples without a YAML
    file: e.g. _FakeConfig(overrides={("rtk", "stub_quality"): "fixed"}).
    """
    def __init__(self, overrides=None):
        self._d = dict(overrides or {})

    def get(self, *keys, default=None):
        return self._d.get(keys, default) if keys else default


def _bare(cls, *args, **kwargs):
    """Construct an adapter without invoking BaseProcess machinery.

    We bypass __init__ for BaseProcess (which spawns), but call the
    adapter's own __init__ which only sets fields. This works because
    BaseProcess.__init__ doesn't actually start the process — start()
    does. We just never call start().
    """
    return cls(*args, **kwargs)


# ════════════════════════════════════════════════════════════
# RTK adapter — STUB
# ════════════════════════════════════════════════════════════
def test_rtk_stub_publishes_default_gps_quality():
    """Stub mode now defaults to GPS-grade quality (not FIXED) so that
    LocalizationProcess in dev falls back to SLAM odometry instead of
    treating synthetic data as centimeter-grade truth."""
    from adapters.rtk_gnss import RtkGnssAdapter
    queues = _FakeQueues()
    cfg = _FakeConfig()
    a = _bare(RtkGnssAdapter, queues, mp.Event(), cfg)
    a.setup()       # forces STUB mode (no /dev/ttyACM0)
    assert a._stub_mode is True
    a.step()
    msg = queues.rtk.get_nowait()
    assert isinstance(msg, RtkFix)
    assert msg.fix_quality == RTK_FIX_GPS
    assert msg.n_satellites > 0
    assert abs(msg.lat) > 0


def test_rtk_stub_quality_can_be_overridden_to_fixed():
    """Operators who explicitly want centimeter-grade synthetic fixes
    (e.g. exercising the 'fixed' branch of localization) opt in via
    rtk.stub_quality."""
    from adapters.rtk_gnss import RtkGnssAdapter
    queues = _FakeQueues()
    cfg = _FakeConfig(overrides={("rtk", "stub_quality"): "fixed"})
    a = _bare(RtkGnssAdapter, queues, mp.Event(), cfg)
    a.setup()
    a.step()
    msg = queues.rtk.get_nowait()
    assert msg.fix_quality == RTK_FIX_FIXED


def test_rtk_stub_publishes_increasing_seq():
    from adapters.rtk_gnss import RtkGnssAdapter
    queues = _FakeQueues()
    a = _bare(RtkGnssAdapter, queues, mp.Event(), _FakeConfig())
    a.setup()
    seqs = []
    for _ in range(3):
        a.step()
        msg = queues.rtk.get(timeout=0.1)
        seqs.append(msg.header.seq)
    assert seqs == sorted(seqs) and seqs[-1] > seqs[0]


# ════════════════════════════════════════════════════════════
# LTE adapter — STUB
# ════════════════════════════════════════════════════════════
def test_lte_stub_publishes_registered_status():
    from adapters.lte_modem import LteModemAdapter
    queues = _FakeQueues()
    a = _bare(LteModemAdapter, queues, mp.Event(), _FakeConfig())
    a.setup()
    assert a._stub is True
    a.step()
    msg = queues.lte_status.get_nowait()
    assert isinstance(msg, LteStatus)
    assert msg.registered == LTE_REGISTERED_HOME
    assert msg.pdp_active is True
    assert msg.ip_address != ""


# ════════════════════════════════════════════════════════════
# External IMU — STUB
# ════════════════════════════════════════════════════════════
def test_external_imu_stub_publishes_gravity_vector():
    from adapters.payload_sensors import ExternalImuAdapter
    queues = _FakeQueues()
    a = _bare(ExternalImuAdapter, queues, mp.Event(), _FakeConfig())
    a.setup()
    a.step()
    msg = queues.imu_external.get_nowait()
    assert isinstance(msg, ImuData)
    # Stub gravity = (0, 0, 9.81) ± 0.05 noise
    assert msg.linear_acc[2] == pytest.approx(9.81, abs=0.5)
    # Gyro near zero
    assert abs(msg.angular_vel).max() < 0.1


# ════════════════════════════════════════════════════════════
# LRF — STUB
# ════════════════════════════════════════════════════════════
def test_lrf_stub_publishes_valid_range():
    from adapters.payload_sensors import LrfAdapter
    queues = _FakeQueues()
    a = _bare(LrfAdapter, queues, mp.Event(), _FakeConfig())
    a.setup()
    a.step()
    msg = queues.lrf.get_nowait()
    assert isinstance(msg, LrfReading)
    assert msg.valid is True
    assert 4.0 < msg.range_m < 6.0     # stub oscillates around 5 m


# ════════════════════════════════════════════════════════════
# IMX678 + Thermal — STUB (need shared memory)
# ════════════════════════════════════════════════════════════
def test_imx678_stub_publishes_camera_ref():
    from adapters.payload_sensors import IMX678Adapter
    queues = _FakeQueues()
    shm = _FakeShm()
    a = _bare(IMX678Adapter, queues, mp.Event(), _FakeConfig(), shm)
    a.setup()
    assert a._stub is True
    a.step()
    msg = queues.imx678_ref.get_nowait()
    assert isinstance(msg, CameraFrameRef)
    assert msg.encoding == "h265"
    assert msg.width == 3840 and msg.height == 2160
    assert shm.writes == 1


def test_thermal_stub_publishes_thermal_ref():
    from adapters.payload_sensors import ThermalCameraAdapter
    queues = _FakeQueues()
    shm = _FakeShm()
    a = _bare(ThermalCameraAdapter, queues, mp.Event(), _FakeConfig(), shm)
    a.setup()
    a.step()
    msg = queues.thermal_ref.get_nowait()
    assert isinstance(msg, ThermalFrameRef)
    assert msg.encoding == "mono16"
    assert msg.width == 640 and msg.height == 512
