"""
Tests for camera 3-way fan-out: MultiConsumerShmPool + IMX678Adapter.

The adapter tests inline their own pools (rather than using the shared
fixture) because the adapter spawns a background listener thread, and
fixture-order interactions with other multiprocessing tests can race
on the small Manager-backed dict the pool uses.
"""
from __future__ import annotations

import multiprocessing as mp
import os
import threading
import time
import uuid
from unittest.mock import MagicMock

import pytest

from adapters.payload_sensors import IMX678Adapter
from core import make_topic_queues
from core.ipc import consume, publish
from core.shm_pool import MultiConsumerShmPool


def _inline_pool(slot_bytes: int = 1024, n_slots: int = 4):
    """Fresh pool with a unique POSIX name. Caller is responsible for teardown."""
    suffix = f"{os.getpid()}_{uuid.uuid4().hex[:8]}"
    p = MultiConsumerShmPool(f"tinl_{suffix}", slot_bytes=slot_bytes,
                              n_slots=n_slots, slot_timeout_s=0.5)
    p.setup()
    return p


@pytest.fixture
def pool():
    p = _inline_pool()
    yield p
    p.teardown()


# ════════════════════════════════════════════════════════════════
# MultiConsumerShmPool
# ════════════════════════════════════════════════════════════════
def test_publish_consumes_one_slot(pool):
    free_before = pool.free_slots()
    name = pool.publish(b"hello world", n_consumers=2)
    assert name is not None
    assert pool.free_slots() == free_before - 1


def test_read_returns_published_payload(pool):
    name = pool.publish(b"hello", n_consumers=1)
    data = pool.read(name, nbytes=5)
    assert data == b"hello"


def test_release_decrements_refcount(pool):
    name = pool.publish(b"x", n_consumers=3)
    assert pool.release(name) == 2
    assert pool.release(name) == 1
    assert pool.release(name) == 0


def test_last_release_returns_slot_to_pool(pool):
    free_before = pool.free_slots()
    name = pool.publish(b"x", n_consumers=2)
    assert pool.free_slots() == free_before - 1
    pool.release(name)
    assert pool.free_slots() == free_before - 1
    pool.release(name)
    assert pool.free_slots() == free_before


def test_pool_exhaustion_returns_none(pool):
    names = [pool.publish(b"x", n_consumers=1) for _ in range(4)]
    assert all(n is not None for n in names)
    assert pool.publish(b"x", n_consumers=1) is None
    pool.release(names[0])
    assert pool.publish(b"x", n_consumers=1) is not None


def test_reap_orphans_force_releases_stale_slots(pool):
    pool.publish(b"x", n_consumers=5)
    free_before = pool.free_slots()
    time.sleep(0.6)
    forced = pool.reap_orphans()
    assert forced == 1
    assert pool.free_slots() == free_before + 1


def test_release_unknown_name_is_safe(pool):
    pool.release("nonexistent_slot_name")
    n = pool.publish(b"x", n_consumers=1)
    assert n is not None


# ════════════════════════════════════════════════════════════════
# IMX678Adapter — fan-out paths
# ════════════════════════════════════════════════════════════════
def _make_adapter(fanout_pool=None):
    queues = make_topic_queues()
    cfg = MagicMock()
    cfg.get.side_effect = lambda *k, default=None: {
        ("imx678", "fps"):     30.0,
        ("imx678", "device"):  "/nonexistent",
        ("system", "cpu_affinity", "imx678"): None,
        ("system", "dev_display"):  False,
    }.get(tuple(k), default)
    cam_shm = MagicMock()
    cam_shm.write.return_value = "legacy_slot"

    a = IMX678Adapter.__new__(IMX678Adapter)
    a.queues = queues
    a.shutdown_event = mp.Event()
    a.cfg = cfg
    a.shm = cam_shm
    a.fanout_pool = fanout_pool
    a._stub = False
    a._seq = 0
    a._lock = threading.Lock()
    a._subscribers = {"ai"}
    a.log = MagicMock()

    # Track every spawned thread so the test can stop them before the
    # pool's Manager subprocess is torn down. Without this, the SHM
    # reaper daemon keeps ticking after pool.teardown(); its next call
    # to fanout_pool.reap_orphans() goes through a Manager proxy whose
    # pipe is already closed, raising BrokenPipeError on Windows. The
    # logged "reaper error: [Errno 32]/[Errno 2]" lines and the cross-
    # test slot-count drift are downstream effects of leaked daemons.
    a._spawned: list = []

    def _spawn(target, name):
        t = threading.Thread(target=target, name=name, daemon=True)
        a._spawned.append(t)
        t.start()
        return t

    a.spawn_thread = _spawn
    a.is_running = lambda: not a.shutdown_event.is_set()
    a.setup()
    time.sleep(0.2)        # listener thread start
    return a, queues


