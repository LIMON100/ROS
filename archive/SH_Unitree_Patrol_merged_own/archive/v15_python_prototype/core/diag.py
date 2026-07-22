"""
Diagnostics infrastructure — every facility a future-you needs at 3 AM.

This module owns five concerns:

  1. STRUCTURED LOGGING
     • JSON-line format → grep-able, ingestable into ELK/Loki without parsing
     • Plain-text format for terminals
     • RotatingFileHandler with size cap → guaranteed disk-bound
     • Correlation ID per (process, thread) for request tracing

  2. CRASH HANDLER
     • Captures the traceback of any unhandled exception
     • Snapshots queue depths, last messages, recent log lines
     • Writes a single self-contained crash file under /var/log/patrol/crashes/
     • Triggered by exceptions, SIGTERM, SIGUSR1 (manual dump)

  3. PROCESS METRICS
     • step_count, step_latency_ms (p50/p95/p99 via reservoir sample)
     • exception_count, last_exception
     • thread liveness map
     • published per-process via mp.Manager dict (read by DiagnosticsProcess)

  4. MESSAGE TRACING
     • Optional: correlate input messages → output messages by tag
     • Lightweight (compile-time disabled in production); zero overhead off

  5. DUMP-ON-DEMAND
     • Send SIGUSR1 to any process → triggers state dump to log
     • `python -m core.diag dump <pid>` from CLI

The intent is "debugging readiness without ROS/profiler/perf".
A single file so it can be vendored into other projects.
"""
from __future__ import annotations

import faulthandler
import json
import logging
import logging.handlers
import os
import signal
import sys
import threading
import time
import traceback
from collections import deque
from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Any, Deque, Dict, List, Optional

# ╔═══════════════════════════════════════════════════════════════╗
# ║  Structured logging                                            ║
# ╚═══════════════════════════════════════════════════════════════╝

# Per-process correlation ID — set once on setup, included in every record
_CORRELATION_ID = ""


def set_correlation_id(cid: str) -> None:
    """Set the correlation ID for this process. Picked up by all loggers."""
    global _CORRELATION_ID
    _CORRELATION_ID = cid


class _JsonLineFormatter(logging.Formatter):
    """One JSON object per line — 'process', 'level', 'msg', + extras."""

    def format(self, record: logging.LogRecord) -> str:
        out: Dict[str, Any] = {
            "ts":      record.created,
            "iso":     time.strftime("%Y-%m-%dT%H:%M:%S",
                                     time.localtime(record.created))
                       + f".{int(record.msecs):03d}",
            "level":   record.levelname,
            "logger":  record.name,
            "process": record.processName,
            "thread":  record.threadName,
            "msg":     record.getMessage(),
            "cid":     _CORRELATION_ID,
        }
        if record.exc_info:
            out["exc"] = self.formatException(record.exc_info)
        # User-supplied structured fields (logger.info(msg, extra={...}))
        for k, v in record.__dict__.items():
            if k.startswith("ext_"):
                out[k[4:]] = v
        # Compact dump — avoid escapes for non-ASCII, single line
        return json.dumps(out, ensure_ascii=False, default=str)


class _PlainLineFormatter(logging.Formatter):
    """Human-readable terminal format with millisecond timestamps."""

    def __init__(self) -> None:
        super().__init__(
            "[%(asctime)s.%(msecs)03d][%(processName)-12s][%(threadName)-10s]"
            "[%(levelname)-7s] %(name)s: %(message)s",
            datefmt="%H:%M:%S",
        )


