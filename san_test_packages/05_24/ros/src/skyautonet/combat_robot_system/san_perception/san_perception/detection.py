"""SAN v1.5 Phase 2-E Turn 11-12 — Detection post-processing (pure logic).

NMS (non-max suppression), confidence filtering, class mapping.
No rclpy, no ROS, no NPU SDK — pytest-testable in isolation.
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

VALID_CLASS_IDS = {
    CLASS_UNKNOWN, CLASS_PERSON, CLASS_VEHICLE,
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
    """Keep only allowed classes. None = keep all VALID_CLASS_IDS."""
    allowed = allowed_class_ids or VALID_CLASS_IDS
    return [d for d in detections if d.class_id in allowed]


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
    """
    # Entirely-outside check (before clamp)
    if d.x1 >= width or d.x2 <= 0 or d.y1 >= height or d.y2 <= 0:
        return None
    x1 = max(0, min(d.x1, width - 1))
    x2 = max(0, min(d.x2, width))
    y1 = max(0, min(d.y1, height - 1))
    y2 = max(0, min(d.y2, height))
    if x2 <= x1 or y2 <= y1:
        return None
    return RawDetection(d.class_id, d.confidence, x1, y1, x2, y2)


def post_process(
    raw: List[RawDetection],
    image_width: int,
    image_height: int,
    min_confidence: float = 0.4,
    iou_threshold: float = 0.5,
    allowed_class_ids: Optional[set] = None,
) -> List[RawDetection]:
    """Canonical post-processing pipeline:
       confidence-filter → class-filter → bbox-clamp → NMS.

    Returns at most N detections after suppression. Stable, deterministic.
    """
    filtered = filter_by_confidence(raw, min_confidence)
    filtered = filter_by_class(filtered, allowed_class_ids)
    clamped  = []
    for d in filtered:
        c = clamp_bbox(d, image_width, image_height)
        if c is not None:
            clamped.append(c)
    return non_max_suppression(clamped, iou_threshold)
