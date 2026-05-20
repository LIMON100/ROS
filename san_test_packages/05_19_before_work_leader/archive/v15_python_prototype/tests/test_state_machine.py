"""
Tests for the 7-Phase connection FSM.

Coverage:
  • happy path BOOT → ... → STREAMING → TEARDOWN → BLE_ADV
  • illegal transitions are rejected without state change
  • ERROR is reachable from every state
  • listeners fire on every accepted transition
  • thread safety (concurrent transition requests)
"""
from __future__ import annotations

import threading
import time
from unittest.mock import MagicMock

from control.state_machine import (
    _VALID_TRANSITIONS,
    ConnectionFsm,
    ErrorCode,
    Opcode,
    Phase,
)


def test_initial_phase_is_boot():
    fsm = ConnectionFsm()
    assert fsm.phase == Phase.BOOT


def test_happy_path_full_cycle():
    fsm = ConnectionFsm()
    sequence = [
        Phase.BLE_ADV, Phase.BLE_CONN, Phase.WIFI_BRINGUP,
        Phase.WIFI_READY, Phase.STREAMING, Phase.TEARDOWN,
        Phase.BLE_ADV,
    ]
    for tgt in sequence:
        assert fsm.request(tgt), f"transition to {tgt.name} rejected"
        assert fsm.phase == tgt
    assert fsm.stats().transitions == len(sequence)
    assert fsm.stats().rejected == 0


def test_illegal_transition_rejected():
    fsm = ConnectionFsm()
    fsm.request(Phase.BLE_ADV)
    # Skipping BLE_CONN/WIFI_BRINGUP straight to STREAMING is illegal
    assert not fsm.request(Phase.STREAMING)
    assert fsm.phase == Phase.BLE_ADV
    assert fsm.stats().rejected == 1


def test_idempotent_self_transition():
    fsm = ConnectionFsm()
    fsm.request(Phase.BLE_ADV)
    # Already in BLE_ADV — no-op, not a rejection
    assert fsm.request(Phase.BLE_ADV)
    assert fsm.stats().rejected == 0


def test_error_reachable_from_every_state():
    """ERROR is the safety landing pad — must be reachable from anywhere."""
    for source in (Phase.BOOT, Phase.BLE_ADV, Phase.BLE_CONN,
                    Phase.WIFI_BRINGUP, Phase.WIFI_READY, Phase.STREAMING,
                    Phase.TEARDOWN):
        assert Phase.ERROR in _VALID_TRANSITIONS[source], \
            f"ERROR not reachable from {source.name}"


def test_to_error_records_code_and_reason():
    fsm = ConnectionFsm()
    fsm.request(Phase.BLE_ADV)
    fsm.request(Phase.BLE_CONN)
    fsm.to_error(ErrorCode.WIFI_MODULE_FAIL, reason="hostapd died")
    assert fsm.phase == Phase.ERROR
    last = fsm.history(1)[-1]
    assert last.error_code == ErrorCode.WIFI_MODULE_FAIL
    assert "hostapd" in last.reason


def test_reset_returns_from_error_to_ble_adv():
    fsm = ConnectionFsm()
    fsm.to_error(ErrorCode.STREAM_FAIL, reason="x")
    assert fsm.phase == Phase.ERROR
    fsm.reset()
    assert fsm.phase == Phase.BLE_ADV


def test_listener_fires_on_every_accepted_transition():
    fsm = ConnectionFsm()
    seen = []
    fsm.add_listener(lambda f, t, r: seen.append((f, t, r)))
    fsm.request(Phase.BLE_ADV, reason="boot done")
    fsm.request(Phase.BLE_CONN, reason="app paired")
    assert len(seen) == 2
    assert seen[0] == (Phase.BOOT, Phase.BLE_ADV, "boot done")
    assert seen[1] == (Phase.BLE_ADV, Phase.BLE_CONN, "app paired")


def test_listener_does_not_fire_on_rejection():
    fsm = ConnectionFsm()
    fsm.request(Phase.BLE_ADV)
    seen = []
    fsm.add_listener(lambda f, t, r: seen.append(t))
    assert not fsm.request(Phase.STREAMING)   # illegal
    assert seen == []


def test_listener_exception_does_not_break_fsm():
    fsm = ConnectionFsm(log=MagicMock())
    fsm.add_listener(lambda f, t, r: 1 / 0)        # raises ZeroDivisionError
    fsm.add_listener(lambda f, t, r: None)         # should still fire
    second = []
    fsm.add_listener(lambda f, t, r: second.append(t))
    assert fsm.request(Phase.BLE_ADV)
    # Even though listener #1 raised, #3 should still have run
    assert second == [Phase.BLE_ADV]


def test_history_capped_at_200():
    fsm = ConnectionFsm()
    # Bounce between BLE_ADV and BLE_CONN to generate transitions cheaply
    fsm.request(Phase.BLE_ADV)
    for _ in range(150):
        fsm.request(Phase.BLE_CONN)
        fsm.request(Phase.BLE_ADV)
    # Total transitions = 1 + 300 = 301; history should be capped
    assert len(fsm.history(1000)) <= 200


def test_concurrent_transitions_are_serialized():
    """Two threads requesting the same legal transition must not corrupt
    the FSM state (one wins, the other is idempotent)."""
    fsm = ConnectionFsm()
    fsm.request(Phase.BLE_ADV)

    def driver():
        for _ in range(50):
            fsm.request(Phase.BLE_CONN)
            fsm.request(Phase.BLE_ADV)

    ts = [threading.Thread(target=driver) for _ in range(4)]
    for t in ts:
        t.start()
    for t in ts:
        t.join()
    # Final state should be one of the two we toggled
    assert fsm.phase in (Phase.BLE_ADV, Phase.BLE_CONN)
    # No stale ghost transitions to weird states
    valid_targets = {Phase.BLE_ADV, Phase.BLE_CONN, Phase.BOOT}
    for ev in fsm.history(1000):
        assert ev.from_phase in valid_targets
        assert ev.to_phase in valid_targets


def test_time_in_phase_increases_monotonically():
    fsm = ConnectionFsm()
    fsm.request(Phase.BLE_ADV)
    t1 = fsm.time_in_phase()
    time.sleep(0.05)
    t2 = fsm.time_in_phase()
    assert t2 > t1


def test_opcodes_match_airys_spec():
    """Verify the opcode table matches SAN-BLE-WIFI-001 §4.3.2."""
    assert int(Opcode.WIFI_ON)     == 0x10
    assert int(Opcode.WIFI_OFF)    == 0x11
    assert int(Opcode.RESET)       == 0x20
    assert int(Opcode.PING)        == 0x31
    assert int(Opcode.SNAPSHOT)    == 0x42


def test_error_codes_match_airys_spec():
    assert int(ErrorCode.WIFI_MODULE_FAIL) == 0x10
    assert int(ErrorCode.HOSTAPD_FAIL)     == 0x11
    assert int(ErrorCode.BLE_LINK_LOST)    == 0x20
    assert int(ErrorCode.STREAM_FAIL)      == 0x31


def test_phase_codes_match_airys_spec():
    assert int(Phase.BOOT)         == 0x00
    assert int(Phase.BLE_ADV)      == 0x01
    assert int(Phase.STREAMING)    == 0x05
    assert int(Phase.ERROR)        == 0x07