def setup_logger(
    name: str,
    level: int = logging.INFO,
    log_dir: Optional[Path] = None,
    json_format: bool = False,
    max_bytes: int = 50 * 1024 * 1024,    # 50 MB per file
    backup_count: int = 5,                 # → 250 MB total per process
) -> logging.Logger:
    """Create or reconfigure the named logger.

    Always installs a stream handler on stdout. If `log_dir` is given,
    also installs a RotatingFileHandler. Always installs a _RecentLogHandler
    so crash dumps can include recent context. Idempotent — repeated calls
    won't multiply handlers.
    """
    logger = logging.getLogger(name)
    # Idempotency guard — strip our own handlers so re-setup picks up
    # new config (e.g., user toggles json_format)
    for h in list(logger.handlers):
        if getattr(h, "_patrol_managed", False):
            logger.removeHandler(h)

    fmt: logging.Formatter = (_JsonLineFormatter() if json_format
                              else _PlainLineFormatter())

    sh = logging.StreamHandler(sys.stdout)
    sh.setFormatter(fmt)
    sh._patrol_managed = True
    logger.addHandler(sh)

    if log_dir is not None:
        log_dir.mkdir(parents=True, exist_ok=True)
        fh = logging.handlers.RotatingFileHandler(
            log_dir / f"{name}.log",
            maxBytes=max_bytes, backupCount=backup_count,
            encoding="utf-8",
        )
        fh.setFormatter(fmt)
        fh._patrol_managed = True
        logger.addHandler(fh)

    # Recent-logs ring for crash dumps. Attached directly because we set
    # propagate=False below — root's handler wouldn't get our records.
    rh = _RecentLogHandler()
    rh.setFormatter(_PlainLineFormatter())
    rh._patrol_managed = True
    logger.addHandler(rh)

    logger.setLevel(level)
    logger.propagate = False        # don't double-print via root
    return logger


# ╔═══════════════════════════════════════════════════════════════╗
# ║  Process metrics                                               ║
# ╚═══════════════════════════════════════════════════════════════╝

@dataclass
class ProcessMetrics:
    """Per-process snapshot suitable for serialization to a manager dict."""
    name: str = ""
    pid: int = 0
    started_at: float = 0.0
    last_step_at: float = 0.0
    step_count: int = 0
    exception_count: int = 0
    last_exception: str = ""
    # Latency reservoir (rolling, last 256 samples)
    latency_samples_ms: List[float] = field(default_factory=list)
    # Thread liveness — name → last-seen monotonic time
    threads: Dict[str, float] = field(default_factory=dict)
    # Free-form counters (CommProcess uploaded_wifi, link_switches, etc.)
    counters: Dict[str, int] = field(default_factory=dict)

    def percentile(self, p: float) -> float:
        if not self.latency_samples_ms:
            return 0.0
        s = sorted(self.latency_samples_ms)
        i = max(0, min(len(s) - 1, int(p / 100.0 * len(s))))
        return s[i]

    def healthy(self, max_step_age_s: float = 5.0) -> bool:
        if self.last_step_at == 0:
            return True   # not yet stepped — not necessarily unhealthy
        return (time.monotonic() - self.last_step_at) < max_step_age_s


class MetricsCollector:
    """Per-process accumulator + publisher.

    Lightweight enough to call on every step(). Publishes a snapshot via
    a shared mp.Manager dict every `publish_period_s`.
    """
    _RESERVOIR = 256

    def __init__(self,
                 name: str,
                 shared_dict: Optional[Any] = None,   # mp.Manager().dict()
                 publish_period_s: float = 1.0):
        self.name = name
        self.shared = shared_dict
        self.publish_period_s = publish_period_s
        self._m = ProcessMetrics(name=name, pid=os.getpid(),
                                 started_at=time.monotonic())
        self._lock = threading.Lock()
        self._last_publish_at = 0.0
        self._step_t0: Optional[float] = None

    # ── step() instrumentation ──
    def step_begin(self) -> None:
        self._step_t0 = time.monotonic()

    def step_end(self) -> None:
        if self._step_t0 is None:
            return
        dt_ms = (time.monotonic() - self._step_t0) * 1000.0
        with self._lock:
            self._m.last_step_at = time.monotonic()
            self._m.step_count += 1
            samples = self._m.latency_samples_ms
            samples.append(dt_ms)
            if len(samples) > self._RESERVOIR:
                # Keep newest — drop oldest. Reservoir sampling is overkill
                # for a 256-element rolling window.
                del samples[0]
        self._step_t0 = None
        self._maybe_publish()

    def thread_alive(self, name: str) -> None:
        with self._lock:
            self._m.threads[name] = time.monotonic()

    def increment(self, counter: str, by: int = 1) -> None:
        with self._lock:
            self._m.counters[counter] = self._m.counters.get(counter, 0) + by

    def record_exception(self, exc: BaseException) -> None:
        with self._lock:
            self._m.exception_count += 1
            self._m.last_exception = (
                f"{type(exc).__name__}: {exc} "
                f"(at {time.strftime('%H:%M:%S')})"
            )

    def snapshot(self) -> ProcessMetrics:
        with self._lock:
            # Shallow copy is enough — caller treats it as read-only
            return ProcessMetrics(
                name=self._m.name, pid=self._m.pid,
                started_at=self._m.started_at,
                last_step_at=self._m.last_step_at,
                step_count=self._m.step_count,
                exception_count=self._m.exception_count,
                last_exception=self._m.last_exception,
                latency_samples_ms=list(self._m.latency_samples_ms),
                threads=dict(self._m.threads),
                counters=dict(self._m.counters),
            )

    def _maybe_publish(self) -> None:
        if self.shared is None:
            return
        now = time.monotonic()
        if (now - self._last_publish_at) < self.publish_period_s:
            return
        self._last_publish_at = now
        snap = self.snapshot()
        # mp.Manager dicts can hold dataclass via asdict
        try:
            self.shared[self.name] = asdict(snap)
        except Exception:
            pass    # never let metrics publishing break the actual work


