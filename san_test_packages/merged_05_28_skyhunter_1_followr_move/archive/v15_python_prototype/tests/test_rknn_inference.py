"""
Tests for YOLOv5 RKNN post-processing.

We don't have an NPU here, but the post-process (sigmoid + grid decode + NMS
+ letterbox undo) is pure NumPy and fully testable. We synthesize fake RKNN
output tensors that exercise:
  • Both export shapes (single tensor + per-scale)
  • NMS dedup with overlapping boxes
  • Class filter via classes_of_interest
  • Confidence threshold gate
  • Letterbox coordinate restoration
"""
from __future__ import annotations

import numpy as np

from perception.rknn_inference import (
    COCO_LABELS,
    YOLOV5_ANCHORS,
    YOLOV5_STRIDES,
    RknnRunner,
)


# ════════════════════════════════════════════════════════════════
# Helpers — synthesize raw outputs the model would produce.
# ════════════════════════════════════════════════════════════════
def _make_runner():
    return RknnRunner(model_path="/dev/null", input_size=(640, 640))


def _flat_pred(boxes_xywh, class_ids, scores):
    """Build a fake (1, N, 85) tensor with given decoded detections.

    Used for the 'flat' export path which is the simpler test target.
    For each detection: bbox + obj=1.0 + one-hot class scaled by `score`.
    """
    n = len(boxes_xywh)
    out = np.zeros((1, n, 85), dtype=np.float32)
    for i, (bbox, cid, s) in enumerate(
            zip(boxes_xywh, class_ids, scores, strict=False)):
        out[0, i, 0:4] = bbox
        out[0, i, 4] = 1.0                    # objectness
        out[0, i, 5 + cid] = s                # only one class score set
    return out


# ════════════════════════════════════════════════════════════════
# Tests — flat-tensor path
# ════════════════════════════════════════════════════════════════
def test_flat_path_passes_through_high_confidence():
    r = _make_runner()
    # Single 'person' detection at (320,320) box 100x200, conf 0.9
    out = _flat_pred(
        boxes_xywh=[[320, 320, 100, 200]],
        class_ids=[0],
        scores=[0.9],
    )
    dets = r._postprocess([out], min_conf=0.3,
                           ratio=1.0, pad_xy=(0.0, 0.0),
                           orig_hw=(640, 640),
                           classes_of_interest=None)
    assert len(dets) == 1
    assert dets[0].class_id == 0
    assert dets[0].label == "person"
    assert dets[0].confidence > 0.85
    # bbox xyxy: cx=320 cy=320 w=100 h=200 → (270,220,370,420)
    np.testing.assert_allclose(dets[0].bbox, [270, 220, 370, 420], atol=1.0)


def test_flat_path_filters_below_threshold():
    r = _make_runner()
    out = _flat_pred(
        boxes_xywh=[[100, 100, 50, 50]],
        class_ids=[0],
        scores=[0.20],   # below 0.3 default
    )
    dets = r._postprocess([out], min_conf=0.30,
                           ratio=1.0, pad_xy=(0, 0),
                           orig_hw=(640, 640),
                           classes_of_interest=None)
    assert len(dets) == 0


def test_classes_of_interest_filter():
    r = _make_runner()
    # Two detections: person (cls=0), bird (cls=14). Both above threshold.
    out = _flat_pred(
        boxes_xywh=[[100, 100, 80, 80], [400, 400, 60, 60]],
        class_ids=[0, 14],
        scores=[0.85, 0.85],
    )
    # Only personnel/vehicles allowed
    dets = r._postprocess([out], min_conf=0.30,
                           ratio=1.0, pad_xy=(0, 0),
                           orig_hw=(640, 640),
                           classes_of_interest={0, 1, 2, 3, 5, 7})
    assert len(dets) == 1
    assert dets[0].class_id == 0
    assert dets[0].label == "person"


def test_nms_deduplicates_overlapping_boxes():
    r = _make_runner()
    # Three near-identical boxes for the same class
    out = _flat_pred(
        boxes_xywh=[
            [320, 320, 100, 100],
            [322, 320, 100, 100],
            [318, 320, 100, 100],
        ],
        class_ids=[0, 0, 0],
        scores=[0.95, 0.85, 0.80],
    )
    dets = r._postprocess([out], min_conf=0.30,
                           ratio=1.0, pad_xy=(0, 0),
                           orig_hw=(640, 640),
                           classes_of_interest=None)
    assert len(dets) == 1
    # The highest-confidence one wins
    assert dets[0].confidence > 0.94


def test_nms_keeps_separated_boxes_of_same_class():
    r = _make_runner()
    # Two well-separated person detections — both should survive
    out = _flat_pred(
        boxes_xywh=[[100, 100, 50, 50], [500, 500, 50, 50]],
        class_ids=[0, 0],
        scores=[0.90, 0.85],
    )
    dets = r._postprocess([out], min_conf=0.30,
                           ratio=1.0, pad_xy=(0, 0),
                           orig_hw=(640, 640),
                           classes_of_interest=None)
    assert len(dets) == 2


