"""
NTRIP client — pulls RTCM3 corrections from a caster and forwards to the
GNSS receiver's serial port.

Protocol
--------
NTRIP is HTTP-1.1-shaped:

  GET /<mountpoint> HTTP/1.1
  User-Agent: NTRIP <client>
  Authorization: Basic <base64(user:pass)>
  Ntrip-Version: Ntrip/2.0
  ...

  HTTP/1.1 200 OK
  ...
  <RTCM3 byte stream — preamble 0xD3, 10-bit length, payload, 3-byte CRC>

For VRS (Virtual Reference Station) mountpoints the caster *requires* the
client's approximate position via NMEA $GPGGA. Without it the caster won't
generate corrections. We forward GGAs from the local receiver every ~10 s.

Threading
---------
This is implemented as a **thread** (not a process) because it shares the
GNSS serial port with the RTK adapter — passing fds across processes is
painful, and the workload is purely I/O bound. The RtkGnssAdapter owns
the serial; this thread:
  • opens its own TCP socket to the caster
  • reads RTCM bytes from socket → writes to RTK serial via injected sink
  • reads GGAs from a tap callback → forwards to caster every 10 s

Robustness
----------
  • Automatic reconnect with exponential backoff (1, 2, 4, 8, 16 s; cap 60)
  • Survives caster timeouts, partial frames, transient network loss
  • RTCM3 frame validity checked (preamble + length + CRC-24Q)
"""
from __future__ import annotations

import base64
import logging
import socket
import threading
import time
from dataclasses import dataclass
from typing import Callable, Optional

log = logging.getLogger(__name__)


# ──────────────────────────────────────────────────────────────────
# RTCM3 framing helpers
# ──────────────────────────────────────────────────────────────────
RTCM3_PREAMBLE = 0xD3

# CRC-24Q polynomial (Qualcomm) used by RTCM3 — table-driven for speed
def _build_crc24q_table():
    poly = 0x1864CFB
    tab = []
    for byte in range(256):
        crc = byte << 16
        for _ in range(8):
            crc <<= 1
            if crc & 0x1000000:
                crc ^= poly
        tab.append(crc & 0xFFFFFF)
    return tab


_CRC24Q = _build_crc24q_table()


def crc24q(data: bytes) -> int:
    crc = 0
    for b in data:
        crc = ((crc << 8) ^ _CRC24Q[((crc >> 16) ^ b) & 0xFF]) & 0xFFFFFF
    return crc


def parse_rtcm3_frame(buf: bytes) -> Optional[tuple[int, bytes, bytes]]:
    """Try to parse one RTCM3 frame from the start of `buf`.

    Returns (frame_length, frame_bytes, leftover) on success, None if more
    data is needed. Discards garbage up to next preamble.

    A frame is: 0xD3 | 6-bit reserved + 10-bit length | <length bytes payload> | 3-byte CRC.
    """
    if len(buf) < 3:
        return None
    if buf[0] != RTCM3_PREAMBLE:
        # Resync: scan for next preamble byte
        idx = buf.find(bytes([RTCM3_PREAMBLE]))
        if idx < 0:
            return 0, b"", b""        # discard whole buffer
        return 0, b"", buf[idx:]      # drop garbage prefix
    payload_len = ((buf[1] & 0x03) << 8) | buf[2]
    total = 3 + payload_len + 3        # header + payload + CRC
    if len(buf) < total:
        return None                    # need more
    frame = bytes(buf[:total])
    expected_crc = (frame[-3] << 16) | (frame[-2] << 8) | frame[-1]
    if crc24q(frame[:-3]) != expected_crc:
        # Bad frame — drop preamble byte and resync
        return 0, b"", buf[1:]
    return total, frame, buf[total:]


# ──────────────────────────────────────────────────────────────────
# NTRIP client thread
# ──────────────────────────────────────────────────────────────────
@dataclass
class NtripConfig:
    host: str
    port: int = 2101
    mountpoint: str = ""
    username: str = ""
    password: str = ""
    user_agent: str = "PatrolNTRIP/1.0"
    # Reconnect/backoff
    initial_backoff_s: float = 1.0
    max_backoff_s: float = 60.0
    socket_timeout_s: float = 30.0
    # GGA forwarding (VRS mountpoints require this)
    gga_period_s: float = 10.0
    # Frame size cap — anything > 10 kB is malformed
    max_frame_bytes: int = 10240


