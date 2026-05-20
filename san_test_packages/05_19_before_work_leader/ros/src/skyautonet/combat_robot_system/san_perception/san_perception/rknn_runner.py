"""SAN v1.5 Phase 2-E Turn 11-12 — NPU runner abstraction.

Decouples PerceptionNode from the real RKNN runtime so the node can:
  * Run on dev machines without NPU (StubRknnRunner)
  * Be unit-tested with a MockRknnRunner
  * Switch backends (RKNN, HailoRT) via runtime parameter

The real RKNN runtime is loaded lazily — its import is inside the
constructor of RealRknnRunner so dev/test boxes without librknnrt
can still import this module.
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
    """

    def __init__(self, *, name: str = "stub"):
        self._name = name
        self._ready = True

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
        # Production: decode H.265 → preprocess → run inference → decode
        # output. Each model architecture needs its own decoder; the
        # decode_output_fn (injected at construction) handles this.
        # This method is wired but the heavy lifting lives in the
        # decoder so unit tests for it stay focused.
        if self._decode_fn is None:
            log.warning("RealRknnRunner.infer called without decode_fn")
            return []
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
    """
    if backend == "stub":
        return StubRknnRunner()
    if backend == "rknn":
        try:
            return RealRknnRunner(model_path, input_width, input_height)
        except Exception as e:
            log.warning(f"Real RKNN unavailable ({e})")
            if stub_on_no_npu:
                return StubRknnRunner(name="rknn_fallback")
            raise
    raise ValueError(f"unknown NPU backend: {backend}")
