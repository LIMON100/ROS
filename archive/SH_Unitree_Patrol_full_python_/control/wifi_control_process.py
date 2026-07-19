"""
WifiControlProcess — on-demand WiFi AP bring up / tear down.

Adapted from AIRYS test_server's wifi_ctrl.c. Triggered by BLE WIFI_ON /
WIFI_OFF commands. The AP is intentionally absent at boot — only comes
up when the operator's phone or server requests a video stream.

Why on-demand: this is the requested architecture from §1:
  • BLE: always-on control channel
  • WiFi: on-demand for video streaming (saves battery + RF emission)

Bring-up sequence (5–10 s blocking on real HW):
  1. ip link set <iface> up + ip addr add 192.168.42.1/24
  2. Generate /tmp/hostapd.conf with fresh SSID/PSK
  3. Start hostapd (5 GHz preferred, 2.4 GHz fallback)
  4. Start dnsmasq for DHCP
  5. Verify both pids alive
  6. Emit WIFI_CRED notification with credentials

Tear-down: SIGTERM hostapd + dnsmasq, ip link down. Idempotent.

Stub mode: when not running as root (or `wifi.simulate=true` in config),
all subprocess calls are replaced with sleeps that simulate progress.
This keeps development on a laptop possible.
"""
from __future__ import annotations

import os
import secrets
import subprocess
import threading
import time
from pathlib import Path
from typing import Dict, Optional

from core.base_process import BaseProcess
from core.ipc import consume, publish

from .state_machine import ErrorCode

HOSTAPD_CONF_TPL = """\
interface={iface}
driver=nl80211
ssid={ssid}
hw_mode={hw_mode}
channel={channel}
auth_algs=1
wpa=2
wpa_key_mgmt=WPA-PSK
wpa_pairwise=CCMP
rsn_pairwise=CCMP
wpa_passphrase={psk}
ieee80211n=1
"""

DNSMASQ_CONF_TPL = """\
interface={iface}
bind-interfaces
dhcp-range={dhcp_start},{dhcp_end},255.255.255.0,12h
"""


