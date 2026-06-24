"""Verify IMX678Adapter spawns SHM reaper to prevent slot leaks."""
import sys
import time

import pytest

from core.shm_pool import MultiConsumerShmPool


def test_imx_adapter_has_reaper_method():
    """IMX678Adapter must expose _shm_reaper_loop for spawn_thread."""
    from adapters.payload_sensors import IMX678Adapter
    assert hasattr(IMX678Adapter, '_shm_reaper_loop')


@pytest.mark.skipif(sys.platform == "win32",
                    reason="MultiConsumerShmPool requires POSIX /dev/shm")
def test_reaper_calls_reap_orphans():
    """When a slot exceeds slot_timeout_s, reap_orphans() force-releases it."""
    pool = MultiConsumerShmPool(
        name_prefix=f"test_reaper_{id(object())}",
        slot_bytes=1024, n_slots=4, slot_timeout_s=0.1)
    pool.setup()
    try:
        # Publish + don't release -> orphan after 0.1s
        name = pool.publish(b"x" * 100, n_consumers=2)
        assert name is not None
        time.sleep(0.15)
        forced = pool.reap_orphans()
        assert forced == 1, f"expected 1 reaped, got {forced}"
    finally:
        pool.teardown()
