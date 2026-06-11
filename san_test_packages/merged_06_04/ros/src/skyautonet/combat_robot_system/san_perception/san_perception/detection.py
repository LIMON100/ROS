# Copyright 2026 SkyAutoNet Inc.
#
# Proprietary and confidential. Unauthorized copying, distribution, or use
# of this file, via any medium, is strictly prohibited.

"""SAN v1.5 Phase 2-E Turn 11-12 — Detection post-processing (pure logic).

NMS (non-max suppression), confidence filtering, class mapping.
No rclpy, no ROS, no NPU SDK — pytest-testable in isolation.

PATCH 2026-05-13 (san_perception deep-dive review):
  * C1 — RawDetection.is_stub: stub runners mark their output so the
    downstream pipeline can drop it (or publish only as informational).
    post_process now skips stub detections by default.
  * L20 — VALID_CLASS_IDS no longer includes CLASS_UNKNOWN; detections
    with class_id=0 are dropped by filter_by_class. The fire-auth
    side asserts class_id ∈ {PERSON, VEHICLE, DRONE} anyway, so a
    leaked CLASS_UNKNOWN is wasted bandwidth.
  * M9 — clamp_bbox right-edge / bottom-edge logic clarified. Both
    edges are now treated as right-exclusive (NumPy-slice convention)
    so a 0..width-1 inclusive bbox shows up as (x1, _, width, _)
    consistently.
"""
from __future__ import annotations

from dataclasses import dataclass
from typing import List, Optional

# Class IDs matching combat_robot_msgs/Detection.msg
CLASS_UNKNOWN  = 0
CLASS_PERSON   = 1
CLASS_VEHICLE  = 2
CLASS_DRONE    = 3
CLASS_WEAPON   = 4
CLASS_ANIMAL   = 5

# ★ PATCH 2026-05-13 (L20): CLASS_UNKNOWN is no longer a "valid" output
# class. Production downstream consumers (fire-auth, mission BT) all
# treat UNKNOWN as garbage; surfacing it just risks confusion. Tests
# that explicitly check UNKNOWN must opt in with an explicit set.
VALID_CLASS_IDS = {
    CLASS_PERSON, CLASS_VEHICLE,
    CLASS_DRONE, CLASS_WEAPON, CLASS_ANIMAL,
}


@dataclass
class RawDetection:
    """Raw NPU output for one detected object."""
    class_id: int
    confidence: float
    # bbox in source-image pixel coords
    x1: int
    y1: int
    x2: int
    y2: int
    # ★ PATCH 2026-05-13 (C1): marks output produced by a stub /
    # mock runner. Stub data is real-looking enough to pass NMS
    # and confidence filters but MUST be discarded before the
    # fire-auth pipeline. post_process(drop_stub=True) handles
    # this by default.
    is_stub: bool = False

    @property
    def area(self) -> int:
        w = max(0, self.x2 - self.x1)
        h = max(0, self.y2 - self.y1)
        return w * h


def iou(a: RawDetection, b: RawDetection) -> float:
    """Intersection-over-union of two bboxes."""
    ix1 = max(a.x1, b.x1)
    iy1 = max(a.y1, b.y1)
    ix2 = min(a.x2, b.x2)
    iy2 = min(a.y2, b.y2)
    inter = max(0, ix2 - ix1) * max(0, iy2 - iy1)
    if inter == 0:
        return 0.0
    union = a.area + b.area - inter
    if union <= 0:
        return 0.0
    return inter / union


def filter_by_confidence(
    detections: List[RawDetection],
    min_confidence: float,
) -> List[RawDetection]:
    """Drop low-confidence detections."""
    return [d for d in detections if d.confidence >= min_confidence]


def filter_by_class(
    detections: List[RawDetection],
    allowed_class_ids: Optional[set] = None,
) -> List[RawDetection]:
    """Keep only allowed classes. None = keep all VALID_CLASS_IDS
    (which since the PATCH excludes CLASS_UNKNOWN)."""
    allowed = allowed_class_ids if allowed_class_ids is not None \
        else VALID_CLASS_IDS
    return [d for d in detections if d.class_id in allowed]


def filter_stub(
    detections: List[RawDetection],
) -> List[RawDetection]:
    """★ PATCH 2026-05-13 (C1): drop stub-marked detections."""
    return [d for d in detections if not d.is_stub]


def non_max_suppression(
    detections: List[RawDetection],
    iou_threshold: float = 0.5,
) -> List[RawDetection]:
    """Standard NMS — keep highest-confidence boxes, suppress overlapping.

    NMS is applied per-class: a vehicle and a person at the same location
    should both survive (they're different categories of detection).
    """
    # Group by class
    by_class: dict = {}
    for d in detections:
        by_class.setdefault(d.class_id, []).append(d)

    kept: List[RawDetection] = []
    for _cls, group in by_class.items():
        # Sort by descending confidence
        sorted_group = sorted(group, key=lambda x: -x.confidence)
        while sorted_group:
            head = sorted_group.pop(0)
            kept.append(head)
            sorted_group = [
                d for d in sorted_group
                if iou(head, d) < iou_threshold
            ]
    # Stable order: sort final list by confidence desc
    kept.sort(key=lambda x: -x.confidence)
    return kept


def clamp_bbox(
    d: RawDetection, width: int, height: int,
) -> Optional[RawDetection]:
    """Clamp bbox to image bounds; drop if it becomes empty.

    Also rejects bboxes that lie entirely outside the image — clamping
    those would yield a degenerate 1-pixel box at the edge, which
    misrepresents the detection's true position.

    PATCH 2026-05-13 (M9): right/bottom edge use the right-exclusive
    NumPy-slice convention. For a 100×100 image, valid x1 ∈ [0, 99],
    valid x2 ∈ [1, 100]. The previous code had inconsistent clamping
    (x1 to width-1, x2 to width) which is the right idea but only
    documented in passing.
    """
    # Entirely-outside check (before clamp)
    if d.x1 >= width or d.x2 <= 0 or d.y1 >= height or d.y2 <= 0:
        return None
    x1 = max(0, min(d.x1, width - 1))
    x2 = max(1, min(d.x2, width))      # right-exclusive
    y1 = max(0, min(d.y1, height - 1))
    y2 = max(1, min(d.y2, height))     # bottom-exclusive
    if x2 <= x1 or y2 <= y1:
        return None
    return RawDetection(d.class_id, d.confidence, x1, y1, x2, y2,
                         is_stub=d.is_stub)


def post_process(
    raw: List[RawDetection],
    image_width: int,
    image_height: int,
    min_confidence: float = 0.4,
    iou_threshold: float = 0.5,
    allowed_class_ids: Optional[set] = None,
    drop_stub: bool = True,
) -> List[RawDetection]:
    """Canonical post-processing pipeline:
       (★ PATCH: drop_stub →) confidence-filter → class-filter →
       bbox-clamp → NMS.

    PATCH 2026-05-13 (C1): drop_stub=True by default. Set False to
    keep stub markers (e.g. for diagnostic / health-tick publishing).
    """
    pipeline = raw
    if drop_stub:
        pipeline = filter_stub(pipeline)
    filtered = filter_by_confidence(pipeline, min_confidence)
    filtered = filter_by_class(filtered, allowed_class_ids)
    clamped  = []
    for d in filtered:
        c = clamp_bbox(d, image_width, image_height)
        if c is not None:
            clamped.append(c)
    return non_max_suppression(clamped, iou_threshold)
