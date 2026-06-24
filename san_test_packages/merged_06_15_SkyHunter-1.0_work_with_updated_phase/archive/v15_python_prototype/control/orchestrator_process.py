"""
OrchestratorProcess — connects BLE / FSM / WiFi / Streaming.

This is the brain of the control plane. It owns the FSM and reacts to:
  • BLE CMD opcodes           → FSM transitions + side effects
  • WiFi progress events      → FSM transitions
  • Stream status events      → FSM transitions
  • Internal disconnect       → FSM teardown

Side effects emitted:
  • wifi_request               → WifiControlProcess
  • stream_request             → StreamingProcess
  • ble_phase / ble_errors     → BleControlProcess (notifications back to app)

Why a separate process: the orchestrator is the single source of truth
for "what phase are we in." Putting it in BleControlProcess would couple
WiFi/streaming logic to the BLE wire layer; putting it in main.py would
mix it with bring-up bookkeeping. As its own process it gets crash dumps
+ schema-validated message handling free from BaseProcess.

Sequence (happy path, app issues WIFI_ON):
    BLE app→robot:  CMD WIFI_ON (0x10)
    Orchestrator:   FSM BLE_CONN → WIFI_BRINGUP   (allowed)
    Orchestrator → wifi_request("up")
    WifiControl:   bring up hostapd + dnsmasq, push WIFI_CRED
    BleControl:    notify WIFI_CRED to app
    WifiControl → wifi_progress(100, "ready")
    Orchestrator:   FSM WIFI_BRINGUP → WIFI_READY
    Orchestrator → stream_request("start", with creds)
    StreamingProc: GStreamer pipeline up
    StreamingProc → stream_status(playing=True)
    Orchestrator:   FSM WIFI_READY → STREAMING
"""
from __future__ import annotations

import time
from typing import Optional

from core.base_process import BaseProcess
from core.ipc import consume, publish
from core.messages import VideoRequest

from .state_machine import (
    ConnectionFsm,
    ErrorCode,
    Opcode,
    Phase,
)
from .video_request import (
    build_video_response,
    filter_for_robot,
    to_stream_requests,
)


