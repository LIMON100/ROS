"""Control plane — BLE/WiFi orchestration + 7-Phase state machine.

Adapted from AIRYS SAN-BLE-WIFI-001 protocol; on the robot side this is
the always-on control channel paired to AIRYS-APP / patrol_server.
"""
from .ble_control_process import BleControlProcess
from .display_process import DisplayProcess
from .http_recordings_process import HttpRecordingsProcess
from .orchestrator_process import OrchestratorProcess
from .state_machine import (
    ConnectionFsm,
    ErrorCode,
    FsmStats,
    Opcode,
    Phase,
    TransitionEvent,
)
from .wifi_control_process import WifiControlProcess
from .ws_telemetry_process import WsTelemetryProcess

__all__ = [
    "Phase", "ErrorCode", "Opcode",
    "ConnectionFsm", "TransitionEvent", "FsmStats",
    "BleControlProcess", "WifiControlProcess", "OrchestratorProcess",
    "WsTelemetryProcess", "HttpRecordingsProcess", "DisplayProcess",
]
