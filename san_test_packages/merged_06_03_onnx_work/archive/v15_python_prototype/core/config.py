"""
Config loader. Single source of truth for all tunables.
Layered: defaults < system.yaml < env vars < CLI overrides.
"""
from __future__ import annotations

import os
from pathlib import Path
from typing import Any, Dict, Optional

try:
    import yaml
except ImportError:                  # YAML optional in test env
    yaml = None


_DEFAULT_CONFIG: Dict[str, Any] = {
    "system": {
        # SAN v1.3 §11 five-tier deployment policy. Base default is the
        # safest: production. Overlays bump down to demo / lab_test /
        # bench / development as needed. See core/deployment.py and
        # docs/coding_standards/deployment_modes.md.
        "deployment_mode": "production",
        # RK3588 8-core layout: 0..3 = A55 (small), 4..7 = A76 (big)
        "cpu_affinity": {
            "go2_adapter":     [4],         # A76 single core, RT
            "localization":    [5],         # A76
            "slam_bridge":     [6, 7],      # A76 dual (heavy)
            "perception":      [3],         # A55 (NPU does heavy lifting)
            "mission":         [2],         # A55
            "map_fusion":      [1],         # A55
            "comm":            [0],         # A55
            "safety":          [0],         # A55 (shared with comm)
            "swarm_bridge":    [0],         # A55 (shared with comm/safety, low rate)
        },
        "rt_priority": {
            "go2_adapter": 50,              # high but below kernel
            "localization": 30,
        },
    },
    "go2": {
        "interface": "eth0",                # Ethernet to Go2
        "robot_ip":  "192.168.123.161",     # Go2 default IP
        "host_ip":   "192.168.123.99",
        "lidar_topic": "rt/utlidar/cloud_deskewed",
        "imu_topic":   "rt/utlidar/imu",
        "camera_topic":"rt/frontvideostream",
        "goal_pose_topic": "/goal_pose",
        "cmd_vel_topic":   "/cmd_vel",
    },
    "shm": {
        "lidar_slot_bytes": 16 * 1024 * 1024,    # 16 MB / scan (huge buffer)
        "lidar_n_slots":    8,
        "camera_slot_bytes":  4 * 1024 * 1024,   # 4 MB / encoded frame
        "camera_n_slots":   16,
    },
    "perception": {
        "ppe_model_path":    "/opt/models/yolov8n_ppe.rknn",
        "hazard_model_path": "/opt/models/yolov8n_hazard.rknn",
        "npu_core_mask":     "auto",
        "min_confidence":    0.45,
    },
    "mission": {
        "patrol_schedule": ["06:00", "10:00", "14:00", "18:00"],
        "dwell_sec_default": 30.0,
        "battery_threshold_start": 0.60,
        "battery_threshold_return": 0.30,
    },
    "comm": {
        "server_url":      "https://patrol.example.com",
        "upload_interval_s": 60,
        "wifi_ssid":       "construction_site",
    },
    "safety": {
        "comm_loss_timeout_s": 30,
        "fall_detect_acc_g": 2.5,
    },
}