# ╔═══════════════════════════════════════════════════════════════╗
# ║  Crash handler — turn exceptions into post-mortem dumps        ║
# ╚═══════════════════════════════════════════════════════════════╝

# Recent log lines kept in memory for inclusion in crash dumps
_RECENT_LOGS: Deque[str] = deque(maxlen=200)


class _RecentLogHandler(logging.Handler):
    def emit(self, record: logging.LogRecord) -> None:
        try:
            _RECENT_LOGS.append(self.format(record))
        except Exception:
            pass


def _gather_runtime_state() -> Dict[str, Any]:
    """Collect everything we'd want at 3 AM:
    threads, frame stacks, recent logs, env, /proc info."""
    state: Dict[str, Any] = {
        "pid": os.getpid(),
        "ppid": os.getppid(),
        "process_name": getattr(os, "process_name", None) or "",
        "wall_time": time.strftime("%Y-%m-%dT%H:%M:%S"),
        "monotonic": time.monotonic(),
        "thread_count": threading.active_count(),
        "threads": [],
        "stacks": {},
        "recent_logs": list(_RECENT_LOGS),
    }
    for t in threading.enumerate():
        state["threads"].append({
            "name": t.name, "ident": t.ident,
            "daemon": t.daemon, "alive": t.is_alive(),
        })
    # Per-thread Python frame stacks
    frames = sys._current_frames()
    for tid, frame in frames.items():
        state["stacks"][str(tid)] = "".join(
            traceback.format_stack(frame, limit=20))
    # /proc/self/status — RSS, threads, fd count
    try:
        with open("/proc/self/status") as f:
            text = f.read()
        for line in text.splitlines():
            if any(line.startswith(k) for k in
                   ("VmRSS:", "VmSize:", "Threads:", "FDSize:", "State:")):
                key, _, val = line.partition(":")
                state.setdefault("proc", {})[key] = val.strip()
    except (FileNotFoundError, PermissionError):
        pass
    try:
        state["fd_count"] = len(os.listdir(f"/proc/{os.getpid()}/fd"))
    except OSError:
        pass
    return state


_CRASH_DIR: Optional[Path] = None
_LOGGER_FOR_CRASH: Optional[logging.Logger] = None


