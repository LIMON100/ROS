"""
Tests for the leader→follower shared-map reassembler.

Pure-logic tests on MapReassembler — no IPC, no spawn, fast.
"""
from __future__ import annotations

import zlib
from typing import List

import numpy as np

from core.messages import Header, SharedMapChunk
from mapping.shared_map_receiver import MapReassembler


# ────────── Test fixture: build chunks from a known grid ──────────
def _make_chunks(map_id: str, grid: np.ndarray, chunk_size: int = 8,
                 origin=(0.0, 0.0), resolution=0.10) -> List[SharedMapChunk]:
    """Compress + chunk a grid the same way SwarmBridge.broadcast does.

    chunk_size defaults to 8 bytes so even small test grids produce
    multiple chunks — needed to exercise reassembly logic.
    """
    raw = grid.astype(np.int8).tobytes()
    blob = zlib.compress(raw, level=3)
    n_chunks = max(1, (len(blob) + chunk_size - 1) // chunk_size)
    out: List[SharedMapChunk] = []
    for i in range(n_chunks):
        out.append(SharedMapChunk(
            header=Header.now(frame_id="map", seq=i),
            map_id=map_id, chunk_id=i, n_chunks=n_chunks,
            payload=blob[i * chunk_size:(i + 1) * chunk_size],
            origin_xy=np.asarray(origin, dtype=np.float32),
            resolution_m=float(resolution),
        ))
    return out


def _make_grid(side: int, seed: int = 42) -> np.ndarray:
    """Random pattern so zlib can't trivially compress to 1 chunk."""
    rng = np.random.default_rng(seed=seed)
    return rng.integers(-1, 100, size=(side, side), dtype=np.int8)


# ════════════════════════════════════════════════════════════
# A. Happy paths
# ════════════════════════════════════════════════════════════
def test_complete_in_order_returns_decompressed_grid():
    grid = _make_grid(32)
    chunks = _make_chunks("map_0", grid)
    assert len(chunks) >= 2, "test fixture must produce multi-chunk maps"
    asm = MapReassembler()
    out = None
    for c in chunks[:-1]:
        assert asm.feed(c) is None     # not done yet
    out = asm.feed(chunks[-1])         # last chunk completes
    assert out is not None
    raw, origin, res = out
    recovered = np.frombuffer(raw, dtype=np.int8).reshape(grid.shape)
    assert np.array_equal(recovered, grid)
    assert asm.stats["maps_complete"] == 1
    assert asm.stats["chunks_in"] == len(chunks)


def test_complete_out_of_order():
    grid = _make_grid(32, seed=7)
    chunks = _make_chunks("m_oo", grid)
    asm = MapReassembler()
    # Feed in reverse order
    out = None
    for c in reversed(chunks):
        out = asm.feed(c)
    assert out is not None
    raw, _, _ = out
    recovered = np.frombuffer(raw, dtype=np.int8).reshape(grid.shape)
    assert np.array_equal(recovered, grid)


def test_single_chunk_map_completes_immediately():
    """If a tiny grid fits in one chunk, feed→complete in one call."""
    grid = _make_grid(2, seed=1)
    chunks = _make_chunks("tiny", grid, chunk_size=4096)
    assert len(chunks) == 1
    asm = MapReassembler()
    out = asm.feed(chunks[0])
    assert out is not None


# ════════════════════════════════════════════════════════════
# B. Duplicates / invalid input
# ════════════════════════════════════════════════════════════
def test_duplicate_chunk_does_not_double_count():
    grid = _make_grid(32)
    chunks = _make_chunks("dup", grid)
    assert len(chunks) >= 2
    asm = MapReassembler()
    asm.feed(chunks[0])
    asm.feed(chunks[0])                # duplicate
    assert asm.stats["chunks_dup"] == 1
    out = None
    for c in chunks[1:]:
        out = asm.feed(c)
    assert out is not None


def test_chunk_id_out_of_range_is_rejected():
    asm = MapReassembler()
    bad = SharedMapChunk(
        header=Header.now(),
        map_id="bad", chunk_id=5, n_chunks=3,    # 5 >= 3
        payload=b"x", origin_xy=np.zeros(2, np.float32), resolution_m=0.1,
    )
    assert asm.feed(bad) is None
    assert asm.stats["chunks_invalid"] == 1


def test_negative_chunk_id_is_rejected():
    asm = MapReassembler()
    bad = SharedMapChunk(
        header=Header.now(),
        map_id="bad", chunk_id=-1, n_chunks=3,
        payload=b"x", origin_xy=np.zeros(2, np.float32), resolution_m=0.1,
    )
    assert asm.feed(bad) is None
    assert asm.stats["chunks_invalid"] == 1


def test_n_chunks_disagreement_drops_chunk():
    grid = _make_grid(32)
    chunks = _make_chunks("disag", grid)
    assert len(chunks) >= 2
    asm = MapReassembler()
    asm.feed(chunks[0])
    rogue = SharedMapChunk(
        header=Header.now(),
        map_id="disag", chunk_id=1, n_chunks=999,
        payload=b"x", origin_xy=np.zeros(2, np.float32), resolution_m=0.1,
    )
    out = asm.feed(rogue)
    assert out is None
    assert asm.stats["chunks_invalid"] == 1


def test_corrupt_payload_returns_none_and_logs_decode_error():
    """Random bytes won't decompress — we should not crash."""
    asm = MapReassembler()
    bad = SharedMapChunk(
        header=Header.now(),
        map_id="garbage", chunk_id=0, n_chunks=1,
        payload=b"\xff\xff\xff\xff", origin_xy=np.zeros(2, np.float32),
        resolution_m=0.1,
    )
    out = asm.feed(bad)
    assert out is None
    assert asm.stats["decode_errors"] == 1


# ════════════════════════════════════════════════════════════
# C. Memory / GC
# ════════════════════════════════════════════════════════════
def test_stale_partial_is_garbage_collected():
    asm = MapReassembler(chunk_timeout_s=1.0)
    grid = _make_grid(32)
    chunks = _make_chunks("stale", grid)
    assert len(chunks) >= 2
    asm.feed(chunks[0], now=100.0)
    assert "stale" in asm._partial
    # Advance time past timeout — GC fires before processing chunks[1]
    asm.feed(chunks[1], now=200.0)
    assert asm.stats["maps_dropped_timeout"] == 1


def test_max_in_flight_evicts_oldest():
    asm = MapReassembler(max_in_flight=2)
    g = _make_grid(32)
    asm.feed(_make_chunks("m1", g)[0], now=10.0)
    asm.feed(_make_chunks("m2", g)[0], now=20.0)
    asm.feed(_make_chunks("m3", g)[0], now=30.0)
    assert "m1" not in asm._partial
    assert asm.stats["maps_dropped_oversize"] == 1


# ════════════════════════════════════════════════════════════
# D. Multi-map interleaving
# ════════════════════════════════════════════════════════════
def test_two_maps_interleaved_complete_independently():
    g1 = _make_grid(32, seed=1)
    g2 = _make_grid(32, seed=2)
    c1 = _make_chunks("a", g1)
    c2 = _make_chunks("b", g2)
    assert len(c1) >= 2 and len(c2) >= 2
    asm = MapReassembler()

    # Feed first chunks of each
    asm.feed(c1[0])
    asm.feed(c2[0])
    # Finish 'b' first
    out_b = None
    for c in c2[1:]:
        out_b = asm.feed(c)
    assert out_b is not None
    # 'a' should still be in flight
    assert "a" in asm._partial
    # Finish 'a'
    out_a = None
    for c in c1[1:]:
        out_a = asm.feed(c)
    assert out_a is not None
    # Each delivers its own grid, not the other's
    raw_b = np.frombuffer(out_b[0], dtype=np.int8).reshape(g2.shape)
    raw_a = np.frombuffer(out_a[0], dtype=np.int8).reshape(g1.shape)
    assert np.array_equal(raw_a, g1)
    assert np.array_equal(raw_b, g2)
    assert not np.array_equal(raw_a, g2)
