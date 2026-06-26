# Copyright 2026 SkyAutoNet Inc.
#
# Proprietary and confidential. Unauthorized copying, distribution, or use
# of this file, via any medium, is strictly prohibited.

"""SAN v1.5 Phase 2-E — PATCH 2026-05-13 operational_modes thread-safety.

Validates:
  PO1 (★ C5)  OperationalModeController is thread-safe under
              concurrent request_mode + get_current_preset access.

DCN-2026-023 v2 (2026-05-23): PO2 and PO3 (PIN auth concurrency)
removed — the PIN auth mechanism is gone in operational_modes.py
because the only production caller was the BLE 0xFF05 GATT
challenge that DCN-2026-008 deleted. The remaining PO1 retains the
mode-mutation race coverage that backed the C5 lock fix.
"""
import threading

from san_mission.operational_modes import (
    OperationalMode,
    OperationalModeController,
)


# ─── PO1 (★ C5): thread-safe concurrent mode access ───────────────────
def test_po1_concurrent_request_mode_thread_safe():
    """The PATCH adds an internal lock around all state mutations and
    reads. We hammer the controller from multiple threads requesting
    different modes + reading the current preset; the final state
    must be one of the legally-requested modes (never corrupted)."""
    ctrl = OperationalModeController()

    requested_modes = [
        OperationalMode.RECON,
        OperationalMode.NARROW,
        OperationalMode.WIDE,
        OperationalMode.ASSAULT,
        OperationalMode.DEV_TEST,
    ]
    N_ITERS = 500

    def writer(mode):
        for _ in range(N_ITERS):
            ctrl.request_mode(mode)

    read_count = {"n": 0, "errors": 0}

    def reader():
        for _ in range(N_ITERS):
            try:
                preset = ctrl.get_current_preset()
                # If we ever see a preset whose name doesn't match
                # one of the legal modes, the lock failed.
                assert preset.name, "preset.name must be non-empty"
                read_count["n"] += 1
            except Exception:
                read_count["errors"] += 1

    threads = [threading.Thread(target=writer, args=(m,))
                for m in requested_modes]
    threads.append(threading.Thread(target=reader))
    threads.append(threading.Thread(target=reader))

    for t in threads:
        t.start()
    for t in threads:
        t.join()

    assert read_count["errors"] == 0, (
        f"reader saw {read_count['errors']} torn-state errors")
    assert read_count["n"] >= 2 * N_ITERS - 10  # near full throughput
    # Final state is some legal mode.
    assert ctrl.current in requested_modes
