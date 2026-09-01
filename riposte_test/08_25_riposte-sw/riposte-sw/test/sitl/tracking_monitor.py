#!/usr/bin/env python3
"""Tracking observer for the guidance SITL test (needs pymavlink).

Watches vehicle telemetry on the GCS port for a window and asserts that the
vehicle is actually tracking: it displaces from its start position and its
horizontal speed stays within (moving, clamped) bounds. The speed ceiling
doubles as an in-flight check of the SM-4 velocity clamp (vmax_h = 5 m/s in
riposte-guidance.ini; guidance itself commands ENGAGE_SPEED = 6 m/s, so the
clamp is what keeps observed speed below the ceiling).

usage: tracking_monitor.py <window_s> <min_displacement_m>
prints TRACKING_OK on success.
"""
import math
import sys
import time

from pymavlink import mavutil

SPEED_CEILING_MPS = 5.5  # vmax_h 5.0 + transient margin — clamp must hold
MOVING_MEAN_MPS = 1.5    # below this the vehicle is not really tracking


def main():
    window_s = float(sys.argv[1]) if len(sys.argv) > 1 else 15.0
    min_disp_m = float(sys.argv[2]) if len(sys.argv) > 2 else 20.0

    m = mavutil.mavlink_connection("udpin:0.0.0.0:14550")
    # wait_heartbeat returns None on timeout (no exception) — continuing would
    # watch telemetry from a link that never came up.
    if m.wait_heartbeat(timeout=30) is None:
        print("no heartbeat from PX4 within 30 s; abort")
        return 1

    start = None
    last = None
    speeds = []
    t_end = time.time() + window_s
    while time.time() < t_end:
        msg = m.recv_match(type="LOCAL_POSITION_NED", blocking=True, timeout=2.0)
        if msg is None:
            continue
        if start is None:
            start = (msg.x, msg.y)
        last = (msg.x, msg.y)
        speeds.append(math.hypot(msg.vx, msg.vy))

    if start is None or not speeds:
        print("no LOCAL_POSITION_NED telemetry")
        return 1

    disp = math.hypot(last[0] - start[0], last[1] - start[1])
    mean_speed = sum(speeds) / len(speeds)
    max_speed = max(speeds)
    print(f"displacement={disp:.1f} m  mean_speed={mean_speed:.2f}  "
          f"max_speed={max_speed:.2f} (n={len(speeds)})")

    if disp < min_disp_m:
        print(f"FAIL: displacement {disp:.1f} < {min_disp_m}")
        return 1
    if mean_speed < MOVING_MEAN_MPS:
        print(f"FAIL: mean speed {mean_speed:.2f} < {MOVING_MEAN_MPS}")
        return 1
    if max_speed > SPEED_CEILING_MPS:
        print(f"FAIL: max speed {max_speed:.2f} > {SPEED_CEILING_MPS} (clamp breach)")
        return 1
    print("TRACKING_OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
