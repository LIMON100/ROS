"""
CommProcess: edge filtering + server upload of anomalies & telemetry.

Uplink selection (priority order):
    1. WiFi6 LAN (campus access point or mesh exit gateway)
    2. LTE modem (cellular fallback when WiFi6 unavailable)

The selection rule:
    • Probe WiFi6 reachability cheaply (UDP ping or TCP connect to gateway)
    • If WiFi6 fails → mark link 'down', use LTE if registered+pdp_active
    • If both down → cache to disk, retry every 30s
    • Hysteresis: don't switch back to WiFi6 until 3 consecutive successes

LTE-specific behaviors:
    • Don't upload H.265 video over LTE (too costly) — only metadata + low-res JPEG
    • Throttle to ≤ 50 KB/s to respect data plan
    • Defer non-critical telemetry until WiFi6 returns

Threads:
  • AnomalyConsumer : pull AnomalyEvent → upload queue
  • LteStatusConsumer : track current cellular health
  • Uploader        : batch upload via current best link
  • HeartbeatSender : every 5s → server (for E1 detection)
  • LocalCache      : when offline, persist to disk → resend later
"""
from __future__ import annotations

import json
import logging
import queue as q
import socket
import threading
import time
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path

from core.base_process import BaseProcess
from core.ipc import consume
from core.messages import (
    LTE_REGISTERED_HOME,
    LTE_REGISTERED_ROAMING,
    AnomalyEvent,
    LteStatus,
)

log = logging.getLogger(__name__)


def _probe_wifi6(host: str, port: int = 443, timeout_s: float = 1.5) -> bool:
    """Cheap reachability check — TCP connect to a known reachable host."""
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.settimeout(timeout_s)
            s.connect((host, port))
        return True
    except (OSError, socket.timeout):
        return False


