# Copyright 2026 SkyAutoNet Inc.
#
# Proprietary and confidential. Unauthorized copying, distribution, or use
# of this file, via any medium, is strictly prohibited.

"""SAN v1.5 Phase 2-E — PATCH 2026-05-13 rknn_runner deep-dive tests.

Validates:
  PR1 (★ C1)  StubRknnRunner output is marked is_stub=True
  PR2 (★ C2)  RealRknnRunner.infer raises NotImplementedError without decoder
  PR3 (★ C7)  make_runner('rknn') falls back to stub on construction error,
              and the fallback is also is_stub-marked
  PR4 (★ C7)  make_runner('rknn') with stub_on_no_npu=False propagates error
  PR5 (★ C7)  make_runner('unknown') raises ValueError
"""
import os
import sys

sys.path.insert(
    0,
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
)

import pytest

from san_perception.detection import CLASS_PERSON
from san_perception.rknn_runner import (
    RealRknnRunner,
    StubRknnRunner,
    make_runner,
)


# ─── PR1 (★ C1): Stub output marked is_stub ───────────────────────────
def test_pr1_stub_output_is_marked():
    runner = StubRknnRunner()
    raw = runner.infer(b"", 640, 480)
    assert len(raw) == 1
    assert raw[0].is_stub is True, (
        "Stub output MUST be marked is_stub=True so post_process can drop it")
    assert raw[0].class_id == CLASS_PERSON
    assert raw[0].confidence == 0.85   # legacy compat — stub still looks real


# ─── PR2 (★ C2): RealRknnRunner raises without decoder ────────────────
def test_pr2_real_runner_raises_without_decoder():
    """Skip if rknn runtime isn't installed (CI / dev box)."""
    pytest.importorskip("rknn.api",
                         reason="rknn.api not available — skipping real-runner test")

    # Mock the rknn module to force the constructor to "succeed", then
    # exercise the infer() path.

    class _FakeRKNN:
        def __init__(self, *a, **k): pass
        def load_rknn(self, *a, **k): return 0
        def init_runtime(self, *a, **k): return 0
        def release(self): pass

    # Cleanest is to just skip — the import-or-skip above handles it.
    # If we got here, the real runtime IS available, but we don't have
    # a real model. So fall back to direct StubRknnRunner test for
    # the decoder-missing path: the StubRknnRunner doesn't have this
    # raise path. Skip the rest gracefully.
    pytest.skip("Real RKNN runtime available but no test model; "
                 "decoder-missing-raise tested via dependency injection elsewhere")


def test_pr2b_real_runner_raises_without_decoder_via_subclass():
    """Test the raise path by subclassing past the import."""
    # Subclass so we don't go through the real RKNN constructor.
    class FakeReal(RealRknnRunner):
        def __init__(self):
            # Skip the RKNN-loading dance.
            self._model_path     = "fake.rknn"
            self._input_width    = 640
            self._input_height   = 640
            self._decode_fn      = None   # ★ the bug surface
            self._ready          = True
            self._rknn           = object()   # sentinel — is_ready=True

    runner = FakeReal()
    assert runner.is_ready()
    with pytest.raises(NotImplementedError) as exc_info:
        runner.infer(b"", 640, 480)
    # Error message must mention the missing decoder concretely.
    assert "decode_output_fn" in str(exc_info.value)


# ─── PR3 (★ C7): rknn fallback to stub keeps is_stub marking ──────────
def test_pr3_rknn_fallback_keeps_is_stub_marking():
    """The fallback StubRknnRunner (when rknn construction fails) must
    still emit is_stub=True. Otherwise a model load failure silently
    becomes a fake-person publisher."""
    # On a dev box without librknnrt, this triggers the fallback path.
    runner = make_runner(backend="rknn", model_path="/nonexistent.rknn",
                          stub_on_no_npu=True)
    # Either Real (if librknnrt + model present) or Stub (fallback).
    # In CI, it WILL be Stub.
    raw = runner.infer(b"", 640, 480)
    if isinstance(runner, StubRknnRunner):
        assert len(raw) >= 1
        assert all(d.is_stub for d in raw), (
            "Fallback stub must still mark output is_stub=True")


# ─── PR4 (★ C7): rknn without stub fallback propagates ────────────────
def test_pr4_rknn_no_fallback_propagates():
    with pytest.raises((RuntimeError, ImportError, OSError)):
        make_runner(backend="rknn",
                     model_path="/nonexistent_model_does_not_exist.rknn",
                     stub_on_no_npu=False)


# ─── PR5 (★ C7): unknown backend raises ValueError ────────────────────
def test_pr5_unknown_backend_raises():
    with pytest.raises(ValueError) as exc_info:
        make_runner(backend="not_a_real_backend")
    assert "unknown" in str(exc_info.value).lower()


# ─── PR6 (★ C7): KeyboardInterrupt NOT caught by make_runner ──────────
def test_pr6_make_runner_does_not_swallow_keyboard_interrupt():
    """Pre-patch had `except Exception` which would have masked
    KeyboardInterrupt. The PATCH narrowed catch to a specific tuple."""
    # We can't easily inject a KeyboardInterrupt into the construction
    # path, but we can verify the catch tuple doesn't include
    # KeyboardInterrupt / SystemExit by reading the source.
    import inspect
    from san_perception import rknn_runner
    src = inspect.getsource(rknn_runner.make_runner)
    # Whitelist approach: confirm the catch is narrow.
    assert "except (ImportError, OSError, RuntimeError, ValueError)" in src, (
        "make_runner must use a narrow except clause, not bare Exception")
    assert "except Exception" not in src, (
        "make_runner must NOT swallow KeyboardInterrupt / SystemExit")
