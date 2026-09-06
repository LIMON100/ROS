#!/usr/bin/env python3
"""GCS stand-in for the Riposte SITL smoke test (needs pymavlink).

Connects to PX4 SITL as a ground station (udp 14550), waits for EKF/home,
arms (with retry) and commands a takeoff, then waits until the vehicle holds
altitude. riposte-obc runs independently on the API port (14540).
"""
import os
import sys
import threading
import time

from pymavlink import mavutil


def start_gcs_heartbeat(m):
    """Send a 1 Hz GCS heartbeat so PX4's 'No connection to the GCS' arming
    check passes. pymavlink does not send heartbeats by itself, and PX4 v1.17
    (gz) enforces the check strictly — arming was TEMPORARILY_REJECTED until
    the stand-in behaved like a real GCS (found in the Gazebo closure run)."""

    def pump():
        while True:
            m.mav.heartbeat_send(mavutil.mavlink.MAV_TYPE_GCS,
                                 mavutil.mavlink.MAV_AUTOPILOT_INVALID, 0, 0, 0)
            time.sleep(1.0)

    threading.Thread(target=pump, daemon=True).start()


def wait_ack(m, cmd, timeout=8.0):
    t0 = time.time()
    while time.time() - t0 < timeout:
        msg = m.recv_match(type="COMMAND_ACK", blocking=True, timeout=1.0)
        if msg and msg.command == cmd:
            return msg.result
    return None


def main():
    m = mavutil.mavlink_connection("udpin:0.0.0.0:14550")
    print("waiting for heartbeat...")
    # wait_heartbeat returns None on timeout (no exception) — continuing would
    # send every command to target_system 0.
    if m.wait_heartbeat(timeout=60) is None:
        print("no heartbeat from PX4 within 60 s; abort")
        return 1
    print(f"heartbeat from sys {m.target_system}")
    start_gcs_heartbeat(m)

    # Wait for a valid global position (EKF ready) before arming.
    print("waiting for EKF position...")
    home_amsl_m = None
    deadline = time.time() + 120
    while time.time() < deadline:
        msg = m.recv_match(type="GLOBAL_POSITION_INT", blocking=True, timeout=2.0)
        if msg is not None:
            home_amsl_m = msg.alt / 1000.0
            break
    if home_amsl_m is None:
        print("no position; abort")
        return 1
    print(f"home AMSL {home_amsl_m:.1f} m")
    time.sleep(5)  # allow home position latch

    print("arming...")
    armed = False
    for attempt in range(6):
        m.mav.command_long_send(m.target_system, m.target_component,
                                mavutil.mavlink.MAV_CMD_COMPONENT_ARM_DISARM,
                                0, 1, 0, 0, 0, 0, 0, 0)
        r = wait_ack(m, mavutil.mavlink.MAV_CMD_COMPONENT_ARM_DISARM)
        print(f"arm ack (try {attempt + 1}): {r}")
        if r == mavutil.mavlink.MAV_RESULT_ACCEPTED:
            armed = True
            break
        time.sleep(3)
    if not armed:
        return 1

    # PX4 NAV_TAKEOFF param7 is AMSL, not relative — command home + TAKEOFF_ALT_M
    # (default 15). Scenarios that manoeuvre vertically raise it: the guidance
    # tracking chases a target whose elevation oscillates, and from 15 m the
    # descent half of that reached SM-3's altitude floor and ended the
    # control session mid-scenario (the floor was doing its job — the scenario simply
    # had no vertical room).
    # Retry transient rejections: one TEMPORARILY_REJECTED would otherwise
    # waste the full 90 s altitude loop below.
    takeoff_alt_m = float(os.environ.get("TAKEOFF_ALT_M", "15"))
    print(f"takeoff to home+{takeoff_alt_m:.0f} m...")
    rejected = (mavutil.mavlink.MAV_RESULT_TEMPORARILY_REJECTED,
                mavutil.mavlink.MAV_RESULT_DENIED)
    for attempt in range(5):
        m.mav.command_long_send(m.target_system, m.target_component,
                                mavutil.mavlink.MAV_CMD_NAV_TAKEOFF,
                                0, 0, 0, 0, float("nan"),
                                float("nan"), float("nan"), home_amsl_m + takeoff_alt_m)
        r = wait_ack(m, mavutil.mavlink.MAV_CMD_NAV_TAKEOFF)
        print(f"takeoff ack (try {attempt + 1}): {r}")
        if r not in rejected:
            break  # accepted / in progress / ack lost: verify via altitude below
        time.sleep(2)
    else:
        print("takeoff rejected 5 times (TEMPORARILY_REJECTED/DENIED); giving up")
        return 1

    # Wait until the vehicle is actually AT altitude (>= 80 % of the commanded
    # climb, floored at 8 m), not merely off the ground. Returning at a fixed
    # 8 m meant the caller engaged while the climb was still in progress: the
    # OBC then owns the setpoints, the climb stops there, and a scenario that
    # manoeuvres downward (the guidance tracking) descends into SM-3's altitude
    # floor with no room to spare. Measured at 1.5 m before this fix.
    want_mm = int(max(8.0, takeoff_alt_m * 0.8) * 1000)
    deadline = time.time() + 90
    last_print = 0.0
    while time.time() < deadline:
        msg = m.recv_match(type="GLOBAL_POSITION_INT", blocking=True, timeout=2.0)
        if msg is None:
            continue
        if time.time() - last_print > 5:
            last_print = time.time()
            print(f"rel_alt={msg.relative_alt / 1000.0:.1f} m")
        if msg.relative_alt >= want_mm:
            print(f"at altitude: rel_alt={msg.relative_alt / 1000.0:.1f} m "
                  f"(target {takeoff_alt_m:.0f} m)")
            print("READY_FOR_ENGAGE")
            return 0
    print("takeoff did not reach altitude")
    return 1


if __name__ == "__main__":
    sys.exit(main())
