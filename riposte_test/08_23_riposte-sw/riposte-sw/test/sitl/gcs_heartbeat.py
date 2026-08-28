#!/usr/bin/env python3
"""Standalone GCS heartbeat for long SITL runs (needs pymavlink).

PX4 v1.17 treats loss of the ground-station link as a failsafe: it enters Hold,
then RTLs and lands. `arm_takeoff.py` keeps a heartbeat alive only while it is
running, so a test whose observation window outlives takeoff loses the link
mid-flight and the vehicle flies home on its own — which looks exactly like the
software under test giving up.

That is not an artefact to paper over: the real aircraft has a permanent GCS
link over SiK, so a scenario without one is the unrealistic case. This process
stands in for it, and is meant to run for the whole scenario.

  gcs_heartbeat.py [udp_url]      default udpin:0.0.0.0:14550

Runs until killed. Prints one line when it starts so a harness can wait for it.
"""
import sys
import time

from pymavlink import mavutil


def main():
    url = sys.argv[1] if len(sys.argv) > 1 else "udpin:0.0.0.0:14550"
    m = mavutil.mavlink_connection(url)
    # Wait for the autopilot so the first heartbeat has somewhere to go; the
    # link is UDP, so sending into the void before PX4 appears is silently lost.
    if m.wait_heartbeat(timeout=60) is None:
        print("gcs_heartbeat: no heartbeat from PX4 within 60 s; abort")
        return 1
    print(f"gcs_heartbeat: linked to sys {m.target_system}, sending 1 Hz")
    sys.stdout.flush()
    while True:
        m.mav.heartbeat_send(mavutil.mavlink.MAV_TYPE_GCS,
                             mavutil.mavlink.MAV_AUTOPILOT_INVALID, 0, 0, 0)
        time.sleep(1.0)


if __name__ == "__main__":
    sys.exit(main())
