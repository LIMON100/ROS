#!/usr/bin/env python3
"""
lte_simulator.py — SkyHunter Networking Node 3 (ROS 2 entry point)

Thin ROS 2 node.  All LTE state logic lives in models/lte_model.py.

Responsibilities here:
  - Declare / load parameters
  - Drive LteModel.tick() on a 2 Hz timer
  - Map LteSnapshot → LteStatus.msg and publish
  - Expose /lte/inject_failure service for test scripts

Package : skyhunter_comm
Topics  : /lte_status  (skyhunter_msgs/LteStatus, 2 Hz)
Services: /lte/inject_failure  (skyhunter_msgs/InjectFailure)
"""

import time

import rclpy
from rclpy.node import Node

from skyhunter_msgs.msg import LteStatus
from skyhunter_msgs.srv import InjectFailure

from skyhunter_comm.models import (
    LteModel,
    LteModelConfig,
    InjectedFailure,
    STATE_NAMES,
)


class LteSimulator(Node):

    def __init__(self) -> None:
        super().__init__("lte_simulator")

        # ── Parameters ───────────────────────────────────────────────────────
        self.declare_parameter("base_rtt_ms", 50.0)
        self.declare_parameter("base_loss_pct", 2.0)
        self.declare_parameter("bandwidth_mbps", 20.0)
        self.declare_parameter("auto_reconnect_s", 15.0)
        self.declare_parameter("initial_state", "connected")
        self.declare_parameter("publish_rate_hz", 2.0)


        p = self.get_parameter
        config = LteModelConfig(
            base_rtt_ms      = p("base_rtt_ms").value,
            base_loss_pct    = p("base_loss_pct").value,
            bandwidth_mbps   = p("bandwidth_mbps").value,
            auto_reconnect_s = p("auto_reconnect_s").value,
            initial_state    = p("initial_state").value,
        )
        publish_rate = p("publish_rate_hz").value

        # ── Model ────────────────────────────────────────────────────────────
        self._model = LteModel(config)

        # ── Publisher ────────────────────────────────────────────────────────
        self._pub = self.create_publisher(LteStatus, "/lte_status", 10)

        # ── Service ──────────────────────────────────────────────────────────
        self.create_service(
            InjectFailure, "/lte/inject_failure", self._inject_failure_cb
        )

        # ── Timer ────────────────────────────────────────────────────────────
        self.create_timer(1.0 / publish_rate, self._tick)

        self.get_logger().info(
            f"lte_simulator ready — "
            f"base RTT: {config.base_rtt_ms} ms, "
            f"base loss: {config.base_loss_pct}%"
        )

    # ── Timer callback ───────────────────────────────────────────────────────

    def _tick(self) -> None:
        snapshot = self._model.tick(time.monotonic())

        msg = LteStatus()
        msg.header.stamp    = self.get_clock().now().to_msg()
        msg.state           = snapshot.state
        msg.rtt_ms          = snapshot.rtt_ms
        msg.packet_loss_pct = snapshot.packet_loss_pct
        msg.bandwidth_mbps  = snapshot.bandwidth_mbps
        msg.uptime_s        = snapshot.uptime_s

        self._pub.publish(msg)

    # ── Service callback ─────────────────────────────────────────────────────

    def _inject_failure_cb(
        self, request: InjectFailure.Request, response: InjectFailure.Response
    ) -> InjectFailure.Response:
        failure = InjectedFailure(
            failure_type = request.failure_type,
            value        = 0.0,            # InjectFailure.srv has no value field
            duration_s   = request.duration_s,
            start_s      = time.monotonic(),
        )
        self._model.inject(failure)

        self.get_logger().warn(
            f"[LTE] Failure injected — type: {request.failure_type}, "
            f"value: 0.0, duration: {request.duration_s}s"
        )

        response.success = True
        response.message = (
            f"Injected {request.failure_type} "
            f"(active failures: {self._model.active_failure_count})"
        )
        return response


def main(args=None) -> None:
    rclpy.init(args=args)
    node = LteSimulator()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":
    main()