"""
SharedMapReceiverProcess — follower-side counterpart to the leader's
SLAM broadcast.

Receives SharedMapChunks from the swarm bridge (`queues.shared_map_in`),
reassembles a complete map_id from its constituent chunks, decompresses
the zlib-deflated occupancy grid, and emits a MapTile on `queues.fused_tile`
so the local map fusion / mission processes can use it.

Reassembly state machine
------------------------
Per map_id we keep:
  • {chunk_id: bytes}             — pieces seen so far
  • n_chunks                      — how many we expect (from any chunk header)
  • origin_xy, resolution_m       — map metadata (must agree across chunks)
  • first_seen_t                  — for timeout / GC

When all chunks arrive → decompress → publish → forget map_id.
If a map_id sits incomplete for `chunk_timeout_s`, drop it.

Why a separate process?
-----------------------
Decompression of large grids and reassembly are CPU-bursty; isolating
keeps the realtime localization/SLAM threads jitter-free.
"""
from __future__ import annotations

import logging
import time
import zlib
from dataclasses import dataclass, field
from typing import Dict, Optional, Tuple

import numpy as np

from core.base_process import BaseProcess
from core.ipc import TopicQueues, consume, publish
from core.messages import AggregatedMap, Header, MapTile, SharedMapChunk
from mapping.aggregated_map import decode_aggregated_map

log = logging.getLogger(__name__)


# ──────────── Reassembler — pure logic (testable without IPC) ────────────
@dataclass
class _PartialMap:
    n_chunks: int
    chunks: Dict[int, bytes] = field(default_factory=dict)
    origin_xy: Optional[np.ndarray] = None
    resolution_m: float = 0.0
    first_seen_t: float = 0.0

    def is_complete(self) -> bool:
        return len(self.chunks) == self.n_chunks

    def assemble(self) -> bytes:
        return b"".join(self.chunks[i] for i in range(self.n_chunks))


class MapReassembler:
    """Stateful chunk reassembler. No threading — caller serializes access.

    Invariants:
      • Duplicate chunks (same map_id, chunk_id) overwrite — newer wins
      • A new map_id with a chunk_id > seen n_chunks is rejected
      • Different chunks of the same map_id must agree on n_chunks; the
        first one's metadata wins (origin_xy, resolution_m)
    """

    def __init__(self, chunk_timeout_s: float = 30.0,
                 max_in_flight: int = 8):
        self.chunk_timeout_s = chunk_timeout_s
        self.max_in_flight = max_in_flight
        self._partial: Dict[str, _PartialMap] = {}
        self.stats = {
            "chunks_in": 0, "chunks_dup": 0, "chunks_invalid": 0,
            "maps_complete": 0, "maps_dropped_timeout": 0,
            "maps_dropped_oversize": 0, "decode_errors": 0,
        }

    def feed(self, chunk: SharedMapChunk, now: Optional[float] = None
             ) -> Optional[Tuple[bytes, np.ndarray, float]]:
        """Feed a chunk. Returns (raw_bytes, origin, resolution) if a complete
        map was assembled, else None.

        `raw_bytes` is the *decompressed* occupancy grid byte string —
        caller reshapes into (H, W) using their own dimension knowledge.
        """
        now = now if now is not None else time.monotonic()
        self.stats["chunks_in"] += 1

        # GC stale partials and enforce in-flight cap
        self._gc(now)

        if chunk.n_chunks <= 0 or chunk.chunk_id < 0 \
                or chunk.chunk_id >= chunk.n_chunks:
            self.stats["chunks_invalid"] += 1
            return None

        p = self._partial.get(chunk.map_id)
        if p is None:
            if len(self._partial) >= self.max_in_flight:
                # Evict the oldest unfinished map
                oldest = min(self._partial.items(),
                             key=lambda kv: kv[1].first_seen_t)
                del self._partial[oldest[0]]
                self.stats["maps_dropped_oversize"] += 1
            p = _PartialMap(
                n_chunks=chunk.n_chunks,
                origin_xy=np.asarray(chunk.origin_xy, dtype=np.float32).copy(),
                resolution_m=float(chunk.resolution_m),
                first_seen_t=now,
            )
            self._partial[chunk.map_id] = p
        else:
            if p.n_chunks != chunk.n_chunks:
                # Conflicting metadata — drop this chunk, keep partial
                self.stats["chunks_invalid"] += 1
                return None

        if chunk.chunk_id in p.chunks:
            self.stats["chunks_dup"] += 1
            # Replace anyway (in case we re-received with corrected data)
        p.chunks[chunk.chunk_id] = bytes(chunk.payload)

        if p.is_complete():
            # Assemble + decompress
            try:
                raw = zlib.decompress(p.assemble())
            except zlib.error as e:
                self.stats["decode_errors"] += 1
                del self._partial[chunk.map_id]
                log.warning(f"map {chunk.map_id} decode failed: {e}")
                return None
            origin = p.origin_xy
            res = p.resolution_m
            del self._partial[chunk.map_id]
            self.stats["maps_complete"] += 1
            return raw, origin, res
        return None

    def _gc(self, now: float) -> None:
        stale = [k for k, p in self._partial.items()
                 if (now - p.first_seen_t) > self.chunk_timeout_s]
        for k in stale:
            del self._partial[k]
            self.stats["maps_dropped_timeout"] += 1