def _stop_adapter(a) -> None:
    """Set shutdown_event and join every spawned thread. MUST be called
    before pool.teardown() so the reaper exits cleanly while the
    Manager is still alive."""
    if a is None:
        return
    a.shutdown_event.set()
    for t in getattr(a, "_spawned", []):
        t.join(timeout=2.0)


def test_legacy_path_publishes_to_imx678_ref():
    a, queues = _make_adapter(fanout_pool=None)
    try:
        a.step()
        msg = consume(queues.imx678_ref, timeout=0.5)
        assert msg is not None
        assert msg.shm_name == "legacy_slot"
    finally:
        _stop_adapter(a)


def test_fanout_default_publishes_only_to_ai():
    pool = _inline_pool()
    a = None
    try:
        a, queues = _make_adapter(fanout_pool=pool)
        a.step()
        ai = consume(queues.camera_ai_ref, timeout=0.5)
        assert ai is not None
        assert consume(queues.camera_stream_ref, timeout=0.0) is None
        assert consume(queues.camera_display_ref, timeout=0.0) is None
    finally:
        _stop_adapter(a)
        pool.teardown()


def test_subscriber_add_stream_extends_fanout():
    pool = _inline_pool()
    a = None
    try:
        a, queues = _make_adapter(fanout_pool=pool)
        publish(queues.camera_subscribers,
                {"action": "add", "consumer": "stream"})
        time.sleep(0.5)
        a.step()
        ai     = consume(queues.camera_ai_ref,     timeout=0.5)
        stream = consume(queues.camera_stream_ref, timeout=0.5)
        assert ai is not None
        assert stream is not None
        assert ai.shm_name == stream.shm_name
        assert consume(queues.camera_display_ref, timeout=0.0) is None
    finally:
        _stop_adapter(a)
        pool.teardown()


def test_subscriber_remove_stream_returns_to_ai_only():
    pool = _inline_pool()
    a = None
    try:
        a, queues = _make_adapter(fanout_pool=pool)
        publish(queues.camera_subscribers,
                {"action": "add", "consumer": "stream"})
        time.sleep(0.5)
        publish(queues.camera_subscribers,
                {"action": "remove", "consumer": "stream"})
        time.sleep(0.5)
        a.step()
        assert consume(queues.camera_ai_ref,     timeout=0.5) is not None
        assert consume(queues.camera_stream_ref, timeout=0.0) is None
    finally:
        _stop_adapter(a)
        pool.teardown()


def test_fanout_with_no_subscribers_drops_frame():
    pool = _inline_pool()
    a = None
    try:
        a, queues = _make_adapter(fanout_pool=pool)
        with a._lock:
            a._subscribers.clear()
        free_before = pool.free_slots()
        a.step()
        assert pool.free_slots() == free_before
        assert consume(queues.camera_ai_ref, timeout=0.0) is None
    finally:
        _stop_adapter(a)
        pool.teardown()


def test_fanout_three_way_each_consumer_releases():
    pool = _inline_pool()
    a = None
    try:
        a, queues = _make_adapter(fanout_pool=pool)
        for who in ("stream", "display"):
            publish(queues.camera_subscribers,
                    {"action": "add", "consumer": who})
        time.sleep(0.5)

        free_before = pool.free_slots()
        a.step()
        assert pool.free_slots() == free_before - 1

        refs = [
            consume(queues.camera_ai_ref,      timeout=0.5),
            consume(queues.camera_stream_ref,  timeout=0.5),
            consume(queues.camera_display_ref, timeout=0.5),
        ]
        assert all(r is not None for r in refs)
        # All consumers see the SAME shm_name — zero-copy fan-out.
        assert refs[0].shm_name == refs[1].shm_name == refs[2].shm_name

        for r in refs:
            data = pool.read(r.shm_name, r.nbytes)
            assert data is not None
            pool.release(r.shm_name)
        assert pool.free_slots() == free_before
    finally:
        _stop_adapter(a)
        pool.teardown()