class NtripClient(threading.Thread):
    """Connect-forever NTRIP client.

    Args:
      cfg:        connection params
      rtcm_sink:  callable(rtcm_frame_bytes) — forward to GNSS serial
      gga_source: callable() → str | None    — supply latest GGA for VRS
                  (return None to skip a GGA cycle)
      stop_event: threading.Event — request graceful exit
    """

    def __init__(self, cfg: NtripConfig,
                 rtcm_sink: Callable[[bytes], None],
                 gga_source: Optional[Callable[[], Optional[str]]] = None,
                 stop_event: Optional[threading.Event] = None):
        super().__init__(name="NtripClient", daemon=True)
        self.cfg = cfg
        self._rtcm_sink = rtcm_sink
        self._gga_source = gga_source or (lambda: None)
        # NB: must NOT use the name `_stop` — that shadows Thread._stop()
        # and breaks Thread.join().
        self._stop_event = stop_event or threading.Event()
        self._stats = {"frames": 0, "bytes": 0, "reconnects": 0,
                       "bad_crc": 0, "last_frame_t": 0.0}

    @property
    def stats(self) -> dict:
        return dict(self._stats)

    def request_stop(self):
        self._stop_event.set()

    def run(self) -> None:
        backoff = self.cfg.initial_backoff_s
        while not self._stop_event.is_set():
            try:
                self._session()
                # graceful caster close — reset backoff
                backoff = self.cfg.initial_backoff_s
            except Exception as e:
                log.warning(f"NTRIP session ended: {e!r}; reconnecting in {backoff:.0f}s")
                self._stats["reconnects"] += 1
                if self._stop_event.wait(backoff):
                    return
                backoff = min(backoff * 2, self.cfg.max_backoff_s)

    # ────────── one NTRIP session ──────────
    def _session(self) -> None:
        c = self.cfg
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
            sock.settimeout(c.socket_timeout_s)
            sock.connect((c.host, c.port))
            sock.sendall(self._build_request())

            # Read and validate the HTTP-style response header
            header, leftover = self._read_response_header(sock)
            self._validate_response(header)

            # Stream loop: alternate read RTCM, send GGA periodically
            buf = bytearray(leftover)
            last_gga_t = 0.0
            sock.settimeout(2.0)             # short to enable GGA send
            while not self._stop_event.is_set():
                # Send periodic GGA if available
                now = time.monotonic()
                if now - last_gga_t > c.gga_period_s:
                    g = self._gga_source()
                    if g:
                        try:
                            sock.sendall(g.encode("ascii"))
                        except OSError as e:
                            raise RuntimeError(f"GGA send failed: {e}") from e
                    last_gga_t = now

                # Read more RTCM bytes (timeout is fine — keeps GGA loop alive)
                try:
                    chunk = sock.recv(4096)
                except socket.timeout:
                    continue
                if not chunk:
                    raise RuntimeError("caster closed connection")
                buf += chunk
                self._drain_rtcm(buf)

    # ────────── HTTP-style request/response helpers ──────────
    def _build_request(self) -> bytes:
        c = self.cfg
        auth = base64.b64encode(f"{c.username}:{c.password}".encode()).decode()
        lines = [
            f"GET /{c.mountpoint} HTTP/1.1",
            f"Host: {c.host}:{c.port}",
            "Ntrip-Version: Ntrip/2.0",
            f"User-Agent: NTRIP {c.user_agent}",
            f"Authorization: Basic {auth}",
            "Connection: close",
            "",
            "",
        ]
        return ("\r\n".join(lines)).encode("ascii")

    def _read_response_header(self, sock: socket.socket) -> tuple[bytes, bytes]:
        """Read until \\r\\n\\r\\n; return (header_bytes, leftover_after_header)."""
        buf = bytearray()
        while b"\r\n\r\n" not in buf:
            chunk = sock.recv(1024)
            if not chunk:
                raise RuntimeError("caster closed before header")
            buf += chunk
            if len(buf) > 8192:
                raise RuntimeError("header too large")
        idx = buf.index(b"\r\n\r\n") + 4
        return bytes(buf[:idx]), bytes(buf[idx:])

    def _validate_response(self, header: bytes) -> None:
        first = header.split(b"\r\n", 1)[0].decode("ascii", "ignore")
        # NTRIP 1.0 returns "ICY 200 OK", 2.0 returns "HTTP/1.1 200 OK"
        if "200" not in first:
            raise RuntimeError(f"NTRIP rejected: {first!r}")

    # ────────── RTCM frame extraction ──────────
    def _drain_rtcm(self, buf: bytearray) -> None:
        c = self.cfg
        while True:
            res = parse_rtcm3_frame(bytes(buf))
            if res is None:
                return                       # need more bytes
            consumed, frame, leftover = res
            # Trim consumed/garbage prefix
            del buf[:len(buf) - len(leftover)]
            if not frame:                    # garbage was discarded
                if consumed == 0 and not leftover:
                    return
                if consumed == 0 and len(leftover) > 0 and leftover[0] != RTCM3_PREAMBLE:
                    self._stats["bad_crc"] += 1
                continue
            if len(frame) > c.max_frame_bytes:
                self._stats["bad_crc"] += 1
                continue
            try:
                self._rtcm_sink(frame)
            except Exception as e:
                log.error(f"RTCM sink error: {e}")
            self._stats["frames"] += 1
            self._stats["bytes"] += len(frame)
            self._stats["last_frame_t"] = time.monotonic()
