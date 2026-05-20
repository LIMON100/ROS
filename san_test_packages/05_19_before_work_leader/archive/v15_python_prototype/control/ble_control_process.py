"""
BleControlProcess — TCP-based BLE GATT server (development simulator).

Wire-protocol-compatible with AIRYS BleSim (SAN-BLE-WIFI-001). Real BLE
on Linux requires BlueZ + D-Bus + system-bus + agent registration —
complex enough that for development we expose the GATT semantics over a
plain TCP socket. AIRYS-APP / patrol_server connects via TCP and speaks
the same JSON envelopes the real BLE link would carry. When we move to
production BLE (BlueZ-based), the wire format and opcodes don't change —
only the transport layer.

Why this is a BaseProcess (not a thread):
  • BLE control runs always-on, independent of the rest of the patrol
    pipeline. If perception or localization crash, BLE must keep
    answering pings so the operator can reset the robot.
  • BaseProcess gives us crash dumps + metrics + correlation ID for
    the BLE channel separately from the heavy data plane.

State sync: a shared FSM is held in the parent (main.py) and observed
through queue messages on this process. Inbound BLE commands go out
on `ble_command` queue; phase changes come in on `phase_changes`
queue and trigger STATE notifications.
"""
from __future__ import annotations

import hashlib
import hmac
import json
import os
import socket
import threading
import time
from typing import Optional

from core.audit_log import publish_audit
from core.base_process import BaseProcess
from core.ipc import consume, publish

from .state_machine import ErrorCode, Opcode, Phase

# AIRYS BleSim defaults from SAN-BLE-WIFI-001 §4.3
DEFAULT_BIND = "0.0.0.0"
DEFAULT_PORT = 5555


