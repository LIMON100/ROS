"""Tests for AHD camera adapter (P2-4)."""
from __future__ import annotations

from unittest.mock import MagicMock

import numpy as np

from adapters.ahd_camera import AhdCameraAdapter, select_camera_adapter


def _config_factory(overrides: dict | None = None):
    """Build a Mock config that mimics the real `config.get(*keys, default=...)`
    chain used by IMX678/AHD adapters."""
    overrides = overrides or {}
    cfg = MagicMock()

    def get(*args, **kwargs):
        # Match by joined key path.
        key = ".".join(str(a) for a in args)
        if key in overrides:
            return overrides[key]
        return kwargs.get("default")

    cfg.get = get
    return cfg


def test_resolution_lookup():
    assert AhdCameraAdapter.RESOLUTIONS["1080p"] == (1920, 1080)
    assert AhdCameraAdapter.RESOLUTIONS["720p"] == (1280, 720)
    assert AhdCameraAdapter.RESOLUTIONS["480p"] == (640, 480)


def test_select_imx678_default():
    cfg = _config_factory()
    assert select_camera_adapter(cfg) == "imx678"


def test_select_ahd_explicit():
    cfg = _config_factory({"payload.camera_type": "ahd"})
    assert select_camera_adapter(cfg) == "ahd"


def test_select_case_insensitive():
    cfg = _config_factory({"payload.camera_type": "AHD"})
    assert select_camera_adapter(cfg) == "ahd"


def test_bgr_to_nv12_shape():
    """I420 output: Y plane (H×W) + UV (H/2×W) = 1.5 H total."""
    bgr = np.random.randint(0, 255, (480, 640, 3), dtype=np.uint8)
    nv12 = AhdCameraAdapter._bgr_to_nv12(bgr)
    assert nv12.shape[0] == 720  # 480 * 1.5
    assert nv12.shape[1] == 640


def test_metrics_format():
    """Bypass __init__ — verify the get_metrics() method shape."""
    adapter = AhdCameraAdapter.__new__(AhdCameraAdapter)
    adapter._frame_count = 100
    adapter._error_count = 2
    adapter.device_path = "/dev/video0"
    adapter.width, adapter.height = 1920, 1080
    m = adapter.get_metrics()
    assert m["frames_captured"] == 100
    assert m["error_count"] == 2
    assert m["device"] == "/dev/video0"
    assert m["resolution"] == "1920x1080"


def test_init_picks_resolution_and_fps():
    cfg = _config_factory({
        "payload.ahd_device": "/dev/video2",
        "payload.ahd_resolution": "720p",
        "payload.ahd_fps": 25,
    })
    queues = MagicMock()
    shutdown = MagicMock()
    adapter = AhdCameraAdapter(queues, shutdown, cfg, camera_shm=None)
    assert adapter.device_path == "/dev/video2"
    assert (adapter.width, adapter.height) == (1280, 720)
    assert adapter.fps == 25


def test_init_default_when_unknown_resolution():
    """Unknown resolution string falls back to 1080p."""
    cfg = _config_factory({"payload.ahd_resolution": "8K"})
    queues = MagicMock()
    shutdown = MagicMock()
    adapter = AhdCameraAdapter(queues, shutdown, cfg, camera_shm=None)
    assert (adapter.width, adapter.height) == (1920, 1080)