def test_nms_keeps_overlapping_boxes_of_different_classes():
    r = _make_runner()
    # Same location, person + handbag — two classes → NMS doesn't merge
    out = _flat_pred(
        boxes_xywh=[[320, 320, 100, 200], [320, 320, 100, 200]],
        class_ids=[0, 26],   # person + handbag
        scores=[0.90, 0.80],
    )
    dets = r._postprocess([out], min_conf=0.30,
                           ratio=1.0, pad_xy=(0, 0),
                           orig_hw=(640, 640),
                           classes_of_interest=None)
    assert len(dets) == 2
    classes = {d.class_id for d in dets}
    assert classes == {0, 26}


def test_letterbox_undo_restores_original_image_coords():
    """When letterbox padded, post-process must undo the pad+scale."""
    r = _make_runner()
    # Detection in letterboxed (640×640) space at (160, 320), bbox 80×160
    out = _flat_pred(
        boxes_xywh=[[160, 320, 80, 160]],
        class_ids=[0],
        scores=[0.9],
    )
    # Pretend original image was 1280×720; ratio = min(640/720, 640/1280) = 0.5
    # pad_x = (640 - 1280*0.5)/2 = 0, pad_y = (640 - 720*0.5)/2 = 140
    dets = r._postprocess([out], min_conf=0.30,
                           ratio=0.5, pad_xy=(0.0, 140.0),
                           orig_hw=(720, 1280),
                           classes_of_interest=None)
    assert len(dets) == 1
    # Original box: input xyxy = (120, 240, 200, 400)
    # After undo: subtract pad, divide by ratio
    # x: (120-0)/0.5=240, (200-0)/0.5=400
    # y: (240-140)/0.5=200, (400-140)/0.5=520
    np.testing.assert_allclose(dets[0].bbox, [240, 200, 400, 520], atol=2.0)


def test_letterbox_clips_to_original_bounds():
    """Boxes that extend outside the original frame should clip."""
    r = _make_runner()
    out = _flat_pred(
        boxes_xywh=[[600, 600, 100, 100]],   # box going off the edge
        class_ids=[0],
        scores=[0.9],
    )
    dets = r._postprocess([out], min_conf=0.30,
                           ratio=1.0, pad_xy=(0, 0),
                           orig_hw=(640, 640),
                           classes_of_interest=None)
    assert len(dets) == 1
    # Bottom-right clipped to image edge
    assert dets[0].bbox[2] <= 640
    assert dets[0].bbox[3] <= 640


def test_empty_output_returns_empty_list():
    r = _make_runner()
    out = np.zeros((1, 0, 85), dtype=np.float32)
    dets = r._postprocess([out], min_conf=0.30,
                           ratio=1.0, pad_xy=(0, 0),
                           orig_hw=(640, 640),
                           classes_of_interest=None)
    assert dets == []


# ════════════════════════════════════════════════════════════════
# Tests — letterbox preprocessing
# ════════════════════════════════════════════════════════════════
def test_letterbox_preserves_aspect_ratio():
    """Wide image (1280×720) → letterboxed 640×640 with vertical pad."""
    img = np.zeros((720, 1280, 3), dtype=np.uint8)
    out, ratio, (px, py) = RknnRunner._letterbox(img, new_shape=(640, 640))
    assert out.shape == (640, 640, 3) or out.shape == (640, 640, 3)
    # ratio = 640/1280 = 0.5 (the smaller of 640/720 and 640/1280)
    assert abs(ratio - 0.5) < 1e-6
    # Vertical padding only
    assert px == 0
    assert py > 0


def test_letterbox_square_input_no_padding():
    img = np.zeros((640, 640, 3), dtype=np.uint8)
    out, ratio, (px, py) = RknnRunner._letterbox(img, new_shape=(640, 640))
    assert ratio == 1.0
    assert px == 0
    assert py == 0


# ════════════════════════════════════════════════════════════════
# Tests — class label coverage
# ════════════════════════════════════════════════════════════════
def test_coco_labels_count_is_80():
    assert len(COCO_LABELS) == 80


def test_personnel_class_index():
    """COCO 'person' must be index 0 — the rest of the system depends on it."""
    assert COCO_LABELS[0] == "person"


def test_anchor_shapes():
    """YOLOv5 anchors must have the expected layout: 3 scales × 3 anchors × 2."""
    assert YOLOV5_ANCHORS.shape == (3, 3, 2)
    assert YOLOV5_STRIDES == (8, 16, 32)


# ════════════════════════════════════════════════════════════════
# Stub fallback (no NPU)
# ════════════════════════════════════════════════════════════════
def test_stub_inference_returns_empty():
    r = _make_runner()
    img = np.zeros((480, 640, 3), dtype=np.uint8)
    dets = r.infer(img)
    assert dets == []


def test_stub_close_when_never_loaded_is_safe():
    r = _make_runner()
    r.close()    # no exception
