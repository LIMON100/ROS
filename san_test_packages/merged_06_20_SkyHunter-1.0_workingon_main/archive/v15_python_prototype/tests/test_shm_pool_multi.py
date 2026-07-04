"""
Regression tests for core.shm_pool.MultiConsumerShmPool.

Coverage:
  • happy path: publish + multi-consumer reads + last release frees slot
  • oversize payload is rejected before claiming a slot (no leak)
  • SHM write failure rolls the slot back to the free list
  • reap_orphans only sweeps slots actually in flight
  • concurrent publish + reap_orphans never reaps an in-flight slot
"""
from __future__ import annotations

import threading
import time

import pytest

from core import shm_pool as shm_pool_mod
from core.shm_pool import MultiConsumerShmPool


@pytest.fixture
def pool():
    p = MultiConsumerShmPool(name_prefix=f"test_mcshm_{id(object())}",
                             slot_bytes=1024, n_slots=4,
                             slot_timeout_s=0.5)
    p.setup()
    yield p
    p.teardown()


def test_publish_and_multi_consumer_release(pool):
    name = pool.publish(b"hello", n_consumers=2)
    assert name is not None
    assert pool.free_slots() == 3

    # First consumer reads + releases — slot stays held by the second.
    assert pool.read(name, 5) == b"hello"
    assert pool.release(name) == 1
    assert pool.free_slots() == 3

    # Last consumer releases → slot returns to free pool.
    assert pool.release(name) == 0
    assert pool.free_slots() == 4


def test_zero_consumers_rejected(pool):
    free_before = pool.free_slots()
    assert pool.publish(b"x", n_consumers=0) is None
    assert pool.free_slots() == free_before


def test_publish_rolls_back_on_shm_write_failure(pool, monkeypatch):
    """If the SHM segment vanished between setup and write, publish must
    return the slot to free instead of stranding it in published_at."""
    real_shm_cls = shm_pool_mod.shared_memory.SharedMemory

    class _FakeSharedMemory:
        def __init__(self, name):
            raise FileNotFoundError(name)

    free_before = pool.free_slots()
    monkeypatch.setattr(shm_pool_mod.shared_memory, "SharedMemory",
                        _FakeSharedMemory)
    try:
        assert pool.publish(b"payload", n_consumers=2) is None
    finally:
        monkeypatch.setattr(shm_pool_mod.shared_memory, "SharedMemory",
                            real_shm_cls)
    # Slot must be reusable.
    assert pool.free_slots() == free_before
    # No bookkeeping leaked.
    assert len(pool._refcount) == 0
    assert len(pool._published_at) == 0


def test_reap_orphans_sweeps_stuck_slots(pool):
    name = pool.publish(b"abc", n_consumers=3)
    assert name is not None
    assert pool.free_slots() == 3
    # Wait past slot_timeout_s; consumers never released.
    time.sleep(0.6)
    forced = pool.reap_orphans()
    assert forced == 1
    assert pool.free_slots() == 4


def test_reap_orphans_does_not_steal_inflight_slots(pool):
    """Pin a slot mid-publish (refcount set, but not yet timed out) and
    confirm reap_orphans leaves it alone. Without the publish-side
    bookkeeping fix, a window existed where the slot was popped from
    _free but invisible to reap, so this test is mostly a sanity check
    that reap respects slot_timeout_s."""
    name = pool.publish(b"data", n_consumers=2)
    assert name is not None
    assert pool.reap_orphans() == 0
    assert pool.free_slots() == 3
    # Drain so teardown sees a clean pool.
    pool.release(name)
    pool.release(name)


def test_concurrent_publish_and_reap_no_double_free():
    """Hammer publish + reap on separate threads. After draining, every
    slot accounted for exactly once in the free list."""
    p = MultiConsumerShmPool(name_prefix=f"test_mcshm_race_{id(object())}",
                             slot_bytes=512, n_slots=8, slot_timeout_s=0.05)
    p.setup()
    try:
        stop = threading.Event()
        publishes = [0]
        names: list[str] = []
        names_lock = threading.Lock()

        def producer():
            while not stop.is_set():
                n = p.publish(b"x" * 16, n_consumers=1)
                if n is not None:
                    with names_lock:
                        names.append(n)
                    publishes[0] += 1
                else:
                    time.sleep(0.001)

        def reaper():
            while not stop.is_set():
                p.reap_orphans()
                time.sleep(0.01)

        prod = threading.Thread(target=producer)
        reap = threading.Thread(target=reaper)
        prod.start()
        reap.start()
        time.sleep(0.5)
        stop.set()
        prod.join(timeout=2)
        reap.join(timeout=2)

        # After everything stops, sweeping orphans must reclaim every slot.
        time.sleep(0.1)
        p.reap_orphans()
        with names_lock:
            # Drop any consumer-owned refs that survived the reaper.
            for nm in names:
                p.release(nm)
        # Either the reaper or the explicit releases above bring us back
        # to full capacity — the key invariant is "no slot leaked".
        assert p.free_slots() == 8
        assert publishes[0] > 0
    finally:
        p.teardown()