class Config:
    def __init__(self, data: Dict[str, Any]):
        self._d = data

    @classmethod
    def load(cls, path: Optional[str] = None,
             deployment_mode: Optional[str] = None) -> "Config":
        """Layered config load.

        Order (later wins): _DEFAULT_CONFIG → base yaml → overlay yaml
        (per deployment_mode) → env overrides (PATROL__*).

        Resolution of `deployment_mode`:
          1. explicit argument (CLI / test)
          2. PATROL__SYSTEM__DEPLOYMENT_MODE env var
          3. base yaml `system.deployment_mode`
          4. default "production"

        Overlay file convention: `<base_dir>/<base_stem>.<mode>.yaml`
        (e.g. `config/system.lab_test.yaml`). Missing overlay file for
        any non-production mode is a hard error so a typo'd mode name
        does NOT silently fall back to production.
        """
        d = _deep_copy(_DEFAULT_CONFIG)
        base_user: Dict[str, Any] = {}
        if path and yaml and Path(path).exists():
            with open(path) as f:
                base_user = yaml.safe_load(f) or {}
            _deep_merge(d, base_user)

        # Resolve the deployment mode before applying overlays.
        env_mode = os.environ.get("PATROL__SYSTEM__DEPLOYMENT_MODE")
        resolved_mode = (deployment_mode
                         or env_mode
                         or (base_user.get("system") or {}).get("deployment_mode")
                         or d["system"].get("deployment_mode")
                         or "production")
        resolved_mode = str(resolved_mode).strip().lower()
        d["system"]["deployment_mode"] = resolved_mode

        # Apply per-mode overlay if not production (production is the
        # base; overlay files are optional only for production).
        if path and yaml and resolved_mode != "production":
            base_path = Path(path)
            overlay = base_path.with_name(
                f"{base_path.stem}.{resolved_mode}{base_path.suffix}")
            if not overlay.exists():
                raise FileNotFoundError(
                    f"deployment_mode={resolved_mode!r} requires overlay "
                    f"file {overlay} but it does not exist")
            with open(overlay) as f:
                overlay_data = yaml.safe_load(f) or {}
            _deep_merge(d, overlay_data)
            # Overlay must not silently downgrade the resolved mode.
            d["system"]["deployment_mode"] = resolved_mode

        # env overrides: PATROL__SECTION__KEY=value
        for k, v in os.environ.items():
            if k.startswith("PATROL__"):
                parts = k[len("PATROL__"):].lower().split("__")
                _set_nested(d, parts, v)
        return cls(d)

    @property
    def deployment_mode(self) -> str:
        """Resolved deployment mode string ("production"/"demo"/...)."""
        return str(self.get("system", "deployment_mode",
                            default="production"))

    def get(self, *keys, default=None):
        cur = self._d
        for k in keys:
            if not isinstance(cur, dict) or k not in cur:
                return default
            cur = cur[k]
        return cur

    def section(self, name: str) -> Dict[str, Any]:
        return self._d.get(name, {})

    def validate_required(self, paths, logger=None) -> list:
        """Check that each dotted path resolves to a non-empty value.

        `paths` accepts either "section.key" strings or tuples of keys.
        A path is considered missing if any segment is absent, or if the
        final value is None / empty string / empty dict / empty list.

        Returns the list of paths that were missing. When `logger` is
        provided, each miss is logged at WARNING level so an operator
        can spot a typo'd YAML override before the system runs with the
        baked-in default.

        Non-fatal by design: load() never raises on missing keys, so
        validation just surfaces them — main.py decides how to react.
        """
        missing = []
        for p in paths:
            keys = p.split(".") if isinstance(p, str) else list(p)
            val = self.get(*keys, default=None)
            empty = (val is None
                     or (isinstance(val, (str, list, dict, tuple)) and len(val) == 0))
            if empty:
                missing.append(".".join(keys))
                if logger is not None:
                    logger.warning(
                        f"config: required key missing or empty: {'.'.join(keys)}")
        return missing


def _deep_copy(d):
    if isinstance(d, dict):
        return {k: _deep_copy(v) for k, v in d.items()}
    if isinstance(d, list):
        return [_deep_copy(v) for v in d]
    return d


def _deep_merge(dst: dict, src: dict) -> None:
    for k, v in src.items():
        if k in dst and isinstance(dst[k], dict) and isinstance(v, dict):
            _deep_merge(dst[k], v)
        else:
            dst[k] = v


def _set_nested(d: dict, keys: list, val: str) -> None:
    cur = d
    for k in keys[:-1]:
        cur = cur.setdefault(k, {})
    # naive type coercion
    if val.isdigit():
        cur[keys[-1]] = int(val)
    else:
        try:
            cur[keys[-1]] = float(val)
        except ValueError:
            cur[keys[-1]] = val
