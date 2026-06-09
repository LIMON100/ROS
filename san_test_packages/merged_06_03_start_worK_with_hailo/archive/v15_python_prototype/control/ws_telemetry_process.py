"""
WsTelemetryProcess — C3 channel: WebSocket telemetry + JSON-RPC.

Per AIRYS SAN-BLE-WIFI-001 §C3:
  • Port 5001, ws:// (no TLS — phone is on the robot's own AP, no MITM
    threat in normal operation; production AIRYS uses wss with self-
    signed for compliance, applied at deployment via reverse-proxy)
  • 30 Hz telemetry push (server → client): pose, battery, FSM phase,
    GPS quality, mission state
  • JSON-RPC inbound (client → server): set_zoom, request_snapshot,
    set_recording, etc.

Lifecycle: brought up by the orchestrator when WiFi enters READY (just
before STREAMING). Tears down on TEARDOWN. WebSocket server runs in an
asyncio loop on a dedicated thread; the BaseProcess shell handles the
crash-dump + correlation-id boilerplate.

Concurrency: one client expected (the operator's app). If two connect,
both get the same telemetry stream — server-side fan-out is cheap.
"""
from __future__ import annotations

import asyncio
import json
import threading
import time
from typing import Dict, Optional, Set

from core.base_process import BaseProcess
from core.ipc import consume, publish

from .video_request import parse_video_request

# NMEA fix_quality int → operator-facing string (consumed by the
# Android app's RTK indicator).
_RTK_QUALITY_STR_BY_INT = {
    0: "GPS",     # invalid / no fix
    1: "GPS",     # standalone GPS fix
    2: "DGPS",
    3: "PPS",
    4: "FIX",     # RTK fixed
    5: "FLOAT",   # RTK float
    6: "DR",      # dead reckoning
    7: "MANUAL",
    8: "SIM",
}


def _rtk_quality_int_to_str(q: object) -> str:
    """Map an NMEA fix_quality code to the operator-facing string.

    Accepts non-int input (e.g. None, str) and falls back to GPS so a
    misshapen heartbeat never breaks the operator UI.
    """
    try:
        return _RTK_QUALITY_STR_BY_INT.get(int(q), "GPS")
    except (TypeError, ValueError):
        return "GPS"


