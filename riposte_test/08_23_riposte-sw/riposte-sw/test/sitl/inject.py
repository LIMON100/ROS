#!/usr/bin/env python3
"""Fault-injection commands for the SM-x SITL tests (needs pymavlink).

Acts as the GCS (udp 14550) and injects operator-side disturbances:
  hold             switch flight mode to Hold (SM-2 external mode override)
  disarm_force     force-disarm in flight (SM-6) — the vehicle WILL fall (SITL only)
  battery_low      drop the simulated battery floor (SIM_BAT_MIN_PCT=10) and wait
                   until remaining <= 18 % (SM-9 in-flight trigger). 10 % keeps a
                   margin above PX4's own BAT_CRIT_THR so only SM-9 acts.
  battery_restore  raise the floor back (SIM_BAT_MIN_PCT=50) and wait until
                   remaining >= 45 % so later scenarios see a healthy battery
"""
import sys
import time

from pymavlink import mavutil

PX4_MAIN_MODE_AUTO = 4
PX4_SUB_MODE_AUTO_LOITER = 3
FORCE_DISARM_MAGIC = 21196  # MAV_CMD_COMPONENT_ARM_DISARM param2
BAT_SIM_PARAM = b"SIM_BAT_MIN_PCT"


def recv_autopilot_heartbeat(m, timeout_s):
    """Next HEARTBEAT *from the autopilot*, or None on timeout.

    The SITL network carries heartbeats from more than the vehicle: MAVSDK
    inside riposte-obc announces itself as a companion computer, and that
    heartbeat has autopilot=INVALID with base_mode=0 — i.e. "not armed". An
    unfiltered recv_match(type="HEARTBEAT") therefore matched the COMPANION and
    reported a disarm that never happened: SM-6 "confirmed" its injection while
    PX4 stayed armed (its log shows no disarm at all), then failed waiting for a
    violation the OBC was correct not to raise. Filter by source system and a
    real autopilot type.
    """
    deadline = time.time() + timeout_s
    while True:
        remaining = deadline - time.time()
        if remaining <= 0:
            return None
        hb = m.recv_match(type="HEARTBEAT", blocking=True, timeout=remaining)
        if hb is None:
            return None
        if hb.get_srcSystem() != m.target_system:
            continue
        if hb.autopilot == mavutil.mavlink.MAV_AUTOPILOT_INVALID:
            continue  # companion/GCS heartbeat, not the vehicle
        if hb.type == mavutil.mavlink.MAV_TYPE_GCS:
            continue
        return hb


def set_param_and_wait_battery(m, floor_pct, done, timeout_s):
    """PARAM_SET the sim battery floor, then wait until SYS_STATUS satisfies
    done(remaining_percent). Confirmation comes from telemetry, not the PARAM
    ack — the same reliability reasoning as the hold/disarm verbs below."""
    for _ in range(4):
        m.mav.param_set_send(m.target_system, m.target_component, BAT_SIM_PARAM,
                             float(floor_pct), mavutil.mavlink.MAV_PARAM_TYPE_REAL32)
        deadline = time.time() + timeout_s / 4.0
        while time.time() < deadline:
            msg = m.recv_match(type="SYS_STATUS", blocking=True, timeout=1.0)
            if msg is None or msg.battery_remaining < 0:  # -1 = unknown
                continue
            if done(msg.battery_remaining):
                print(f"battery: remaining={msg.battery_remaining}% "
                      f"(floor {floor_pct}%) confirmed")
                return 0
    print(f"battery: condition not reached (floor {floor_pct}%)")
    return 1


def main():
    verbs = ("hold", "disarm_force", "battery_low", "battery_restore")
    if len(sys.argv) < 2 or sys.argv[1] not in verbs:
        print(__doc__)
        return 2
    verb = sys.argv[1]

    m = mavutil.mavlink_connection("udpin:0.0.0.0:14550")
    # wait_heartbeat returns None on timeout (no exception) — continuing would
    # send every command to target_system 0.
    if m.wait_heartbeat(timeout=30) is None:
        print("no heartbeat from PX4 within 30 s; abort")
        return 1

    if verb == "battery_low":
        # <=18 keeps the deny-reengage check running above PX4's 15 % low-battery
        # warning band while being safely below the SM-9 land threshold (20 %).
        return set_param_and_wait_battery(m, 10.0, lambda r: r <= 18, 90.0)
    if verb == "battery_restore":
        return set_param_and_wait_battery(m, 50.0, lambda r: r >= 45, 30.0)

    if verb == "hold":
        # Success criterion is the observed flight mode in HEARTBEAT (acks for
        # DO_SET_MODE proved unreliable over the SITL GCS link), with retries.
        for _ in range(4):
            m.mav.command_long_send(
                m.target_system, m.target_component,
                mavutil.mavlink.MAV_CMD_DO_SET_MODE, 0,
                mavutil.mavlink.MAV_MODE_FLAG_CUSTOM_MODE_ENABLED,
                PX4_MAIN_MODE_AUTO, PX4_SUB_MODE_AUTO_LOITER, 0, 0, 0, 0)
            deadline = time.time() + 3.0
            while time.time() < deadline:
                hb = recv_autopilot_heartbeat(m, 1.0)
                if hb is None:
                    continue
                main_mode = (hb.custom_mode >> 16) & 0xFF
                sub_mode = (hb.custom_mode >> 24) & 0xFF
                if main_mode == PX4_MAIN_MODE_AUTO and \
                        sub_mode == PX4_SUB_MODE_AUTO_LOITER:
                    print("hold: mode confirmed by heartbeat")
                    return 0
        print("hold: mode change not confirmed")
        return 1

    # disarm_force — success criterion is the armed flag in HEARTBEAT (acks
    # are unreliable over the SITL GCS UDP link), with retries.
    for _ in range(4):
        m.mav.command_long_send(
            m.target_system, m.target_component,
            mavutil.mavlink.MAV_CMD_COMPONENT_ARM_DISARM, 0,
            0, FORCE_DISARM_MAGIC, 0, 0, 0, 0, 0)
        deadline = time.time() + 3.0
        while time.time() < deadline:
            hb = recv_autopilot_heartbeat(m, 1.0)
            if hb is None:
                continue
            if not hb.base_mode & mavutil.mavlink.MAV_MODE_FLAG_SAFETY_ARMED:
                print("disarm_force: disarm confirmed by heartbeat")
                return 0
    print("disarm_force: not confirmed")
    return 1


if __name__ == "__main__":
    sys.exit(main())
