"""
7-Phase connection state machine.

Adapted from AIRYS SAN-BLE-WIFI-001 §4.3.1. The state machine governs the
robot's network bring-up sequence:

    BOOT (0x00)
       │ system init done
       ▼
    BLE_ADV (0x01)              ← waiting for app to connect over BLE
       │ app connects + auth
       ▼
    BLE_CONN (0x02)             ← BLE control channel up; idle
       │ app issues WIFI_ON (CMD 0x10)
       ▼
    WIFI_BRINGUP (0x03)         ← spinning up hostapd + dnsmasq
       │ AP up, IP assigned
       ▼
    WIFI_READY (0x04)           ← waiting for app to join hotspot
       │ app's UDP/SRT pull starts
       ▼
    STREAMING (0x05)            ← GStreamer pipeline running
       │ app issues WIFI_OFF (CMD 0x11) | BLE link drops
       ▼
    TEARDOWN (0x06)             ← cleanup; back to BLE_ADV
       │
       ▼
    [BLE_ADV again]   or   ERROR (0x07)

The state machine is BLE-driven: BLE keeps running through every state
(it's the always-on control channel). WiFi only exists between
WIFI_BRINGUP and TEARDOWN.

Why we model it explicitly: the transitions need careful sequencing —
e.g. starting GStreamer before WiFi is up produces a 30-second timeout;
tearing down WiFi while GStreamer is still pushing produces zombie
encoders. The FSM is also queried by the dashboard to display the robot's
current phase.
"""
from __future__ import annotations

import threading
import time
from dataclasses import dataclass, field
from enum import IntEnum
from typing import Callable, Dict, List


class Phase(IntEnum):
    BOOT          = 0x00
    BLE_ADV       = 0x01
    BLE_CONN      = 0x02
    WIFI_BRINGUP  = 0x03
    WIFI_READY    = 0x04
    STREAMING     = 0x05
    TEARDOWN      = 0x06
    ERROR         = 0x07

    def name_kr(self) -> str:
        return {
            Phase.BOOT:         "부팅",
            Phase.BLE_ADV:      "BLE 광고",
            Phase.BLE_CONN:     "BLE 연결됨",
            Phase.WIFI_BRINGUP: "WiFi 시작중",
            Phase.WIFI_READY:   "WiFi 준비됨",
            Phase.STREAMING:    "스트리밍 중",
            Phase.TEARDOWN:     "해제 중",
            Phase.ERROR:        "오류",
        }[self]


# AIRYS error codes — SAN-BLE-WIFI-001 §4.3.4
class ErrorCode(IntEnum):
    NONE              = 0x00
    WIFI_MODULE_FAIL  = 0x10
    HOSTAPD_FAIL      = 0x11
    DNSMASQ_FAIL      = 0x12
    BAD_INTERFACE     = 0x13
    BLE_LINK_LOST     = 0x20
    AUTH_FAIL         = 0x21
    STREAM_TIMEOUT    = 0x30
    STREAM_FAIL       = 0x31
    SRT_HANDSHAKE     = 0x33
    BATTERY_LOW       = 0x40
    THERMAL_LIMIT     = 0x41
    INTERNAL_ERROR    = 0xFE
    UNKNOWN           = 0xFF


# AIRYS opcodes — SAN-BLE-WIFI-001 §4.3.2
class Opcode(IntEnum):
    WIFI_ON       = 0x10
    WIFI_OFF      = 0x11
    RESET         = 0x20
    REBOOT        = 0x21
    KEEP_ALIVE    = 0x30
    PING          = 0x31
    REC_TOGGLE    = 0x40
    LRF_TRIGGER   = 0x41
    SNAPSHOT      = 0x42


# Allowed transitions table. Anything not listed here is a programming
# error — the FSM logs and refuses the transition (without crashing).
_VALID_TRANSITIONS: Dict[Phase, set] = {
    Phase.BOOT:         {Phase.BLE_ADV, Phase.ERROR},
    Phase.BLE_ADV:      {Phase.BLE_CONN, Phase.ERROR},
    Phase.BLE_CONN:     {Phase.WIFI_BRINGUP, Phase.BLE_ADV, Phase.ERROR},
    Phase.WIFI_BRINGUP: {Phase.WIFI_READY, Phase.ERROR, Phase.TEARDOWN},
    Phase.WIFI_READY:   {Phase.STREAMING, Phase.TEARDOWN, Phase.ERROR},
    Phase.STREAMING:    {Phase.TEARDOWN, Phase.ERROR},
    Phase.TEARDOWN:     {Phase.BLE_ADV, Phase.ERROR},
    Phase.ERROR:        {Phase.BLE_ADV, Phase.TEARDOWN},
}


