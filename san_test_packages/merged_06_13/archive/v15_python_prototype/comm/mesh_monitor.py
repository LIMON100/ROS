"""OpenWrt Wi-Fi 6 mesh + WAN-failover monitor (PHASE 6 §6.3).

The Hub UGV SBC#2 polls the upstream OpenWrt router on a ~5 s cadence
and publishes a MeshStatus rollup on `queues.hub_mesh_status`. The
router commands (`batctl o`, `mwan3 status`) are invoked over SSH;
this module keeps the *parsing* pure so unit tests can validate the
batctl/mwan3 line shapes without an actual network.

Wire-up (separate follow-up PR):
    MeshMonitor(cmd_runner=run_ssh_batctl_mwan3)
    every 5 s → poll() → publish(queues.hub_mesh_status, status)

`cmd_runner` is injected so the test suite passes captured router
output as plain strings.
"""
from __future__ import annotations

import re
import time
from typing import Callable, Dict, Optional

from core.messages import MeshStatus

# Default poll period — fast enough that an operator sees a WAN
# failover within ~5 s, slow enough that the SSH overhead doesn't
# saturate the router's mgmt CPU.
DEFAULT_POLL_PERIOD_S = 5.0


# A CommandRunner takes a logical command name + arg string and returns
# stdout as a string (raises on non-zero exit, transports SSH errors
# upstream). The default real implementation lives in a follow-up PR.
CommandRunner = Callable[[str], str]


# ─── parsers ───────────────────────────────────────────────────────────

def parse_batctl_originators(output: str) -> int:
    """Count distinct mesh peers from `batctl o -H` output.

    The `-H` flag drops the header; each remaining non-empty line is
    one originator. Sample line:
        de:ad:be:ef:00:01    1.234s   (200) ae:bb:cc:dd:ee:ff [   mesh0]
    """
    peers = 0
    for line in output.splitlines():
        s = line.strip()
        if not s or s.startswith("[") or s.lower().startswith("originator"):
            # Skip header rows + bracketed footnotes.
            continue
        peers += 1
    return peers


_MWAN3_INTERFACE_RE = re.compile(
    r"^\s*interface\s+(\S+)\s+is\s+(online|offline)\b",
    re.IGNORECASE,
)


def parse_mwan3_status(output: str) -> Dict[str, str]:
    """Extract interface → 'online'|'offline' from `mwan3 status`.

    Sample line:
        interface wan is online and tracking is active
        interface wan_lte is offline and tracking is active
    Interfaces not matching the pattern are ignored (mwan3 prints a
    lot of bookkeeping lines this parser deliberately doesn't touch).
    """
    state: Dict[str, str] = {}
    for line in output.splitlines():
        m = _MWAN3_INTERFACE_RE.search(line)
        if m:
            iface, status = m.group(1), m.group(2).lower()
            state[iface] = status
    return state


def _is_online(state: Dict[str, str], iface: str) -> bool:
    return state.get(iface, "").lower() == "online"


# ─── monitor ───────────────────────────────────────────────────────────

class MeshMonitor:
    """Periodic OpenWrt poller that emits MeshStatus snapshots.

    Pure compute beyond the `cmd_runner` callback — tests inject a
    fake runner that returns canned batctl/mwan3 strings. Threading:
    not internally synchronised; caller wraps if multiple threads
    invoke `poll()`.
    """

    BATCTL_CMD = "batctl o -H"
    MWAN3_CMD  = "mwan3 status"

    PRIMARY_WAN_IFACE  = "wan"
    FALLBACK_WAN_IFACE = "wan_lte"

    def __init__(
        self,
        cmd_runner: CommandRunner,
        mesh_id: str = "san-mesh-001",
        poll_period_s: float = DEFAULT_POLL_PERIOD_S,
    ):
        if poll_period_s <= 0:
            raise ValueError(
                f"poll_period_s must be positive: {poll_period_s}")
        if not mesh_id:
            raise ValueError("mesh_id must be non-empty")
        self._cmd = cmd_runner
        self._mesh_id = str(mesh_id)
        self._poll_period_s = float(poll_period_s)
        self._stats = {"polls": 0, "parse_failures": 0}

    @property
    def poll_period_s(self) -> float:
        return self._poll_period_s

    @property
    def mesh_id(self) -> str:
        return self._mesh_id

    @property
    def stats(self) -> dict:
        return dict(self._stats)

    def poll(self, now_ms: Optional[int] = None) -> Optional[MeshStatus]:
        """Run both commands, build a MeshStatus. Returns None on parse
        failure (logged via stats so the consumer can spot it).
        """
        self._stats["polls"] += 1
        try:
            batctl_out = self._cmd(self.BATCTL_CMD)
            mwan3_out  = self._cmd(self.MWAN3_CMD)
        except Exception:
            self._stats["parse_failures"] += 1
            return None
        try:
            peer_count = parse_batctl_originators(batctl_out)
            wan_state  = parse_mwan3_status(mwan3_out)
        except Exception:
            self._stats["parse_failures"] += 1
            return None

        primary_alive = _is_online(wan_state, self.PRIMARY_WAN_IFACE)
        fallback_alive = _is_online(wan_state, self.FALLBACK_WAN_IFACE)
        failover_active = fallback_alive and not primary_alive

        msg = MeshStatus(
            peer_count=peer_count,
            wan_primary_alive=primary_alive,
            wan_failover_active=failover_active,
            mesh_id=self._mesh_id,
            timestamp_ms=now_ms if now_ms is not None else _now_ms(),
        )
        msg.validate()
        return msg


def _now_ms() -> int:
    return int(time.time() * 1000)


__all__ = (
    "DEFAULT_POLL_PERIOD_S",
    "MeshMonitor",
    "parse_batctl_originators",
    "parse_mwan3_status",
)
