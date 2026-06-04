#!/usr/bin/env python3
# Copyright 2026 SkyAutoNet Inc.
#
# Proprietary and confidential. Unauthorized copying, distribution, or use
# of this file, via any medium, is strictly prohibited.

"""
lora_simulator.py — SkyHunter Networking Node 5 (ROS 2 entry point)

Thin ROS 2 node.  All LoRa simulation logic lives in models/lora_model.py.

Responsibilities here:
  - Declare / load parameters
  - Staggered per-robot heartbeat timers
  - Delayed heartbeat publish (one-shot timer after airtime delay)
  - /lora/status publisher at 0.2 Hz
  - /lora/send_estop service
  - /lora/inject_failure service

Package : san_comm_sim
Topics  : /lora/heartbeat  (san_comm_msgs/LoraHeartbeat, staggered ~1.6 Hz total)
          /lora/status     (san_comm_msgs/LoraStatus, 0.2 Hz)
          /lora/estop      (san_comm_msgs/LoraEstop, on-demand)
Services: /lora/send_estop      (san_comm_msgs/SendEstop)
          /lora/inject_failure  (san_comm_msgs/InjectFailure)
"""

import time

import rclpy
from rclpy.node import Node
from rclpy.timer import Timer

from san_comm_msgs.msg import LoraHeartbeat, LoraEstop, LoraStatus
from san_comm_msgs.srv import SendEstop, InjectFailure

from san_comm_sim.models import (
    LoraModel,
    LoraModelConfig,
)


