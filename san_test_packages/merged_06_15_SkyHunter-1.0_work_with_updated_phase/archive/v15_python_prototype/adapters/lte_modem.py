"""
LTE Modem adapter.

Hardware: Quectel EM06 / EC25, SIMCom SIM7600, or Telit LM960 — 4G/5G modems
connected to RK3588 via mini-PCIe or USB-CDC. Exposes an AT-command control
port (typ. /dev/ttyUSB2) and a PPP/QMI data port (/dev/ttyUSB3 or wwan0).

This adapter:
  • Polls modem status via AT commands at low rate (~0.5 Hz) — registration
    state, signal strength (RSRP/RSRQ/SINR), operator, PDP context, IP.
  • Publishes LteStatus on `queues.lte_status`.

It does NOT manage the data plane (PPP/QMI bring-up) — that's done once at
boot by ModemManager/QMIcli on the OS, leaving wwan0 up. We only observe.

CommProcess uses LteStatus to decide when to fail over uploads from
WiFi6 LAN → LTE. The decision logic lives in CommProcess (this file is
just the sensor).

Falls back to STUB mode if the modem AT port is absent (CI / dev box) —
emits plausible "registered with weak signal" status at 0.5 Hz.
"""
from __future__ import annotations

import logging
import re
import time
from typing import Optional

from core.base_process import BaseProcess
from core.ipc import publish
from core.messages import LTE_REGISTERED_HOME, Header, LteStatus

log = logging.getLogger(__name__)


# ────────── AT response parsers ──────────
def parse_creg(line: str) -> Optional[int]:
    """+CREG: <n>,<stat>[,<lac>,<ci>] — return registration stat (0..5)."""
    m = re.search(r"\+CREG:\s*\d+\s*,\s*(\d+)", line)
    return int(m.group(1)) if m else None


def parse_cops(line: str) -> Optional[str]:
    """+COPS: <mode>,<format>,<oper>[,<AcT>] — return operator name."""
    m = re.search(r'\+COPS:\s*\d+,\d+,"([^"]+)"', line)
    return m.group(1) if m else None


def parse_cesq(line: str):
    """+CESQ: <rxlev>,<ber>,<rscp>,<ecno>,<rsrq>,<rsrp>

    rsrp index 0..97 → dBm = -140 + index (97 = invalid)
    rsrq index 0..34 → dB  = -20 + index/2 (255 = invalid)
    Returns (rsrp_dbm, rsrq_db) or None.
    """
    m = re.search(r"\+CESQ:\s*\d+,\d+,\d+,\d+,(\d+),(\d+)", line)
    if not m:
        return None
    rsrq_idx = int(m.group(1))
    rsrp_idx = int(m.group(2))
    rsrp = -140.0 + rsrp_idx if rsrp_idx != 255 else -140.0
    rsrq = -20.0 + rsrq_idx / 2.0 if rsrq_idx != 255 else -20.0
    return rsrp, rsrq


def parse_qcsq(line: str):
    """Quectel proprietary: +QCSQ: "LTE",<rssi>,<rsrp>,<sinr>,<rsrq>"""
    m = re.search(r'\+QCSQ:\s*"[^"]+",\s*-?\d+,\s*(-?\d+),\s*(-?\d+),\s*(-?\d+)',
                  line)
    if not m:
        return None
    rsrp = float(m.group(1))
    sinr = float(m.group(2))
    rsrq = float(m.group(3))
    return rsrp, sinr, rsrq


def parse_cgpaddr(line: str) -> Optional[str]:
    """+CGPADDR: <cid>,<ip>"""
    m = re.search(r'\+CGPADDR:\s*\d+,\s*"?([\d\.]+)"?', line)
    return m.group(1) if m else None


