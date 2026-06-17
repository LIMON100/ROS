"""
NV12Pool — refcounted slot pool for zero-copy camera frame fan-out.

Adapted from AIRYS v6 nv12_pool_board.cpp. The pool is a fixed-size ring
of pre-allocated NV12 buffers; the producer (V4L2 capture) writes into a
free slot and bumps refcount once per consumer. Each consumer release
drops refcount; when it hits zero the slot is reusable.

Why this matters: with 3 consumers (AI, Stream, Display) and naive
queues we'd be copying every frame three times (~6 MB/frame ×
30 fps × 3 = 540 MB/s of pointless memcpy + L2 cache thrash). With
refcounted slots the frame is written once and pointer-shared.

In a pure-Python deployment this pool is implemented over multiprocessing
SharedMemory (numpy view, no copy). In a real RK3588 BSP build, the same
slot would carry a dma_fd which mpph265enc + RGA + RKNN can import
zero-copy. The slot abstraction is identical; the buffer source differs
by deployment target.

Lifecycle:
    pool = NV12Pool(num_slots=12, width=1920, height=1080)
    pool.attach_consumer("ai")
    pool.attach_consumer("stream")
    pool.attach_consumer("display")

    # Producer side
    slot = pool.acquire_for_write()      # blocks if all in flight
    fill(slot.array)                     # numpy view into shm
    pool.publish(slot)                   # bumps refcount per consumer

    # Consumer side (one thread per consumer)
    slot = pool.consume("ai", timeout=0.1)
    do_work(slot.array)
    pool.release("ai", slot)             # decrements refcount
"""
from __future__ import annotations

import threading
import time
from dataclasses import dataclass
from multiprocessing.shared_memory import SharedMemory
from typing import Dict, List, Optional

import numpy as np


@dataclass
class NV12Slot:
    """A single refcounted buffer slot.

    array: a NumPy view onto the shared-memory backing buffer.
        For NV12: shape = (H * 3 // 2, W) uint8 — Y plane (H rows) then
        UV interleaved (H/2 rows of W bytes).
    seq: monotonically increasing sequence number assigned by the producer.
    pts_us: presentation timestamp in microseconds (V4L2-supplied).
    """
    slot_id:    int
    seq:        int
    pts_us:     int
    width:      int
    height:     int
    array:      np.ndarray              # (H*3/2, W) uint8 view


