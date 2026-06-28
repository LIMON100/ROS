"""
Tests for NV12Pool — refcounted zero-copy slot pool.

Coverage:
  • single-consumer happy path (acquire → publish → consume → release)
  • multiple consumers see the same frame independently
  • free count drops only when ALL consumers released
  • slow consumer drop-oldest policy
  • detach releases pending refs
  • thread safety under concurrent producers/consumers
"""
from __future__ import annotations

import threading
import time

import numpy as np
import pytest

from streaming.nv12_pool import NV12Pool


@pytest.fixture
def pool():
    p = NV12Pool(num_slots=4, width=320, height=180)
    yield p
    p.close()


def test_array_shape_is_nv12(pool):
    slot = pool.acquire_for_write(timeout=0.1)
    assert slot is not None
    # NV12: H*3/2 rows × W cols
    assert slot.array.shape == (180 * 3 // 2, 320)
    assert slot.array.dtype == np.uint8


def test_publish_without_consumers_keeps_slot_free(pool):
    slot = pool.acquire_for_write()
    pool.publish(slot)
    # No consumers attached → no refs → slot still free
    assert pool.free_count() == 4


def test_single_consumer_round_trip(pool):
    pool.attach_consumer("ai")
    s = pool.acquire_for_write()
    s.array[0, 0] = 42
    pool.publish(s, pts_us=12345)

    assert pool.free_count() == 3   # one slot held by consumer

    got = pool.consume("ai", timeout=0.5)
    assert got is not None
    assert got.array[0, 0] == 42
    assert got.pts_us == 12345
    assert got.seq == 1

    pool.release("ai", got)
    assert pool.free_count() == 4


def test_multiple_consumers_each_get_frame(pool):
    pool.attach_consumer("ai")
    pool.attach_consumer("stream")
    pool.attach_consumer("display")

    s = pool.acquire_for_write()
    s.array[0, 0] = 99
    pool.publish(s)

    # All three see the same slot
    a = pool.consume("ai", timeout=0.2)
    b = pool.consume("stream", timeout=0.2)
    c = pool.consume("display", timeout=0.2)
    assert a is not None and b is not None and c is not None
    assert a.slot_id == b.slot_id == c.slot_id
    assert a.array[0, 0] == b.array[0, 0] == c.array[0, 0] == 99


def test_slot_freed_only_after_all_consumers_release(pool):
    pool.attach_consumer("ai")
    pool.attach_consumer("stream")

    s = pool.acquire_for_write()
    pool.publish(s)
    assert pool.free_count() == 3

    a = pool.consume("ai")
    pool.release("ai", a)
    assert pool.free_count() == 3   # stream hasn't released yet

    b = pool.consume("stream")
    pool.release("stream", b)
    assert pool.free_count() == 4


def test_slow_consumer_drops_oldest():
    """A consumer that doesn't drain its queue must lose old frames so
    new ones can flow."""
    # Need num_slots > queue_cap (4) so the producer can keep advancing
    # while old slots are still pinned by the slow consumer's pending.
    with NV12Pool(num_slots=8, width=160, height=120) as pool:
        pool.attach_consumer("slow")
        # Publish 6 frames without consuming any (queue cap is 4)
        for i in range(6):
            s = pool.acquire_for_write(timeout=0.5)
            assert s is not None, f"acquire {i} timed out unexpectedly"
            s.array[0, 0] = i
            pool.publish(s)
        # 4 should be pending; 2 should have been dropped
        stats = pool.stats()
        assert stats["dropped_full"] >= 1
        # The two oldest frames are gone; only frames 2..5 remain
        seen = []
        while True:
            got = pool.consume("slow", timeout=0.05)
            if got is None:
                break
            seen.append(int(got.array[0, 0]))
            pool.release("slow", got)
        assert len(seen) == 4
        # Frame 0 and 1 were dropped — newest 4 survive
        assert seen == [2, 3, 4, 5]


def test_detach_consumer_releases_outstanding_refs(pool):
    pool.attach_consumer("ai")
    pool.attach_consumer("stream")

    for _ in range(2):
        s = pool.acquire_for_write()
        pool.publish(s)

    # Both consumers have 2 frames pending each → 2 slots in flight
    assert pool.free_count() == 2
    pool.detach_consumer("stream")
    # Stream's refs released → both slots only held by ai now
    # (still 2 slots in flight, but the refcount per slot dropped to 1)
    a1 = pool.consume("ai")
    pool.release("ai", a1)
    a2 = pool.consume("ai")
    pool.release("ai", a2)
    assert pool.free_count() == 4


def test_acquire_blocks_when_all_in_flight(pool):
    pool.attach_consumer("ai")
    # Fill the pool — every slot held by consumer, none free
    for _ in range(4):
        s = pool.acquire_for_write(timeout=0.1)
        pool.publish(s)
    # Consumer's queue cap is 4; all 4 slots are now refcounted
    assert pool.free_count() == 0
    # Next acquire should time out
    blocked = pool.acquire_for_write(timeout=0.2)
    assert blocked is None


def test_concurrent_producer_and_consumer():
    """Stress: 1 producer + 2 consumers running concurrently. Verify no
    races, no negative refcounts, the producer makes forward progress, and
    consumers eventually drain everything that wasn't dropped."""
    with NV12Pool(num_slots=8, width=160, height=120) as pool:
        pool.attach_consumer("ai")
        pool.attach_consumer("stream")

        N = 100
        produced = 0
        consumed = {"ai": 0, "stream": 0}
        stop = threading.Event()

        def producer():
            nonlocal produced
            for _ in range(N):
                s = pool.acquire_for_write(timeout=1.0)
                if s is None:
                    break
                pool.publish(s)
                produced += 1
                # Tiny breathing room so consumers can keep up
                time.sleep(0.0005)
            stop.set()

        def consumer(name):
            while not stop.is_set() or pool.free_count() < 8:
                got = pool.consume(name, timeout=0.05)
                if got is None:
                    if stop.is_set():
                        break
                    continue
                pool.release(name, got)
                consumed[name] += 1

        prod = threading.Thread(target=producer)
        cons1 = threading.Thread(target=consumer, args=("ai",))
        cons2 = threading.Thread(target=consumer, args=("stream",))
        for t in (prod, cons1, cons2):
            t.start()
        for t in (prod, cons1, cons2):
            t.join(timeout=10)

        # Producer succeeded — no permanent stall
        assert produced >= N - 5
        # Each consumer got at least some frames; the rest are documented
        # as drops in stats. Drops + consumed ≈ produced for each consumer.
        stats = pool.stats()
        for name in ("ai", "stream"):
            assert consumed[name] > 0, f"{name} got nothing"
        assert stats["consumed_total"] >= 2
        # All slots back in the free pool — no refcount leak
        assert pool.free_count() == 8