# ────────── Adapter process ──────────
class LteModemAdapter(BaseProcess):
    """Polls AT control port; publishes LteStatus at ~0.5 Hz."""

    def __init__(self, queues, shutdown_event, config, **diag):
        super().__init__(
            name="LteModemAdapter",
            shutdown_event=shutdown_event,
            rate_hz=config.get("lte", "poll_hz", default=0.5),
            cpu_affinity=config.get("system", "cpu_affinity", "lte") or [],
            **diag,
        )
        self.queues = queues
        self.cfg = config
        self._serial = None
        self._stub = False
        self._seq = 0
        # Last known fields (kept across polls — not all AT responses come every cycle)
        self._state = LteStatus(header=Header.now(frame_id="lte"))

    def setup(self) -> None:
        device = self.cfg.get("lte", "at_device", default="/dev/ttyUSB2")
        baud   = self.cfg.get("lte", "baud", default=115200)
        try:
            import serial
            self._serial = serial.Serial(device, baud, timeout=0.3)
            log.info(f"LTE AT port open: {device}")
            # Verbose responses so we can parse them
            self._at("ATE0")     # echo off
            self._at("AT+CMEE=2")  # verbose error codes
        except Exception as e:
            log.warning(f"LTE modem unavailable ({e}) → STUB mode")
            self._stub = True

    def step(self) -> None:
        if self._stub:
            self._publish_stub()
            return
        try:
            # Registration
            for line in self._at("AT+CREG?"):
                stat = parse_creg(line)
                if stat is not None:
                    self._state.registered = stat
            # Operator
            for line in self._at("AT+COPS?"):
                op = parse_cops(line)
                if op:
                    self._state.operator = op
            # Signal — try Quectel proprietary first (more info), then 3GPP CESQ
            got_signal = False
            for line in self._at("AT+QCSQ"):
                r = parse_qcsq(line)
                if r:
                    self._state.rsrp_dbm, self._state.sinr_db, self._state.rsrq_db = r
                    self._state.rat = "LTE"
                    got_signal = True
                    break
            if not got_signal:
                for line in self._at("AT+CESQ"):
                    r = parse_cesq(line)
                    if r:
                        self._state.rsrp_dbm, self._state.rsrq_db = r
            # IP
            self._state.pdp_active = False
            for line in self._at("AT+CGPADDR=1"):
                ip = parse_cgpaddr(line)
                if ip and ip != "0.0.0.0":
                    self._state.ip_address = ip
                    self._state.pdp_active = True
        except Exception as e:
            log.error(f"LTE poll error: {e}; switching to STUB")
            self._stub = True
            return
        self._publish_current()

    def teardown(self) -> None:
        if self._serial is not None:
            try:
                self._serial.close()
            except Exception:
                pass

    # ─ helpers ─
    def _at(self, cmd: str, timeout_s: float = 0.5) -> list:
        """Send AT command, collect response lines until OK/ERROR or timeout."""
        self._serial.reset_input_buffer()
        self._serial.write((cmd + "\r\n").encode("ascii"))
        deadline = time.monotonic() + timeout_s
        out = []
        buf = b""
        while time.monotonic() < deadline:
            chunk = self._serial.read(256)
            if not chunk:
                continue
            buf += chunk
            while b"\r\n" in buf:
                line, buf = buf.split(b"\r\n", 1)
                line_s = line.decode("ascii", errors="ignore").strip()
                if not line_s:
                    continue
                out.append(line_s)
                if line_s in ("OK", "ERROR") or line_s.startswith("+CME ERROR"):
                    return out
        return out

    def _publish_current(self) -> None:
        self._state.header = Header.now(frame_id="lte", seq=self._seq)
        self._seq += 1
        publish(self.queues.lte_status, self._state)

    def _publish_stub(self) -> None:
        # Plausible "registered home, marginal signal" — exercises consumers
        s = LteStatus(
            header=Header.now(frame_id="lte", seq=self._seq),
            registered=LTE_REGISTERED_HOME, operator="STUB-LTE",
            rat="LTE", rsrp_dbm=-95.0, rsrq_db=-12.0, sinr_db=5.0,
            band=7, cell_id=0,
            pdp_active=True, ip_address="10.64.0.42", apn="lte.stub",
        )
        self._seq += 1
        publish(self.queues.lte_status, s)