class NV12Pool:
    """Thread-safe N-slot NV12 buffer pool with per-consumer refcounts."""

    def __init__(self, num_slots: int = 12,
                 width: int = 1920, height: int = 1080,
                 *, name_prefix: str = "patrol_nv12"):
        if num_slots < 2:
            raise ValueError("need >=2 slots so producer can advance")
        self.num_slots = num_slots
        self.width = width
        self.height = height
        self.frame_bytes = width * (height * 3 // 2)   # NV12 YUV 4:2:0

        # SharedMemory makes the buffers visible across processes (V4L2 +
        # consumers may live in different processes). For single-process
        # tests we just use one big SharedMemory and slice into it.
        # UUID-based name avoids /dev/shm/ collisions across pytest sessions.
        import uuid
        unique = uuid.uuid4().hex[:12]
        total = self.frame_bytes * num_slots
        self._shm = SharedMemory(create=True, size=total,
                                  name=f"{name_prefix}_{unique}")
        self._slots: List[NV12Slot] = []
        for i in range(num_slots):
            offset = i * self.frame_bytes
            view = np.ndarray(
                shape=(height * 3 // 2, width),
                dtype=np.uint8,
                buffer=self._shm.buf,
                offset=offset,
            )
            self._slots.append(NV12Slot(
                slot_id=i, seq=0, pts_us=0,
                width=width, height=height, array=view,
            ))

        # Refcount per slot, indexed by consumer name.
        # 0 = free, >0 = in use by N consumers.
        self._lock = threading.Lock()
        self._cond = threading.Condition(self._lock)
        self._refcount: List[int] = [0] * num_slots
        self._consumers: List[str] = []
        # Per-consumer pending queue (slot_id, seq) — FIFO; bounded so a
        # slow consumer can't pin every slot.
        self._pending: Dict[str, List[int]] = {}
        self._max_pending: Dict[str, int] = {}
        self._next_seq = 1

        # Stats — useful for diagnostics + dashboard
        self._stats = {
            "produced": 0,
            "consumed_total": 0,
            "dropped_full": 0,
            "released_total": 0,
        }

    # ───────── Consumer management ─────────
    def attach_consumer(self, name: str, *, max_pending: int = 4) -> None:
        with self._lock:
            if name in self._consumers:
                raise ValueError(f"consumer {name!r} already attached")
            self._consumers.append(name)
            self._pending[name] = []
            self._max_pending[name] = max_pending

    def detach_consumer(self, name: str) -> None:
        """Drop a consumer; release its outstanding refs."""
        with self._lock:
            if name not in self._consumers:
                return
            for slot_id in self._pending[name]:
                self._refcount[slot_id] -= 1
            self._consumers.remove(name)
            self._pending.pop(name, None)
            self._max_pending.pop(name, None)
            self._cond.notify_all()

    # ───────── Producer side ─────────
    def acquire_for_write(self, timeout: float = 0.5) -> Optional[NV12Slot]:
        """Block until a slot with refcount=0 is available. None on timeout."""
        deadline = time.monotonic() + timeout
        with self._lock:
            while True:
                for i, rc in enumerate(self._refcount):
                    if rc == 0:
                        return self._slots[i]
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    return None
                self._cond.wait(timeout=remaining)

    def publish(self, slot: NV12Slot, *, pts_us: int = 0) -> None:
        """Bump refcount for every attached consumer; enqueue."""
        with self._lock:
            slot.seq = self._next_seq
            slot.pts_us = pts_us
            self._next_seq += 1
            self._stats["produced"] += 1

            n = len(self._consumers)
            if n == 0:
                # Nothing to do — slot stays free
                return
            self._refcount[slot.slot_id] = n
            for name in self._consumers:
                pq = self._pending[name]
                if len(pq) >= self._max_pending[name]:
                    # Drop OLDEST for this consumer (slow consumer policy)
                    dropped = pq.pop(0)
                    self._refcount[dropped] -= 1
                    self._stats["dropped_full"] += 1
                pq.append(slot.slot_id)
            self._cond.notify_all()

    # ───────── Consumer side ─────────
    def consume(self, name: str, timeout: float = 0.1) -> Optional[NV12Slot]:
        """Pop the oldest pending slot for this consumer. None on timeout."""
        deadline = time.monotonic() + timeout
        with self._lock:
            while True:
                pq = self._pending.get(name)
                if pq is None:
                    return None
                if pq:
                    slot_id = pq.pop(0)
                    self._stats["consumed_total"] += 1
                    return self._slots[slot_id]
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    return None
                self._cond.wait(timeout=remaining)

    def release(self, name: str, slot: NV12Slot) -> None:
        """Drop one refcount on the slot. When all consumers release, the
        slot returns to the free pool."""
        with self._lock:
            if name not in self._consumers:
                return
            self._refcount[slot.slot_id] -= 1
            if self._refcount[slot.slot_id] < 0:
                # Programming error — recover by clamping to 0
                self._refcount[slot.slot_id] = 0
            self._stats["released_total"] += 1
            if self._refcount[slot.slot_id] == 0:
                self._cond.notify_all()

    # ───────── Diagnostics ─────────
    def stats(self) -> Dict[str, int]:
        with self._lock:
            return dict(self._stats)

    def free_count(self) -> int:
        with self._lock:
            return sum(1 for rc in self._refcount if rc == 0)

    def close(self) -> None:
        try:
            self._shm.close()
            self._shm.unlink()
        except (FileNotFoundError, OSError):
            pass

    def __enter__(self):  return self
    def __exit__(self, *a):  self.close()
