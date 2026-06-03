# Copyright 2026 SkyAutoNet Inc.
#
# Proprietary and confidential. Unauthorized copying, distribution, or use
# of this file, via any medium, is strictly prohibited.

"""SAN v1.5 Phase 2-E Turn 11-12 — NPU runner abstraction.

Decouples PerceptionNode from the real RKNN runtime so the node can:
  * Run on dev machines without NPU (StubRknnRunner)
  * Be unit-tested with a MockRknnRunner
  * Switch backends (RKNN, HailoRT) via runtime parameter

The real RKNN runtime is loaded lazily — its import is inside the
constructor of RealRknnRunner so dev/test boxes without librknnrt
can still import this module.

PATCH 2026-05-13 (san_perception deep-dive review):
  * C1 — StubRknnRunner now marks its output with is_stub=True. The
    downstream post_process(drop_stub=True) will discard them before
    they reach the fire-auth pipeline. The stub remains useful for
    exercising message flow (the bbox / confidence are realistic) but
    cannot be mistaken for a real detection.
  * C2 — RealRknnRunner.infer() raises NotImplementedError when no
    decode_fn is wired, instead of silently returning []. Returning
    empty in production looked like "no targets in frame" — a false
    negative is just as dangerous as the stub-false-positive in C1.
  * C7 — make_runner narrowed Exception to the construction-time
    failures we actually want to recover from (ImportError, OSError,
    RuntimeError, ValueError). KeyboardInterrupt and SystemExit now
    propagate.
"""
from __future__ import annotations

import logging
import time
from typing import Any, List, Optional, Protocol

from san_perception.detection import CLASS_PERSON, RawDetection

log = logging.getLogger(__name__)


class RknnRunnerInterface(Protocol):
    """Abstract NPU inference runner."""

    def is_ready(self) -> bool: ...

    def infer(self, image_bytes: bytes,
               width: int, height: int) -> List[RawDetection]: ...

    def close(self) -> None: ...


# ─── Stub runner — build/CI fallback ────────────────────────────────────

class StubRknnRunner:
    """No-op runner returning a single canned detection.

    Used when:
      * RKNN runtime is unavailable (dev machines)
      * stub_on_no_npu = true and real model load fails
    Lets the downstream pipeline exercise message flow without HW.

    ★ PATCH 2026-05-13 (C1): output is marked is_stub=True. The
    post_process pipeline drops stub detections by default, so the
    stub no longer poses a fire-auth false-positive risk.
    """

    def __init__(self, *, name: str = "stub"):
        self._name = name
        self._ready = True
        # Warn loudly so operators don't accidentally ship stub.
        log.warning(
            "StubRknnRunner active (name=%s) — output is marked is_stub=True "
            "and WILL be dropped by post_process(drop_stub=True). For real "
            "operation use backend='rknn' with a valid model_path.", name)

    def is_ready(self) -> bool:
        return self._ready

    def infer(self, image_bytes: bytes,
               width: int, height: int) -> List[RawDetection]:
        # Single static "person" detection in the image center,
        # confidence 0.85 — matches Python prototype's stub.
        cx, cy = width // 2, height // 2
        return [RawDetection(
            class_id=CLASS_PERSON, confidence=0.85,
            x1=cx - 50, y1=cy - 100,
            x2=cx + 50, y2=cy + 100,
            is_stub=True,        # ★ PATCH 2026-05-13 (C1)
        )]

    def close(self) -> None:
        self._ready = False


# ─── Real runner — RKNN runtime ─────────────────────────────────────────

class RealRknnRunner:
    """Wraps the RKNN runtime.

    Construction can fail (missing librknnrt, .rknn model not found,
    etc.) — caller should catch and fall back to StubRknnRunner.

    The actual RKNN API is not invoked here (factory-only); the model
    output decoder is delegated to `decode_output_fn` so different
    model architectures (YOLOv5/v8/RT-DETR) can plug in.
    """

    def __init__(self, model_path: str,
                 input_width: int, input_height: int,
                 decode_output_fn: Optional[Any] = None):
        self._model_path     = model_path
        self._input_width    = input_width
        self._input_height   = input_height
        self._decode_fn      = decode_output_fn
        self._ready          = False
        self._rknn           = None

        # Lazy import — raises on missing runtime, caller handles fallback
        try:
            from rknn.api import RKNN  # type: ignore
        except ImportError as e:
            raise RuntimeError(
                f"RKNN runtime not available: {e}") from e

        self._rknn = RKNN(verbose=False)
        if self._rknn.load_rknn(model_path) != 0:
            raise RuntimeError(
                f"failed to load RKNN model: {model_path}")
        if self._rknn.init_runtime() != 0:
            raise RuntimeError("RKNN init_runtime failed")
        self._ready = True
        log.info(f"RealRknnRunner ready: {model_path}")

    def is_ready(self) -> bool:
        return self._ready and self._rknn is not None

    def infer(self, image_bytes: bytes,
               width: int, height: int) -> List[RawDetection]:
        if not self.is_ready():
            return []
        # ★ PATCH 2026-05-13 (C2): without a decoder, the previous
        # code silently returned [] which is indistinguishable from
        # "no targets in frame" — a false negative dangerous in
        # combat. Raise loudly so configuration mistakes surface at
        # the first frame, not after an engagement.
        if self._decode_fn is None:
            raise NotImplementedError(
                "RealRknnRunner.infer requires decode_output_fn — none was "
                "supplied at construction. This indicates a configuration "
                "bug: real NPU inference cannot proceed without a model-"
                "specific output decoder.")
        t0 = time.time()
        # outputs = self._rknn.inference(inputs=[preprocessed])
        outputs: List[Any] = []     # placeholder — model dependent
        raw = self._decode_fn(outputs, width, height)
        log.debug(f"infer took {(time.time() - t0) * 1000:.1f} ms, "
                   f"{len(raw)} raw detections")
        return raw

    def close(self) -> None:
        if self._rknn is not None:
            try:
                self._rknn.release()
            except Exception:
                pass
            self._rknn = None
        self._ready = False


# ─── Factory ────────────────────────────────────────────────────────────

def make_runner(
    backend: str,
    model_path: str = "",
    input_width: int = 640,
    input_height: int = 640,
    stub_on_no_npu: bool = True,
) -> RknnRunnerInterface:
    """Construct a runner per backend name.

    backend:
      "stub"    — always StubRknnRunner
      "rknn"    — try RealRknnRunner; fall back to StubRknnRunner if
                  stub_on_no_npu=True and real construction fails

    PATCH 2026-05-13 (C7): exception catch narrowed to construction-
    time failures. KeyboardInterrupt and SystemExit propagate.
    """
    if backend == "stub":
        return StubRknnRunner()
    if backend == "rknn":
        try:
            return RealRknnRunner(model_path, input_width, input_height)
        except (ImportError, OSError, RuntimeError, ValueError) as e:
            log.warning(f"Real RKNN unavailable ({type(e).__name__}: {e})")
            if stub_on_no_npu:
                return StubRknnRunner(name="rknn_fallback")
            raise
    raise ValueError(f"unknown NPU backend: {backend}")
