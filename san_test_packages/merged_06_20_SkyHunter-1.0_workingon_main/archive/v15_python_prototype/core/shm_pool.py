"""
SharedMemory pool — spawn-safe via mp.Queue as free-slot tracker.

Two API levels:

  HIGH-LEVEL (preferred for blob payloads — H.265 frames, thermal mono16):
      writer:  shm_name = pool.write(payload_bytes)        # returns name or None
      reader:  data     = pool.read(shm_name, n_bytes)     # returns bytes
               pool.release(shm_name)                      # mark slot free

  LOW-LEVEL (preferred for in-place numpy fills — point clouds, big arrays):
      writer:  slot = pool.acquire()                       # SharedMemory or None
               slot.buf[:] = ...                           # in-place fill
               # publish(slot.name, ...); the consumer releases the slot
      reader:  shm = ShmPool.attach(name)
               arr = ShmPool.view_array(shm, shape, dtype)
               # Use arr while shm stays alive in this scope.
               shm.close()
               pool.release(name)

The high-level API is what payload_sensors.py (IMX678/Thermal) expects;
the low-level API is what unitree_go2.py uses for LiDAR.
"""
from __future__ import annotations

import multiprocessing as mp
import queue as q
from multiprocessing import shared_memory
from typing import Optional, Tuple

import numpy as np


class ShmPool:
    def __init__(self, name_prefix: str, slot_bytes: int, n_slots: int = 8):
        self.name_prefix = name_prefix
        self.slot_bytes = slot_bytes
        self.n_slots = n_slots
        # mp.Queue is spawn-safe; used as the free-slot pool itself.
        self._free: mp.Queue = mp.Queue(maxsize=n_slots)
        self._slot_names = [f"{name_prefix}_{i}" for i in range(n_slots)]
        # Keep SHM handles alive so segments persist on Windows (Windows
        # destroys named file mappings when the last handle is closed).
        self._shm_handles: dict = {}

    def setup(self) -> None:
        for name in self._slot_names:
            try:
                shm = shared_memory.SharedMemory(create=True,
                                                 size=self.slot_bytes,
                                                 name=name)
                self._shm_handles[name] = shm
            except FileExistsError:
                # Stale segment from a previous crashed run — adopt it.
                pass
            self._free.put_nowait(name)

    # ────────── Low-level (slot handle) ──────────
    def acquire(self,
                timeout: float = 0.0
                ) -> Optional[shared_memory.SharedMemory]:
        try:
            name = (self._free.get(timeout=timeout)
                    if timeout > 0 else self._free.get_nowait())
        except q.Empty:
            return None
        return shared_memory.SharedMemory(name=name)

    def release(self, name: str) -> None:
        try:
            self._free.put_nowait(name)
        except q.Full:
            pass

    @staticmethod
    def attach(name: str) -> shared_memory.SharedMemory:
        return shared_memory.SharedMemory(name=name)

    @staticmethod
    def view_array(shm: shared_memory.SharedMemory,
                   shape: Tuple[int, ...],
                   dtype=np.float32) -> np.ndarray:
        """Build a view over shm.buf. Caller MUST keep `shm` alive while
        the returned array is in use — closing shm invalidates the buffer.

        For payloads that don't need a numpy view, use read() instead.
        """
        return np.ndarray(shape, dtype=dtype, buffer=shm.buf)

    # ────────── High-level (bytes copy) ──────────
    def write(self, payload: bytes) -> Optional[str]:
        """Acquire a slot, copy `payload` in, return slot name. None if pool full.

        On size overflow we drop the frame rather than truncate — silent
        truncation could produce decodable but corrupt H.265 frames downstream.
        """
        if len(payload) > self.slot_bytes:
            return None
        slot = self.acquire()
        if slot is None:
            return None
        try:
            slot.buf[:len(payload)] = payload
            return slot.name
        finally:
            slot.close()        # detach our handle; data persists in shm

    def read(self, name: str, nbytes: int) -> Optional[bytes]:
        """Attach a slot, copy `nbytes` out as bytes, detach.

        Note this is a copy — safe to use after the underlying slot is
        released or even unlinked. Don't call view_array() on the result.
        """
        try:
            shm = shared_memory.SharedMemory(name=name)
        except FileNotFoundError:
            return None
        try:
            n = min(nbytes, shm.size)
            return bytes(shm.buf[:n])
        finally:
            shm.close()

    # ────────── Lifecycle ──────────
    def teardown(self) -> None:
        for name in self._slot_names:
            try:
                shm = shared_memory.SharedMemory(name=name)
                shm.close()
                shm.unlink()
            except FileNotFoundError:
                pass
        for shm in self._shm_handles.values():
            try:
                shm.close()
            except Exception:
                pass
        self._shm_handles.clear()


