# Copyright 2026 SkyAutoNet Inc.
#
# Proprietary and confidential. Unauthorized copying, distribution, or use
# of this file, via any medium, is strictly prohibited.

"""SAN v1.5 Phase 2-E Turn 11-12 — Detection post-processing tests.

Pure-logic pytest (no rclpy, no NPU).

Coverage:
   D1  iou identical bbox = 1.0
   D2  iou disjoint = 0.0
   D3  iou half-overlap = 1/3
   D4  filter_by_confidence drops below threshold
   D5  filter_by_class keeps only allowed
   D6  NMS suppresses overlapping same-class
   D7  NMS preserves different classes at same location
   D8  NMS keeps highest-confidence
   D9  clamp_bbox shrinks to bounds + drops empty
  D10  post_process full pipeline
  D11  empty input returns empty
"""
import os
import sys

sys.path.insert(
    0,
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
)

from san_perception.detection import (
    CLASS_DRONE,
    CLASS_PERSON,
    CLASS_VEHICLE,
    RawDetection,
    clamp_bbox,
    filter_by_class,
    filter_by_confidence,
    iou,
    non_max_suppression,
    post_process,
)

# ─── iou ────────────────────────────────────────────────────────────────

def test_d1_iou_identical():
    a = RawDetection(CLASS_PERSON, 0.9, 0, 0, 100, 100)
    assert iou(a, a) == 1.0


def test_d2_iou_disjoint():
    a = RawDetection(CLASS_PERSON, 0.9, 0,   0, 50, 50)
    b = RawDetection(CLASS_PERSON, 0.9, 100, 100, 200, 200)
    assert iou(a, b) == 0.0


def test_d3_iou_half_overlap():
    # 100×100 each, overlap 50×100 = 5000
    # union = 100×100 + 100×100 - 5000 = 15000
    # iou = 5000/15000 = 1/3
    a = RawDetection(CLASS_PERSON, 0.9, 0,  0, 100, 100)
    b = RawDetection(CLASS_PERSON, 0.9, 50, 0, 150, 100)
    assert abs(iou(a, b) - 1.0/3.0) < 1e-6


# ─── Filters ────────────────────────────────────────────────────────────

def test_d4_filter_by_confidence():
    dets = [
        RawDetection(CLASS_PERSON, 0.3, 0, 0, 10, 10),
        RawDetection(CLASS_PERSON, 0.5, 0, 0, 10, 10),
        RawDetection(CLASS_PERSON, 0.9, 0, 0, 10, 10),
    ]
    kept = filter_by_confidence(dets, min_confidence=0.4)
    assert len(kept) == 2
    assert {d.confidence for d in kept} == {0.5, 0.9}


def test_d5_filter_by_class():
    dets = [
        RawDetection(CLASS_PERSON,  0.9, 0, 0, 10, 10),
        RawDetection(CLASS_VEHICLE, 0.9, 0, 0, 10, 10),
        RawDetection(CLASS_DRONE,   0.9, 0, 0, 10, 10),
    ]
    kept = filter_by_class(dets, allowed_class_ids={CLASS_PERSON, CLASS_DRONE})
    assert len(kept) == 2
    assert CLASS_VEHICLE not in {d.class_id for d in kept}


# ─── NMS ────────────────────────────────────────────────────────────────

def test_d6_nms_suppresses_overlapping_same_class():
    # Two highly-overlapping person detections — keep the higher-confidence
    a = RawDetection(CLASS_PERSON, 0.9, 0,  0, 100, 100)
    b = RawDetection(CLASS_PERSON, 0.7, 5,  5, 105, 105)   # iou ~0.81
    kept = non_max_suppression([a, b], iou_threshold=0.5)
    assert len(kept) == 1
    assert kept[0].confidence == 0.9


def test_d7_nms_preserves_different_classes_same_location():
    # Person and vehicle at the same bbox — both survive
    a = RawDetection(CLASS_PERSON,  0.9, 0, 0, 100, 100)
    b = RawDetection(CLASS_VEHICLE, 0.8, 0, 0, 100, 100)
    kept = non_max_suppression([a, b], iou_threshold=0.5)
    assert len(kept) == 2


def test_d8_nms_keeps_highest_confidence():
    a = RawDetection(CLASS_PERSON, 0.6, 0, 0, 100, 100)
    b = RawDetection(CLASS_PERSON, 0.9, 1, 1, 101, 101)
    c = RawDetection(CLASS_PERSON, 0.7, 2, 2, 102, 102)
    kept = non_max_suppression([a, b, c], iou_threshold=0.5)
    assert len(kept) == 1
    assert kept[0].confidence == 0.9


# ─── clamp_bbox ─────────────────────────────────────────────────────────

def test_d9_clamp_bbox():
    # bbox extends beyond image — clamp
    d = RawDetection(CLASS_PERSON, 0.9, -10, -10, 200, 200)
    c = clamp_bbox(d, 100, 100)
    assert c is not None
    assert c.x1 == 0 and c.y1 == 0
    assert c.x2 == 100 and c.y2 == 100

    # entirely outside — drop
    d2 = RawDetection(CLASS_PERSON, 0.9, 200, 200, 300, 300)
    assert clamp_bbox(d2, 100, 100) is None


# ─── post_process ───────────────────────────────────────────────────────

def test_d10_post_process_full_pipeline():
    raw = [
        RawDetection(CLASS_PERSON,  0.3, 0,   0, 100, 100),   # low conf
        RawDetection(CLASS_PERSON,  0.9, 10,  10, 110, 110),  # keep
        RawDetection(CLASS_PERSON,  0.8, 15,  15, 115, 115),  # NMS suppress
        RawDetection(CLASS_VEHICLE, 0.85, 200, 200, 300, 300),# keep
    ]
    kept = post_process(raw, image_width=400, image_height=400,
                         min_confidence=0.4)
    # After filter: 0.9 person + 0.8 person + 0.85 vehicle
    # After NMS:    0.9 person + 0.85 vehicle (0.8 suppressed)
    assert len(kept) == 2
    assert kept[0].confidence == 0.9      # highest first
    classes = {d.class_id for d in kept}
    assert classes == {CLASS_PERSON, CLASS_VEHICLE}


def test_d11_empty_input():
    assert post_process([], 100, 100) == []
    assert non_max_suppression([]) == []
    assert filter_by_confidence([], 0.5) == []
