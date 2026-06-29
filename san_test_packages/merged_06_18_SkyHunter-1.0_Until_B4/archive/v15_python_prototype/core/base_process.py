"""
BaseProcess: standard lifecycle for all worker processes.

RK3588 optimizations applied:
  • CPU affinity (big.LITTLE aware): pin RT-critical processes to A76 (cores 4-7)
  • RT scheduling (SCHED_FIFO) optional for time-sensitive loops
  • Graceful shutdown via shared mp.Event
  • Threading objects created in run() (spawn pickle-safe)

Threading inside processes:
  • spawn_thread() helper — auto-join on shutdown
  • is_running() — check both shutdown_event and per-process thread stop
"""
from __future__ import annotations

import logging
import multiprocessing as mp
import os
import signal
import threading
import time
from abc import ABC, abstractmethod
from pathlib import Path
from typing import Any, Callable, List, Optional

from core.diag import (
    HangDetector,
    MetricsCollector,
    _StepProfiler,
    install_crash_handler,
    set_correlation_id,
    set_schema_check,
    setup_logger,
)


class BaseProcess(mp.Process, ABC):
    """
    Args:
        name: process display name
        shutdown_event: parent-set event → all children exit
        rate_hz: step() loop rate (Hz)
        cpu_affinity: list of CPU core indices to pin to (RK3588: 0-7)
                      Recommended:
                        A76 cores (4-7): RT, compute-heavy
                        A55 cores (0-3): I/O, comm, logging
        rt_priority: SCHED_FIFO priority [1-99], 0 = no RT
                     Use sparingly — only for hard-real-time loops.
    """

    def __init__(
        self,
        name: str,
        shutdown_event: mp.Event,
        rate_hz: float = 10.0,
        cpu_affinity: Optional[List[int]] = None,
        rt_priority: int = 0,
        metrics_dict: Optional[Any] = None,    # mp.Manager().dict() shared by all
        log_dir: Optional[str] = None,
        log_json: bool = False,
        crash_dir: Optional[str] = None,
        # ── Debug-mode parameters (default off → zero overhead) ──
        profile: bool = False,
        profile_dir: Optional[str] = None,
        profile_dump_period_s: float = 60.0,
        hang_timeout_s: float = 0.0,           # 0 = disabled
        schema_check: bool = False,
    ):
        super().__init__(name=name, daemon=False)
        self.shutdown_event = shutdown_event
        self.rate_hz = rate_hz
        self.cpu_affinity = cpu_affinity
        self.rt_priority = rt_priority
        self.log: Optional[logging.Logger] = None
        self._threads: List[threading.Thread] = []
        self._thread_stop: Optional[threading.Event] = None
        # Diagnostics — metrics dict / log dir / crash dir captured at __init__
        # so they survive pickle for spawn (only str/path/Manager-proxy types).
        self._metrics_dict = metrics_dict
        self._log_dir = Path(log_dir) if log_dir else None
        self._log_json = log_json
        self._crash_dir = Path(crash_dir) if crash_dir else None
        self.metrics: Optional[MetricsCollector] = None
        # Debug
        self._profile_enabled = profile
        self._profile_dir = Path(profile_dir) if profile_dir else None
        self._profile_dump_period_s = profile_dump_period_s
        self._hang_timeout_s = hang_timeout_s
        self._schema_check = schema_check
        self._profiler: Optional[_StepProfiler] = None
        self._hang_detector: Optional[HangDetector] = None

    # ─── Subclass implements ───
    @abstractmethod
    def setup(self) -> None: ...

    @abstractmethod
    def step(self) -> None: ...

    def teardown(self) -> None:
        pass

    # ─── Helpers ───
    def spawn_thread(self, target: Callable, name: str, args: tuple = ()) -> threading.Thread:
        # Wrap target so any unhandled exception inside the thread is captured
        # in metrics + recorded as a thread-level crash dump.
        wrapped = self._instrument_thread(target, name)
        t = threading.Thread(target=wrapped, name=name, args=args, daemon=True)
        t.start()
        self._threads.append(t)
        if self.log:
            self.log.info(f"thread '{name}' started")
        return t

    def _instrument_thread(self, target: Callable, name: str) -> Callable:
        def run_thread(*args):
            try:
                # Heartbeat thread liveness once per second from inside long
                # consumers. Most threads have a ~0.1s consume loop already,
                # so we just rely on threading.excepthook to capture failures.
                target(*args)
            except Exception as e:        # pylint: disable=broad-except
                if self.metrics:
                    self.metrics.record_exception(e)
                if self.log:
                    self.log.exception(f"thread '{name}' crashed: {e}")
                # Re-raise so threading.excepthook → diag.crash_handler runs
                raise
        return run_thread

    def is_running(self) -> bool:
        if self._thread_stop is None:
            return not self.shutdown_event.is_set()
        return not (self._thread_stop.is_set() or self.shutdown_event.is_set())

    def _apply_cpu_affinity(self) -> None:
        """Pin process to specific cores. RK3588: A76=4..7, A55=0..3."""
        if self.cpu_affinity:
            try:
                os.sched_setaffinity(0, set(self.cpu_affinity))
                self.log.info(f"CPU affinity set to {self.cpu_affinity}")
            except (AttributeError, PermissionError, OSError) as e:
                self.log.warning(f"CPU affinity failed: {e}")

    def _apply_rt_priority(self) -> None:
        """Set SCHED_FIFO priority. Requires CAP_SYS_NICE or root."""
        if self.rt_priority > 0:
            try:
                param = os.sched_param(self.rt_priority)
                os.sched_setscheduler(0, os.SCHED_FIFO, param)
                self.log.info(f"RT priority {self.rt_priority} (SCHED_FIFO) set")
            except (AttributeError, PermissionError, OSError) as e:
                self.log.warning(f"RT priority failed: {e}")

    # ─── Main loop ───
    def run(self) -> None:
        signal.signal(signal.SIGINT, signal.SIG_IGN)
        self._thread_stop = threading.Event()

        # Logger first — every other facility wants to log
        self.log = setup_logger(
            self.name,
            log_dir=self._log_dir,
            json_format=self._log_json,
        )
        # Correlation: one ID per (process, pid) — included in every log line
        cid = f"{self.name}:{os.getpid()}"
        set_correlation_id(cid)

        # Crash handler — exceptions, hangs, SIGUSR1 → JSON dump
        if self._crash_dir is not None:
            install_crash_handler(self._crash_dir, logger=self.log)

        # Metrics collector — published to shared dict every ~1s
        self.metrics = MetricsCollector(self.name,
                                        shared_dict=self._metrics_dict)

        # Optional: cProfile profiler with periodic stat dumps
        if self._profile_enabled and self._profile_dir is not None:
            self._profiler = _StepProfiler(
                dump_dir=self._profile_dir,
                process_name=self.name,
                dump_period_s=self._profile_dump_period_s,
            )
            self._profiler.enable()
            self.log.info(
                f"profiler enabled → {self._profile_dir} "
                f"every {self._profile_dump_period_s:.0f}s")

        # Optional: hang detector — step exceeding timeout → crash dump
        if self._hang_timeout_s > 0.0 and self._crash_dir is not None:
            self._hang_detector = HangDetector(
                self.name, timeout_s=self._hang_timeout_s)
            self.log.info(
                f"hang detector armed at {self._hang_timeout_s:.1f}s")

        # Optional: dataclass schema validation on every publish
        if self._schema_check:
            set_schema_check(True)
            self.log.info("schema_check ON — every publish() validates types")

        self._apply_cpu_affinity()
        self._apply_rt_priority()
        self.log.info(f"process starting (pid={os.getpid()}, cid={cid})")

        try:
            self.setup()
            period = 1.0 / self.rate_hz
            next_t = time.monotonic()
            while not self.shutdown_event.is_set():
                if self._hang_detector is not None:
                    self._hang_detector.arm()
                self.metrics.step_begin()
                try:
                    self.step()
                except Exception as e:        # pylint: disable=broad-except
                    self.metrics.record_exception(e)
                    self.log.exception(f"step() exception: {e}")
                    # Keep looping — one bad step shouldn't kill the process
                self.metrics.step_end()
                if self._hang_detector is not None:
                    self._hang_detector.disarm()
                if self._profiler is not None:
                    dumped = self._profiler.maybe_dump()
                    if dumped is not None:
                        self.log.info(f"profiler dumped → {dumped}")
                next_t += period
                sleep = next_t - time.monotonic()
                if sleep > 0:
                    time.sleep(sleep)
                else:
                    next_t = time.monotonic()
        except Exception as e:        # pylint: disable=broad-except
            # Setup failed, or step() raised in a way we couldn't recover from
            if self.metrics:
                self.metrics.record_exception(e)
            if self.log:
                self.log.exception(f"unhandled exception in run(): {e}")
        finally:
            if self._thread_stop is not None:
                self._thread_stop.set()
            for t in self._threads:
                t.join(timeout=2.0)
            try:
                self.teardown()
            except Exception as e:        # pylint: disable=broad-except
                if self.log:
                    self.log.error(f"teardown error: {e}")
            # Flush profile if active
            if self._profiler is not None:
                final_dump = self._profiler.disable_and_flush()
                if final_dump and self.log:
                    self.log.info(f"profiler final dump → {final_dump}")
            if self.log:
                final = self.metrics.snapshot() if self.metrics else None
                if final:
                    self.log.info(
                        f"process stopped — steps={final.step_count} "
                        f"exceptions={final.exception_count} "
                        f"p95_ms={final.percentile(95):.2f}"
                    )
                else:
                    self.log.info("process stopped")
