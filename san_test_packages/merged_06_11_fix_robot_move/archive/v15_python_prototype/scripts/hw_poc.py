#!/usr/bin/env python3
"""
Hardware PoC checklist runner — runs once on the RK3588 to validate that
each external dependency is reachable before launching `python3 main.py`.

Six checks (run in order; fail-fast):
  1. Operating system / required kernel modules
  2. Go2 robot reachable via Ethernet
  3. RTK GNSS device present + emitting NMEA
  4. LTE modem device present + AT-responsive
  5. WiFi6 mesh / gateway reachable
  6. Camera devices (IMX678 + thermal) enumerated by V4L2

Each check returns:
   ✓ pass / ✗ fail / ⚠ degraded (works but limited)

Run as:
    sudo python3 scripts/hw_poc.py            # full checklist
    sudo python3 scripts/hw_poc.py --only rtk # single check
    sudo python3 scripts/hw_poc.py --json     # CI-friendly output
"""
from __future__ import annotations

import argparse
import json
import os
import socket
import subprocess
import sys
import time
from dataclasses import asdict, dataclass, field
from typing import List


@dataclass
class CheckResult:
    name: str
    status: str            # "pass" | "fail" | "degraded"
    detail: str = ""
    duration_s: float = 0.0
    metrics: dict = field(default_factory=dict)


# ─────────── Individual checks ───────────
def check_kernel(_) -> CheckResult:
    """Verify Ubuntu 22.04 + RK3588 platform tag and required modules loaded."""
    t0 = time.monotonic()
    detail = []
    # Platform identifier
    try:
        with open("/proc/device-tree/compatible") as f:
            compat = f.read()
    except FileNotFoundError:
        compat = ""
    if "rockchip,rk3588" not in compat:
        return CheckResult("kernel", "degraded",
                           detail=f"not RK3588 (compat={compat[:80]!r})",
                           duration_s=time.monotonic() - t0)
    # Required modules
    mods_needed = ("rknpu", "v4l2_core")
    try:
        mods = subprocess.check_output(["lsmod"], text=True)
    except FileNotFoundError:
        mods = ""
    missing = [m for m in mods_needed if m not in mods]
    return CheckResult(
        "kernel",
        "pass" if not missing else "degraded",
        detail=", ".join(detail) if detail else
               (f"missing modules: {missing}" if missing else "ok"),
        duration_s=time.monotonic() - t0,
        metrics={"compat": compat.strip("\x00")[:80],
                 "missing_mods": missing},
    )


def check_go2(args) -> CheckResult:
    """Ping Go2 over Ethernet at 192.168.123.161 (default Unitree IP)."""
    t0 = time.monotonic()
    ip = args.go2_ip
    try:
        rc = subprocess.call(["ping", "-c", "1", "-W", "2", ip],
                             stdout=subprocess.DEVNULL,
                             stderr=subprocess.DEVNULL)
    except FileNotFoundError:
        return CheckResult("go2", "fail",
                           detail="`ping` not installed",
                           duration_s=time.monotonic() - t0)
    if rc != 0:
        return CheckResult("go2", "fail",
                           detail=f"{ip} unreachable (Ethernet down? robot off?)",
                           duration_s=time.monotonic() - t0)
    # Probe DDS port (Cyclone DDS default 7400)
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.settimeout(1.0)
            s.connect((ip, 22))         # SSH on Unitree EDU images
            ssh_open = True
    except OSError:
        ssh_open = False
    return CheckResult("go2", "pass",
                       detail=f"{ip} reachable (ssh: {ssh_open})",
                       duration_s=time.monotonic() - t0,
                       metrics={"ssh_open": ssh_open})


def check_rtk(args) -> CheckResult:
    """Open the GNSS UART, verify NMEA sentences arrive within 3 s."""
    t0 = time.monotonic()
    device = args.rtk_device
    if not os.path.exists(device):
        return CheckResult("rtk", "fail",
                           detail=f"device {device} not present",
                           duration_s=time.monotonic() - t0)
    try:
        import serial
    except ImportError:
        return CheckResult("rtk", "fail",
                           detail="pyserial not installed",
                           duration_s=time.monotonic() - t0)
    n_lines = 0
    n_gga_with_fix = 0
    fix_qualities = []
    try:
        with serial.Serial(device, args.rtk_baud, timeout=1.0) as s:
            t_end = time.monotonic() + 3.0
            while time.monotonic() < t_end:
                line = s.readline().decode("ascii", errors="ignore")
                if not line:
                    continue
                n_lines += 1
                if "GGA" in line:
                    parts = line.split(",")
                    if len(parts) > 6 and parts[6].isdigit():
                        q = int(parts[6])
                        fix_qualities.append(q)
                        if q in (1, 2, 4, 5):
                            n_gga_with_fix += 1
    except serial.SerialException as e:
        return CheckResult("rtk", "fail",
                           detail=f"serial: {e}",
                           duration_s=time.monotonic() - t0)
    if n_lines == 0:
        return CheckResult("rtk", "fail",
                           detail="no NMEA in 3 s — wrong baud or device?",
                           duration_s=time.monotonic() - t0)
    best = max(fix_qualities, default=0)
    return CheckResult(
        "rtk",
        "pass" if best >= 4 else ("degraded" if best >= 1 else "fail"),
        detail=f"{n_lines} NMEA sentences, best fix quality {best} "
               f"(4=Fixed, 5=Float)",
        duration_s=time.monotonic() - t0,
        metrics={"n_lines": n_lines, "n_gga_fix": n_gga_with_fix,
                 "best_quality": best},
    )