def install_crash_handler(crash_dir: Path,
                          logger: Optional[logging.Logger] = None) -> None:
    """Hook sys.excepthook + threading exception hook + faulthandler +
    SIGUSR1 handler. Idempotent.

    Crash files are written under crash_dir/<process>_<ts>_<reason>.json,
    one per event. Latest crash is also tee'd to logger if provided.
    """
    global _CRASH_DIR, _LOGGER_FOR_CRASH
    crash_dir.mkdir(parents=True, exist_ok=True)
    _CRASH_DIR = crash_dir
    _LOGGER_FOR_CRASH = logger

    # Capture last 200 log lines for dumps
    root = logging.getLogger()
    if not any(isinstance(h, _RecentLogHandler) for h in root.handlers):
        h = _RecentLogHandler()
        h.setFormatter(_PlainLineFormatter())
        root.addHandler(h)

    # Native faulthandler: hard crashes (SIGSEGV) get a Python traceback to stderr.
    # We open a dedicated file because stderr may be redirected/closed by mp.spawn.
    fh_path = crash_dir / f"faulthandler_{os.getpid()}.log"
    try:
        fh_file = open(fh_path, "a", buffering=1)   # line-buffered
        faulthandler.enable(file=fh_file)
        # Ten-second hangs → also dump
        faulthandler.dump_traceback_later(timeout=300, repeat=True,
                                          file=fh_file)
    except (OSError, RuntimeError):
        # On some systems / inside some CI runners faulthandler can't open.
        pass

    sys.excepthook = _excepthook
    threading.excepthook = _thread_excepthook    # type: ignore[attr-defined]

    # SIGUSR1 → manual state dump (no exit). Useful from `kill -USR1 <pid>`.
    try:
        signal.signal(signal.SIGUSR1, _on_sigusr1)
    except (ValueError, OSError, AttributeError):
        # Not main thread, unsupported platform, or signal not available (Windows)
        pass


def _write_crash(reason: str, *,
                 exc: Optional[BaseException] = None,
                 process_name: str = "") -> Optional[Path]:
    if _CRASH_DIR is None:
        return None
    state = _gather_runtime_state()
    state["reason"] = reason
    state["process"] = process_name or state.get("process_name", "")
    if exc is not None:
        state["exception"] = {
            "type": type(exc).__name__,
            "msg":  str(exc),
            "traceback": "".join(
                traceback.format_exception(type(exc), exc, exc.__traceback__)),
        }
    ts = time.strftime("%Y%m%d_%H%M%S")
    pname = state["process"] or f"pid{os.getpid()}"
    path = _CRASH_DIR / f"{pname}_{ts}_{reason}.json"
    try:
        path.write_text(json.dumps(state, indent=2, default=str),
                        encoding="utf-8")
    except OSError:
        return None
    if _LOGGER_FOR_CRASH:
        _LOGGER_FOR_CRASH.error(
            f"crash dump written: {path} (reason={reason}, "
            f"exception={type(exc).__name__ if exc else '-'})"
        )
    return path


def _excepthook(exc_type: type, exc_value: BaseException, tb) -> None:
    # Don't capture KeyboardInterrupt — that's an intentional shutdown
    if issubclass(exc_type, KeyboardInterrupt):
        sys.__excepthook__(exc_type, exc_value, tb)
        return
    _write_crash("unhandled", exc=exc_value)
    sys.__excepthook__(exc_type, exc_value, tb)


def _thread_excepthook(args) -> None:
    if issubclass(args.exc_type, SystemExit):
        return
    _write_crash(f"thread_{args.thread.name if args.thread else 'unknown'}",
                 exc=args.exc_value)


def _on_sigusr1(_signum, _frame) -> None:
    """Async-signal-safe is hard in Python — we just enqueue a write."""
    # In practice the file write happens from the signal handler, which is
    # not strictly safe but works in CPython for non-async-signal builds.
    _write_crash("sigusr1_dump")


# ╔═══════════════════════════════════════════════════════════════╗
# ║  Profiler — cProfile wrapper with periodic dumps              ║
# ╚═══════════════════════════════════════════════════════════════╝

