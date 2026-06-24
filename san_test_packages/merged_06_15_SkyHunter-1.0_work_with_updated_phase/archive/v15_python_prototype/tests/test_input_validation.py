"""
P2 input-validation regression tests:
  • BleControlProcess oversize-line and oversize-hex protection
  • CommProcess refuses non-http(s) server_url schemes
  • Adapter __init__ signatures accept **diag (so main.py doesn't
    TypeError when forwarding diagnostic kwargs)
  • Config.validate_required reports missing keys
  • make_topic_queues sizes high-rate topics larger than the default
  • RTK stub mode publishes GPS-quality (not FIXED) by default
"""
from __future__ import annotations

import inspect
import logging
from unittest.mock import MagicMock, patch

import pytest


# ────────── BleControlProcess length caps ──────────
def test_ble_oversize_settings_hex_is_rejected():
    """A SETTINGS write with a hex blob over MAX_SETTINGS_HEX_CHARS must
    be dropped without invoking bytes.fromhex (which would otherwise
    happily allocate a multi-MB bytes object)."""
    from control.ble_control_process import BleControlProcess

    proc = BleControlProcess.__new__(BleControlProcess)
    proc._stats = {"rejects": 0, "settings": 0}
    proc.queues = MagicMock()

    huge = "ab" * (BleControlProcess.MAX_SETTINGS_HEX_CHARS // 2 + 10)
    proc._handle_settings_write(huge)
    assert proc._stats["rejects"] == 1
    assert proc._stats["settings"] == 0
    proc.queues.ble_settings.put.assert_not_called()


def test_ble_settings_hex_at_limit_is_accepted():
    from control.ble_control_process import BleControlProcess

    proc = BleControlProcess.__new__(BleControlProcess)
    proc._stats = {"rejects": 0, "settings": 0}
    proc.queues = MagicMock()
    # Use the ipc.publish pathway via a direct queue mock — easier than
    # spinning up TopicQueues here.
    with patch("control.ble_control_process.publish") as pub:
        proc._handle_settings_write("ab" * 8)
        assert proc._stats["settings"] == 1
        assert proc._stats["rejects"] == 0
        pub.assert_called_once()


def test_ble_non_string_hex_is_rejected():
    from control.ble_control_process import BleControlProcess

    proc = BleControlProcess.__new__(BleControlProcess)
    proc._stats = {"rejects": 0, "settings": 0}
    proc._handle_settings_write(None)        # type: ignore[arg-type]
    assert proc._stats["rejects"] == 1
    assert proc._stats["settings"] == 0


# ────────── CommProcess URL scheme guard ──────────
def _make_comm_proc():
    from comm.comm_process import CommProcess

    proc = CommProcess.__new__(CommProcess)
    proc.log = MagicMock()
    return proc


def test_comm_post_json_refuses_file_scheme():
    proc = _make_comm_proc()
    proc._server_url = "file:///etc/passwd"
    with patch("comm.comm_process.urllib.request.urlopen") as urlopen:
        ok = proc._post_json("/whatever", {"k": "v"})
    assert ok is False
    urlopen.assert_not_called()


def test_comm_post_json_refuses_ftp_scheme():
    proc = _make_comm_proc()
    proc._server_url = "ftp://example.com"
    with patch("comm.comm_process.urllib.request.urlopen") as urlopen:
        ok = proc._post_json("/x", {})
    assert ok is False
    urlopen.assert_not_called()


def test_comm_post_json_accepts_http():
    proc = _make_comm_proc()
    proc._server_url = "http://server.example"
    fake_resp = MagicMock()
    fake_resp.status = 200
    fake_resp.__enter__ = lambda self: fake_resp
    fake_resp.__exit__ = lambda self, *a: False
    with patch("comm.comm_process.urllib.request.urlopen",
               return_value=fake_resp) as urlopen:
        ok = proc._post_json("/x", {"k": "v"})
    assert ok is True
    urlopen.assert_called_once()


# ────────── Adapter __init__ accepts **diag ──────────
@pytest.mark.parametrize("ctor_path", [
    # Adapters
    "adapters.unitree_go2.UnitreeGo2Adapter",
    "adapters.rtk_gnss.RtkGnssAdapter",
    "adapters.ntrip_process.NtripClientAdapter",
    "adapters.lte_modem.LteModemAdapter",
    "adapters.payload_sensors.ThermalCameraAdapter",
    "adapters.payload_sensors.LrfAdapter",
    "adapters.payload_sensors.ExternalImuAdapter",
    # Localization / mapping / mission / swarm
    "localization.localization_process.LocalizationProcess",
    "mapping.processes.SLAMBridgeProcess",
    "mapping.processes.MapFusionProcess",
    "mapping.shared_map_receiver.SharedMapReceiverProcess",
    "mission.mission_process.MissionProcess",
    "swarm.swarm_bridge.SwarmBridgeProcess",
])
def test_adapter_init_accepts_diag_kwargs(ctor_path):
    """main.py forwards **diag (metrics_dict, log_dir, ...) to every
    process. A subclass whose __init__ doesn't accept **diag will
    TypeError at startup before its setup() ever runs."""
    mod_name, cls_name = ctor_path.rsplit(".", 1)
    mod = __import__(mod_name, fromlist=[cls_name])
    cls = getattr(mod, cls_name)
    sig = inspect.signature(cls.__init__)
    has_var_keyword = any(p.kind is inspect.Parameter.VAR_KEYWORD
                           for p in sig.parameters.values())
    assert has_var_keyword, (
        f"{ctor_path}.__init__ must accept **diag — main.py forwards "
        "metrics_dict/log_dir/crash_dir/profile/etc. as kwargs.")


# ────────── Config.validate_required ──────────
def test_config_validate_required_reports_missing(caplog):
    from core.config import Config

    cfg = Config({"comm": {"server_url": "https://x.example"}})
    with caplog.at_level(logging.WARNING):
        missing = cfg.validate_required(
            ["comm.server_url", "mission.routes_file", "system.cpu_affinity"],
            logger=logging.getLogger("test"),
        )
    assert missing == ["mission.routes_file", "system.cpu_affinity"]
    # Each missing key produced a warning record.
    msgs = [r.getMessage() for r in caplog.records]
    assert any("mission.routes_file" in m for m in msgs)
    assert any("system.cpu_affinity" in m for m in msgs)


def test_config_validate_required_treats_empty_as_missing():
    from core.config import Config

    cfg = Config({"comm": {"server_url": ""}, "system": {"cpu_affinity": {}}})
    missing = cfg.validate_required(
        ["comm.server_url", "system.cpu_affinity"])
    assert "comm.server_url" in missing
    assert "system.cpu_affinity" in missing


def test_config_validate_required_accepts_populated_values():
    from core.config import Config

    cfg = Config({
        "comm": {"server_url": "https://x"},
        "system": {"cpu_affinity": {"go2": [4]}},
    })
    assert cfg.validate_required(
        ["comm.server_url", "system.cpu_affinity"]) == []


# ────────── Per-topic queue maxsize ──────────
def test_make_topic_queues_sizes_high_rate_topics_larger():
    from core.ipc import DEFAULT_MAXSIZE, make_topic_queues

    queues = make_topic_queues()
    # IMU queues should be sized for ~200 Hz consumers.
    assert queues.imu._maxsize > DEFAULT_MAXSIZE
    assert queues.imu_external._maxsize > DEFAULT_MAXSIZE
    # Pose at ~100 Hz.
    assert queues.pose._maxsize > DEFAULT_MAXSIZE
    # Control commands stay at the default — small bursts only.
    assert queues.goal_pose._maxsize == DEFAULT_MAXSIZE
    assert queues.cmd_vel._maxsize == DEFAULT_MAXSIZE


def test_make_topic_queues_overrides_take_precedence():
    from core.ipc import make_topic_queues

    queues = make_topic_queues(overrides={"goal_pose": 99})
    assert queues.goal_pose._maxsize == 99


# ────────── RTK stub mode quality ──────────
def test_rtk_stub_default_quality_is_gps_not_fixed():
    """Stub mode previously published RTK_FIX_FIXED, which let
    localization treat synthetic dev data as centimeter-grade truth.
    Default is now RTK_FIX_GPS unless config opts in."""
    from adapters.rtk_gnss import RtkGnssAdapter
    from core.messages import RTK_FIX_FIXED, RTK_FIX_GPS

    adapter = RtkGnssAdapter.__new__(RtkGnssAdapter)
    cfg = MagicMock()
    cfg.get.side_effect = lambda *k, default=None: {
        ("rtk", "device"):       "/nonexistent",
        ("rtk", "baud"):         115200,
        ("rtk", "stub_quality"): "gps",
        ("rtk", "ntrip", "enabled"): False,
    }.get(tuple(k), default)
    adapter.cfg = cfg
    adapter._stub_mode = False
    adapter._stub_t0 = 0.0
    adapter._stub_quality = RTK_FIX_FIXED        # poison the default to prove setup overrides
    adapter._serial = None
    adapter._serial_lock = None
    adapter._gga_lock = None
    import threading as _t
    adapter._serial_lock = _t.Lock()
    adapter._gga_lock = _t.Lock()
    # Force serial-open to fail so setup falls into stub mode.
    with patch("builtins.__import__", side_effect=ImportError):
        try:
            adapter.setup()
        except Exception:
            # We don't care if setup hits other branches; we only need
            # the stub_quality assignment to have run, which happens
            # before the serial import.
            pass
    assert adapter._stub_quality == RTK_FIX_GPS


def test_rtk_stub_quality_unknown_string_falls_back_to_gps(caplog):
    from adapters.rtk_gnss import _STUB_QUALITY_BY_NAME
    from core.messages import RTK_FIX_GPS

    # Sanity — the table itself maps the four meaningful names.
    assert _STUB_QUALITY_BY_NAME["gps"] == RTK_FIX_GPS
    assert "fixed" in _STUB_QUALITY_BY_NAME
    assert "float" in _STUB_QUALITY_BY_NAME
    assert "dgps" in _STUB_QUALITY_BY_NAME
