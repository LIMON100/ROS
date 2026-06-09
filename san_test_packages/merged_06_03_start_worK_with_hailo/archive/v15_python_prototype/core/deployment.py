"""
Deployment mode + swarm robot-ID mapping (SAN v1.3 §11, SOP §1).

Five deployment tiers govern which safety policies, hardware backends, and
operator gates are active at boot:

  • PRODUCTION   — Real swarm in operational territory. Strictest safety,
                   audit, comm timeouts; no stubs.
  • DEMO         — Live robots in a controlled demo (trade show, customer
                   site). Production safety + relaxed mission cadence so
                   the swarm is visually paced.
  • LAB_TEST     — Indoor lab with real robots but reduced kinematics
                   (lower speeds, indoor geofence, RTK-Float allowed since
                   indoor sky is occluded).
  • BENCH        — Single-robot HIL bench, no swarm. Stubs allowed for
                   peer sensors. Used for adapter / driver bring-up.
  • DEVELOPMENT  — Pure software harness (laptop / CI). All hardware stubs
                   allowed. Gated by DEVELOPER_AUTH_TOKEN env var so the
                   mode cannot be entered on a real-robot boot by accident.

Five-tier policy (transition rules):
  production → demo         : allowed (operator authority)
  production → lab_test     : DENIED (live robots must not silently drop
                              into reduced-safety mode without a reboot)
  any        → development  : requires DEVELOPER_AUTH_TOKEN
  development → production  : DENIED (must reboot to clear dev state)

Swarm composition (always 8 robots when full):
  S1 (robot_id=1) — Leader Go2 quadruped (PredictiveLeader, breadcrumbs)
  S2 (robot_id=2) — Hub UGV (SLAM fuse, leadership takeover, comm gw)
  S3..S8         — Follower UGVs

Deputy chain (leadership takeover order) defaults to [2,3,4,5,6,7,8]:
the Hub UGV is first deputy by construction (extra compute, extra comm),
followed by the remaining followers in numeric order. Operators may
override per-mission via config (`swarm.deputy_chain` overlay).
"""
from __future__ import annotations

import enum
import logging
import os
import time
from typing import Iterable, Tuple

log = logging.getLogger("deployment")


# ──────────────────────────────────────────────────────────────────────
# Robot-ID mapping (SAN v1.3 §11, fixed at code level — DO NOT change
# without coordinating mesh routing, video port allocation, and the
# Android operator app's hard-coded role badges).
# ──────────────────────────────────────────────────────────────────────
MAX_ROBOTS: int = 8
LEADER_ROBOT_ID: int = 1
HUB_ROBOT_ID: int = 2
DEFAULT_DEPUTY_CHAIN: Tuple[int, ...] = (2, 3, 4, 5, 6, 7, 8)
FOLLOWER_ROBOT_IDS: Tuple[int, ...] = (3, 4, 5, 6, 7, 8)


class DeploymentMode(enum.Enum):
    """Five-tier deployment policy (SAN-SDD-SWARM-001 v1.3 §11)."""

    PRODUCTION = "production"
    DEMO = "demo"
    LAB_TEST = "lab_test"
    BENCH = "bench"
    DEVELOPMENT = "development"

    @classmethod
    def parse(cls, value: str) -> "DeploymentMode":
        """Parse from yaml/CLI string. Raises ValueError on unknown mode.

        Case-insensitive; whitespace ignored.
        """
        if value is None:
            raise ValueError("deployment_mode must not be None")
        key = str(value).strip().lower()
        for m in cls:
            if m.value == key:
                return m
        valid = ", ".join(m.value for m in cls)
        raise ValueError(
            f"unknown deployment_mode={value!r}; valid: {valid}")

    @property
    def is_live_robot(self) -> bool:
        """True when actual swarm hardware is expected on the bus."""
        return self in (DeploymentMode.PRODUCTION,
                        DeploymentMode.DEMO,
                        DeploymentMode.LAB_TEST)

    @property
    def allows_stubs(self) -> bool:
        """True when sensor / adapter stubs are accepted."""
        return self in (DeploymentMode.BENCH, DeploymentMode.DEVELOPMENT)


# ──────────────────────────────────────────────────────────────────────
# Mode transition policy
# ──────────────────────────────────────────────────────────────────────
# Map of (from_mode, to_mode) → allowed.
# Anything not listed is denied — explicit allowlist over implicit deny.
_ALLOWED_TRANSITIONS = frozenset({
    # Operator may demote production → demo (slow the swarm for visitors).
    (DeploymentMode.PRODUCTION, DeploymentMode.DEMO),
    (DeploymentMode.DEMO, DeploymentMode.PRODUCTION),
    # Any mode → development is conditionally allowed via the auth gate;
    # the check itself is in validate_developer_auth(). We list it here
    # so policy-aware callers don't deny at the wrong layer.
    (DeploymentMode.PRODUCTION, DeploymentMode.DEVELOPMENT),
    (DeploymentMode.DEMO, DeploymentMode.DEVELOPMENT),
    (DeploymentMode.LAB_TEST, DeploymentMode.DEVELOPMENT),
    (DeploymentMode.BENCH, DeploymentMode.DEVELOPMENT),
    # Bench ↔ lab_test transition is allowed (lab bring-up workflow).
    (DeploymentMode.BENCH, DeploymentMode.LAB_TEST),
    (DeploymentMode.LAB_TEST, DeploymentMode.BENCH),
    # Identity transitions are no-ops but should not be rejected.
})


