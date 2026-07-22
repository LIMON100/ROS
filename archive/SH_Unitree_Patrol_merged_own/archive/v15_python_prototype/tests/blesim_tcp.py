"""BleSim — TCP-based BLE GATT simulator for tests.

Stands in for the real bluez stack so tests run without hardware.
Wire format mimics GATT characteristic write / read / notify, encoded
as one JSON object per line:

  → {"op":"write", "uuid": 0xFF01, "data": "<hex>"}
  ← {"ack": true}

  → {"op":"read",  "uuid": 0xFF00}
  ← {"data": "<hex>"}

  ← {"op":"notify", "uuid": 0xFF04, "data": "<hex>"}   (server push)

Six characteristics are pre-allocated (SDD Rev.A.6 §15.2):
  0xFF00 R     Device info (32-byte payload)
  0xFF01 W     WiFi provisioning JSON
  0xFF02 N     Phase notification
  0xFF03 W     cmd_vel fallback
  0xFF04 N     Status notification (5 Hz)
  0xFF05 W/R   PIN authentication challenge-response
"""
from __future__ import annotations

import asyncio
import json
import threading
from typing import Callable, Dict, Optional

# UUID may arrive as int (0xFF01) or hex string ("FF01" / "0xFF01"). Tests
# can pass whichever form; we normalize to int internally.
_UUID_KEYS = (0xFF00, 0xFF01, 0xFF02, 0xFF03, 0xFF04, 0xFF05)


def _coerce_uuid(value) -> int:
    if isinstance(value, int):
        return value
    if isinstance(value, str):
        return int(value, 16) if value.lower().startswith("0x") \
            else int(value, 16)
    raise ValueError(f"unrecognized uuid form: {value!r}")


class BleSimServer:
    """TCP server pretending to be a BLE GATT peer."""

    def __init__(self, host: str = "127.0.0.1", port: int = 12345):
        self.host = host
        self.port = port
        self._server: Optional[asyncio.base_events.Server] = None
        self._writers: list = []
        self._writers_lock = threading.Lock()
        self._loop: Optional[asyncio.AbstractEventLoop] = None
        self._thread: Optional[threading.Thread] = None
        self._chars: Dict[int, bytes] = {u: b"" for u in _UUID_KEYS}
        self._chars[0xFF00] = b"\x00" * 32           # default 32-byte payload
        self._handlers: Dict[int, Callable[[bytes], None]] = {}

    # ─── Public API used by tests ───
    def on_write(self, uuid: int,
                 handler: Callable[[bytes], None]) -> None:
        self._handlers[_coerce_uuid(uuid)] = handler

    def set_char(self, uuid: int, value: bytes) -> None:
        """Pre-populate characteristic value (e.g. challenge bytes)."""
        self._chars[_coerce_uuid(uuid)] = bytes(value)

    def get_char(self, uuid: int) -> bytes:
        return self._chars[_coerce_uuid(uuid)]

    async def notify(self, uuid: int, data: bytes) -> None:
        """Push a notification frame to all connected clients."""
        msg = json.dumps({
            "op": "notify",
            "uuid": _coerce_uuid(uuid),
            "data": data.hex(),
        }).encode() + b"\n"
        with self._writers_lock:
            writers = list(self._writers)
        for w in writers:
            try:
                w.write(msg)
                await w.drain()
            except Exception:        # noqa: BLE001 — best-effort fan-out
                pass

    def start(self, ready_timeout_s: float = 2.0) -> None:
        ready = threading.Event()

        def _loop_thread():
            self._loop = asyncio.new_event_loop()
            asyncio.set_event_loop(self._loop)
            ready.set()
            try:
                self._loop.run_until_complete(self._run())
            except asyncio.CancelledError:
                pass
            except RuntimeError:
                pass
            finally:
                try:
                    self._loop.close()
                except RuntimeError:
                    pass

        self._thread = threading.Thread(
            target=_loop_thread, name="BleSimLoop", daemon=True)
        self._thread.start()
        ready.wait(timeout=ready_timeout_s)
        # Wait briefly for the server to bind.
        for _ in range(20):
            if self._server is not None:
                break
            threading.Event().wait(0.025)

    def stop(self) -> None:
        if self._loop is None:
            return
        try:
            self._loop.call_soon_threadsafe(self._loop.stop)
        except RuntimeError:
            pass
        if self._thread is not None:
            self._thread.join(timeout=2.0)

    # ─── Internal asyncio plumbing ───
    async def _run(self) -> None:
        self._server = await asyncio.start_server(
            self._handle_client, self.host, self.port)
        async with self._server:
            await self._server.serve_forever()

    async def _handle_client(self, reader: asyncio.StreamReader,
                             writer: asyncio.StreamWriter) -> None:
        with self._writers_lock:
            self._writers.append(writer)
        try:
            while True:
                line = await reader.readline()
                if not line:
                    break
                try:
                    msg = json.loads(line.decode())
                except json.JSONDecodeError:
                    continue
                op = msg.get("op")
                try:
                    uuid = _coerce_uuid(msg.get("uuid"))
                except ValueError:
                    continue
                if op == "write":
                    data = bytes.fromhex(msg.get("data", ""))
                    self._chars[uuid] = data
                    handler = self._handlers.get(uuid)
                    if handler is not None:
                        try:
                            handler(data)
                        except Exception:        # noqa: BLE001
                            pass
                    writer.write(b'{"ack":true}\n')
                    await writer.drain()
                elif op == "read":
                    payload = self._chars.get(uuid, b"")
                    writer.write(json.dumps({
                        "data": payload.hex(),
                    }).encode() + b"\n")
                    await writer.drain()
        except (asyncio.IncompleteReadError, ConnectionResetError):
            pass
        finally:
            with self._writers_lock:
                if writer in self._writers:
                    self._writers.remove(writer)
            writer.close()
            try:
                await writer.wait_closed()
            except Exception:        # noqa: BLE001
                pass
