# Copyright 2026 SkyAutoNet Inc.
#
# Proprietary and confidential. Unauthorized copying, distribution, or use
# of this file, via any medium, is strictly prohibited.

"""SAN v1.5 Phase 2-E — PATCH 2026-05-13 detection deep-dive tests.

Validates:
  PD1 (★ C1)  RawDetection.is_stub field defaults to False
  PD2 (★ C1)  filter_stub drops marked detections
  PD3 (★ C1)  post_process(drop_stub=True, default) drops stubs
  PD4 (★ C1)  post_process(drop_stub=False) keeps stubs (diagnostic mode)
  PD5 (★ C1)  clamp_bbox preserves is_stub flag
  PD6 (★ L20) CLASS_UNKNOWN not in VALID_CLASS_IDS — drops by default
  PD7 (★ L20) CLASS_UNKNOWN can be opted-in via allowed_class_ids
"""
import os
import sys

sys.path.insert(
    0,
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
)

from san_perception.detection import (
    CLASS_PERSON,
    CLASS_UNKNOWN,
    CLASS_VEHICLE,
    RawDetection,
    VALID_CLASS_IDS,
    clamp_bbox,
    filter_by_class,
    filter_stub,
    post_process,
)


# ─── PD1 (★ C1): is_stub default False ────────────────────────────────
def test_pd1_is_stub_defaults_false():
    d = RawDetection(CLASS_PERSON, 0.9, 0, 0, 100, 100)
    assert d.is_stub is False


# ─── PD2 (★ C1): filter_stub drops marked detections ──────────────────
def test_pd2_filter_stub_drops_marked():
    dets = [
        RawDetection(CLASS_PERSON, 0.9, 0, 0, 100, 100, is_stub=True),
        RawDetection(CLASS_PERSON, 0.9, 50, 0, 150, 100, is_stub=False),
        RawDetection(CLASS_VEHICLE, 0.7, 200, 200, 300, 300, is_stub=True),
    ]
    kept = filter_stub(dets)
    assert len(kept) == 1
    assert kept[0].x1 == 50
    assert kept[0].is_stub is False


# ─── PD3 (★ C1): post_process(drop_stub=True) drops stubs ─────────────
def test_pd3_post_process_drops_stubs_by_default():
    """The most critical patch: stub-marked detections must NEVER reach
    the fire-auth pipeline. post_process drops them by default."""
    raw = [
        # Realistic-looking stub (centered, 0.85 confidence)
        RawDetection(CLASS_PERSON, 0.85, 320, 240, 420, 440, is_stub=True),
        # Real detection
        RawDetection(CLASS_PERSON, 0.9, 100, 100, 200, 200, is_stub=False),
    ]
    kept = post_process(raw, image_width=640, image_height=480,
                         min_confidence=0.4)
    assert len(kept) == 1
    assert kept[0].is_stub is False
    assert kept[0].x1 == 100


# ─── PD4 (★ C1): drop_stub=False keeps stubs (diagnostic mode) ────────
def test_pd4_post_process_can_keep_stubs():
    raw = [
        RawDetection(CLASS_PERSON, 0.85, 320, 240, 420, 440, is_stub=True),
        RawDetection(CLASS_PERSON, 0.9, 100, 100, 200, 200, is_stub=False),
    ]
    kept = post_process(raw, image_width=640, image_height=480,
                         min_confidence=0.4,
                         drop_stub=False)
    # Both survive (no spatial overlap → no NMS suppression)
    assert len(kept) == 2
    # Confidence-sorted: real (0.9) before stub (0.85)
    assert kept[0].is_stub is False
    assert kept[1].is_stub is True


# ─── PD5 (★ C1): clamp_bbox preserves is_stub ─────────────────────────
def test_pd5_clamp_bbox_preserves_is_stub():
    # Bbox needs clamping
    d = RawDetection(CLASS_PERSON, 0.9, -10, -10, 200, 200, is_stub=True)
    c = clamp_bbox(d, 100, 100)
    assert c is not None
    assert c.is_stub is True


# ─── PD6 (★ L20): CLASS_UNKNOWN not in VALID_CLASS_IDS ────────────────
def test_pd6_class_unknown_not_in_valid():
    assert CLASS_UNKNOWN not in VALID_CLASS_IDS

    dets = [
        RawDetection(CLASS_UNKNOWN, 0.9, 0, 0, 100, 100),
        RawDetection(CLASS_PERSON,  0.9, 0, 0, 100, 100),
    ]
    kept = filter_by_class(dets)   # default = VALID_CLASS_IDS
    assert len(kept) == 1
    assert kept[0].class_id == CLASS_PERSON


# ─── PD7 (★ L20): CLASS_UNKNOWN opt-in ────────────────────────────────
def test_pd7_class_unknown_opt_in():
    """Operators who explicitly want CLASS_UNKNOWN (e.g. for diagnostic
    logging) can pass an explicit allowed_class_ids set."""
    dets = [
        RawDetection(CLASS_UNKNOWN, 0.9, 0, 0, 100, 100),
        RawDetection(CLASS_PERSON,  0.9, 0, 0, 100, 100),
    ]
    kept = filter_by_class(dets,
                            allowed_class_ids={CLASS_UNKNOWN, CLASS_PERSON})
    assert len(kept) == 2


# ─── PD8 (★ realistic stub scenario): the C1 case end-to-end ──────────
def test_pd8_realistic_stub_scenario():
    """Models the real production hazard:
       * RKNN runtime fails to load (no librknnrt on CI / dev box)
       * stub_on_no_npu=true → fallback to StubRknnRunner
       * Stub emits centered 'person' at 0.85 confidence
       * Without the patch, this passes confidence floor (0.4),
         passes NMS (no overlap), reaches fire-auth as a VALID
         person detection at (cx-50, cy-100, cx+50, cy+100).
       * With the PATCH, drop_stub=True removes it BEFORE
         confidence/NMS.
    """
    cx, cy = 320, 240   # center of a 640×480 frame
    stub_output = [RawDetection(
        class_id=CLASS_PERSON, confidence=0.85,
        x1=cx - 50, y1=cy - 100,
        x2=cx + 50, y2=cy + 100,
        is_stub=True,
    )]
    # Default post_process: nothing reaches fire-auth.
    kept = post_process(stub_output, image_width=640, image_height=480,
                         min_confidence=0.4)
    assert kept == [], (
        "Stub-marked detection MUST NOT reach the fire-auth pipeline")


# ─── PD9 (★ M9): clamp_bbox right-edge inclusive of width ─────────────
def test_pd9_clamp_bbox_right_edge():
    """Bottom-right corner of a 100×100 image — x2 can be 100
    (right-exclusive), and the resulting bbox is non-empty."""
    d = RawDetection(CLASS_PERSON, 0.9, 80, 80, 100, 100)
    c = clamp_bbox(d, 100, 100)
    assert c is not None
    assert c.x1 == 80 and c.x2 == 100
    assert c.y1 == 80 and c.y2 == 100