class WsTelemetryProcess(BaseProcess):
    """WebSocket server pushing telemetry at TELEMETRY_HZ.

    Telemetry sources (drained from existing queues):
      • pose        — from Localization
      • robot_status — from Unitree adapter (battery, locomotion mode)
      • ble_phase   — from Orchestrator (current FSM phase)
      • mission_state — from Mission process

    Inbound (JSON-RPC):
      • set_recording {"on": true|false}   — toggles video recording
      • request_snapshot {}                 — single high-res capture
      • get_status {}                       — pull-mode status

    Inbound commands are forwarded to the orchestrator via `app_rpc` queue
    for processing.
    """

    TELEMETRY_HZ = 30.0
    DEFAULT_PORT = 5001

    def __init__(self, queues, shutdown_event, config, **diag):
        super().__init__(
            name="WsTelemetry", shutdown_event=shutdown_event,
            rate_hz=1.0,             # step() prints periodic stats only
            cpu_affinity=config.get("system", "cpu_affinity", "comm"),
            **diag,
        )
        self.queues = queues
        self.cfg = config
        self._port: int = self.DEFAULT_PORT
        self._loop: Optional[asyncio.AbstractEventLoop] = None
        self._loop_thread: Optional[threading.Thread] = None
        self._server_task = None
        self._clients: Set = set()
        self._clients_lock: Optional[asyncio.Lock] = None
        # Latest telemetry snapshot — updated by queue consumers, read
        # by the broadcast pump.
        self._snapshot_lock: threading.Lock = None
        self._snapshot: Dict = {
            # Legacy single-robot fields (preserved for backward compat
            # with existing operator clients that read `data.phase` etc.).
            "phase": 0,
            "pose": None,
            "battery_soc": 1.0,
            "locomotion_mode": "stand",
            "mission": None,
            "ts_mono": 0.0,
            # P1-11 (S3): swarm-aware additions used to build params
            # in the JSON-RPC notification envelope.
            "tier": "T0",
            "rtk_quality_str": "GPS",
            "leader_id": None,
            "speed_mps": 0.0,
            "swarm_health": {
                "in_rollback": False,
                "struggling_ratio": 0.0,
                "anomaly_count": 0,
            },
        }
        self._stats = {
            "clients_connected": 0,
            "clients_total": 0,
            "broadcasts": 0,
            "rpc_received": 0,
            "rpc_errors": 0,
            "anomaly_fanout": 0,
            "heartbeat_fanout": 0,
        }
        # One-shot event fan-out queue: (type, payload) tuples consumed
        # by the broadcast pump and sent as a separate WS message.
        self._event_lock: threading.Lock = None
        self._event_buf: list = []

    # ───────── Lifecycle ─────────
    def setup(self) -> None:
        self._port = int(self.cfg.get("ws", "port",
                                       default=self.DEFAULT_PORT))
        bind = self.cfg.get("ws", "bind", default="0.0.0.0")
        self._snapshot_lock = threading.Lock()
        self._event_lock = threading.Lock()
        # _event_buf may already exist when __init__ ran in the parent
        # process; reinit defensively for fixtures that bypass __init__.
        if not hasattr(self, "_event_buf") or self._event_buf is None:
            self._event_buf = []

        # Spawn telemetry-source consumers on this process's main thread
        self.spawn_thread(self._pose_consumer,        name="WsPose")
        self.spawn_thread(self._status_consumer,      name="WsStatus")
        self.spawn_thread(self._phase_consumer,       name="WsPhase")
        self.spawn_thread(self._mission_consumer,     name="WsMission")
        # Anomaly + heartbeat fan-out (Patrol-Server-deferred mode)
        if hasattr(self.queues, "ws_anomaly"):
            self.spawn_thread(self._anomaly_consumer, name="WsAnomaly")
        if hasattr(self.queues, "ws_heartbeat"):
            self.spawn_thread(self._heartbeat_consumer, name="WsHeartbeat")
        # P1-11: per-robot tier + swarm_health rollups for the new
        # robots[] / swarm_health params block.
        if hasattr(self.queues, "sw_tier"):
            self.spawn_thread(self._tier_consumer, name="WsTier")
        if hasattr(self.queues, "swarm_health"):
            self.spawn_thread(self._swarm_health_consumer,
                              name="WsSwarmHealth")

        # Spin up the asyncio loop on its own thread
        ready = threading.Event()
        self._loop_thread = threading.Thread(
            target=self._run_loop, args=(bind, self._port, ready),
            name="WsLoop", daemon=True,
        )
        self._loop_thread.start()
        ready.wait(timeout=3.0)
        self.log.info(f"WS telemetry listening on ws://{bind}:{self._port}")

    def step(self) -> None:
        # Periodic stats line ~ every 30 s (rate_hz=1)
        if self._stats["broadcasts"] % 900 == 1:
            self.log.info(
                f"ws  clients={self._stats['clients_connected']} "
                f"broadcasts={self._stats['broadcasts']} "
                f"rpc={self._stats['rpc_received']}"
            )

    def teardown(self) -> None:
        if self._loop is not None:
            try:
                self._loop.call_soon_threadsafe(self._loop.stop)
            except RuntimeError:
                pass
        if self._loop_thread is not None:
            self._loop_thread.join(timeout=2.0)

    # ───────── asyncio loop owner ─────────
    def _run_loop(self, bind: str, port: int, ready: threading.Event):
        self._loop = asyncio.new_event_loop()
        asyncio.set_event_loop(self._loop)
        self._clients_lock = asyncio.Lock()
        try:
            self._loop.run_until_complete(self._serve(bind, port, ready))
            self._loop.run_forever()
        except Exception as e:        # pylint: disable=broad-except
            self.log.exception(f"WS loop crashed: {e}")
        finally:
            try:
                self._loop.close()
            except RuntimeError:
                pass

    async def _serve(self, bind: str, port: int, ready: threading.Event):
        import websockets
        try:
            self._server = await websockets.serve(
                self._handle_client, bind, port,
                ping_interval=10.0, ping_timeout=15.0,
            )
            ready.set()
            # Start the broadcast pump as a background task
            asyncio.create_task(self._broadcast_pump())
        except OSError as e:
            self.log.error(f"WS serve failed on :{port} — {e}")
            ready.set()

    # ───────── Client handler ─────────
    async def _handle_client(self, ws):
        addr = ws.remote_address
        async with self._clients_lock:
            self._clients.add(ws)
            self._stats["clients_connected"] = len(self._clients)
            self._stats["clients_total"] += 1
        self.log.info(f"WS client connected: {addr}")
        try:
            async for message in ws:
                await self._handle_inbound(message)
        except Exception as e:        # pylint: disable=broad-except
            self.log.debug(f"WS client {addr} loop ended: {e}")
        finally:
            async with self._clients_lock:
                self._clients.discard(ws)
                self._stats["clients_connected"] = len(self._clients)
            self.log.info(f"WS client disconnected: {addr}")

    async def _handle_inbound(self, raw: str):
        """JSON-RPC 2.0 dispatcher (SDD Rev.A.6 §15.3).

        Eight supported methods (P1-10) plus a pass-through for legacy
        single-arg commands. Unknown methods return error -32601;
        malformed JSON returns -32700; handler exceptions return -32000.

        Result/error frames are broadcast to every connected client
        (small-team operator setup; per-client targeting can be added
        later by threading the originating ws through the dispatcher).
        """
        self._stats["rpc_received"] += 1
        try:
            obj = json.loads(raw)
        except json.JSONDecodeError:
            self._stats["rpc_errors"] += 1
            self.log.warning(f"WS: invalid JSON ({raw[:60]!r})")
            await self._send_error(None, -32700, "parse error")
            return
        method = obj.get("method")
        params = obj.get("params") or {}
        rpc_id = obj.get("id")
        if not isinstance(method, str):
            self._stats["rpc_errors"] += 1
            await self._send_error(rpc_id, -32600, "invalid request")
            return

        handlers = {
            "formation.set":       self._h_formation_set,
            "mission.start":       self._h_mission_start,
            "mission.abort":       self._h_mission_abort,
            "telemetry.subscribe": self._h_telemetry_subscribe,
            "state.set":           self._h_state_set,
            "leader.takeover":     self._h_leader_takeover,
            "map.upload_brief":    self._h_map_upload_brief,
            "video.request":       self._h_video_request,
            "set_recording":       self._h_set_recording,   # legacy
        }
        handler = handlers.get(method)
        if handler is None:
            self._stats["rpc_errors"] += 1
            await self._send_error(rpc_id, -32601,
                                   f"unknown method: {method}")
            return
        try:
            result = await handler(params)
        except Exception as e:        # noqa: BLE001 — surface to client
            self._stats["rpc_errors"] += 1
            await self._send_error(rpc_id, -32000, str(e))
            return
        await self._send_result(rpc_id, result)

    async def _send_result(self, rpc_id, result):
        text = json.dumps({"jsonrpc": "2.0", "id": rpc_id,
                           "result": result})
        await self._broadcast_text(text)

    async def _send_error(self, rpc_id, code: int, message: str):
        text = json.dumps({"jsonrpc": "2.0", "id": rpc_id,
                           "error": {"code": code, "message": message}})
        await self._broadcast_text(text)

    async def _broadcast_text(self, text: str):
        async with self._clients_lock:
            clients = list(self._clients)
        failed = []
        for ws in clients:
            try:
                await ws.send(text)
            except Exception:        # noqa: BLE001
                failed.append(ws)
        if failed:
            async with self._clients_lock:
                for ws in failed:
                    self._clients.discard(ws)

    # ───────── JSON-RPC method handlers ─────────
    async def _forward_to_app_rpc(self, payload: dict) -> None:
        """Push a command dict onto app_rpc without blocking the loop."""
        loop = asyncio.get_running_loop()
        await loop.run_in_executor(
            None, publish, self.queues.app_rpc, payload)

    async def _h_formation_set(self, params: dict) -> dict:
        cmd = {
            "type": "formation_change",
            "formation": params.get("type", "V_SHAPE"),
            "d": float(params.get("d", 5.0)),
            "theta_deg": float(params.get("theta_deg", 90.0)),
            "dev_mode": bool(params.get("dev_mode", False)),
            "transition_time": float(params.get("transition_time", 5.0)),
            "ts_mono": time.monotonic(),
        }
        await self._forward_to_app_rpc(cmd)
        return {"ok": True, "transition_id": int(time.time() * 1000)}

    async def _h_mission_start(self, params: dict) -> dict:
        mission_id = params.get("mission_id")
        if not mission_id:
            raise ValueError("mission_id required")
        cmd = {
            "type": "mission_start",
            "mission_id": mission_id,
            "waypoints": params.get("waypoints", []),
            "ts_mono": time.monotonic(),
        }
        await self._forward_to_app_rpc(cmd)
        return {"ok": True, "mission_handle": mission_id}

    async def _h_mission_abort(self, _params: dict) -> dict:
        await self._forward_to_app_rpc({
            "type": "mission_abort",
            "ts_mono": time.monotonic(),
        })
        return {"ok": True}

    async def _h_telemetry_subscribe(self, params: dict) -> dict:
        rate_hz = float(params.get("rate_hz", self.TELEMETRY_HZ))
        return {"ok": True, "rate_hz": rate_hz}

    async def _h_state_set(self, params: dict) -> dict:
        phase = params.get("phase")
        await self._forward_to_app_rpc({
            "type": "state_set", "phase": phase,
            "ts_mono": time.monotonic(),
        })
        return {"ok": True, "current_phase": phase}

    async def _h_leader_takeover(self, params: dict) -> dict:
        new_leader_id = int(params.get("new_leader_id", 0))
        await self._forward_to_app_rpc({
            "type": "leader_takeover",
            "new_leader_id": new_leader_id,
            "ts_mono": time.monotonic(),
        })
        return {"ok": True, "election_started": True}

    async def _h_map_upload_brief(self, params: dict) -> dict:
        await self._forward_to_app_rpc({
            "type": "map_brief",
            "bbox": params.get("bbox"),
            "polygon": params.get("polygon"),
            "mission_id": params.get("mission_id"),
            "ts_mono": time.monotonic(),
        })
        # Actual fetch is async on the orchestrator side; we ack 0 KB so
        # the client doesn't hang waiting for a download size.
        return {"ok": True, "downloaded_size_kb": 0}

    async def _h_video_request(self, params: dict) -> dict:
        """Tablet → robot video.request RPC.

        Parses the JSON-RPC params into a typed `VideoRequest`, validates
        it (parse_video_request calls validate()), and publishes onto
        `tablet_video_request` for OrchestratorProcess to consume. The
        ack carries the negotiated sequence so the app can correlate the
        eventual VideoResponse without waiting for the stream to start.
        """
        msg = parse_video_request(params)
        publish(self.queues.tablet_video_request, msg)
        return {
            "ok": True,
            "sequence": msg.sequence,
            "target_robot_id": msg.target_robot_id,
            "action": msg.action,
        }

    async def _h_set_recording(self, params: dict) -> dict:
        on = bool(params.get("on", False))
        await self._forward_to_app_rpc({
            "type": "set_recording", "on": on,
            "ts_mono": time.monotonic(),
        })
        return {"ok": True, "recording": on}

    # ───────── Broadcast pump (30 Hz) ─────────
    async def _broadcast_pump(self):
        period = 1.0 / self.TELEMETRY_HZ
        while not self.shutdown_event.is_set():
            await asyncio.sleep(period)
            if not self._clients:
                continue
            # Snapshot under the threading.Lock (consumers update it)
            with self._snapshot_lock:
                snap = dict(self._snapshot)
            snap["ts_mono"] = time.monotonic()

            # Legacy single-robot envelope (existing operator clients).
            legacy = {
                "phase": snap.get("phase", 0),
                "pose": snap.get("pose"),
                "battery_soc": snap.get("battery_soc", 1.0),
                "locomotion_mode": snap.get("locomotion_mode", "stand"),
                "mission": snap.get("mission"),
                "ts_mono": snap.get("ts_mono", 0.0),
            }
            # New robots[] + swarm_health params (SDD §15.5).
            params = {
                "ts_ms": int(time.time() * 1000),
                "leader_id": snap.get("leader_id"),
                "robots": [self._build_self_robot(snap)],
                "swarm_health": snap.get("swarm_health", {}),
            }
            # Single message carries both shapes — backward-compatible.
            telemetry_frame = json.dumps({
                "type": "telemetry", "data": legacy,
                "method": "telemetry", "params": params,
                "jsonrpc": "2.0",
            })
            messages = [telemetry_frame]

            # Drain any one-shot events (anomaly etc.) into the same batch.
            ev_lock = getattr(self, "_event_lock", None)
            if ev_lock is not None:
                with ev_lock:
                    pending = self._event_buf
                    self._event_buf = []
                for kind, data in pending:
                    if kind == "anomaly" and isinstance(data, dict):
                        # P1-12: dual envelope — legacy `data` + JSON-RPC
                        # `params` with severity/image_url enrichment.
                        params = {
                            "ts_ms": int(time.time() * 1000),
                            "type": data.get("type", "unknown"),
                            "robot_id": data.get("robot_id"),
                            "location": data.get("location"),
                            "confidence": float(data.get("confidence", 0.0)),
                            "image_url": data.get("image_url"),
                            "ai_class": data.get("ai_class"),
                            "severity": self._severity_from_type(
                                data.get("type", "")),
                        }
                        messages.append(json.dumps({
                            "type": "anomaly", "data": data,
                            "method": "anomaly", "params": params,
                            "jsonrpc": "2.0",
                        }))
                    else:
                        messages.append(json.dumps(
                            {"type": kind, "data": data}))

            async with self._clients_lock:
                clients = list(self._clients)
            failed = []
            for ws in clients:
                try:
                    for text in messages:
                        await ws.send(text)
                except Exception:        # pylint: disable=broad-except
                    failed.append(ws)
            if failed:
                async with self._clients_lock:
                    for ws in failed:
                        self._clients.discard(ws)
            self._stats["broadcasts"] += 1

    # ───────── Snapshot → robots[] assembly ─────────
    def _build_self_robot(self, snap: Dict) -> Dict:
        """Compose the per-robot dict for this node from the latest
        snapshot. The single-robot legacy fields (battery_soc, pose,
        phase, locomotion_mode) are surfaced into the swarm-aware
        shape used by SDD §15.5 telemetry."""
        battery_pct = int(round(float(snap.get("battery_soc", 1.0)) * 100))
        return {
            "id": str(self.cfg.get("system", "robot_id",
                                   default="self")),
            "role": str(self.cfg.get("system", "robot_role",
                                     default="follower") or "follower"),
            "pose": snap.get("pose"),
            "speed_mps": float(snap.get("speed_mps", 0.0)),
            "battery_pct": battery_pct,
            "tier": str(snap.get("tier", "T0")),
            "rtk_quality": str(snap.get("rtk_quality_str", "GPS")),
            "errors": [],
            "phase": snap.get("phase", 0),
            "locomotion_mode": snap.get("locomotion_mode", "stand"),
        }

    # ───────── Telemetry source consumers ─────────
    def _pose_consumer(self):
        while self.is_running():
            p = consume(self.queues.pose, timeout=0.2)
            if p is None:
                continue
            with self._snapshot_lock:
                self._snapshot["pose"] = [
                    float(p.position[0]), float(p.position[1]),
                    float(p.position[2]),
                    float(p.orientation[0]), float(p.orientation[1]),
                    float(p.orientation[2]), float(p.orientation[3]),
                ]

    def _status_consumer(self):
        while self.is_running():
            s = consume(self.queues.robot_status, timeout=0.2)
            if s is None:
                continue
            with self._snapshot_lock:
                self._snapshot["battery_soc"] = float(s.battery_soc)
                self._snapshot["locomotion_mode"] = str(s.locomotion_mode)

    def _phase_consumer(self):
        # ble_phase is consumed by BleControl in production — use a
        # shadow queue (ws_phase) to avoid stealing notifications.
        while self.is_running():
            ph = consume(self.queues.ws_phase, timeout=0.2)
            if ph is None:
                continue
            with self._snapshot_lock:
                self._snapshot["phase"] = int(ph)

    def _mission_consumer(self):
        while self.is_running():
            m = consume(self.queues.mission_state, timeout=0.2)
            if m is None:
                continue
            with self._snapshot_lock:
                self._snapshot["mission"] = {
                    "name": getattr(m, "name", None),
                    "wp_idx": getattr(m, "wp_idx", None),
                    "phase": getattr(m, "phase", None),
                }

    def _anomaly_consumer(self):
        """Forward anomaly dicts (from CommProcess) as standalone WS msgs."""
        while self.is_running():
            ev = consume(self.queues.ws_anomaly, timeout=0.2)
            if ev is None:
                continue
            with self._event_lock:
                self._event_buf.append(("anomaly", ev))
                self._stats["anomaly_fanout"] += 1

    @staticmethod
    def _severity_from_type(anomaly_type: str) -> str:
        """Map anomaly type → severity (SDD Rev.A.6 §15.6).

        critical : link / leadership failures (operator must act now)
        warn     : navigable but should be reviewed
        info     : everything else (default)
        """
        if anomaly_type in ("comm_loss", "leader_lost"):
            return "critical"
        if anomaly_type in ("road_blocked", "ai_detection"):
            return "warn"
        return "info"

    def _tier_consumer(self):
        """Consume per-robot tier transitions from sw_tier."""
        while self.is_running():
            ev = consume(self.queues.sw_tier, timeout=0.2)
            if ev is None:
                continue
            tier_name = (
                getattr(ev, "tier_name", None)
                or getattr(ev, "cur", None)
                or getattr(ev, "tier", None))
            if tier_name is None:
                continue
            with self._snapshot_lock:
                self._snapshot["tier"] = str(tier_name)

    def _swarm_health_consumer(self):
        """Consume swarm-health snapshots from LeaderRollbackChecker."""
        while self.is_running():
            h = consume(self.queues.swarm_health, timeout=0.5)
            if h is None:
                continue
            with self._snapshot_lock:
                self._snapshot["swarm_health"] = {
                    "in_rollback": bool(getattr(h, "in_rollback",
                                                 h.get("in_rollback", False)
                                                 if isinstance(h, dict) else False)),
                    "struggling_ratio": float(getattr(h, "ratio",
                                                       h.get("struggling_ratio", 0.0)
                                                       if isinstance(h, dict) else 0.0)),
                    "anomaly_count": int(getattr(h, "anomaly_count",
                                                  h.get("anomaly_count", 0)
                                                  if isinstance(h, dict) else 0)),
                }

    def _heartbeat_consumer(self):
        """Capture heartbeat dicts and update the rolling snapshot.

        Heartbeats arrive ~5 s apart from CommProcess; keeping them in the
        snapshot means a fresh client picks up the latest battery / pose /
        rtk state on its first telemetry frame, even before the per-source
        consumers have produced a value.
        """
        while self.is_running():
            hb = consume(self.queues.ws_heartbeat, timeout=0.2)
            if hb is None:
                continue
            with self._snapshot_lock:
                if hb.get("pose") is not None:
                    self._snapshot["pose"] = hb["pose"]
                self._snapshot["battery_soc"] = float(hb.get("battery_soc", 1.0))
                # Heartbeat carries NMEA fix_quality as int; the snapshot
                # key consumed by _build_self_robot() is "rtk_quality_str".
                # Map the int → operator-facing string here so the app's
                # RTK indicator actually reflects the live heartbeat.
                self._snapshot["rtk_quality_str"] = _rtk_quality_int_to_str(
                    hb.get("rtk_quality", 0))
            with self._event_lock:
                self._stats["heartbeat_fanout"] += 1