# ════════════════════════════════════════════════════════════════════════
#  Multi-consumer fan-out pool — for camera frames feeding AI / Stream /
#  Display in parallel without copies.
#
#  Producer writes once → publishes the same shm_name to N consumer queues
#  → ref-count = N. Each consumer decrements on read; the last release
#  returns the slot to the free pool.
#
#  Why a separate class instead of bolting onto ShmPool: ref-counting needs
#  cross-process atomic state (mp.Manager dict + Lock). The single-consumer
#  ShmPool stays simpler and faster. Use this only where fan-out is needed.
#
#  Production replacement: Linux DMABUF + dma_fd shared via Unix sockets;
#  the kernel manages refs via drm_gem. This Python class is the dev/CI
#  fallback that uses POSIX shared memory and userspace ref-counts.
# ════════════════════════════════════════════════════════════════════════
class MultiConsumerShmPool:
    """ShmPool with N-consumer ref-counting on each slot.

    Lifecycle:
        pool = MultiConsumerShmPool("imx678", 4_000_000, n_slots=16)
        pool.setup()                  # creates SHM + Manager-backed counters

        # Producer
        shm_name = pool.publish(payload, n_consumers=3)   # ref_count=3, returns name
        # publish to 3 queues with this shm_name…

        # Each consumer
        data = pool.read(shm_name, nbytes)
        pool.release(shm_name)        # decrement; last release frees slot

    Timeouts: if a consumer crashes without releasing, the slot leaks.
    A reaper thread (see _reaper) sweeps slots whose age exceeds
    `slot_timeout_s` and force-releases them.
    """

    def __init__(self, name_prefix: str, slot_bytes: int,
                  n_slots: int = 16, slot_timeout_s: float = 2.0):
        self.name_prefix = name_prefix
        self.slot_bytes = slot_bytes
        self.n_slots = n_slots
        self.slot_timeout_s = slot_timeout_s
        # A Manager.list backed by a Lock is more predictable than mp.Queue
        # for "immediately read what we just wrote" semantics — mp.Queue
        # uses a background feeder thread that introduces a few-ms lag.
        self._slot_names = [f"{name_prefix}_{i}" for i in range(n_slots)]
        self._shm_handles: dict = {}
        self._mgr: Optional[mp.Manager] = None
        self._free = None            # Manager.list — free slot names
        self._refcount = None        # Manager.dict — slot_name → int
        self._published_at = None    # Manager.dict — slot_name → monotonic
        self._lock = None            # Manager.Lock for paired mutations

    def setup(self) -> None:
        """Create SHM segments + Manager state. Call once in producer process
        before forking children."""
        self._mgr = mp.Manager()
        self._free = self._mgr.list()
        self._refcount = self._mgr.dict()
        self._published_at = self._mgr.dict()
        self._lock = self._mgr.Lock()
        for name in self._slot_names:
            try:
                stale = shared_memory.SharedMemory(name=name)
                stale.close()
                stale.unlink()
            except FileNotFoundError:
                pass
            try:
                shm = shared_memory.SharedMemory(create=True,
                                                  size=self.slot_bytes,
                                                  name=name)
                self._shm_handles[name] = shm
            except FileExistsError:
                pass
            self._free.append(name)

    def publish(self, payload: bytes, n_consumers: int) -> Optional[str]:
        """Producer: claim a slot, write payload, set ref-count, return name.

        Slot bookkeeping (refcount + published_at) is set in the SAME
        critical section as the free-list pop so that a producer crash
        or a write failure can't strand the slot in a state that's
        invisible to reap_orphans.
        """
        if n_consumers <= 0:
            return None
        with self._lock:
            if not self._free:
                return None
            name = self._free.pop(0)
            self._refcount[name] = n_consumers
            self._published_at[name] = q_now()
        try:
            shm = shared_memory.SharedMemory(name=name)
            try:
                n = min(len(payload), shm.size)
                shm.buf[:n] = payload[:n]
            finally:
                shm.close()
        except (FileNotFoundError, OSError):
            with self._lock:
                self._refcount.pop(name, None)
                self._published_at.pop(name, None)
                self._return_to_free(name)
            return None
        return name

    def _return_to_free(self, name: str) -> None:
        # Caller holds self._lock
        if name not in self._free:
            self._free.append(name)

    def free_slots(self) -> int:
        with self._lock:
            return len(self._free)

    def read(self, name: str, nbytes: int) -> Optional[bytes]:
        """Consumer: copy out `nbytes` from the named slot."""
        try:
            shm = shared_memory.SharedMemory(name=name)
        except FileNotFoundError:
            return None
        try:
            n = min(nbytes, shm.size)
            return bytes(shm.buf[:n])
        finally:
            shm.close()

    def release(self, name: str) -> int:
        """Consumer: decrement ref-count. Last release frees the slot.
        Returns the post-decrement count."""
        with self._lock:
            cur = self._refcount.get(name, 0) - 1
            if cur <= 0:
                self._refcount.pop(name, None)
                self._published_at.pop(name, None)
                self._return_to_free(name)
                return 0
            self._refcount[name] = cur
            return cur

    def reap_orphans(self) -> int:
        """Force-release slots that have been outstanding too long.
        Runs once per call — schedule from a background thread."""
        now = q_now()
        forced = 0
        with self._lock:
            stale = [n for n, t in self._published_at.items()
                      if (now - t) > self.slot_timeout_s]
            for name in stale:
                self._refcount.pop(name, None)
                self._published_at.pop(name, None)
                self._return_to_free(name)
                forced += 1
        return forced

    def teardown(self) -> None:
        """Unlink all SHM segments + shut Manager down."""
        for name in self._slot_names:
            try:
                shm = shared_memory.SharedMemory(name=name)
                shm.close()
                shm.unlink()
            except FileNotFoundError:
                pass
        for shm in self._shm_handles.values():
            try:
                shm.close()
            except Exception:
                pass
        self._shm_handles.clear()
        if self._mgr is not None:
            self._mgr.shutdown()
            self._mgr = None


def q_now() -> float:
    """Monotonic seconds — wrapper so the timestamp source is mockable in tests."""
    import time as _t
    return _t.monotonic()
