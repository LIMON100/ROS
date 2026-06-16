# Copyright 2026 SkyAutoNet Inc.
#
# Proprietary and confidential. Unauthorized copying, distribution, or use
# of this file, via any medium, is strictly prohibited.

# SAN-TST-S20-4 — Fire Authorization End-to-End test.
#
# Verifies the complete fire control chain (Phase 2-D / DCN-2026-001 D-004):
#   1. HMAC-SHA256 message authentication
#   2. Two-key (operator + safety officer) approval
#   3. Audit log (JSON Lines + sha256 chain + UUIDv4)
#
# Topology fix: the previous test looked for
# /robot_1/fire_authorization_node/{fire_authorized,fire_denied} — but
# the real node publishes /swarm/fire/authorization_response (global,
# P1 RELIABLE depth 10), per fire_authorization_node.cpp:35.
# Soft-fallback to "any topic containing 'fire_authorization'" hid this.
#
# Strict-mode design: assert the correct global topics, then publish
# an unsigned FireAuthorizationRequest and expect a DENIED response
# (HMAC verification will fail on a zero MAC). This proves the node
# actually processes inbound requests, not just brings up topics.

import os
import stat
import tempfile
import time
import unittest

import launch
import launch.actions
import launch_ros.actions
import launch_testing.actions
import launch_testing.markers
import pytest
import rclpy
from combat_robot_msgs.msg import (
    FireAuthorizationRequest,
    FireAuthorizationResponse,
)
from rclpy.node import Node
from rclpy.qos import QoSProfile, ReliabilityPolicy

FIRE_AUTH_DISCOVERY_S = 5.0
RESPONSE_DEADLINE_S   = 5.0

# HmacAuthenticator is fail-closed by design (hmac_authenticator.hpp:71):
# a missing / wrong-size / wrong-permission secret file makes the node ctor
# throw and the process die before any subscriber is wired. So we ship a
# real-but-dummy secret (32 zero bytes, mode 0400) for the launch. The
# test's intent — proving the node responds with REASON_DENIED_HMAC_FAIL
# to an empty-HMAC request — still holds: the probe sends "" as the
# signature, which decodes to nothing and fails the constant-time compare
# against the zero-secret-derived expected HMAC.
def _write_dummy_secret() -> str:
    fd, path = tempfile.mkstemp(prefix="san_tst_s20_4_secret_", suffix=".bin")
    try:
        os.write(fd, b"\x00" * 32)
    finally:
        os.close(fd)
    os.chmod(path, stat.S_IRUSR)   # 0400 — matches SDD-SWARM v1.5 §10
    return path


@pytest.mark.launch_test
@launch_testing.markers.keep_alive
def generate_test_description():
    """Launch fire_authorization_node with a dummy 32-byte secret so the
    fail-closed ctor succeeds and HMAC verification on the test request
    (which carries an empty signature) returns REASON_DENIED_HMAC_FAIL."""
    secret_path = _write_dummy_secret()
    audit_path = tempfile.mkstemp(
        prefix="san_tst_s20_4_audit_", suffix=".log")[1]
    return launch.LaunchDescription([
        launch_ros.actions.Node(
            package="san_fire_authorization",
            executable="fire_authorization_node",
            name="fire_authorization_node",
            namespace="/robot_1",
            output="screen",
            parameters=[{
                "secret_path": secret_path,
                "audit_log_path": audit_path,
            }],
        ),
        launch.actions.TimerAction(
            period=3.0,
            actions=[launch_testing.actions.ReadyToTest()],
        ),
    ])


class FireAuthProbe(Node):
    """Publishes a (deliberately invalid) FireAuthorizationRequest and
    listens on the response topic."""
    def __init__(self):
        super().__init__("fire_auth_probe")
        self.responses = []  # list of FireAuthorizationResponse

        p1_qos = QoSProfile(depth=10, reliability=ReliabilityPolicy.RELIABLE)
        self.req_pub = self.create_publisher(
            FireAuthorizationRequest,
            "/swarm/fire/authorization_request",
            p1_qos,
        )
        self.create_subscription(
            FireAuthorizationResponse,
            "/swarm/fire/authorization_response",
            self._on_response,
            p1_qos,
        )

    def _on_response(self, msg):
        self.responses.append(msg)

    def send_unsigned_request(self):
        msg = FireAuthorizationRequest()
        msg.request_id            = 1
        msg.sequence              = 1
        msg.operator_id           = "test-operator"
        msg.nonce                 = 0
        msg.request_timestamp_ms  = int(time.time() * 1000)
        msg.command_type          = FireAuthorizationRequest.TWO_KEY_KEY1_TARGET_TAP
        msg.target_lat_e7         = 0
        msg.target_lon_e7         = 0
        msg.target_alt_mm         = 0
        msg.hmac_signature        = ""   # empty — HMAC verify must fail
        self.req_pub.publish(msg)


class TestFireAuthorization(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        rclpy.init()
        cls.probe = FireAuthProbe()

    @classmethod
    def tearDownClass(cls):
        cls.probe.destroy_node()
        rclpy.shutdown()

    def _wait_for_topic(self, name, timeout_s):
        deadline = time.monotonic() + timeout_s
        while time.monotonic() < deadline:
            rclpy.spin_once(self.probe, timeout_sec=0.2)
            topics = dict(self.probe.get_topic_names_and_types())
            if name in topics:
                return True
        return False

    def test_01_global_fire_topics_present(self):
        """fire_authorization_node publishes on the global /swarm/fire/*
        namespace, not /robot_*/fire_authorization_node/*. Strict
        assertion — no soft fallback."""
        required = [
            "/swarm/fire/authorization_request",
            "/swarm/fire/authorization_response",
        ]
        for t in required:
            self.assertTrue(
                self._wait_for_topic(t, FIRE_AUTH_DISCOVERY_S),
                f"required global topic {t} not in graph after "
                f"{FIRE_AUTH_DISCOVERY_S}s"
            )

    def test_02_unsigned_request_is_denied(self):
        """Publish a request with no HMAC; node MUST respond denied.
        Proves end-to-end wiring: request_sub → onRequest → HMAC verify
        → resp_pub → our subscription."""
        # Give the publisher a moment to discover the subscription
        time.sleep(0.5)
        rclpy.spin_once(self.probe, timeout_sec=0.1)

        self.probe.send_unsigned_request()

        deadline = time.monotonic() + RESPONSE_DEADLINE_S
        while time.monotonic() < deadline:
            rclpy.spin_once(self.probe, timeout_sec=0.2)
            if self.probe.responses:
                break

        self.assertTrue(
            self.probe.responses,
            "No FireAuthorizationResponse received within "
            f"{RESPONSE_DEADLINE_S}s — node not processing requests"
        )
        # Schema (FireAuthorizationResponse.msg):
        #   bool   granted
        #   uint8  reason  ∈ {REASON_GRANTED, REASON_DENIED_HMAC_FAIL, ...}
        # An empty HMAC must produce granted=false with reason ==
        # REASON_DENIED_HMAC_FAIL.
        last = self.probe.responses[-1]
        self.assertFalse(
            last.granted,
            f"Unsigned request was unexpectedly granted; response={last}"
        )
        self.assertEqual(
            last.reason,
            FireAuthorizationResponse.REASON_DENIED_HMAC_FAIL,
            f"Expected REASON_DENIED_HMAC_FAIL, got reason={last.reason}"
        )
        print(f"\n[TST S20-4] Unsigned request correctly denied "
              f"(reason={last.reason}, {len(self.probe.responses)} resp)")