def check_lte(args) -> CheckResult:
    """Send AT and AT+CREG? — modem must respond OK + registration code."""
    t0 = time.monotonic()
    device = args.lte_device
    if not os.path.exists(device):
        return CheckResult("lte", "fail",
                           detail=f"device {device} not present",
                           duration_s=time.monotonic() - t0)
    try:
        import serial
    except ImportError:
        return CheckResult("lte", "fail", detail="pyserial not installed")
    try:
        with serial.Serial(device, 115200, timeout=1.0) as s:
            for cmd in (b"AT\r\n", b"AT+CREG?\r\n"):
                s.reset_input_buffer()
                s.write(cmd)
                time.sleep(0.3)
            data = s.read(2048).decode("ascii", errors="ignore")
    except serial.SerialException as e:
        return CheckResult("lte", "fail", detail=f"serial: {e}")
    if "OK" not in data:
        return CheckResult("lte", "fail",
                           detail=f"no OK response (got {data[:80]!r})",
                           duration_s=time.monotonic() - t0)
    registered = "+CREG: 0,1" in data or "+CREG: 0,5" in data \
                 or "+CREG: 2,1" in data or "+CREG: 2,5" in data
    return CheckResult(
        "lte",
        "pass" if registered else "degraded",
        detail="registered" if registered else "responsive but not registered",
        duration_s=time.monotonic() - t0,
        metrics={"registered": registered, "raw": data[:200]},
    )


def check_wifi(args) -> CheckResult:
    """TCP connect to a known reachable host on port 443."""
    t0 = time.monotonic()
    host = args.wifi_host
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.settimeout(2.0)
            s.connect((host, 443))
        # measure latency
        latency_ms = (time.monotonic() - t0) * 1000
        return CheckResult("wifi6", "pass",
                           detail=f"{host}:443 reachable, {latency_ms:.0f} ms",
                           duration_s=time.monotonic() - t0,
                           metrics={"latency_ms": latency_ms})
    except (OSError, socket.timeout) as e:
        return CheckResult("wifi6", "fail",
                           detail=f"{host}:443 unreachable: {e}",
                           duration_s=time.monotonic() - t0)


def check_cameras(args) -> CheckResult:
    """Enumerate /dev/video* and verify both expected devices exist."""
    t0 = time.monotonic()
    videos = sorted(p for p in os.listdir("/dev")
                    if p.startswith("video") and p[5:].isdigit())
    missing = []
    if args.imx678_device.split("/")[-1] not in videos:
        missing.append(args.imx678_device)
    if args.thermal_device.split("/")[-1] not in videos:
        missing.append(args.thermal_device)
    return CheckResult(
        "cameras",
        "pass" if not missing else "degraded",
        detail=(f"video devices: {videos}; missing: {missing}"
                if missing else f"video devices: {videos}"),
        duration_s=time.monotonic() - t0,
        metrics={"videos": videos, "missing": missing},
    )


CHECKS = {
    "kernel":  check_kernel,
    "go2":     check_go2,
    "rtk":     check_rtk,
    "lte":     check_lte,
    "wifi6":   check_wifi,
    "cameras": check_cameras,
}


# ─────────── CLI ───────────
def _color(code, text, force):
    if not force and not sys.stdout.isatty():
        return text
    return f"\033[{code}m{text}\033[0m"


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--only", choices=list(CHECKS.keys()),
                   help="Run a single check")
    p.add_argument("--json", action="store_true",
                   help="Output machine-readable JSON")
    p.add_argument("--color", action="store_true",
                   help="Force color output even when piped")
    # Per-check parameters
    p.add_argument("--go2-ip", default="192.168.123.161")
    p.add_argument("--rtk-device", default="/dev/ttyACM0")
    p.add_argument("--rtk-baud", type=int, default=115200)
    p.add_argument("--lte-device", default="/dev/ttyUSB2")
    p.add_argument("--wifi-host", default="1.1.1.1")
    p.add_argument("--imx678-device", default="/dev/video0")
    p.add_argument("--thermal-device", default="/dev/video2")
    args = p.parse_args()

    selected = ([args.only] if args.only else list(CHECKS.keys()))
    results: List[CheckResult] = []
    for name in selected:
        try:
            results.append(CHECKS[name](args))
        except Exception as e:
            results.append(CheckResult(name, "fail", detail=f"exception: {e}"))

    if args.json:
        print(json.dumps([asdict(r) for r in results], indent=2))
        ok = all(r.status != "fail" for r in results)
        return 0 if ok else 1

    # Pretty terminal output
    print()
    print(f"{'CHECK':<10}  {'STATUS':<10}  {'DURATION':<10}  DETAIL")
    print("─" * 80)
    for r in results:
        status_color = {"pass": "32", "degraded": "33", "fail": "31"}[r.status]
        marker = {"pass": "✓", "degraded": "⚠", "fail": "✗"}[r.status]
        status_str = f"{marker} {r.status}"
        print(f"{r.name:<10}  "
              f"{_color(status_color, status_str, args.color):<19}  "
              f"{r.duration_s:>5.1f} s    {r.detail[:60]}")
    print()

    n_fail = sum(1 for r in results if r.status == "fail")
    n_degr = sum(1 for r in results if r.status == "degraded")
    if n_fail:
        print(_color("31", f"FAIL: {n_fail} check(s) failed.", args.color))
    elif n_degr:
        print(_color("33", f"PASS WITH DEGRADATIONS: {n_degr} need attention.",
                     args.color))
    else:
        print(_color("32", "ALL CHECKS PASSED — safe to launch main.py",
                     args.color))
    return 0 if n_fail == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
