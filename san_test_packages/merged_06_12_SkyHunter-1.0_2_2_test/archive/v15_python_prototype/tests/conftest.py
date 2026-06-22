"""pytest config — make repo importable from anywhere."""
import sys
import threading
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))


# ────────────────────────────────────────────────────────────────────
# Multiprocessing-test isolation
# ────────────────────────────────────────────────────────────────────
# Two failure modes manifest after the test suite finishes:
#
# 1. Stale POSIX SHM segments
#    /dev/shm/* names persist past Python object lifetimes. Without an
#    explicit unlink, the next test's create=True hits FileExistsError
#    and adopts a stale segment of the wrong size.
#
# 2. Leaked daemon consumer threads (e.g. "ImxSubs", "WsLoop")
#    Several tests build adapters via __new__ + a fake spawn_thread that
#    just starts a daemon thread, but never sets the shutdown_event —
#    so the consumer loop (consume() on an mp.Queue) stays live until
#    interpreter shutdown. CPython then races to tear down the queue's
#    background feeder thread against the BufferedWriter on stderr,
#    producing:
#       Fatal Python error: _enter_buffered_busy: could not acquire
#       lock for <_io.BufferedWriter name='<stderr>'> at interpreter
#       shutdown, possibly due to daemon threads
#    The error doesn't fail the suite (357 passed) but it spams the CI
#    log and inflates exit code on some runners.
def pytest_sessionfinish(session, exitstatus):
    import os
    # ── 1. Sweep stale SHM segments ──
    shm_dir = "/dev/shm"
    if os.path.isdir(shm_dir):
        swept = 0
        for name in os.listdir(shm_dir):
            if name.startswith(("tinl_", "tfan_", "test_fanout_",
                                 "patrol_test_", "sm_test_")):
                try:
                    os.unlink(os.path.join(shm_dir, name))
                    swept += 1
                except OSError:
                    pass
        if swept:
            print(f"\n  [conftest] swept {swept} stale SHM segments")

    # ── 2. Drain leaked daemon threads ──
    # Test fixtures that build adapters via __new__ + a fake spawn_thread
    # leave consumer loops alive (e.g. ImxSubs in test_camera_fanout). The
    # consumers spin on `not shutdown_event.is_set()`. We can't reach the
    # individual fixture instances from here, so we walk the gc heap for
    # mp.Event() objects and set them all — the cost is negligible (a
    # handful of events) and any production-Event still attached to a
    # live process won't matter because the test session is shutting down.
    import gc
    import multiprocessing as _mp
    event_type = type(_mp.Event())
    n_events = 0
    for obj in gc.get_objects():
        if isinstance(obj, event_type):
            try:
                obj.set()
                n_events += 1
            except Exception:        # pylint: disable=broad-except
                pass

    # Now give the just-signalled consumer loops a moment to bail out,
    # then join with a short timeout. Stragglers that ignore events (no
    # is_running() check) are reported but not blocked.
    time.sleep(0.5)
    main = threading.main_thread()
    stragglers = []
    for t in threading.enumerate():
        if t is main or not t.daemon:
            continue
        t.join(timeout=2.0)
        if t.is_alive():
            stragglers.append(t.name)
    if stragglers:
        # One log line — the CI log stays readable.
        print(f"\n  [conftest] daemon stragglers at exit "
              f"(events_signalled={n_events}): "
              f"{len(stragglers)} ({', '.join(sorted(set(stragglers))[:8])})")


# Force gc between tests so __del__ on SharedMemory wrappers fires.
# Late imports — must come after the session-finish hook above. E402 noqa.
import gc as _gc  # noqa: E402

import pytest as _pytest  # noqa: E402


@_pytest.fixture(autouse=True, scope="function")
def _gc_after_test():
    yield
    _gc.collect()


def pytest_configure(config):
    """Register custom markers used by tests/kpp/* (P2-13)."""
    config.addinivalue_line(
        "markers",
        "kpp: KPP §2.1.1 verification test (formation, avoidance, latency, "
        "reconfiguration, assembly success).")
