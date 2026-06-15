# Copyright 2026 SkyAutoNet Inc.
#
# Proprietary and confidential. Unauthorized copying, distribution, or use
# of this file, via any medium, is strictly prohibited.

"""SAN v1.5 Phase 0 PR-D — CommandAuthGate unit tests."""
import pytest

import rclpy
from rclpy.node import Node

from san_operator_tools.command_auth import CommandAuthGate


@pytest.fixture
def rclpy_session():
    """Init+shutdown rclpy around each test."""
    rclpy.init(args=[])
    yield
    rclpy.shutdown()


def _make_node(parameter_overrides):
    """Create a Node with the supplied parameter overrides."""
    from rclpy.parameter import Parameter
    overrides = [Parameter(k, value=v) for k, v in parameter_overrides.items()]
    node = Node("test_gate_node",
                allow_undeclared_parameters=False,
                automatically_declare_parameters_from_overrides=False,
                parameter_overrides=overrides)
    return node


def test_publishes_with_operator_id(rclpy_session):
    """operator_id supplied → check_and_log returns True."""
    node = _make_node({"operator_id": "op_alpha", "production_mode": True})
    gate = CommandAuthGate(node, "WaypointCommand")
    assert gate.check_and_log("waypoints=4", target_id=2) is True
    node.destroy_node()


def test_refuses_without_operator_id_in_production(rclpy_session):
    """production_mode=true + empty operator_id → refuse (False)."""
    node = _make_node({"operator_id": "", "production_mode": True})
    gate = CommandAuthGate(node, "FormationCommand")
    # Three attempts in a row should all be refused.
    assert gate.check_and_log("formation=wedge") is False
    assert gate.check_and_log("formation=line")  is False
    assert gate.check_and_log("formation=column") is False
    node.destroy_node()


def test_allows_without_operator_id_in_dev_mode(rclpy_session):
    """production_mode=false → empty operator_id still publishes (with WARN)."""
    node = _make_node({"operator_id": "", "production_mode": False})
    gate = CommandAuthGate(node, "WaypointCommand")
    assert gate.check_and_log("waypoints=2") is True
    node.destroy_node()


def test_publish_counter_increments(rclpy_session):
    """check_and_log increments counter on each accepted call."""
    node = _make_node({"operator_id": "op_test", "production_mode": False})
    gate = CommandAuthGate(node, "WaypointCommand")
    assert gate.check_and_log("s1") is True
    assert gate.check_and_log("s2") is True
    assert gate.check_and_log("s3") is True
    # Counter is private but we can verify via reflection.
    assert gate._publish_counter == 3  # noqa: SLF001
    node.destroy_node()


def test_refusal_does_not_increment_counter(rclpy_session):
    """check_and_log refusal in production mode does NOT count toward publishes."""
    node = _make_node({"operator_id": "", "production_mode": True})
    gate = CommandAuthGate(node, "FormationCommand")
    assert gate.check_and_log("formation=wedge") is False
    assert gate._publish_counter == 0  # noqa: SLF001
    node.destroy_node()
