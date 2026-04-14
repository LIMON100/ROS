"""
health_evaluators.py — Link health evaluation for SkyHunter comm stack.

Pure Python — no ROS dependency.  Each evaluator takes the cached message
and the age of that message (in seconds), and returns True/False.

Keeping evaluators here makes them unit-testable without spinning up a node.
"""

from statistics import mean
from typing import Optional

from skyhunter_msgs.msg import LteStatus, LoraStatus, MeshMetrics


def evaluate_wifi6(
    metrics: Optional[MeshMetrics],
    age_s: float,
    rssi_fail_threshold: float,    # dBm — below this = failing
    loss_fail_threshold: float,    # % — above this = failing
    stale_timeout_s: float = 5.0,
) -> bool:
    """
    Return True if the WiFi6 mesh is healthy enough to operate on.

    Uses the 'fail' thresholds — intentionally lenient so the FSM only
    triggers after sustained degradation (hysteresis handles the rest).
    """
    if metrics is None or age_s > stale_timeout_s:
        return False

    connected = [lnk for lnk in metrics.links if lnk.connected]
    if not connected:
        return False

    avg_rssi = mean(lnk.rssi_dbm for lnk in connected)
    avg_loss = mean(lnk.packet_loss_pct for lnk in metrics.links)

    return avg_rssi > rssi_fail_threshold and avg_loss < loss_fail_threshold


def evaluate_wifi6_recovered(
    metrics: Optional[MeshMetrics],
    age_s: float,
    rssi_recovery_threshold: float,  # dBm — stricter than fail threshold
    stale_timeout_s: float = 5.0,
) -> bool:
    """
    Return True if WiFi6 has recovered well enough to switch back to it.

    Stricter RSSI threshold than evaluate_wifi6() to create hysteresis
    dead-band: link must degrade below -75 dBm to leave WiFi6, but must
    recover above -65 dBm to return.
    """
    if metrics is None or age_s > stale_timeout_s:
        return False

    # Any jammed link blocks recovery — partial jams should not allow
    # the FSM recovery timer to arm
    if any(lnk.jammed for lnk in metrics.links):
        return False

    connected = [lnk for lnk in metrics.links if lnk.connected]
    if not connected:
        return False

    avg_rssi = mean(lnk.rssi_dbm for lnk in connected)
    return avg_rssi > rssi_recovery_threshold


def evaluate_lte(
    status: Optional[LteStatus],
    age_s: float,
    rtt_fail_threshold_ms: float,
    stale_timeout_s: float,
) -> bool:
    """
    Return True if LTE is connected and RTT is within threshold.

    Uses state == 1 (CONNECTED) per LteStatus.msg.
    DEGRADED (state == 2) is treated as unhealthy — let the FSM decide
    whether to fall through to LoRa.
    """
    if status is None or age_s > stale_timeout_s:
        return False

    return status.state == 1 and status.rtt_ms < rtt_fail_threshold_ms


def evaluate_lora(
    status: Optional[LoraStatus],
    age_s: float,
    heartbeat_timeout_s: float,
) -> bool:
    """
    Return True if the LoRa link is alive and recent heartbeat received.
    """
    if status is None or age_s > heartbeat_timeout_s:
        return False

    return status.active