# ──────────── Process wrapper ────────────
class SharedMapReceiverProcess(BaseProcess):
    """Drains queues.shared_map_in, publishes MapTile on fused_tile when ready."""

    def __init__(self, queues: TopicQueues, shutdown_event, config, **diag):
        super().__init__(
            name="SharedMapReceiver",
            shutdown_event=shutdown_event,
            rate_hz=20.0,
            cpu_affinity=config.get("system", "cpu_affinity",
                                    "shared_map_rx") or [],
            **diag,
        )
        self.queues = queues
        self.cfg = config
        self._asm: Optional[MapReassembler] = None
        # Default grid shape for reshape; if leader publishes alongside this is
        # unused. Production should encode shape in SharedMapChunk metadata.
        self._tile_shape: Tuple[int, int] = (0, 0)
        self._seq = 0

    def setup(self) -> None:
        self._asm = MapReassembler(
            chunk_timeout_s=self.cfg.get("shared_map_rx",
                                         "chunk_timeout_s", default=30.0),
            max_in_flight=self.cfg.get("shared_map_rx",
                                       "max_in_flight", default=8),
        )
        # If leader publishes square tiles, infer dimension from sqrt; else
        # caller must publish dimension out-of-band.
        h = self.cfg.get("shared_map_rx", "tile_height", default=0)
        w = self.cfg.get("shared_map_rx", "tile_width",  default=0)
        self._tile_shape = (int(h), int(w))
        self.spawn_thread(self._aggregated_map_consumer, name="AggMapSub")

    def step(self) -> None:
        # Drain up to a few chunks per cycle
        for _ in range(16):
            chunk: Optional[SharedMapChunk] = consume(
                self.queues.shared_map_in, timeout=0.0)
            if chunk is None:
                break
            result = self._asm.feed(chunk)
            if result is None:
                continue
            raw_bytes, origin, resolution = result
            tile = self._raw_to_tile(raw_bytes, origin, resolution, chunk.map_id)
            if tile is not None:
                publish(self.queues.fused_tile, tile)

    def _aggregated_map_consumer(self) -> None:
        """Drain hub_aggregated_map; decode PNG → MapTile → fused_tile.

        Runs alongside the chunked path so a follower benefits from
        whichever broadcast arrives first. Decode errors (corrupt PNG,
        dimension mismatch, missing Pillow) are logged once per error
        class to avoid flooding the log.
        """
        decode_warned = False
        while self.is_running():
            msg: Optional[AggregatedMap] = consume(
                self.queues.hub_aggregated_map, timeout=0.5)
            if msg is None:
                continue
            try:
                grid, origin, resolution = decode_aggregated_map(msg)
            except (RuntimeError, ValueError) as e:
                if not decode_warned:
                    log.warning(f"aggregated-map decode failed: {e}")
                    decode_warned = True
                continue
            # Mirror the encoder's convention: 127 = unknown → -1, else
            # linear into [0, 1]. Anything off-127 is treated as a real
            # occupancy reading even when neighbouring; the encoder only
            # writes 127 for explicit -1 inputs.
            occ = grid.astype(np.float32) / 255.0
            occ[grid == 127] = -1.0
            tile = MapTile(
                tile_id=f"hub_agg_{msg.sequence}",
                origin_xy=(origin.x, origin.y),
                size_m=msg.width_cells * msg.resolution_m,
                resolution=msg.resolution_m,
                occupancy=occ,
                confidence=0.8,
                source="hub_fused",
                last_update=msg.timestamp_ms / 1000.0,
            )
            publish(self.queues.fused_tile, tile)

    def _raw_to_tile(self, raw: bytes, origin: np.ndarray,
                     resolution: float, map_id: str) -> Optional[MapTile]:
        h, w = self._tile_shape
        if h == 0 or w == 0:
            # Try sqrt heuristic — safest only for square maps
            n = len(raw)
            side = int(round(n ** 0.5))
            if side * side != n:
                log.warning(
                    f"received map {map_id} of {n} bytes; can't infer shape")
                return None
            h = w = side
        elif h * w != len(raw):
            log.warning(f"map {map_id} size mismatch: expected {h*w}, got {len(raw)}")
            return None
        grid = np.frombuffer(raw, dtype=np.int8).reshape(h, w).astype(np.float32)
        tile = MapTile(
            header=Header.now(frame_id="map", seq=self._seq),
            tile_id=map_id,
            origin_xy=origin.astype(np.float32),
            resolution_m=resolution,
            occupancy=grid,
            confidence=0.7,           # broadcast tiles get moderate trust
        )
        self._seq += 1
        return tile
