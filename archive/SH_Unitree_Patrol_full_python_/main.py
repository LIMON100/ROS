"""
Main entry: Quadruped Patrol Platform on RK3588 + Unitree Go2 EDU.

Process layout (17 processes, each with multiple threads; +1 on hub node):
  Hardware:    UnitreeGo2 / RtkGnss / NtripClient / LteModem / ExtImu / Lrf / IMX678 / Thermal
  State est:   Localization / SLAMBridge / SharedMapReceiver
  Map+mission: MapFusion / Perception / Mission
  External:    Comm / Safety / SwarmBridge
  Hub-only:    HubUgvAdapter (conditional on robot_role=hub)

Debugging facilities (all enabled by default; disable with --no-* flags):
  • Per-process JSON logs in /var/log/patrol/<process>.log (rotating, 50 MB × 5)
  • Crash dumps in /var/log/patrol/crashes/  (auto on exception/SIGUSR1)
  • Live metrics published to /tmp/patrol-metrics.json (every 1 s)
  • Run scripts/debug_dashboard.py in another terminal to watch live
  • SIGUSR1 to any process pid → forces a full state dump
"""
import argparse
import json
import logging
import multiprocessing as mp
import os
import signal
import sys
import threading
import time
from pathlib import Path

from adapters import (
    ExternalImuAdapter,
    HubUgvAdapter,
    IMX678Adapter,
    LrfAdapter,
    LteModemAdapter,
    NtripClientAdapter,
    RtkGnssAdapter,
    ThermalCameraAdapter,
    UnitreeGo2Adapter,
)
from comm import CommProcess
from control import BleControlProcess, OrchestratorProcess
from core import Config, make_topic_queues
from core.audit_log import AuditLogger
from core.diag import install_crash_handler, setup_logger
from core.ipc import consume
from core.shm_pool import ShmPool
from localization import LocalizationProcess
from mapping import MapFusionProcess, SharedMapReceiverProcess, SLAMBridgeProcess
from mission import MissionProcess
from perception import PerceptionProcess
from safety import SafetyProcess
from swarm import SwarmBridgeProcess


def _auth_state_relay(queues, auth_state_proxy,
                      stop: threading.Event):
    """Drain queues.auth_state → mutate the cross-process auth proxy.

    BLE publishes auth transitions as queue events (one per change). The
    proxy holds the *current* state for every consumer to poll on its own
    tick — level-triggered semantics. Without this relay, a process that
    boots after BLE has already authenticated would miss the event and
    sit forever at dev_override=False.
    """
    while not stop.is_set():
        ev = consume(queues.auth_state, timeout=0.2)
        if ev is None:
            continue
        try:
            auth_state_proxy["authenticated"] = bool(ev.get("authenticated"))
            auth_state_proxy["ts_mono"] = float(ev.get("ts_mono", 0.0))
            auth_state_proxy["reason"] = str(ev.get("reason", ""))
        except Exception as e:        # mp.Manager dicts can raise on disconnect
            logging.getLogger("auth_relay").warning(
                "auth_state proxy write failed: %s", e)


def _audit_writer(queues, logger: AuditLogger,
                  stop: threading.Event):
    """Drain queues.audit_event → AuditLogger.log().

    Single-writer pattern: only this thread calls logger.log(), so the
    sha256 hash chain stays consistent across multi-process publishers.
    Producers across the system call core.audit_log.publish_audit(),
    which puts the entry dict on the bus; this thread does the actual
    disk write.
    """
    while not stop.is_set():
        ev = consume(queues.audit_event, timeout=0.5)
        if ev is None:
            continue
        try:
            logger.log(
                category=ev.get("category", "unknown"),
                event=ev.get("event", "unknown"),
                actor=ev.get("actor"),
                params=ev.get("params") or {},
                fsm_phase=ev.get("fsm_phase"),
                mission_id=ev.get("mission_id"),
            )
        except Exception as e:        # never let an audit write kill the loop
            logging.getLogger("audit_writer").error(
                "audit log() raised on %s/%s: %s",
                ev.get("category"), ev.get("event"), e)


