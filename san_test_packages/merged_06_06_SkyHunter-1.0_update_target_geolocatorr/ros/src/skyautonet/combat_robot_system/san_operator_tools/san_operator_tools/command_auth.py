# Copyright 2026 SkyAutoNet Inc.
#
# Proprietary and confidential. Unauthorized copying, distribution, or use
# of this file, via any medium, is strictly prohibited.

"""SAN v1.5 Phase 0 PR-D — Operator command authorization gate.

The codebase review (safety-critical agent, finding #11) flagged that
operator tools issuing kinetic-effect commands (`/swarm/waypoint_command`,
`/swarm/formation_command`) have no authentication: any peer on the
DDS domain can publish mass-waypoint or formation-change messages.

Fire authorization already has HMAC + Two-key + audit (PR-A #118).
Kinetic-area commands deserve equivalent treatment, but full HMAC
integration requires:
  * adding a hmac_signature field to WaypointCommand / FormationCommand
    (combat_robot_msgs schema change — separate PR)
  * distributing the operator-side signing key alongside the gate's
    /etc/san/mesh_secret.bin (deployment work)
  * gate-side verifier (san_operation_control or a new gate node)

For Tier 0 the minimum compensating control is:
  * Require operator_id parameter — no anonymous commands. Refuse to
    publish if empty AND production_mode=true.
  * Log every kinetic-effect publish at WARN with the operator_id and
    a monotonically-increasing publish counter, so post-hoc audit
    can reconstruct who-sent-what (until HMAC + nonce land).
  * Document the gap so it's visible.

This module is the shared helper; waypoint_sender / formation_switcher
call into it.
"""
from typing import Optional

from rclpy.node import Node


# Phase 0 sentinel — flips to True once HMAC msg fields land + verifier
# is wired. Future code can guard production deploys with this.
HMAC_INTEGRATION_AVAILABLE: bool = False


class CommandAuthGate:
    """Tier 0 compensating control for unsigned kinetic commands.

    Owner code declares two parameters on its Node:
        operator_id       (string, default "")
        production_mode   (bool,   default False)

    Then before each publish, calls `check_and_log()`. Returns True
    when the publish should proceed; returns False AND logs an ERROR
    when blocked. Always logs every accepted publish at WARN with
    the operator_id so the audit reconstruction has a trail.
    """

    def __init__(self, node: Node, command_kind: str):
        self._node = node
        self._kind = command_kind   # "WaypointCommand" / "FormationCommand"
        # Don't re-declare if owner already declared them — Node
        # parameter system will throw on duplicate declarations.
        if not node.has_parameter("operator_id"):
            node.declare_parameter("operator_id", "")
        if not node.has_parameter("production_mode"):
            node.declare_parameter("production_mode", False)
        self._publish_counter = 0

    def operator_id(self) -> str:
        return str(self._node.get_parameter("operator_id").value)

    def production_mode(self) -> bool:
        return bool(self._node.get_parameter("production_mode").value)

    def check_and_log(self,
                       command_summary: str,
                       target_id: Optional[int] = None) -> bool:
        """Returns True when caller may publish, False to refuse.

        command_summary: short text included in the audit log, e.g.
            "waypoints=4 leader=2" or "formation=wedge".
        target_id: optional target_robot_id / leader_robot_id from msg.
        """
        op = self.operator_id()
        prod = self.production_mode()

        if not op:
            if prod:
                self._node.get_logger().error(
                    f"[{self._kind}] REFUSING to publish — operator_id "
                    f"is empty AND production_mode=true. Set "
                    f"`-p operator_id:=op_<name>` to authorize. "
                    f"Summary: {command_summary}")
                return False
            else:
                self._node.get_logger().warn(
                    f"[{self._kind}] publishing with EMPTY operator_id "
                    f"(production_mode=false — dev only). Summary: "
                    f"{command_summary}")

        self._publish_counter += 1
        target_txt = f" target={target_id}" if target_id is not None else ""
        self._node.get_logger().warn(
            f"[{self._kind}] publish #{self._publish_counter} "
            f"operator={op or '<anon>'} {command_summary}{target_txt} "
            f"(NOTE: not yet HMAC-signed — Phase 1+ TODO)")
        return True