class _StepProfiler:
    """Tiny wrapper around cProfile that:
       • only profiles when enabled
       • dumps stats periodically (every dump_period_s) so you can inspect
         a long-running process without stopping it
       • dumps stats on teardown via flush()

    The cProfile.Profile is per-process — never share across spawn boundaries.
    Cost when enabled: ~5–15% CPU. Always-off in production.
    """
    def __init__(self,
                 dump_dir: Path,
                 process_name: str,
                 dump_period_s: float = 60.0):
        self.dump_dir = Path(dump_dir)
        self.dump_dir.mkdir(parents=True, exist_ok=True)
        self.process_name = process_name
        self.dump_period_s = dump_period_s
        self._profiler = None
        self._last_dump_t = 0.0
        self._enabled = False

    def enable(self) -> None:
        import cProfile
        if self._profiler is not None:
            return
        self._profiler = cProfile.Profile()
        self._profiler.enable()
        self._last_dump_t = time.monotonic()
        self._enabled = True

    def disable_and_flush(self) -> Optional[Path]:
        if self._profiler is None:
            return None
        self._profiler.disable()
        path = self._dump()
        self._profiler = None
        self._enabled = False
        return path

    def maybe_dump(self) -> Optional[Path]:
        """Call from the main loop occasionally — emits a stats file
        every dump_period_s without disabling profiling."""
        if not self._enabled:
            return None
        now = time.monotonic()
        if (now - self._last_dump_t) < self.dump_period_s:
            return None
        # cProfile.Profile.dump_stats works while still running.
        path = self._dump()
        self._last_dump_t = now
        return path

    def _dump(self) -> Path:
        ts = time.strftime("%Y%m%d_%H%M%S")
        path = self.dump_dir / f"{self.process_name}_{ts}.pstats"
        self._profiler.dump_stats(str(path))
        return path


# ╔═══════════════════════════════════════════════════════════════╗
# ║  HangDetector — "step latency above N seconds → thread dump"  ║
# ╚═══════════════════════════════════════════════════════════════╝

class HangDetector:
    """Watchdog that triggers a state dump if step() takes too long.

    Wired by BaseProcess: before step(), call arm(timeout); after, call
    disarm(). If the timer fires before disarm, _write_crash() is called
    with reason='hang_<process>'.

    Implementation: a single threading.Timer per arm/disarm cycle. The
    cost when not firing is one thread create+cancel per step (~10 µs).
    For very tight loops (>100 Hz) this overhead matters — adapt with
    arm_every_n=1 default = arm every step.
    """
    def __init__(self,
                 process_name: str,
                 timeout_s: float = 5.0):
        self.process_name = process_name
        self.timeout_s = timeout_s
        self._timer: Optional[threading.Timer] = None

    def arm(self) -> None:
        if self.timeout_s <= 0:
            return
        self._timer = threading.Timer(self.timeout_s, self._on_timeout)
        self._timer.daemon = True
        self._timer.start()

    def disarm(self) -> None:
        if self._timer is not None:
            self._timer.cancel()
            self._timer = None

    def _on_timeout(self) -> None:
        # Synthesize an exception so we get a useful crash dump
        try:
            raise TimeoutError(
                f"step() exceeded {self.timeout_s:.1f}s — possible hang")
        except TimeoutError as e:
            _write_crash(f"hang_{self.process_name}",
                         exc=e,
                         process_name=self.process_name)


# ╔═══════════════════════════════════════════════════════════════╗
# ║  Schema validation — runtime type check for published msgs    ║
# ╚═══════════════════════════════════════════════════════════════╝

# Imports are deliberately deferred to this section so they sit next to the
# code that uses them (the schema-check helpers below). E402 noqa.
import dataclasses  # noqa: E402
import typing as _typing  # noqa: E402

_SCHEMA_CHECK = False
_SCHEMA_FIELDS_CACHE: Dict[type, list] = {}


def set_schema_check(enabled: bool) -> None:
    """Toggle dataclass-field type validation on publish().

    When enabled (debug mode only), every msg passed to ipc.publish() is
    checked: each dataclass field must match its annotated type. Type
    errors raise RuntimeError, halting the offending step() loop and
    triggering a crash dump.

    Off by default — production cost would be too high (every step pays
    a getattr × n_fields).
    """
    global _SCHEMA_CHECK
    _SCHEMA_CHECK = enabled


def schema_check_enabled() -> bool:
    return _SCHEMA_CHECK