class LoraSimulator(Node):

    def __init__(self) -> None:
        super().__init__("lora_simulator")

        # ── Parameters ───────────────────────────────────────────────────────
        self.declare_parameter("num_robots",8)
        self.declare_parameter("heartbeat_interval_s", 5.0)
        self.declare_parameter("base_delay_ms", 350.0)
        self.declare_parameter("delay_jitter_ms", 150.0)
        self.declare_parameter("base_loss_rate", 0.07)
        self.declare_parameter("duty_cycle_limit", 0.01)
        self.declare_parameter("spreading_factor", 10)
        self.declare_parameter("max_payload_bytes", 222)

        p = self.get_parameter
        config = LoraModelConfig(
            num_robots           = p("num_robots").value,
            heartbeat_interval_s = p("heartbeat_interval_s").value,
            base_delay_ms        = p("base_delay_ms").value,
            delay_jitter_ms      = p("delay_jitter_ms").value,
            base_loss_rate       = p("base_loss_rate").value,
            duty_cycle_limit     = p("duty_cycle_limit").value,
            spreading_factor     = p("spreading_factor").value,
            max_payload_bytes    = p("max_payload_bytes").value,
        )
        self._num_robots          = config.num_robots
        self._heartbeat_interval  = config.heartbeat_interval_s

        # ── Model ────────────────────────────────────────────────────────────
        self._model = LoraModel(config)
        self._model.start(time.monotonic())

        # ── Publishers ───────────────────────────────────────────────────────
        self._heartbeat_pub = self.create_publisher(LoraHeartbeat, "/lora/heartbeat", 10)
        self._estop_pub     = self.create_publisher(LoraEstop,     "/lora/estop",     10)
        self._status_pub    = self.create_publisher(LoraStatus,    "/lora/status",    10)

        # ── Services ─────────────────────────────────────────────────────────
        self.create_service(SendEstop,     "/lora/send_estop",     self._send_estop_cb)
        self.create_service(InjectFailure, "/lora/inject_failure", self._inject_failure_cb)

        # ── Staggered heartbeat timers ────────────────────────────────────────
        # Offset each robot by heartbeat_interval / num_robots so they don't
        # all fire at the same instant (= 625ms between each for defaults).
        self._delay_timers: list[Timer] = []
        stagger_s = self._heartbeat_interval / self._num_robots

        for robot_id in range(1, self._num_robots + 1):
            offset_s = (robot_id - 1) * stagger_s
            # One-shot timer to fire the first heartbeat at the correct offset,
            # then a repeating timer takes over.
            t = self.create_timer(
                offset_s if offset_s > 0 else self._heartbeat_interval,
                lambda r=robot_id: self._first_heartbeat(r),
            )
            self._delay_timers.append(t)

        # ── Status timer (0.2 Hz) ─────────────────────────────────────────────
        self.create_timer(5.0, self._publish_status)

        self.get_logger().info(
            f"lora_simulator ready — "
            f"{self._num_robots} robots, "
            f"heartbeat every {self._heartbeat_interval}s, "
            f"stagger {stagger_s:.3f}s"
        )

    # ── First heartbeat (one-shot offset) ────────────────────────────────────

    def _first_heartbeat(self, robot_id: int) -> None:
        """
        Called once at the staggered offset for this robot.
        Cancels the offset timer, fires the first heartbeat,
        then starts the repeating timer.
        """
        # Cancel the offset one-shot
        idx = robot_id - 1
        if idx < len(self._delay_timers):
            self._delay_timers[idx].cancel()

        # Fire first heartbeat
        self._try_heartbeat(robot_id)

        # Start repeating timer at full interval
        self.create_timer(
            self._heartbeat_interval,
            lambda r=robot_id: self._try_heartbeat(r),
        )

    # ── Heartbeat attempt ─────────────────────────────────────────────────────

    def _try_heartbeat(self, robot_id: int) -> None:
        """Attempt to generate and schedule a heartbeat for robot_id."""
        data = self._model.try_heartbeat(robot_id, time.monotonic())

        if data is None:
            self.get_logger().debug(
                f"[LoRa] Heartbeat dropped — robot {robot_id} "
                f"(duty: {self._model.duty_cycle_used_pct():.1f}%)"
            )
            return

        # Schedule delayed publish to simulate LoRa airtime
        delay_s = self._model.heartbeat_delay_s()
        self.create_timer(
            delay_s,
            lambda d=data: self._publish_heartbeat_once(d),
        )

    def _publish_heartbeat_once(self, data) -> None:
        """Publish one heartbeat message (called after airtime delay)."""
        msg = LoraHeartbeat()
        msg.header.stamp = self.get_clock().now().to_msg()
        msg.robot_id     = data.robot_id
        msg.battery_pct  = data.battery_pct
        msg.latitude     = data.latitude
        msg.longitude    = data.longitude
        msg.status_flags = data.status_flags
        msg.seq_number   = data.seq_number
        self._heartbeat_pub.publish(msg)

    # ── Status publisher ──────────────────────────────────────────────────────

    def _publish_status(self) -> None:
        now_s = time.monotonic()
        msg = LoraStatus()
        msg.header.stamp       = self.get_clock().now().to_msg()
        msg.active             = self._model.active
        msg.uptime_s           = self._model.uptime_s(now_s)
        msg.packets_sent       = self._model.packets_sent
        msg.packets_received   = 0   # stub: not tracking inbound
        msg.packets_dropped    = self._model.packets_dropped
        msg.duty_cycle_used_pct = self._model.duty_cycle_used_pct()
        self._status_pub.publish(msg)

    # ── E-stop service ────────────────────────────────────────────────────────

    def _send_estop_cb(
        self, request: SendEstop.Request, response: SendEstop.Response
    ) -> SendEstop.Response:
        result = self._model.try_estop(time.monotonic())

        self.get_logger().warn(
            f"[LoRa] E-stop — target: {request.target_robot}, "
            f"success: {result.success}, {result.message}"
        )

        if result.success:
            msg = LoraEstop()
            msg.header.stamp  = self.get_clock().now().to_msg()
            msg.command_type  = 1   # estop
            msg.target_robot  = request.target_robot
            msg.auth_token    = 0
            msg.seq_number    = self._model.next_estop_seq()
            self._estop_pub.publish(msg)

        response.success    = result.success
        response.ack_time_ms = float(result.ack_time_ms)
        response.message    = result.message
        return response

    # ── Inject failure service ────────────────────────────────────────────────

    def _inject_failure_cb(
        self, request: InjectFailure.Request, response: InjectFailure.Response
    ) -> InjectFailure.Response:
        if request.failure_type == "disconnect":
            self._model.active = False
            self.get_logger().warn("[LoRa] Failure injected — LoRa deactivated")
            response.success = True
            response.message = "LoRa deactivated"
        else:
            response.success = False
            response.message = f"Unknown failure type: {request.failure_type}"

        return response


def main(args=None) -> None:
    rclpy.init(args=args)
    node = LoraSimulator()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":
    main()