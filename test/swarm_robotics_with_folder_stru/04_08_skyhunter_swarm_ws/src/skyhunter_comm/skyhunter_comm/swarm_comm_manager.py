#!/usr/bin/env python3
"""
swarm_comm_manager.py — SkyHunter Networking Node 2 (ROS 2 entry point)

Thin ROS 2 node.  All FSM logic lives in comm_fsm.py.
All health evaluation logic lives in health_evaluators.py.

Responsibilities here:
  - Declare / load parameters
  - Subscribe to /mesh_metrics, /lte_status, /lora/status
  - Cache latest messages + timestamps
  - Call health evaluators → feed results into CommFSM
  - Publish /comm_state at 2 Hz

Package : tin3_networking
Topic   : /comm_state  (skyhunter_msgs/CommState, 2 Hz)
"""

import time
from statistics import mean

import rclpy
from rclpy.node import Node

from skyhunter_msgs.msg import CommState, LteStatus, LoraStatus, MeshMetrics

from skyhunter_comm.models.comm_fsm import CommFSM, CommFSMParams, TIER_NAMES
from skyhunter_comm.models.health_evaluators import (
    evaluate_wifi6,
    evaluate_wifi6_recovered,
    evaluate_lte,
    evaluate_lora,
)


class SwarmCommManager(Node):

    def __init__(self) -> None:
        super().__init__("swarm_comm_manager")

        # ── Parameters ───────────────────────────────────────────────────────
        self.declare_parameter("wifi6_rssi_fail_threshold", -75.0)
        self.declare_parameter("wifi6_loss_fail_threshold", 30.0)
        self.declare_parameter("wifi6_fail_duration_s", 3.0)
        self.declare_parameter("wifi6_recovery_rssi", -65.0)
        self.declare_parameter("wifi6_recovery_duration_s", 5.0)
        self.declare_parameter("lte_timeout_s", 10.0)
        self.declare_parameter("lte_rtt_fail_threshold_ms", 500.0)
        self.declare_parameter("lora_heartbeat_timeout_s", 30.0)
        self.declare_parameter("publish_rate_hz", 2.0)

        p = self.get_parameter
        self._p_wifi6_rssi_fail    = p("wifi6_rssi_fail_threshold").value
        self._p_wifi6_loss_fail    = p("wifi6_loss_fail_threshold").value
        self._p_wifi6_recover_rssi = p("wifi6_recovery_rssi").value
        self._p_lte_timeout        = p("lte_timeout_s").value
        self._p_lte_rtt_threshold  = p("lte_rtt_fail_threshold_ms").value
        self._p_lora_timeout       = p("lora_heartbeat_timeout_s").value
        publish_rate               = p("publish_rate_hz").value

        fsm_params = CommFSMParams(
            wifi6_fail_duration_s     = p("wifi6_fail_duration_s").value,
            wifi6_recovery_duration_s = p("wifi6_recovery_duration_s").value,
            lte_fail_duration_s       = self._p_lte_timeout,
        )

        # ── FSM ──────────────────────────────────────────────────────────────
        self._fsm = CommFSM(fsm_params)

        # ── Message cache ────────────────────────────────────────────────────
        self._wifi6_metrics = None
        self._wifi6_ts: float = 0.0

        self._lte_status = None
        self._lte_ts: float = 0.0

        self._lora_status = None
        self._lora_ts: float = 0.0

        # ── Subscribers ──────────────────────────────────────────────────────
        self.create_subscription(MeshMetrics, "/mesh_metrics", self._cb_wifi6, 10)
        self.create_subscription(LteStatus,   "/lte_status",   self._cb_lte,   10)
        self.create_subscription(LoraStatus,  "/lora/status",  self._cb_lora,  10)

        # ── Publisher ────────────────────────────────────────────────────────
        self._pub = self.create_publisher(CommState, "/comm_state", 10)

        # ── Timer ────────────────────────────────────────────────────────────
        self.create_timer(1.0 / publish_rate, self._tick)

        self.get_logger().info(
            f"swarm_comm_manager ready — "
            f"initial tier: {TIER_NAMES[self._fsm.current_tier]}"
        )

    # ── Callbacks ────────────────────────────────────────────────────────────

    def _cb_wifi6(self, msg: MeshMetrics) -> None:
        self._wifi6_metrics = msg
        self._wifi6_ts = time.monotonic()

    def _cb_lte(self, msg: LteStatus) -> None:
        self._lte_status = msg
        self._lte_ts = time.monotonic()

    def _cb_lora(self, msg: LoraStatus) -> None:
        self._lora_status = msg
        self._lora_ts = time.monotonic()

    # ── Main tick ────────────────────────────────────────────────────────────

    def _tick(self) -> None:
        now_s = time.monotonic()

        wifi6_age = now_s - self._wifi6_ts
        lte_age   = now_s - self._lte_ts
        lora_age  = now_s - self._lora_ts

        wifi6_ok = evaluate_wifi6(
            self._wifi6_metrics, wifi6_age,
            self._p_wifi6_rssi_fail, self._p_wifi6_loss_fail,
        )
        wifi6_recovered = evaluate_wifi6_recovered(
            self._wifi6_metrics, wifi6_age,
            self._p_wifi6_recover_rssi,
        )
        lte_ok = evaluate_lte(
            self._lte_status, lte_age,
            self._p_lte_rtt_threshold, self._p_lte_timeout,
        )
        lora_ok = evaluate_lora(
            self._lora_status, lora_age,
            self._p_lora_timeout,
        )

        transitioned = self._fsm.update(
            wifi6_ok, wifi6_recovered, lte_ok, lora_ok, now_s
        )

        if transitioned:
            self.get_logger().warn(
                f"[FSM] → {TIER_NAMES[self._fsm.current_tier]}"
            )

        self._publish(wifi6_ok, lte_ok, lora_ok, lora_age)

    # ── Publisher helper ─────────────────────────────────────────────────────

    def _publish(
        self,
        wifi6_ok: bool,
        lte_ok:   bool,
        lora_ok:  bool,
        lora_age: float,
    ) -> None:
        fsm = self._fsm
        msg = CommState()
        msg.header.stamp = self.get_clock().now().to_msg()

        msg.current_tier      = fsm.current_tier
        msg.previous_tier     = fsm.previous_tier
        msg.time_in_current_s = fsm.time_in_tier_s

        # WiFi6 snapshot
        if self._wifi6_metrics is not None:
            connected = [lnk for lnk in self._wifi6_metrics.links if lnk.connected]
            msg.wifi6_avg_rssi_dbm = (
                mean(lnk.rssi_dbm for lnk in connected) if connected else -999.0
            )
            msg.wifi6_packet_loss_pct = (
                mean(lnk.packet_loss_pct for lnk in self._wifi6_metrics.links)
                if self._wifi6_metrics.links else 100.0
            )
            msg.wifi6_active_links = len(connected)
        else:
            msg.wifi6_avg_rssi_dbm    = -999.0
            msg.wifi6_packet_loss_pct = 100.0
            msg.wifi6_active_links    = 0

        # LTE snapshot
        msg.lte_connected = lte_ok
        msg.lte_rtt_ms    = self._lte_status.rtt_ms if self._lte_status else 0.0

        # LoRa snapshot
        msg.lora_active               = lora_ok
        msg.lora_last_heartbeat_age_s = float(lora_age)

        self._pub.publish(msg)


def main(args=None) -> None:
    rclpy.init(args=args)
    node = SwarmCommManager()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":
    main()