def _check_one(value: Any, expected: Any) -> bool:
    """Loose isinstance check that handles common typing forms."""
    # Plain class
    if isinstance(expected, type):
        return isinstance(value, expected)
    # typing.Optional[X], Union[X, Y]
    origin = _typing.get_origin(expected)
    args = _typing.get_args(expected)
    if origin is _typing.Union or str(origin) == "types.UnionType":
        return any(_check_one(value, a) for a in args)
    # typing.List / typing.Dict / etc — check container, not contents
    if origin in (list, tuple, dict, set, frozenset):
        return isinstance(value, origin)
    # numpy array — match by name (avoid hard import)
    expected_name = getattr(expected, "__name__", "")
    if expected_name in ("ndarray",):
        try:
            import numpy as np
            return isinstance(value, np.ndarray)
        except ImportError:
            return True
    # Fallback: trust the call site
    return True


def validate_message(msg: Any) -> None:
    """Raise RuntimeError if `msg` doesn't match its dataclass annotations.

    Cheap: caches dataclasses.fields() per type. Skipped silently for
    non-dataclass values (numpy arrays, primitives — those go through
    other queues).
    """
    if not _SCHEMA_CHECK:
        return
    cls = type(msg)
    if not dataclasses.is_dataclass(cls):
        return
    fields = _SCHEMA_FIELDS_CACHE.get(cls)
    if fields is None:
        # Resolve string annotations once (handles `from __future__`)
        try:
            hints = _typing.get_type_hints(cls)
        except Exception:
            hints = {}
        fields = [(f.name, hints.get(f.name, f.type))
                  for f in dataclasses.fields(cls)]
        _SCHEMA_FIELDS_CACHE[cls] = fields
    for name, expected in fields:
        try:
            value = getattr(msg, name)
        except AttributeError:
            raise RuntimeError(
                f"schema_check: {cls.__name__}.{name} is missing") from None
        if not _check_one(value, expected):
            raise RuntimeError(
                f"schema_check: {cls.__name__}.{name} expected {expected!r}, "
                f"got {type(value).__name__} (value={value!r})")


# ╔═══════════════════════════════════════════════════════════════╗
# ║  Message tracer (cheap when disabled)                          ║
# ╚═══════════════════════════════════════════════════════════════╝

# Default off — flip with set_tracing(True) at startup
_TRACE_ENABLED = False
_TRACE_BUFFER: Deque[Dict[str, Any]] = deque(maxlen=2000)


def set_tracing(enabled: bool) -> None:
    global _TRACE_ENABLED
    _TRACE_ENABLED = enabled


def trace(event: str, **fields: Any) -> None:
    """Record a message-flow event. Zero-cost when disabled.

    Common patterns:
        trace("rtk_publish", seq=msg.header.seq, q=fix_quality)
        trace("loc_pose",    source=src, sigma=sigma)
    """
    if not _TRACE_ENABLED:
        return
    _TRACE_BUFFER.append({
        "t": time.monotonic(),
        "ev": event,
        **fields,
    })


def get_trace_buffer() -> List[Dict[str, Any]]:
    return list(_TRACE_BUFFER)


# ╔═══════════════════════════════════════════════════════════════╗
# ║  CLI: `python -m core.diag dump <pid>`                        ║
# ╚═══════════════════════════════════════════════════════════════╝

def _main() -> int:
    import argparse
    p = argparse.ArgumentParser(description="Diagnostics CLI")
    sub = p.add_subparsers(dest="cmd")

    d = sub.add_parser("dump", help="Send SIGUSR1 to a running patrol PID")
    d.add_argument("pid", type=int)

    args = p.parse_args()
    if args.cmd == "dump":
        try:
            os.kill(args.pid, signal.SIGUSR1)
            print(f"Sent SIGUSR1 to PID {args.pid}; check /var/log/patrol/crashes/")
            return 0
        except ProcessLookupError:
            print(f"No such process: {args.pid}", file=sys.stderr)
            return 1
        except PermissionError:
            print("Permission denied (try sudo)", file=sys.stderr)
            return 1
    p.print_help()
    return 1


if __name__ == "__main__":
    sys.exit(_main())