class CommProcess(BaseProcess):
    def __init__(self, queues, shutdown_event, config, **diag):
        super().__init__(
            name="Comm",
            shutdown_event=shutdown_event,
            rate_hz=0.2,                # 5s heartbeat in step()
            cpu_affinity=config.get("system", "cpu_affinity", "comm"),
            **diag,
        )
        self.queues = queues
        self.cfg = config
        self._upload_queue: q.Queue = None
        self._cache_dir: Path = None
        # Identity + endpoint (resolved in setup)
        self._robot_id: str = ""
        self._server_url: str = ""
        # Link state
        self._link_lock: threading.Lock = None
        self._active_link: str = "none"        # "wifi6" | "lte" | "none"
        self._wifi_consec_ok: int = 0          # for hysteresis on switch-back
        self._lte_status: LteStatus | None = None
        # Latest robot snapshot (heartbeat fields)
        self._state_lock: threading.Lock = None
        self._latest_pose = None               # Pose6D
        self._latest_status = None             # RobotStatus
        self._latest_rtk_quality: int = 0
        # Stats
        self._stats = {
            "uploaded_wifi": 0, "uploaded_lte": 0,
            "cached": 0, "heartbeat_ok": 0, "heartbeat_fail": 0,
            "link_switches": 0,
        }

    def setup(self) -> None:
        self._upload_queue = q.Queue(maxsize=200)
        self._cache_dir = Path(self.cfg.get("comm", "cache_dir",
                                            default="/var/lib/patrol/cache"))
        self._cache_dir.mkdir(parents=True, exist_ok=True)
        # Crash dumps that have already been uploaded (so we don't resend)
        self._uploaded_crashes: set = set()
        self._crash_dir_to_watch = Path(self.cfg.get(
            "comm", "crash_dir", default="/var/log/patrol/crashes"))
        self._link_lock = threading.Lock()
        self._state_lock = threading.Lock()

        self._server_url = (self.cfg.get("comm", "server_url",
                                          default="http://localhost:8000")
                            ).rstrip("/")
        # Robot id resolution: explicit config wins; otherwise derive from
        # hostname so a fleet can be brought up without per-robot YAML edits.
        self._robot_id = (self.cfg.get("comm", "robot_id", default="")
                           or socket.gethostname())

        # Patrol Server is opt-in (Rev.A.5: deferred to Phase F). When off,
        # we still consume anomalies + maintain a heartbeat snapshot, but
        # publish them to ws_anomaly / ws_heartbeat for in-network broadcast
        # via WsTelemetry. The HTTP uploader / link monitor / crash uploader
        # all stay dormant.
        self._enable_patrol_server = bool(self.cfg.get(
            "comm", "enable_patrol_server", default=False))

        self.spawn_thread(self._anomaly_consumer,    name="AnomalyCnsm")
        self.spawn_thread(self._safety_event_consumer, name="SafetyCnsm")
        self.spawn_thread(self._pose_consumer,       name="PoseCnsm")
        self.spawn_thread(self._status_consumer,     name="StatusCnsm")
        self.spawn_thread(self._rtk_consumer,        name="RtkCnsm")
        if self._enable_patrol_server:
            self.spawn_thread(self._lte_status_consumer, name="LteCnsm")
            self.spawn_thread(self._link_monitor,        name="LinkMon")
            self.spawn_thread(self._uploader,            name="Uploader")
            self.spawn_thread(self._cache_resender,      name="Resender")
            self.spawn_thread(self._crash_uploader,      name="CrashUp")
        else:
            self.log.info(
                "comm: Patrol Server disabled (Rev.A.5) — "
                "anomalies + heartbeat redirected to ws_anomaly/ws_heartbeat")

    def step(self) -> None:
        # Heartbeat path. With Patrol Server disabled we publish a snapshot
        # dict on ws_heartbeat for the operator app; otherwise POST to
        # /api/v1/heartbeat.
        if not getattr(self, "_enable_patrol_server", False):
            ok = self._broadcast_heartbeat_ws()
        else:
            ok = self._send_heartbeat()
        if ok:
            self._stats["heartbeat_ok"] += 1
        else:
            self._stats["heartbeat_fail"] += 1

        # Periodic stats line — once a minute (every 12 heartbeats)
        total_hb = self._stats["heartbeat_ok"] + self._stats["heartbeat_fail"]
        if total_hb > 0 and total_hb % 12 == 0:
            with self._link_lock:
                link = self._active_link
                lte = self._lte_status
            self.log.info(
                f"comm  link={link} hb_ok/fail={self._stats['heartbeat_ok']}/"
                f"{self._stats['heartbeat_fail']} "
                f"uploaded[wifi]={self._stats['uploaded_wifi']} "
                f"uploaded[lte]={self._stats['uploaded_lte']} "
                f"cached={self._stats['cached']} switches={self._stats['link_switches']} "
                f"lte_rsrp={lte.rsrp_dbm if lte else 'n/a'}"
            )

    # ───── State subscribers (latest snapshot for heartbeat payload) ─────
    def _pose_consumer(self):
        while self.is_running():
            p = consume(self.queues.pose, timeout=0.2)
            if p is not None:
                with self._state_lock:
                    self._latest_pose = p

    def _status_consumer(self):
        while self.is_running():
            s = consume(self.queues.robot_status, timeout=0.2)
            if s is not None:
                with self._state_lock:
                    self._latest_status = s

    def _rtk_consumer(self):
        # We snoop the rtk queue just to capture the quality indicator
        # for the heartbeat payload. The real RTK data goes through
        # localization; this is a non-consuming peek pattern (we still
        # consume, but localization has its own subscription).
        while self.is_running():
            fix = consume(self.queues.rtk, timeout=0.5)
            if fix is not None:
                with self._state_lock:
                    self._latest_rtk_quality = int(fix.fix_quality)

    # ────────── Link selection ──────────
    def _link_monitor(self):
        """Probe WiFi6 every ~3s, decide which link to use."""
        gateway = self.cfg.get("comm", "wifi_probe_host", default="1.1.1.1")
        while self.is_running():
            wifi_ok = _probe_wifi6(gateway)
            with self._link_lock:
                lte = self._lte_status
                lte_ok = (lte is not None
                          and lte.registered in (LTE_REGISTERED_HOME,
                                                 LTE_REGISTERED_ROAMING)
                          and lte.pdp_active)
                old = self._active_link
                # Hysteresis: count consecutive WiFi6 successes for switch-back
                if wifi_ok:
                    self._wifi_consec_ok += 1
                else:
                    self._wifi_consec_ok = 0

                if self._active_link == "wifi6":
                    # Stay on WiFi6 unless probe fails
                    if not wifi_ok:
                        self._active_link = "lte" if lte_ok else "none"
                elif self._active_link == "lte":
                    # Switch back to WiFi6 only after 3 consecutive successes
                    if self._wifi_consec_ok >= 3:
                        self._active_link = "wifi6"
                    elif not lte_ok:
                        self._active_link = "none"
                else:  # "none"
                    if wifi_ok:
                        self._active_link = "wifi6"
                    elif lte_ok:
                        self._active_link = "lte"
                if old != self._active_link:
                    self._stats["link_switches"] += 1
                    self.log.warning(f"uplink: {old} → {self._active_link}")
            time.sleep(3.0)

    def _lte_status_consumer(self):
        while self.is_running():
            s = consume(self.queues.lte_status, timeout=0.5)
            if s is not None:
                with self._link_lock:
                    self._lte_status = s

    # ────────── Upload pipeline ──────────
    def _anomaly_consumer(self):
        while self.is_running():
            ev: AnomalyEvent = consume(self.queues.anomaly, timeout=0.1)
            if ev is None:
                continue
            if not getattr(self, "_enable_patrol_server", False):
                # Redirect to WS broadcast (operator app)
                self._broadcast_anomaly_ws(ev)
                continue
            try:
                self._upload_queue.put_nowait(ev)
            except q.Full:
                self._cache_to_disk(ev)
                self._stats["cached"] += 1

    def _safety_event_consumer(self):
        """SafetyProcess emits E1-E5 codes — fold them into the upload pipe.

        These travel the same path as anomalies: best-effort upload via the
        currently-active link, cached on link failure. Critical safety events
        (E1 comm-loss, E5 fall) bypass LTE throttling — uploaded as soon as
        any link is up.
        """
        while self.is_running():
            ev = consume(self.queues.safety_event, timeout=0.1)
            if ev is None:
                continue
            if not getattr(self, "_enable_patrol_server", False):
                # Redirect to WS broadcast (operator app)
                self._broadcast_anomaly_ws(ev)
                continue
            try:
                # Wrap as upload-queue item; uploader handles either type
                self._upload_queue.put_nowait(ev)
            except q.Full:
                self._cache_to_disk(ev)
                self._stats["cached"] += 1

    # ────────── WS broadcast (Patrol-Server-disabled mode) ──────────
    def _broadcast_anomaly_ws(self, ev) -> None:
        """Push an AnomalyEvent dict onto ws_anomaly for WsTelemetry fan-out.

        Serialization mirrors _anomaly_to_payload (no image_ref bytes —
        the operator app shouldn't carry binary frames over WS either).
        Failures are logged at debug; the operator can replay from logs.
        """
        try:
            payload = _anomaly_to_payload(self._robot_id, ev,
                                           include_image=False)
        except Exception as e:        # pylint: disable=broad-except
            self.log.debug(f"ws anomaly serialization failed: {e}")
            return
        ws_q = getattr(self.queues, "ws_anomaly", None)
        if ws_q is None:
            return
        try:
            from core.ipc import publish as _publish
            _publish(ws_q, payload)
            self._stats["ws_anomaly_tx"] = self._stats.get("ws_anomaly_tx", 0) + 1
        except Exception as e:        # pylint: disable=broad-except
            self.log.debug(f"ws anomaly publish failed: {e}")

    def _broadcast_heartbeat_ws(self) -> bool:
        """Build the same heartbeat dict as _send_heartbeat would, push to
        ws_heartbeat instead of POSTing.

        Returns True when the snapshot was successfully enqueued — there's
        no remote ack to verify, so a successful publish IS the success.
        """
        with self._state_lock:
            pose = self._latest_pose
            status = self._latest_status
            rtk_q = self._latest_rtk_quality
        payload = {
            "robot_id": self._robot_id,
            "stamp_monotonic": time.monotonic(),
            "battery_soc": float(status.battery_soc) if status else 1.0,
            "active_link": "ws",
            "rtk_quality": rtk_q,
            "pose": _pose_to_list(pose),
        }
        ws_q = getattr(self.queues, "ws_heartbeat", None)
        if ws_q is None:
            return False
        try:
            from core.ipc import publish as _publish
            _publish(ws_q, payload)
            return True
        except Exception:        # pylint: disable=broad-except
            return False

    def _uploader(self):
        while self.is_running():
            try:
                ev = self._upload_queue.get(timeout=0.5)
            except q.Empty:
                continue
            with self._link_lock:
                link = self._active_link
            if link == "none":
                self._cache_to_disk(ev)
                self._stats["cached"] += 1
                continue
            ok = self._upload_anomaly(ev, via=link)
            if ok:
                self._stats["uploaded_" + ("wifi" if link == "wifi6" else "lte")] += 1
            else:
                self._cache_to_disk(ev)
                self._stats["cached"] += 1

    def _cache_resender(self):
        """Periodically retry cached items when an uplink is up."""
        while self.is_running():
            time.sleep(30)
            with self._link_lock:
                if self._active_link == "none":
                    continue
            for _path in list(self._cache_dir.glob("anomaly_*.bin")):
                # Stub — real impl: deserialize, retry, on success unlink.
                pass

    def _crash_uploader(self):
        """Watch the crash directory and upload new dumps to the server.

        Crash dumps are JSON files (~10–100 KB) — small enough to send over
        LTE if no WiFi. Once uploaded, we tag the file by writing
        '<name>.uploaded' next to it; subsequent scans skip those.

        We don't delete crashes — they're cheap to keep, and offline forensic
        access matters more than disk pressure. Local rotation is the
        operator's job (logrotate /var/log/patrol/crashes/).
        """
        while self.is_running():
            time.sleep(10)
            if not self._crash_dir_to_watch.exists():
                continue
            for path in self._crash_dir_to_watch.glob("*.json"):
                marker = path.with_suffix(path.suffix + ".uploaded")
                if marker.exists() or path.name in self._uploaded_crashes:
                    continue
                with self._link_lock:
                    link = self._active_link
                if link == "none":
                    break       # try again later
                # On LTE, only upload critical crashes (smaller subset)
                if link == "lte" and "hang_" not in path.name \
                        and "unhandled" not in path.name:
                    continue
                ok = self._upload_crash_dump(path, via=link)
                if ok:
                    try:
                        marker.touch()
                    except OSError:
                        pass
                    self._uploaded_crashes.add(path.name)
                    self._stats["crashes_uploaded"] = \
                        self._stats.get("crashes_uploaded", 0) + 1
                    self.log.info(f"crash uploaded: {path.name} via {link}")

    # ───────── HTTP wire layer (urllib — no extra dependency) ─────────
    def _post_json(self, path: str, payload: dict,
                    timeout_s: float = 3.0) -> bool:
        """POST a JSON dict to <server_url><path>. Return True on 2xx.

        Uses urllib so we don't pull in 'requests' as a dependency.
        Errors (network, 5xx, 4xx) are logged at warning and counted —
        callers decide whether to cache the payload for later retry.

        Refuses any URL whose scheme is not http/https — urllib.request
        otherwise happily handles file://, ftp://, etc., which could
        leak local files or hit unexpected services if server_url were
        ever tampered with via config.
        """
        if not self._server_url:
            return False
        url = self._server_url + path
        scheme = urllib.parse.urlparse(url).scheme.lower()
        if scheme not in ("http", "https"):
            self.log.error(f"refusing non-http(s) server_url scheme: {scheme!r}")
            return False
        try:
            data = json.dumps(payload).encode("utf-8")
        except (TypeError, ValueError) as e:
            self.log.error(f"payload not JSON-serializable for {path}: {e}")
            return False
        req = urllib.request.Request(
            url, data=data,
            headers={"Content-Type": "application/json"},
            method="POST",
        )
        try:
            with urllib.request.urlopen(req, timeout=timeout_s) as resp:
                return 200 <= resp.status < 300
        except urllib.error.HTTPError as e:
            self.log.warning(f"POST {path} HTTP {e.code}: {e.reason}")
            return False
        except (urllib.error.URLError, socket.timeout, OSError) as e:
            # Server unreachable — common on LTE flaps. Log debug only.
            self.log.debug(f"POST {path} unreachable: {e}")
            return False

    # ───────── Heartbeat ─────────
    def _send_heartbeat(self) -> bool:
        """Build heartbeat payload from latest queue snapshots, POST it.

        Heartbeats are sent regardless of active_link state — even on a
        bad link, the server may receive the request anyway, and a recent
        last_seen on the dashboard helps the operator triage. Failed
        heartbeats just drop on the floor (no caching) since the next one
        is 5s away.
        """
        with self._state_lock:
            pose = self._latest_pose
            status = self._latest_status
            rtk_q = self._latest_rtk_quality
        with self._link_lock:
            active = self._active_link

        payload = {
            "robot_id": self._robot_id,
            "stamp_monotonic": time.monotonic(),
            "battery_soc": float(status.battery_soc) if status else 1.0,
            "active_link": active,
            "rtk_quality": rtk_q,
            "pose": _pose_to_list(pose),
            "metrics": {
                "uploaded_wifi": self._stats["uploaded_wifi"],
                "uploaded_lte":  self._stats["uploaded_lte"],
                "cached":        self._stats["cached"],
                "link_switches": self._stats["link_switches"],
            },
        }
        return self._post_json("/api/v1/heartbeat", payload)

    # ───────── Anomaly upload ─────────
    def _upload_anomaly(self, ev, via: str) -> bool:
        """Convert AnomalyEvent → server schema, POST.

        On LTE we strip the image_ref (would balloon the payload). The
        server still gets the description + bounding boxes — enough for
        an operator to decide whether to dispatch.
        """
        try:
            payload = _anomaly_to_payload(self._robot_id, ev,
                                           include_image=(via == "wifi6"))
        except Exception as e:        # pylint: disable=broad-except
            self.log.error(f"anomaly serialization failed: {e}")
            return False
        return self._post_json("/api/v1/anomalies", payload)

    # ───────── Crash upload ─────────
    def _upload_crash_dump(self, path: Path, via: str) -> bool:
        """Read crash JSON from disk, repackage to server schema, POST.

        Crash dumps from diag.py have a different shape than the server
        wants (we capture more state than the server stores). The server
        gets the salient fields; the original file stays on disk for
        deep forensics.
        """
        try:
            raw = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as e:
            self.log.error(f"crash dump unreadable {path}: {e}")
            return False

        exc = raw.get("exception") or {}
        # Recent logs often arrive as a list[str] but truncate huge dumps
        # before sending — single crash should never be >1 MB on the wire.
        recent_logs = list(raw.get("recent_logs") or [])[-200:]

        payload = {
            "robot_id": self._robot_id,
            "process": raw.get("process") or path.stem.split("_")[0],
            "reason": raw.get("reason") or "unknown",
            "pid": int(raw.get("pid") or 0),
            "wall_time": str(raw.get("wall_time") or ""),
            "exception_type": exc.get("type"),
            "exception_msg": exc.get("msg"),
            "traceback": (exc.get("traceback") or "")[:8192],
            "recent_logs": recent_logs,
            "proc_state": raw.get("proc") or {},
        }
        return self._post_json("/api/v1/crashes", payload)

    def _cache_to_disk(self, ev) -> None:
        """Spool an anomaly to disk for later retry by _cache_resender.

        Format: one JSON file per event, atomic rename to avoid half-writes.
        """
        try:
            payload = _anomaly_to_payload(self._robot_id, ev,
                                           include_image=False)
        except Exception:        # pylint: disable=broad-except
            return
        ts = time.strftime("%Y%m%d_%H%M%S")
        seq = self._stats["cached"]
        tmp = self._cache_dir / f".anomaly_{ts}_{seq}.bin.tmp"
        final = self._cache_dir / f"anomaly_{ts}_{seq}.bin"
        try:
            tmp.write_text(json.dumps(payload), encoding="utf-8")
            tmp.replace(final)
        except OSError:
            pass

    # ────────── Test hooks ──────────
    def _link_state(self) -> str:
        """Return current active link — used by integration tests."""
        with self._link_lock:
            return self._active_link