class OrchestratorProcess(BaseProcess):
    """Wire the BLE / WiFi / Stream control plane to the FSM."""

    # Time budget for each major bring-up phase. Past these we consider the
    # phase failed and tear down. AIRYS uses similar budgets (SAN-BLE-WIFI-001).
    WIFI_BRINGUP_BUDGET_S = 15.0
    STREAM_START_BUDGET_S = 10.0

    def __init__(self, queues, shutdown_event, config, **diag):
        super().__init__(
            name="Orchestrator", shutdown_event=shutdown_event,
            rate_hz=2.0,            # 500ms control loop tick
            cpu_affinity=config.get("system", "cpu_affinity", "comm"),
            **diag,
        )
        self.queues = queues
        self.cfg = config
        self._fsm: Optional[ConnectionFsm] = None
        self._latest_creds: dict = {}
        self._budget_deadline_at: Optional[float] = None
        self._stats = {
            "ble_cmds": 0, "wifi_ups": 0, "wifi_downs": 0,
            "stream_starts": 0, "stream_stops": 0,
            "fsm_errors": 0, "budget_timeouts": 0,
            "video_reqs_accepted": 0, "video_reqs_rejected": 0,
        }
        self._my_robot_id: int = 0
        # Last VideoRequest accepted for this robot; consumed by the
        # stream_status path to correlate the response back to the app.
        self._last_video_request: Optional[VideoRequest] = None
        # Monotonic ms when the current stream first reported playing.
        self._stream_start_ms: int = 0
        self._stream_passphrase: str = ""

    # ───────── Lifecycle ─────────
    def setup(self) -> None:
        self._fsm = ConnectionFsm(log=self.log)

        # Notify BLE clients on every phase change — this is how the app
        # knows what state we're in. The listener fires synchronously
        # under the FSM lock; just pushes to a queue so it's non-blocking.
        self._fsm.add_listener(self._on_phase_changed)

        # Start in BLE_ADV — robot is up, BLE is advertising
        self._fsm.request(Phase.BLE_ADV, reason="orchestrator boot")

        self._my_robot_id = int(self.cfg.get(
            "system", "robot_id", default=0))
        self._stream_passphrase = str(self.cfg.get(
            "stream", "passphrase", default=""))

        self.spawn_thread(self._ble_command_consumer,    name="OrchBle")
        self.spawn_thread(self._wifi_progress_consumer,  name="OrchWifi")
        self.spawn_thread(self._stream_status_consumer,  name="OrchStream")
        self.spawn_thread(self._video_request_consumer,  name="OrchVideoReq")

    def step(self) -> None:
        # Watchdog: if a transient phase has been pending too long, error out
        if self._budget_deadline_at is not None \
                and time.monotonic() > self._budget_deadline_at:
            current = self._fsm.phase
            self.log.error(f"orchestrator: budget timeout in {current.name}")
            self._stats["budget_timeouts"] += 1
            err = (ErrorCode.WIFI_MODULE_FAIL
                    if current == Phase.WIFI_BRINGUP
                    else ErrorCode.STREAM_TIMEOUT
                    if current == Phase.WIFI_READY
                    else ErrorCode.UNKNOWN)
            self._fsm.to_error(err, reason=f"budget_in_{current.name}")
            self._budget_deadline_at = None
            self._teardown_all()

    def teardown(self) -> None:
        self._teardown_all()

    # ───────── FSM listener (notifies BLE + camera fan-out) ─────────
    def _on_phase_changed(self, _from: Phase, to: Phase, _reason: str) -> None:
        # Push the new phase to the BLE notify pump. Non-blocking publish —
        # if the app isn't connected, the queue absorbs it.
        publish(self.queues.ble_phase, int(to))
        if to == Phase.ERROR:
            self._stats["fsm_errors"] += 1

        # Toggle camera 'stream' subscription based on phase. The IMX678/
        # Go2 capture adapters watch this queue and only publish to the
        # camera_stream_ref queue while STREAM is in the subscriber set.
        # Dev-mode 'display' is set once at boot and stays on; only
        # 'stream' is dynamic.
        if to == Phase.STREAMING:
            publish(self.queues.camera_subscribers,
                    {"action": "add", "consumer": "stream"})
        elif _from == Phase.STREAMING:
            # Leaving STREAMING — tell capture to stop fanning out frames
            publish(self.queues.camera_subscribers,
                    {"action": "remove", "consumer": "stream"})

    # ───────── Tablet video request consumer ─────────
    def _video_request_consumer(self):
        """Drain tablet_video_request, filter by my robot_id, forward as
        legacy stream_request dict(s) to StreamingProcess.

        change_quality expands to a stop→start pair so the GStreamer
        pipeline rebuilds at the new bitrate; ordering is preserved by
        publishing one dict at a time in the list returned by
        to_stream_requests().
        """
        while self.is_running():
            msg = consume(self.queues.tablet_video_request, timeout=0.2)
            if msg is None:
                continue
            mine = filter_for_robot(msg, self._my_robot_id)
            if mine is None:
                self._stats["video_reqs_rejected"] += 1
                self.log.debug(
                    f"video request rejected (target={msg.target_robot_id}, "
                    f"my={self._my_robot_id})")
                continue
            self._stats["video_reqs_accepted"] += 1
            self.log.info(
                f"video request accepted: action={mine.action} "
                f"quality={mine.quality} codec={mine.codec} "
                f"protocol={mine.protocol} seq={mine.sequence}")
            # Cache the request so _stream_status_consumer can correlate
            # the eventual response back to this sequence + negotiated
            # params. A 'stop' action also overwrites — the response we
            # emit on stream_status playing=False then echoes the most
            # recent sequence the app sent.
            self._last_video_request = mine
            for req in to_stream_requests(mine):
                publish(self.queues.stream_request, req)

    # ───────── BLE CMD consumer ─────────
    def _ble_command_consumer(self):
        while self.is_running():
            cmd = consume(self.queues.ble_command, timeout=0.2)
            if cmd is None:
                continue
            self._stats["ble_cmds"] += 1
            self._handle_ble_command(cmd)

    def _handle_ble_command(self, cmd: dict) -> None:
        opcode = cmd.get("opcode")
        if opcode == "_internal_disconnect":
            # BLE link dropped — fall back to BLE_ADV
            self.log.info("orchestrator: BLE disconnect → teardown")
            self._teardown_all()
            self._fsm.request(Phase.TEARDOWN, reason="ble_disconnect")
            self._fsm.request(Phase.BLE_ADV, reason="back to advertising")
            return

        if not isinstance(opcode, int):
            return

        try:
            op = Opcode(opcode)
        except ValueError:
            self.log.warning(f"orchestrator: unknown opcode 0x{opcode:02X}")
            return

        # First-time CMD from app implies BLE_ADV → BLE_CONN
        if self._fsm.phase == Phase.BLE_ADV:
            self._fsm.request(Phase.BLE_CONN, reason="first_cmd")

        # Dispatch
        if op == Opcode.WIFI_ON:
            self._on_wifi_on()
        elif op == Opcode.WIFI_OFF:
            self._on_wifi_off()
        elif op == Opcode.PING or op == Opcode.KEEP_ALIVE:
            # No-op — the BLE keep-alive already saw the bytes
            pass
        elif op == Opcode.RESET:
            self._on_reset()
        elif op == Opcode.REBOOT:
            self.log.warning("orchestrator: REBOOT requested (not implemented)")
        else:
            # SNAPSHOT, REC_TOGGLE, LRF_TRIGGER — domain-specific, dispatch to
            # the relevant processes via dedicated queues. Not wired yet.
            self.log.debug(f"orchestrator: opcode {op.name} not yet wired")

    def _on_wifi_on(self) -> None:
        if self._fsm.phase != Phase.BLE_CONN:
            self.log.warning(
                f"orchestrator: WIFI_ON ignored in {self._fsm.phase.name}")
            return
        if not self._fsm.request(Phase.WIFI_BRINGUP, reason="cmd_wifi_on"):
            return
        self._stats["wifi_ups"] += 1
        publish(self.queues.wifi_request, {"action": "up"})
        self._budget_deadline_at = (
            time.monotonic() + self.WIFI_BRINGUP_BUDGET_S)

    def _on_wifi_off(self) -> None:
        # WIFI_OFF is valid in WIFI_BRINGUP / WIFI_READY / STREAMING
        if self._fsm.phase not in (Phase.WIFI_BRINGUP, Phase.WIFI_READY,
                                     Phase.STREAMING):
            self.log.warning(
                f"orchestrator: WIFI_OFF ignored in {self._fsm.phase.name}")
            return
        self._teardown_all()
        self._fsm.request(Phase.TEARDOWN, reason="cmd_wifi_off")
        self._fsm.request(Phase.BLE_ADV, reason="back to idle")

    def _on_reset(self) -> None:
        # Always allow reset — clears ERROR, returns to BLE_ADV
        self._teardown_all()
        if self._fsm.phase == Phase.ERROR:
            self._fsm.reset()
        elif self._fsm.phase != Phase.BLE_ADV:
            self._fsm.request(Phase.TEARDOWN, reason="cmd_reset")
            self._fsm.request(Phase.BLE_ADV, reason="reset")

    # ───────── WiFi progress consumer ─────────
    def _wifi_progress_consumer(self):
        while self.is_running():
            ev = consume(self.queues.wifi_progress, timeout=0.2)
            if ev is None:
                continue
            pct = ev.get("pct", 0)
            phase_label = ev.get("phase", "")
            self.log.info(f"orchestrator: wifi {pct}% — {phase_label}")
            if pct >= 100 and self._fsm.phase == Phase.WIFI_BRINGUP:
                # WiFi up; advance FSM and start the stream
                self._fsm.request(Phase.WIFI_READY, reason="wifi ready")
                self._budget_deadline_at = (
                    time.monotonic() + self.STREAM_START_BUDGET_S)
                publish(self.queues.stream_request, {
                    "action": "start",
                    "creds": dict(self._latest_creds),
                })
                self._stats["stream_starts"] += 1

    # ───────── Stream status consumer ─────────
    def _stream_status_consumer(self):
        while self.is_running():
            ev = consume(self.queues.stream_status, timeout=0.2)
            if ev is None:
                continue
            playing = ev.get("playing", False)
            if playing and self._fsm.phase == Phase.WIFI_READY:
                self._fsm.request(Phase.STREAMING, reason="gst playing")
                self._budget_deadline_at = None
            elif not playing and self._fsm.phase == Phase.STREAMING:
                # Stream died unexpectedly
                err = ErrorCode(ev.get("error_code", ErrorCode.STREAM_FAIL))
                self.log.error(f"orchestrator: stream lost ({err.name})")
                publish(self.queues.ble_errors, int(err))
                self._fsm.to_error(err, reason="stream_lost")
            # Set the stream_start_ms sentinel on first 'playing' edge;
            # zero it on stop/error so the response carries 0 when the
            # stream is down.
            now_ms = int(time.time() * 1000)
            if playing and self._stream_start_ms == 0:
                self._stream_start_ms = now_ms
            elif not playing:
                self._stream_start_ms = 0
            # Build + publish the typed response for the tablet.
            try:
                resp = build_video_response(
                    self._last_video_request, ev,
                    robot_id=self._my_robot_id,
                    stream_start_ms=self._stream_start_ms,
                    passphrase=self._stream_passphrase,
                    now_ms=now_ms,
                )
            except ValueError as e:
                self.log.warning(f"video response build failed: {e}")
                continue
            publish(self.queues.tablet_video_response, resp)

    # ───────── Helper: tear everything down ─────────
    def _teardown_all(self) -> None:
        publish(self.queues.stream_request, {"action": "stop"})
        publish(self.queues.wifi_request, {"action": "down"})
        self._stats["wifi_downs"] += 1
        self._stats["stream_stops"] += 1
        self._budget_deadline_at = None