def _publish_metrics_to_disk(metrics_dict, queues, path: Path,
                             stop: threading.Event):
    """Persist the live shared metrics dict to a JSON file the dashboard reads.

    Why a file rather than a socket: ssh-friendly (cat from another tty),
    process-tree-independent (read from any user), survives a single-process
    crash. Cost: ~5 ms per second to serialize the (small) snapshot.
    """
    while not stop.is_set():
        try:
            snap = dict(metrics_dict)              # shallow copy
            # Embed queue depths as a synthetic process "_ipc"
            ipc_counters = {}
            for attr in dir(queues):
                if attr.startswith("_"):
                    continue
                q = getattr(queues, attr, None)
                if hasattr(q, "qsize"):
                    try:
                        ipc_counters[f"depth:{attr}"] = q.qsize()
                        ipc_counters[f"cap:{attr}"]   = q._maxsize
                    except (NotImplementedError, AttributeError):
                        pass    # macOS qsize() throws — skip
            snap["_ipc"] = {
                "name": "_ipc", "pid": os.getpid(),
                "counters": ipc_counters,
                "last_step_at": time.monotonic(),
                "step_count": 0, "exception_count": 0, "last_exception": "",
                "latency_samples_ms": [], "threads": {},
            }
            tmp = path.with_suffix(".json.tmp")
            tmp.write_text(json.dumps(snap, default=str))
            tmp.replace(path)
        except Exception:
            pass    # never let the dashboard publish kill the system
        stop.wait(1.0)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", default="config/system.yaml")
    parser.add_argument("--log-level", default="INFO")
    parser.add_argument("--log-dir",     default="/var/log/patrol",
                        help="Per-process rotating logs (.log files)")
    parser.add_argument("--crash-dir",   default="/var/log/patrol/crashes",
                        help="JSON crash dumps from unhandled exceptions / SIGUSR1")
    parser.add_argument("--audit-dir",   default="/var/log/patrol/audit",
                        help="Hash-chained JSONL audit log (SDD §9.7)")
    parser.add_argument("--no-audit",    action="store_true",
                        help="Disable the audit log writer (events published "
                             "to the bus are silently dropped)")
    parser.add_argument("--metrics-file", default="/tmp/patrol-metrics.json",
                        help="Where to publish live metrics for debug_dashboard.py")
    parser.add_argument("--log-json",    action="store_true",
                        help="Emit logs as one-JSON-per-line (default: human)")
    parser.add_argument("--no-log-dir",  action="store_true",
                        help="Don't write per-process log files")
    parser.add_argument("--no-crashes",  action="store_true",
                        help="Disable crash dump capture")
    # ── Debug-mode (off by default — production stays clean) ──
    parser.add_argument("--profile", action="store_true",
                        help="Enable cProfile in every process. "
                             "Adds 5-15%% CPU overhead.")
    parser.add_argument("--profile-dir", default="/var/log/patrol/profiles",
                        help="Where to write per-process .pstats files")
    parser.add_argument("--profile-period-s", type=float, default=60.0,
                        help="Profile dump cadence (seconds)")
    parser.add_argument("--hang-timeout-s", type=float, default=0.0,
                        help="Step()s longer than this trigger a crash dump. "
                             "0 = disabled.")
    parser.add_argument("--schema-check", action="store_true",
                        help="Validate every published message's dataclass "
                             "fields. Slow — debug only.")
    args = parser.parse_args()

    # Resolve diagnostic paths (None = disabled)
    log_dir   = None if args.no_log_dir else Path(args.log_dir)
    crash_dir = None if args.no_crashes else Path(args.crash_dir)
    audit_dir = None if args.no_audit  else Path(args.audit_dir)
    metrics_path = Path(args.metrics_file)
    metrics_path.parent.mkdir(parents=True, exist_ok=True)

    # Main-process logger + crash handler (children get their own via BaseProcess)
    log = setup_logger("main", level=getattr(logging, args.log_level),
                       log_dir=log_dir, json_format=args.log_json)
    if crash_dir is not None:
        install_crash_handler(crash_dir, logger=log)

    cfg = Config.load(args.config)
    log.info(f"config loaded from {args.config}; log_dir={log_dir} "
             f"crash_dir={crash_dir} audit_dir={audit_dir} "
             f"metrics={metrics_path}")
    # Surface (don't enforce) missing keys so a typo in a YAML override
    # gets flagged at boot rather than masked by a baked-in default.
    cfg.validate_required([
        "system.cpu_affinity",
        "comm.server_url",
        "mission.routes_file",
        "mission.patrol_schedule",
        "shm.lidar_slot_bytes",
        "shm.camera_slot_bytes",
    ], logger=log)

    # Shared metrics dict (mp.Manager) — every BaseProcess subclass writes here
    manager = mp.Manager()
    metrics_dict = manager.dict()

    # Cross-process auth state proxy — single source of truth for "is the
    # operator currently PIN-authenticated?" Updated by _auth_state_relay
    # from BLE events; read each tick by SafetyProcess (dev_override on
    # BatteryMonitor/Geofence) and MissionProcess (OperationalModeController).
    # Level-triggered: a process that boots after auth happened still sees
    # the right value because we keep the latest snapshot, not the event.
    auth_state_proxy = manager.dict()
    auth_state_proxy["authenticated"] = False
    auth_state_proxy["ts_mono"] = 0.0
    auth_state_proxy["reason"] = "boot"

    # SHM pools — must be created BEFORE child processes spawn
    lidar_shm = ShmPool(
        name_prefix="patrol_lidar",
        slot_bytes=cfg.get("shm", "lidar_slot_bytes"),
        n_slots=cfg.get("shm", "lidar_n_slots"),
    )
    camera_shm = ShmPool(
        name_prefix="patrol_camera",
        slot_bytes=cfg.get("shm", "camera_slot_bytes"),
        n_slots=cfg.get("shm", "camera_n_slots"),
    )
    lidar_shm.setup()
    camera_shm.setup()
    log.info(f"SHM pools allocated  lidar={cfg.get('shm','lidar_n_slots')} slots, "
             f"camera={cfg.get('shm','camera_n_slots')} slots")

    shutdown = mp.Event()
    queues = make_topic_queues(maxsize=20)

    # Common kwargs every child gets — pickle-safe (str + Manager proxy + bool)
    diag = dict(
        metrics_dict=metrics_dict,
        log_dir=str(log_dir) if log_dir else None,
        log_json=args.log_json,
        crash_dir=str(crash_dir) if crash_dir else None,
        # Debug-mode flags — all default off; values forwarded to BaseProcess
        profile=args.profile,
        profile_dir=str(args.profile_dir) if args.profile else None,
        profile_dump_period_s=args.profile_period_s,
        hang_timeout_s=args.hang_timeout_s,
        schema_check=args.schema_check,
    )

    procs: list = []
    publish_stop = threading.Event()
    publish_thread = None
    audit_stop = threading.Event()
    audit_thread = None
    audit_logger: AuditLogger | None = None
    auth_relay_stop = threading.Event()
    auth_relay_thread = None
    try:
        # Build child instances inside the try so SHM/manager teardown
        # in the finally still runs if any constructor raises (e.g. a
        # missing **diag forward, a bad config key).
        procs = [
            UnitreeGo2Adapter(queues, shutdown, cfg, lidar_shm, camera_shm, **diag),
            RtkGnssAdapter(queues, shutdown, cfg, **diag),
            NtripClientAdapter(queues, shutdown, cfg, **diag),
            LteModemAdapter(queues, shutdown, cfg, **diag),
            ExternalImuAdapter(queues, shutdown, cfg, **diag),
            LrfAdapter(queues, shutdown, cfg, **diag),
            IMX678Adapter(queues, shutdown, cfg, camera_shm, **diag),
            ThermalCameraAdapter(queues, shutdown, cfg, camera_shm, **diag),
            LocalizationProcess(queues, shutdown, cfg, **diag),
            SLAMBridgeProcess(queues, shutdown, cfg, lidar_shm, **diag),
            SharedMapReceiverProcess(queues, shutdown, cfg, **diag),
            MapFusionProcess(queues, shutdown, cfg, **diag),
            PerceptionProcess(queues, shutdown, cfg, camera_shm, **diag),
            MissionProcess(queues, shutdown, cfg,
                           auth_state_proxy=auth_state_proxy, **diag),
            CommProcess(queues, shutdown, cfg, **diag),
            SafetyProcess(queues, shutdown, cfg,
                          auth_state_proxy=auth_state_proxy, **diag),
            SwarmBridgeProcess(queues, shutdown, cfg, **diag),
            # Control plane — BLE/Orchestrator must run so the PIN-auth
            # producer (BLE) actually publishes auth_state events for the
            # relay to consume. Without these procs the consumer chain
            # would be wired but inert (audit finding C1).
            BleControlProcess(queues, shutdown, cfg, **diag),
            OrchestratorProcess(queues, shutdown, cfg, **diag),
        ]
        # Hub UGV (Rev.A.5 §6.7) — only spawned on the dedicated hub node.
        # The leader/follower nodes don't carry the extra compute payload,
        # so the adapter would just run idle there.
        robot_role = str(cfg.get("system", "robot_role", default="follower")).lower()
        if robot_role == "hub":
            procs.append(HubUgvAdapter(queues, shutdown, cfg, **diag))
            log.info("robot_role=hub → HubUgvAdapter scheduled")

        def handle_sig(*_):
            log.info("shutdown signal received")
            shutdown.set()
        signal.signal(signal.SIGINT, handle_sig)
        signal.signal(signal.SIGTERM, handle_sig)

        log.info(f"starting {len(procs)} processes")
        for p in procs:
            p.start()
            log.info(f"  ├ {p.name}  pid={p.pid}")
        log.info(f"main pid={os.getpid()} — kill -USR1 to dump state to crash_dir")

        # Disk-publish thread for the live dashboard
        publish_thread = threading.Thread(
            target=_publish_metrics_to_disk,
            args=(metrics_dict, queues, metrics_path, publish_stop),
            name="MetricsPub", daemon=True,
        )
        publish_thread.start()

        # Audit writer — single hash-chained logger drains audit_event bus.
        # All processes publish via core.audit_log.publish_audit(); only
        # this thread calls AuditLogger.log(), so the sha256 chain stays
        # consistent across the spawn-and-multiprocess fan-out.
        if audit_dir is not None:
            robot_id = str(cfg.get("system", "robot_id", default="robot-000"))
            audit_logger = AuditLogger(
                log_dir=str(audit_dir),
                robot_id=robot_id,
                on_drop=lambda _entry, exc: log.error(
                    f"audit write dropped: {exc}"),
            )
            audit_thread = threading.Thread(
                target=_audit_writer,
                args=(queues, audit_logger, audit_stop),
                name="AuditWriter", daemon=True,
            )
            audit_thread.start()
            log.info(f"audit log writer started → {audit_dir}")

        # Auth-state relay — drains queues.auth_state into auth_state_proxy
        # so SafetyProcess / MissionProcess can poll the latest BLE PIN
        # auth status (level-triggered, see _auth_state_relay docstring).
        auth_relay_thread = threading.Thread(
            target=_auth_state_relay,
            args=(queues, auth_state_proxy, auth_relay_stop),
            name="AuthRelay", daemon=True,
        )
        auth_relay_thread.start()

        while not shutdown.is_set():
            for p in procs:
                if not p.is_alive():
                    log.error(
                        f"{p.name} died (exit={p.exitcode}) — triggering shutdown")
                    shutdown.set()
                    break
            time.sleep(0.5)
    finally:
        log.info("joining processes...")
        publish_stop.set()
        audit_stop.set()
        auth_relay_stop.set()
        for p in procs:
            if p.pid is None:
                continue        # never started (constructor raised)
            p.join(timeout=5.0)
            if p.is_alive():
                log.warning(f"{p.name} did not exit, terminating")
                p.terminate()
        # Drain any remaining audit events before tearing down the writer.
        # The thread is daemon, but giving it a short join window lets
        # security events emitted during shutdown reach disk.
        if audit_thread is not None:
            audit_thread.join(timeout=2.0)
        if auth_relay_thread is not None:
            auth_relay_thread.join(timeout=1.0)
        lidar_shm.teardown()
        camera_shm.teardown()
        log.info("system stopped")


if __name__ == "__main__":
    mp.set_start_method("spawn", force=True)
    sys.exit(main())