class BleControlProcess(BaseProcess):
    """Always-on TCP server speaking the AIRYS BLE GATT wire protocol.

    Exposes:
      • `ble_command` queue (out): {opcode, raw, ts_mono} per CMD write
      • `ble_settings` queue (out): {bytes, ts_mono} per SETTINGS write
      • `phase_changes` queue (in): notifies clients via STATE messages
      • `ble_creds` queue (in): notifies clients via WIFI_CRED message
      • `ble_errors` queue (in): notifies clients via ERROR message

    Client lifecycle:
      • One concurrent client at a time (matches BLE semantics: a single
        bonded peripheral). Subsequent connects close the older one.
      • Server owns the keep-alive timeout; AIRYS-APP must send PING
        (opcode 0x31) at least every PING_TIMEOUT_S, else we close.
    """

    PING_TIMEOUT_S = 30.0
    # Hard caps so a misbehaving (or hostile) peer can't OOM us by sending
    # a stream without newlines, or one giant SETTINGS hex blob.
    MAX_LINE_BYTES = 64 * 1024
    MAX_SETTINGS_HEX_CHARS = 4096

    def __init__(self, queues, shutdown_event, config, **diag):
        super().__init__(
            name="BleControl", shutdown_event=shutdown_event,
            rate_hz=1.0,
            cpu_affinity=config.get("system", "cpu_affinity", "comm"),
            **diag,
        )
        self.queues = queues
        self.cfg = config
        self._listen_sock: Optional[socket.socket] = None
        self._client_sock: Optional[socket.socket] = None
        self._client_lock: threading.Lock = None
        self._client_addr: tuple = ("", 0)
        self._last_seen_at: float = 0.0
        self._stats = {
            "connects": 0, "disconnects": 0,
            "cmds": 0, "settings": 0,
            "notifies_state": 0, "notifies_creds": 0, "notifies_error": 0,
            "rejects": 0,
            "pin_challenges": 0, "pin_success": 0, "pin_fail": 0,
        }
        # 0xFF05 PIN challenge-response auth. Constructed here (not in
        # setup()) so tests that build a process instance without spawning
        # it can still drive _handle_line() directly.
        self._pin_auth = PinAuthChallenge()

    # ───────── Lifecycle ─────────
    def setup(self) -> None:
        bind = self.cfg.get("ble", "bind_addr", default=DEFAULT_BIND)
        port = int(self.cfg.get("ble", "tcp_port",  default=DEFAULT_PORT))

        self._client_lock = threading.Lock()
        self._listen_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._listen_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self._listen_sock.bind((bind, port))
        self._listen_sock.listen(1)
        self._listen_sock.settimeout(0.5)
        self.log.info(f"BLE GATT (TCP) listening on {bind}:{port}")

        self.spawn_thread(self._accept_loop,    name="BleAccept")
        self.spawn_thread(self._notify_pump,    name="BleNotify")
        self.spawn_thread(self._keepalive_watchdog, name="BleKeepAlive")

    def step(self) -> None:
        # Periodic stats line every ~30 s (rate_hz=1)
        if (self._stats["cmds"] + self._stats["connects"]) % 30 == 0:
            self.log.debug(
                f"ble  conns={self._stats['connects']} "
                f"cmds={self._stats['cmds']} "
                f"notifies={self._stats['notifies_state']}"
            )

    def teardown(self) -> None:
        self._close_client("teardown")
        if self._listen_sock:
            try:
                self._listen_sock.close()
            except OSError:
                pass

    # ───────── Accept loop (single-client BLE semantics) ─────────
    def _accept_loop(self):
        while self.is_running():
            try:
                cs, addr = self._listen_sock.accept()
            except socket.timeout:
                continue
            except OSError:
                break
            self._adopt_client(cs, addr)

    def _adopt_client(self, cs: socket.socket, addr: tuple):
        # If we already had one, kick it — BLE has one bonded peer
        with self._client_lock:
            if self._client_sock is not None:
                self.log.warning(
                    f"BLE: replacing existing client "
                    f"{self._client_addr} with {addr}")
                self._close_client_locked("replaced")
            self._client_sock = cs
            self._client_addr = addr
            self._last_seen_at = time.monotonic()
            self._stats["connects"] += 1
        self.log.info(f"BLE client connected: {addr}")
        # Spawn per-client reader so we can serve notifications concurrently
        threading.Thread(
            target=self._client_reader, args=(cs, addr),
            name=f"BleRead({addr[0]})", daemon=True,
        ).start()

    def _close_client(self, reason: str, owner: Optional[socket.socket] = None):
        """Close the current client.

        If `owner` is given, close only if `self._client_sock is owner`. This
        is used by per-client reader threads to avoid closing a *new* client
        socket that has already replaced theirs (a race we hit in tests when
        a second client connect adoption interleaved with the first reader's
        shutdown handler).
        """
        with self._client_lock:
            if owner is not None and self._client_sock is not owner:
                # Our cs has already been replaced; nothing to do.
                return
            self._close_client_locked(reason)

    def _close_client_locked(self, reason: str):
        if self._client_sock is None:
            return
        try:
            self._client_sock.shutdown(socket.SHUT_RDWR)
        except OSError:
            pass
        try:
            self._client_sock.close()
        except OSError:
            pass
        self.log.info(f"BLE client disconnected ({reason}): {self._client_addr}")
        # PIN auth must not survive a reconnection — clear it before the
        # next client can claim the inherited authenticated state. The
        # PinAuthChallenge.reset() docstring spells this out, but the
        # contract only holds if *something* calls it on disconnect.
        was_authenticated = self._pin_auth.is_authenticated
        self._pin_auth.reset()
        if was_authenticated:
            publish_audit(
                self.queues, category="permission",
                event="pin_auth_reset_on_disconnect",
                actor=f"ble:{self._client_addr[0]}",
                params={"reason": reason})
            publish(self.queues.auth_state, {
                "authenticated": False,
                "ts_mono": time.monotonic(),
                "reason": f"ble_disconnect:{reason}",
            })
        self._client_sock = None
        self._client_addr = ("", 0)
        self._stats["disconnects"] += 1
        # Tell the FSM to drop back to BLE_ADV
        publish(self.queues.ble_command,
                {"opcode": "_internal_disconnect", "reason": reason,
                 "ts_mono": time.monotonic()})

    # ───────── Per-client reader ─────────
    def _client_reader(self, cs: socket.socket, addr: tuple):
        cs.settimeout(1.0)
        buf = bytearray()
        while self.is_running():
            with self._client_lock:
                if self._client_sock is not cs:
                    return                    # superseded by another client
            try:
                chunk = cs.recv(1024)
            except socket.timeout:
                continue
            except OSError:
                self._close_client("recv_error", owner=cs)
                return
            if not chunk:
                self._close_client("eof", owner=cs)
                return
            buf += chunk
            self._last_seen_at = time.monotonic()
            # Line-oriented JSON — split on newlines
            while b"\n" in buf:
                line, _, rest = buf.partition(b"\n")
                buf = bytearray(rest)
                self._handle_line(line.decode("utf-8", errors="replace").strip())
            if len(buf) > self.MAX_LINE_BYTES:
                self._stats["rejects"] += 1
                self.log.warning(
                    f"BLE: oversize line from {addr} ({len(buf)} B), dropping client")
                self._close_client("oversize_line", owner=cs)
                return

    def _handle_line(self, line: str):
        if not line:
            return
        try:
            msg = json.loads(line)
        except json.JSONDecodeError:
            self._stats["rejects"] += 1
            self.log.warning(f"BLE: invalid JSON: {line[:60]!r}")
            return

        op = msg.get("op")
        if op == "connect":
            # Pairing handshake — already accepted at TCP layer.
            # Echo a STATE notification so the app knows we're alive.
            self._send_notify({"notify": "STATE", "val": int(Phase.BLE_CONN)})
        elif op == "disconnect":
            self._close_client("client_request")
        elif op == "read":
            # 0xFF05 PIN challenge read — the only read op exposed today.
            # Real BLE GATT reads route through the characteristic handler;
            # the TCP shim folds them into the JSON wire so the protocol
            # is symmetric with the write op.
            char = msg.get("char")
            if char == "PIN_AUTH":
                self._handle_pin_read()
            else:
                self._stats["rejects"] += 1
                self.log.warning(f"BLE: unknown read char={char!r}")
        elif op == "write":
            char = msg.get("char")
            if char == "CMD":
                self._handle_cmd_write(msg.get("val"))
            elif char == "SETTINGS":
                self._handle_settings_write(msg.get("hex", ""))
            elif char == "PIN_AUTH":
                self._handle_pin_write(msg.get("hex", ""))
            else:
                self._stats["rejects"] += 1
                self.log.warning(f"BLE: unknown char={char!r}")
        else:
            self._stats["rejects"] += 1
            self.log.warning(f"BLE: unknown op={op!r}")

    def _handle_cmd_write(self, val):
        if not isinstance(val, int) or not (0 <= val <= 0xFF):
            self._stats["rejects"] += 1
            return
        try:
            opcode = Opcode(val)
            label = opcode.name
        except ValueError:
            opcode = None
            label = f"0x{val:02X}"
        self._stats["cmds"] += 1
        self.log.info(f"BLE CMD: {label}")
        publish(self.queues.ble_command, {
            "opcode": int(val),
            "label": label,
            "ts_mono": time.monotonic(),
        })

    def _handle_settings_write(self, hex_str: str):
        if not isinstance(hex_str, str) or len(hex_str) > self.MAX_SETTINGS_HEX_CHARS:
            self._stats["rejects"] += 1
            return
        try:
            payload = bytes.fromhex(hex_str)
        except ValueError:
            self._stats["rejects"] += 1
            return
        self._stats["settings"] += 1
        publish(self.queues.ble_settings, {
            "bytes": payload,
            "ts_mono": time.monotonic(),
        })

    # ───────── 0xFF05 PIN auth ─────────
    def _handle_pin_read(self):
        """App reads 0xFF05 — emit a fresh 32-byte challenge.

        Returns hex="" if a previous lockout window is still open. The
        app's expected next step is to HMAC-SHA256(challenge, PIN) and
        write the result back to 0xFF05.
        """
        challenge = self._pin_auth.generate_challenge()
        if not challenge:
            self.log.info("BLE PIN_AUTH read during lockout — empty challenge")
            self._send_notify({"notify": "PIN_CHALLENGE", "hex": ""})
            return
        self._stats["pin_challenges"] += 1
        self._send_notify({
            "notify": "PIN_CHALLENGE",
            "hex": challenge.hex(),
        })

    def _handle_pin_write(self, hex_str: str):
        """App writes its HMAC response to 0xFF05 — verify + announce."""
        if not isinstance(hex_str, str) or len(hex_str) > 128:
            self._stats["rejects"] += 1
            return
        try:
            response = bytes.fromhex(hex_str)
        except ValueError:
            self._stats["rejects"] += 1
            return

        prev_attempts = self._pin_auth._failed_attempts
        ok = self._pin_auth.verify_response(response)
        actor = f"ble:{self._client_addr[0]}"
        if ok:
            self._stats["pin_success"] += 1
            self.log.info("BLE PIN auth succeeded")
            publish_audit(
                self.queues, category="permission", event="pin_auth_success",
                actor=actor)
            publish(self.queues.auth_state, {
                "authenticated": True,
                "ts_mono": time.monotonic(),
                "reason": "pin_verified",
            })
            self._send_notify({"notify": "PIN_RESULT", "ok": True})
            return

        self._stats["pin_fail"] += 1
        # MAX_ATTEMPTS-th failure rolls _failed_attempts back to 0 and
        # arms _lockout_until — detect that transition for a separate
        # audit event so an operator reviewing the chain can tell apart
        # routine mistypes from a lockout-arming attack.
        locked_out = (
            self._pin_auth._failed_attempts == 0
            and prev_attempts == self._pin_auth.MAX_ATTEMPTS - 1
        )
        event = "pin_auth_lockout" if locked_out else "pin_auth_fail"
        self.log.warning(f"BLE PIN auth {event}")
        publish_audit(
            self.queues, category="permission", event=event, actor=actor,
            params={"prev_attempts": prev_attempts})
        self._send_notify({
            "notify": "PIN_RESULT", "ok": False,
            "locked_out": locked_out,
        })

    # ───────── Notify pump ─────────
    def _notify_pump(self):
        """Pull from ble_phase / ble_creds / ble_errors queues and emit
        notifications to the connected client."""
        while self.is_running():
            phase = consume(self.queues.ble_phase, timeout=0.1)
            if phase is not None:
                self._send_notify({"notify": "STATE", "val": int(phase)})
                self._stats["notifies_state"] += 1

            creds = consume(self.queues.ble_creds, timeout=0.0)
            if creds is not None:
                self._send_notify({"notify": "WIFI_CRED", "payload": creds})
                self._stats["notifies_creds"] += 1

            err = consume(self.queues.ble_errors, timeout=0.0)
            if err is not None:
                self._send_notify({"notify": "ERROR", "code": int(err)})
                self._stats["notifies_error"] += 1

    def _send_notify(self, obj: dict) -> bool:
        line = (json.dumps(obj) + "\n").encode("utf-8")
        with self._client_lock:
            if self._client_sock is None:
                return False
            try:
                self._client_sock.sendall(line)
                return True
            except OSError as e:
                self.log.warning(f"BLE notify failed: {e}")
                self._close_client_locked("send_error")
                return False

    # ───────── Keep-alive watchdog ─────────
    def _keepalive_watchdog(self):
        """If no bytes received from the client for PING_TIMEOUT_S,
        treat the link as lost. Mimics the BLE supervision timeout."""
        while self.is_running():
            time.sleep(2.0)
            with self._client_lock:
                has_client = self._client_sock is not None
                last = self._last_seen_at
            if has_client and (time.monotonic() - last) > self.PING_TIMEOUT_S:
                self.log.warning("BLE: keep-alive timeout, closing client")
                self._close_client("keepalive_timeout")
                publish(self.queues.ble_errors, int(ErrorCode.BLE_LINK_LOST))


