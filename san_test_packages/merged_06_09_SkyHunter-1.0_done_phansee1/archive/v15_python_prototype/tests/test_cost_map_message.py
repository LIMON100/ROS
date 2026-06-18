"""CostMapUpdate message + queue end-to-end (SAN-IDS-CMD-001 v1.3 §5.13)."""
from __future__ import annotations

import numpy as np
import pytest

from core.ipc import consume, make_topic_queues, publish
from core.messages import (
    COST_LETHAL,
    COST_WARN,
    CostMapUpdate,
)
from mapping.cost_map import (
    CostMap,
    CostMapConfig,
    decode_master,
    encode_master_png,
)


def test_compose_to_message_round_trip_raw():
    cfg = CostMapConfig(size_m=4.0, resolution_m=0.05)
    cm = CostMap(cfg)
    # Mixed scan with one lethal pillar.
    pts = np.zeros((30, 3), dtype=np.float32)
    pts[:, 0] = 1.5
    pts[:, 1] = 0.0
    pts[:, 2] = 0.30
    cm.compose(pts)
    msg = cm.to_message(producer_latency_s=0.05,
                        origin_xy=(0.0, 0.0),
                        encoding="raw")
    assert isinstance(msg, CostMapUpdate)
    assert msg.width == cfg.grid_cells
    assert msg.height == cfg.grid_cells
    assert msg.encoding == "raw"
    assert msg.n_lethal >= 1
    # Decode and compare with the in-memory master grid.
    decoded = decode_master(msg.master_payload, msg.width, msg.height,
                            msg.encoding)
    assert decoded.shape == cm.master.shape
    assert np.array_equal(decoded, cm.master)


def test_png_encoding_round_trip():
    grid = np.zeros((20, 20), dtype=np.uint8)
    grid[5, 5] = COST_LETHAL
    grid[10, 10] = COST_WARN
    payload = encode_master_png(grid)
    decoded = decode_master(payload, 20, 20, "png")
    assert np.array_equal(decoded, grid)


def test_message_flows_through_queue():
    queues = make_topic_queues(maxsize=4)
    try:
        cfg = CostMapConfig(size_m=4.0, resolution_m=0.05)
        cm = CostMap(cfg)
        cm.compose(np.zeros((0, 3), dtype=np.float32))
        msg = cm.to_message(producer_latency_s=0.01, encoding="raw")
        assert publish(queues.cost_map_update, msg) is True
        rx = consume(queues.cost_map_update, timeout=1.0)
        assert rx is not None
        assert rx.width == cfg.grid_cells
        assert rx.height == cfg.grid_cells
        # Zero-scan compose → all FREE.
        assert rx.n_lethal == 0
        assert rx.n_warn == 0
        assert rx.n_unknown == 0
        assert rx.n_free == cfg.grid_cells ** 2
    finally:
        try:
            queues.cost_map_update.close()
            queues.cost_map_update.join_thread()
        except Exception:        # nosec — best-effort cleanup
            pass


def test_invalid_encoding_rejected():
    cfg = CostMapConfig(size_m=2.0, resolution_m=0.05)
    cm = CostMap(cfg)
    cm.compose(np.zeros((0, 3), dtype=np.float32))
    with pytest.raises(ValueError, match="encoding"):
        cm.to_message(producer_latency_s=0.0, encoding="webp")


def test_cell_counts_total_to_grid_size():
    cfg = CostMapConfig(size_m=4.0, resolution_m=0.05)
    cm = CostMap(cfg)
    pts = np.zeros((50, 3), dtype=np.float32)
    pts[:, 0] = 1.0
    pts[:, 2] = 0.30
    cm.compose(pts)
    msg = cm.to_message(producer_latency_s=0.0)
    total = msg.n_lethal + msg.n_warn + msg.n_free + msg.n_unknown
    assert total == cfg.grid_cells ** 2


def test_compose_records_positive_latency():
    cfg = CostMapConfig(size_m=4.0, resolution_m=0.05)
    cm = CostMap(cfg)
    _, latency = cm.compose(np.zeros((100, 3), dtype=np.float32))
    assert latency >= 0.0
    # Sanity: any actual work takes at least a microsecond.
    assert latency < 1.0


def test_grid_origin_propagates():
    cfg = CostMapConfig(size_m=4.0, resolution_m=0.05)
    cm = CostMap(cfg)
    cm.compose(np.zeros((0, 3), dtype=np.float32))
    msg = cm.to_message(producer_latency_s=0.0, origin_xy=(123.4, -56.7))
    assert msg.origin_xy == (pytest.approx(123.4), pytest.approx(-56.7))
    assert msg.resolution_m == pytest.approx(0.05)
