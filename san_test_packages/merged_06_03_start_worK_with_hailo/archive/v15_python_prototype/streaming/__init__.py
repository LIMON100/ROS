"""Streaming + camera fan-out modules — AIRYS-style 4-channel architecture.

Submodules:
  • nv12_pool          — refcounted slot pool (zero-copy producer→3 consumers)
  • streaming_process  — GStreamer UDP/SRT (gst-launch subprocess)
"""
from .nv12_pool import NV12Pool, NV12Slot
from .streaming_process import StreamingProcess

__all__ = ["NV12Pool", "NV12Slot", "StreamingProcess"]