class PinAuthChallenge:
    """0xFF05 PIN authentication via 32-byte HMAC challenge-response.

    Robot generates a random challenge on read; app responds with
    HMAC-SHA256(challenge, PIN). On success the BLE session is marked
    dev_mode-authenticated. After 3 failed attempts the robot enters a
    30-second lockout (challenge generation returns b"" until elapsed).

    Stateful per-session: caller invokes reset() on BLE disconnect so
    dev_mode does not survive a reconnection.
    """

    PIN_HASH_FILE = "/etc/patrol/pin_hash"     # production override
    DEV_DEFAULT_PIN = "1234"                   # only when file missing
    LOCKOUT_S = 30.0
    MAX_ATTEMPTS = 3

    def __init__(self):
        self._challenge: bytes = b""
        self._authenticated: bool = False
        self._lockout_until: float = 0.0
        self._failed_attempts: int = 0

    def generate_challenge(self) -> bytes:
        """Called via 0xFF05 read. Returns 32 random bytes, or b"" if
        the lockout window is still open."""
        if time.time() < self._lockout_until:
            return b""
        self._challenge = os.urandom(32)
        return self._challenge

    def verify_response(self, response: bytes) -> bool:
        """App's HMAC response to the active challenge.

        Returns True iff the HMAC matches and we're not currently locked
        out. Three consecutive failures arm the lockout window.
        """
        if time.time() < self._lockout_until:
            return False
        if not self._challenge:
            return False
        pin = self._load_pin()
        expected = hmac.new(
            pin.encode(), self._challenge, hashlib.sha256).digest()
        if hmac.compare_digest(expected, response):
            self._authenticated = True
            self._failed_attempts = 0
            return True
        self._failed_attempts += 1
        if self._failed_attempts >= self.MAX_ATTEMPTS:
            self._lockout_until = time.time() + self.LOCKOUT_S
            self._failed_attempts = 0
        return False

    def _load_pin(self) -> str:
        try:
            with open(self.PIN_HASH_FILE) as f:
                return f.read().strip()
        except FileNotFoundError:
            return self.DEV_DEFAULT_PIN

    @property
    def is_authenticated(self) -> bool:
        return self._authenticated

    def reset(self) -> None:
        """Called on BLE disconnect. Clears auth + active challenge.
        Lockout window is preserved deliberately — a misbehaving client
        can't bypass the cooldown by reconnecting."""
        self._authenticated = False
        self._challenge = b""