class WifiControlProcess(BaseProcess):
    """Listen for WIFI_ON/WIFI_OFF requests; manage hostapd + dnsmasq."""

    def __init__(self, queues, shutdown_event, config, **diag):
        super().__init__(
            name="WifiControl", shutdown_event=shutdown_event,
            rate_hz=2.0,
            cpu_affinity=config.get("system", "cpu_affinity", "comm"),
            **diag,
        )
        self.queues = queues
        self.cfg = config
        self._lock: threading.Lock = None
        self._is_up: bool = False
        self._hostapd_proc: Optional[subprocess.Popen] = None
        self._dnsmasq_proc: Optional[subprocess.Popen] = None
        self._creds: Dict[str, str] = {}     # last-published credentials
        self._stats = {"up_count": 0, "down_count": 0, "errors": 0}

    # ───────── Lifecycle ─────────
    def setup(self) -> None:
        self._lock = threading.Lock()
        # Spawn the request consumer
        self.spawn_thread(self._request_loop, name="WifiReq")

    def step(self) -> None:
        # Poll subprocess health if up
        if self._is_up:
            for name, proc in (("hostapd", self._hostapd_proc),
                                ("dnsmasq", self._dnsmasq_proc)):
                if proc is not None and proc.poll() is not None:
                    self.log.error(f"{name} died (rc={proc.returncode})")
                    self._stats["errors"] += 1
                    self._teardown_internal("subprocess_died")
                    publish(self.queues.ble_errors,
                            int(ErrorCode.HOSTAPD_FAIL if name == "hostapd"
                                else ErrorCode.DNSMASQ_FAIL))
                    break

    def teardown(self) -> None:
        if self._is_up:
            self._teardown_internal("process_exit")

    # ───────── Request handler ─────────
    def _request_loop(self):
        while self.is_running():
            req = consume(self.queues.wifi_request, timeout=0.2)
            if req is None:
                continue
            action = req.get("action")
            if action == "up":
                self._bringup_internal()
            elif action == "down":
                self._teardown_internal("requested")
            else:
                self.log.warning(f"unknown wifi request: {req}")

    # ───────── Bring-up ─────────
    def _bringup_internal(self) -> bool:
        with self._lock:
            if self._is_up:
                self.log.info("wifi: already up — re-emitting CRED")
                publish(self.queues.ble_creds, dict(self._creds))
                return True

            iface = self.cfg.get("wifi", "interface", default="wlan0")
            ssid_prefix = self.cfg.get("wifi", "ssid_prefix",
                                        default="patrol-robot")
            ip_addr = self.cfg.get("wifi", "ap_ip", default="192.168.42.1")
            netmask = self.cfg.get("wifi", "ap_netmask", default="24")
            channel = int(self.cfg.get("wifi", "channel", default=44))
            video_port = int(self.cfg.get("wifi", "video_port", default=5000))
            ws_port = int(self.cfg.get("wifi", "ws_port", default=5001))
            transport = self.cfg.get("wifi", "stream_transport",
                                      default="srt_listener")
            simulate = bool(self.cfg.get("wifi", "simulate",
                                          default=(os.geteuid() != 0)))

            # Per-bringup credentials — fresh PSK each time
            ssid = f"{ssid_prefix}-{int(time.time()) & 0xFFFF:04X}"
            psk = secrets.token_urlsafe(12)        # 16-char base64

            phases = [(25, "iface_up"), (50, "hostapd"),
                       (75, "dnsmasq"), (100, "ready")]
            for pct, label in phases:
                try:
                    if simulate:
                        time.sleep(0.3)            # ~1.2s total fake bringup
                    else:
                        self._do_real_step(label, iface, ip_addr, netmask,
                                            ssid, psk, channel)
                    self.log.info(f"wifi bringup {pct}%: {label}")
                    publish(self.queues.wifi_progress,
                            {"pct": pct, "phase": label})
                except subprocess.SubprocessError as e:
                    self.log.error(f"wifi bringup failed at {label}: {e}")
                    self._stats["errors"] += 1
                    self._teardown_internal(f"bringup_failed_{label}")
                    publish(self.queues.ble_errors,
                            int(ErrorCode.WIFI_MODULE_FAIL))
                    return False

            self._is_up = True
            self._stats["up_count"] += 1
            self._creds = {
                "ssid": ssid, "psk": psk,
                "ip": ip_addr,
                "video_port": video_port, "ws_port": ws_port,
                "transport": transport,
            }
            publish(self.queues.ble_creds, dict(self._creds))
            self.log.info(f"wifi up: ssid={ssid} ip={ip_addr}")
            return True

    def _do_real_step(self, label: str, iface: str,
                      ip: str, mask: str,
                      ssid: str, psk: str, channel: int) -> None:
        """One step of the real-hardware bringup. Raises on failure."""
        if label == "iface_up":
            subprocess.run(["ip", "link", "set", iface, "up"],
                            check=True, timeout=5)
            subprocess.run(["ip", "addr", "flush", "dev", iface],
                            check=True, timeout=5)
            subprocess.run(["ip", "addr", "add",
                             f"{ip}/{mask}", "dev", iface],
                            check=True, timeout=5)
            return

        if label == "hostapd":
            conf = Path("/tmp/patrol_hostapd.conf")
            conf.write_text(HOSTAPD_CONF_TPL.format(
                iface=iface, ssid=ssid,
                hw_mode="a" if channel >= 36 else "g",
                channel=channel, psk=psk,
            ))
            self._hostapd_proc = subprocess.Popen(
                ["hostapd", str(conf)],
                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
            )
            time.sleep(1.0)
            if self._hostapd_proc.poll() is not None:
                raise subprocess.SubprocessError(
                    f"hostapd exited with {self._hostapd_proc.returncode}")
            return

        if label == "dnsmasq":
            base = ip.rsplit(".", 1)[0]
            conf = Path("/tmp/patrol_dnsmasq.conf")
            conf.write_text(DNSMASQ_CONF_TPL.format(
                iface=iface,
                dhcp_start=f"{base}.100", dhcp_end=f"{base}.200",
            ))
            self._dnsmasq_proc = subprocess.Popen(
                ["dnsmasq", "-C", str(conf), "-k"],
                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
            )
            time.sleep(0.5)
            if self._dnsmasq_proc.poll() is not None:
                raise subprocess.SubprocessError(
                    f"dnsmasq exited with {self._dnsmasq_proc.returncode}")

    # ───────── Tear-down ─────────
    def _teardown_internal(self, reason: str) -> None:
        with self._lock:
            if not self._is_up and self._hostapd_proc is None \
                    and self._dnsmasq_proc is None:
                return
            self.log.info(f"wifi teardown: {reason}")
            for _name, proc in (("hostapd", self._hostapd_proc),
                                ("dnsmasq", self._dnsmasq_proc)):
                if proc is None:
                    continue
                try:
                    proc.terminate()
                    proc.wait(timeout=2.0)
                except subprocess.TimeoutExpired:
                    proc.kill()
                except OSError:
                    pass
            self._hostapd_proc = None
            self._dnsmasq_proc = None
            iface = self.cfg.get("wifi", "interface", default="wlan0")
            simulate = bool(self.cfg.get("wifi", "simulate",
                                          default=(os.geteuid() != 0)))
            if not simulate:
                try:
                    subprocess.run(["ip", "link", "set", iface, "down"],
                                    check=False, timeout=3)
                except (subprocess.SubprocessError, FileNotFoundError):
                    pass
            self._is_up = False
            self._stats["down_count"] += 1

    # ───────── Test/inspection helpers ─────────
    def is_up(self) -> bool:
        with self._lock:
            return self._is_up

    def credentials(self) -> Dict[str, str]:
        with self._lock:
            return dict(self._creds)