def is_mode_transition_allowed(old: DeploymentMode,
                               new: DeploymentMode) -> bool:
    """Policy gate for runtime deployment-mode changes.

    Identity transition is always allowed (no-op). Anything else must
    appear in _ALLOWED_TRANSITIONS. Notable denials:

      production  → lab_test    : real robots must not silently drop
                                  safety levels mid-mission.
      development → anything    : a dev session has fewer safety checks;
                                  the only sanctioned exit is a reboot
                                  with a clean process tree.
    """
    if old is new:
        return True
    return (old, new) in _ALLOWED_TRANSITIONS


# ──────────────────────────────────────────────────────────────────────
# Developer auth gate (DEVELOPMENT mode only)
# ──────────────────────────────────────────────────────────────────────
DEVELOPER_AUTH_ENV: str = "DEVELOPER_AUTH_TOKEN"


class DeveloperAuthError(RuntimeError):
    """Raised when DEVELOPMENT mode is requested without a valid token."""


def validate_developer_auth(mode: DeploymentMode,
                            env: dict | None = None) -> None:
    """Enforce DEVELOPER_AUTH_TOKEN for DEVELOPMENT mode.

    Non-development modes pass through. For DEVELOPMENT, the env var
    must be set and non-empty; otherwise DeveloperAuthError is raised
    so main() can FATAL-log and exit non-zero.

    `env` defaults to os.environ; tests can pass a dict to avoid
    polluting the real environment.
    """
    if mode is not DeploymentMode.DEVELOPMENT:
        return
    e = os.environ if env is None else env
    token = (e.get(DEVELOPER_AUTH_ENV) or "").strip()
    if not token:
        raise DeveloperAuthError(
            f"deployment_mode=development requires the "
            f"{DEVELOPER_AUTH_ENV} env var to be set to a non-empty "
            f"token. Refusing to boot.")


def emit_development_banner(logger: logging.Logger | None = None) -> None:
    """Print a loud WARN banner so the operator never confuses a dev
    boot with production. Called from main.py after auth passes.
    """
    L = logger or log
    bar = "=" * 64
    L.warning(bar)
    L.warning("  DEPLOYMENT_MODE = DEVELOPMENT  —  HARDWARE STUBS ENABLED")
    L.warning("  Safety gates are RELAXED. Do not connect to live robots.")
    L.warning(bar)


# ──────────────────────────────────────────────────────────────────────
# Deputy chain validation
# ──────────────────────────────────────────────────────────────────────
def build_operation_state(cfg, n_alive_followers: int = 0):
    """Construct a fresh OperationState heartbeat from a Config snapshot.

    Imported lazily to avoid pulling numpy into modules that only want
    the policy constants (e.g. test files that monkey-patch env vars).
    """
    from .messages import Header, OperationState  # local import to avoid cycle

    role = str(cfg.get("system", "robot_role", default="follower")).lower()
    mode = str(cfg.get("system", "deployment_mode", default="production"))
    rid = int(cfg.get("system", "robot_id_int", default=0) or 0)
    chain_raw = cfg.get("swarm", "deputy_chain", default=None)
    chain = normalize_deputy_chain(chain_raw) if chain_raw else DEFAULT_DEPUTY_CHAIN
    return OperationState(
        header=Header(stamp=time.monotonic()),
        deployment_mode=mode,
        robot_id=rid,
        robot_role=role,
        leader_robot_id=LEADER_ROBOT_ID,
        hub_robot_id=HUB_ROBOT_ID,
        deputy_chain=chain,
        n_alive_followers=int(n_alive_followers),
    )


def normalize_deputy_chain(chain: Iterable[int] | None) -> Tuple[int, ...]:
    """Coerce an operator-supplied deputy chain to a clean tuple.

    Rules:
      • Each id must be in 1..MAX_ROBOTS.
      • The leader (id 1) must NOT appear (it can't depute itself).
      • Duplicates are dropped (first occurrence wins).
      • Empty / None falls back to DEFAULT_DEPUTY_CHAIN.

    Raises ValueError on out-of-range or leader-included input so a
    typo in an overlay yaml fails loud at boot rather than producing
    a silently-broken takeover order.
    """
    if not chain:
        return DEFAULT_DEPUTY_CHAIN
    seen: list[int] = []
    for raw in chain:
        rid = int(raw)
        if rid < 1 or rid > MAX_ROBOTS:
            raise ValueError(
                f"deputy_chain id={rid} out of range [1..{MAX_ROBOTS}]")
        if rid == LEADER_ROBOT_ID:
            raise ValueError(
                f"deputy_chain must not include LEADER_ROBOT_ID={LEADER_ROBOT_ID}")
        if rid not in seen:
            seen.append(rid)
    return tuple(seen)