@dataclass
class TransitionEvent:
    """Recorded for diagnostics + dashboard timeline."""
    from_phase: Phase
    to_phase:   Phase
    at_mono:    float
    reason:     str = ""
    error_code: ErrorCode = ErrorCode.NONE


@dataclass
class FsmStats:
    transitions: int = 0
    rejected:    int = 0
    last_error:  ErrorCode = ErrorCode.NONE
    time_per_phase_s: Dict[Phase, float] = field(default_factory=dict)


# Listener type — fired on every successful transition. Callbacks must be
# fast (run inline under the FSM lock) and non-blocking; long work goes on
# a separate thread.
PhaseListener = Callable[[Phase, Phase, str], None]


class ConnectionFsm:
    """Thread-safe 7-phase FSM.

    All transitions go through `request()`. Listeners are notified
    synchronously while holding the lock — keep them short.
    """

    def __init__(self, log=None):
        self.log = log
        self._lock = threading.RLock()
        self._phase = Phase.BOOT
        self._entered_at = time.monotonic()
        self._history: List[TransitionEvent] = []
        self._listeners: List[PhaseListener] = []
        self._stats = FsmStats()

    # ───────── Listeners ─────────
    def add_listener(self, fn: PhaseListener) -> None:
        with self._lock:
            self._listeners.append(fn)

    # ───────── State queries ─────────
    @property
    def phase(self) -> Phase:
        with self._lock:
            return self._phase

    def time_in_phase(self) -> float:
        with self._lock:
            return time.monotonic() - self._entered_at

    def history(self, limit: int = 20) -> List[TransitionEvent]:
        with self._lock:
            return list(self._history[-limit:])

    def stats(self) -> FsmStats:
        with self._lock:
            # Update current phase elapsed time
            self._stats.time_per_phase_s[self._phase] = (
                self._stats.time_per_phase_s.get(self._phase, 0.0)
                + self.time_in_phase()
            )
            return self._stats

    # ───────── Transitions ─────────
    def request(self, target: Phase, *,
                reason: str = "",
                error_code: ErrorCode = ErrorCode.NONE) -> bool:
        """Attempt a transition. Returns True if accepted."""
        with self._lock:
            current = self._phase
            if target == current:
                return True   # idempotent — caller asked for what we already are
            allowed = _VALID_TRANSITIONS.get(current, set())
            if target not in allowed:
                self._stats.rejected += 1
                if self.log:
                    self.log.warning(
                        f"FSM: rejected transition {current.name} → "
                        f"{target.name} (allowed: {[p.name for p in allowed]})")
                return False

            now = time.monotonic()
            elapsed = now - self._entered_at
            self._stats.time_per_phase_s[current] = (
                self._stats.time_per_phase_s.get(current, 0.0) + elapsed
            )

            ev = TransitionEvent(
                from_phase=current, to_phase=target,
                at_mono=now, reason=reason, error_code=error_code,
            )
            self._history.append(ev)
            if len(self._history) > 200:
                self._history = self._history[-200:]
            self._phase = target
            self._entered_at = now
            self._stats.transitions += 1
            if error_code != ErrorCode.NONE:
                self._stats.last_error = error_code

            if self.log:
                lvl = self.log.error if target == Phase.ERROR else self.log.info
                lvl(f"FSM: {current.name} → {target.name}"
                    + (f"  reason={reason}" if reason else "")
                    + (f"  err={error_code.name}"
                       if error_code != ErrorCode.NONE else ""))

            # Notify listeners under lock — they'd better be quick
            for fn in self._listeners:
                try:
                    fn(current, target, reason)
                except Exception as e:        # pylint: disable=broad-except
                    if self.log:
                        self.log.exception(
                            f"FSM listener raised on {current.name}→"
                            f"{target.name}: {e}")
            return True

    # ───────── Convenience driver helpers ─────────
    def to_error(self, code: ErrorCode, reason: str) -> None:
        """Force-transition to ERROR from any state. Always succeeds — ERROR
        is reachable from every other state."""
        with self._lock:
            if self._phase == Phase.ERROR:
                # Already errored — just record the new code
                self._stats.last_error = code
                return
            # ERROR is allowed from anywhere; make it explicit by
            # appending the target to the allowed set if needed.
        self.request(Phase.ERROR, reason=reason, error_code=code)

    def reset(self) -> None:
        """Clear ERROR / TEARDOWN back to BLE_ADV."""
        with self._lock:
            if self._phase in (Phase.ERROR, Phase.TEARDOWN):
                self._phase = Phase.BLE_ADV
                self._entered_at = time.monotonic()
                if self.log:
                    self.log.info("FSM: reset → BLE_ADV")