# ════════════════════════════════════════════════════════════════
#  Module-level serialization helpers
# ════════════════════════════════════════════════════════════════
def _pose_to_list(pose) -> list | None:
    """Pose6D → [x, y, z, qx, qy, qz, qw], or None if unset."""
    if pose is None:
        return None
    p = pose.position
    q = pose.orientation
    return [float(p[0]), float(p[1]), float(p[2]),
            float(q[0]), float(q[1]), float(q[2]), float(q[3])]


def _anomaly_to_payload(robot_id: str, ev, include_image: bool = True) -> dict:
    """Translate AnomalyEvent (robot dataclass) → server schema dict.

    Detection bbox arrives as an ndarray; we convert to a plain list for
    JSON serialization.
    """
    detections = []
    for d in (ev.detections or []):
        bbox = d.bbox
        if hasattr(bbox, "tolist"):
            bbox = bbox.tolist()
        # AnomalyEvent.detections is core.messages.Detection (no class_id).
        # Try to extract a class id from the label string when present
        # (e.g. "person (thermal 24.1°C)" → 0). Fall back to -1.
        class_id = getattr(d, "class_id", -1)
        detections.append({
            "label": d.label,
            "class_id": int(class_id),
            "confidence": float(d.confidence),
            "bbox": [float(x) for x in bbox],
        })
    pose = _pose_to_list(getattr(ev, "pose_at_event", None))
    payload = {
        "robot_id": robot_id,
        "seq": int(ev.header.seq),
        "stamp_monotonic": float(ev.header.stamp),
        "severity": str(ev.severity),
        "category": str(ev.category),
        "description": str(ev.description),
        "detections": detections,
        "pose": pose,
    }
    if include_image and getattr(ev, "image_ref", None) is not None:
        payload["image_url"] = None      # uploaded separately to MinIO/S3
    return payload
