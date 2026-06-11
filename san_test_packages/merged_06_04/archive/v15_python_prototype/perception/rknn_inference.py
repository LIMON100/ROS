"""
RKNN inference wrapper for RK3588 NPU (6 TOPS).

Domain: 군집제어 소형전술 로봇 (swarm-controlled small tactical robot).
Detector: YOLOv5s (640×640, COCO-80) — interim model
          (models/yolov5s-640-640_rk3588_251205_640.rknn)

Device runtime: rknn-toolkit-lite2 (rknnlite.api.RKNNLite) on RK3588.
Dev fallback: pure-NumPy stub returning empty detection list, so the rest
              of the pipeline can be exercised without the NPU.

NPU has 3 cores. We pin the detector to core 0 by default; cores 1–2 stay
free for future models (thermal, intent classifier, etc.) running in
parallel.

Why YOLOv5 and not YOLOv8: this is a temporary model, supplied pre-built
for RK3588. We'll swap in a domain-tuned model later. The post-process
below handles BOTH possible RKNN export shapes:
  • Single-tensor output (1, 25200, 85)             — exported with --rknn=False
  • Three-tensor output  (1, 3, H, W, 85) per scale — exported with --rknn=True
"""
from __future__ import annotations

import logging
from dataclasses import dataclass
from typing import List, Optional, Sequence

import numpy as np

try:
    from rknnlite.api import RKNNLite
    RKNN_AVAILABLE = True
except ImportError:
    RKNN_AVAILABLE = False
    RKNNLite = None


log = logging.getLogger(__name__)


# COCO 80 class names — order matches the YOLOv5 default training labels.
# We keep the full list so detections can be reported with human-readable
# names; the perception process filters by `classes_of_interest`.
COCO_LABELS = [
    "person", "bicycle", "car", "motorcycle", "airplane", "bus", "train",
    "truck", "boat", "traffic light", "fire hydrant", "stop sign",
    "parking meter", "bench", "bird", "cat", "dog", "horse", "sheep", "cow",
    "elephant", "bear", "zebra", "giraffe", "backpack", "umbrella", "handbag",
    "tie", "suitcase", "frisbee", "skis", "snowboard", "sports ball", "kite",
    "baseball bat", "baseball glove", "skateboard", "surfboard",
    "tennis racket", "bottle", "wine glass", "cup", "fork", "knife", "spoon",
    "bowl", "banana", "apple", "sandwich", "orange", "broccoli", "carrot",
    "hot dog", "pizza", "donut", "cake", "chair", "couch", "potted plant",
    "bed", "dining table", "toilet", "tv", "laptop", "mouse", "remote",
    "keyboard", "cell phone", "microwave", "oven", "toaster", "sink",
    "refrigerator", "book", "clock", "vase", "scissors", "teddy bear",
    "hair drier", "toothbrush",
]

# Anchor boxes for YOLOv5 (used when the export gives per-scale tensors).
# These are the default anchors used for COCO-trained YOLOv5s.
YOLOV5_ANCHORS = np.array([
    [[10, 13], [16, 30], [33, 23]],          # P3/8  (80×80)
    [[30, 61], [62, 45], [59, 119]],         # P4/16 (40×40)
    [[116, 90], [156, 198], [373, 326]],     # P5/32 (20×20)
], dtype=np.float32)
YOLOV5_STRIDES = (8, 16, 32)


@dataclass
class RawDetection:
    label: str
    class_id: int
    confidence: float
    bbox: np.ndarray      # (4,) x1, y1, x2, y2 in input-image pixel space


class RknnRunner:
    """One YOLOv5 model on one NPU core.

    Lifecycle:
        r = RknnRunner(model_path)
        r.load()
        for frame in stream:
            dets = r.infer(frame_bgr, min_conf=0.4, classes_of_interest={0,2,7})
        r.close()
    """

    CORE_MASKS = {
        "auto":  0,
        "core0": 1,
        "core1": 2,
        "core2": 4,
        "0_1_2": 7,
    }

    def __init__(self, model_path: str, core: str = "auto",
                 input_size: Sequence[int] = (640, 640),
                 labels: Optional[List[str]] = None,
                 iou_thresh: float = 0.45):
        self.model_path = model_path
        self.core = core
        self.input_size = tuple(input_size)
        self.labels = labels or COCO_LABELS
        self.iou_thresh = iou_thresh
        self._rknn: Optional[RKNNLite] = None
        # Precomputed anchor grids; rebuilt on first inference once we know the
        # actual output layout.
        self._grid_cache: dict = {}

    # ────────── Lifecycle ──────────
    def load(self) -> None:
        if not RKNN_AVAILABLE:
            log.info("rknnlite unavailable — using stub inference")
            return
        self._rknn = RKNNLite()
        ret = self._rknn.load_rknn(self.model_path)
        if ret != 0:
            raise RuntimeError(f"load_rknn failed: {ret}")
        ret = self._rknn.init_runtime(core_mask=self.CORE_MASKS[self.core])
        if ret != 0:
            raise RuntimeError(f"init_runtime failed: {ret}")
        log.info(f"loaded RKNN model: {self.model_path} on {self.core}")

    def close(self) -> None:
        if self._rknn is not None:
            self._rknn.release()
            self._rknn = None

    # ────────── Inference ──────────
    def infer(self,
              image_bgr: np.ndarray,
              min_conf: float = 0.40,
              classes_of_interest: Optional[set] = None
              ) -> List[RawDetection]:
        """Run one frame through the detector.

        image_bgr is HxWx3 uint8 (OpenCV convention). For RKNN models exported
        with ``--rknn`` the model expects NHWC uint8 directly, so we don't
        normalize — RKNN's quantization handles scale.

        ``classes_of_interest`` filters by COCO class id; pass None for all.
        """
        if not RKNN_AVAILABLE or self._rknn is None:
            return self._stub_infer(image_bgr, min_conf)

        h0, w0 = image_bgr.shape[:2]
        img, ratio, (pad_x, pad_y) = self._letterbox(image_bgr,
                                                     new_shape=self.input_size)
        # NHWC uint8 — RKNN handles RGB/BGR internally as configured at export.
        # If the model was exported assuming RGB, convert here.
        img_rgb = img[:, :, ::-1]
        outputs = self._rknn.inference(inputs=[img_rgb])
        return self._postprocess(outputs, min_conf, ratio, (pad_x, pad_y),
                                 (h0, w0), classes_of_interest)

    # ────────── Pre/post-process (testable in isolation) ──────────
    @staticmethod
    def _letterbox(img: np.ndarray,
                   new_shape=(640, 640),
                   color=(114, 114, 114)) -> tuple:
        """Aspect-preserving resize + pad — same as YOLOv5's letterbox."""
        h0, w0 = img.shape[:2]
        nh, nw = new_shape
        r = min(nh / h0, nw / w0)
        new_unpad_w, new_unpad_h = int(round(w0 * r)), int(round(h0 * r))
        pad_x = (nw - new_unpad_w) / 2
        pad_y = (nh - new_unpad_h) / 2
        # Lazy import: cv2 only needed in real inference path
        try:
            import cv2
            img = cv2.resize(img, (new_unpad_w, new_unpad_h),
                             interpolation=cv2.INTER_LINEAR)
            top, bottom = int(round(pad_y - 0.1)), int(round(pad_y + 0.1))
            left, right = int(round(pad_x - 0.1)), int(round(pad_x + 0.1))
            img = cv2.copyMakeBorder(img, top, bottom, left, right,
                                     cv2.BORDER_CONSTANT, value=color)
        except ImportError:
            # NumPy fallback (no aspect-aware resize); only used in stub.
            img = np.zeros((nh, nw, 3), dtype=np.uint8)
        return img, r, (pad_x, pad_y)

    def _postprocess(self, outputs, min_conf: float,
                     ratio: float, pad_xy: tuple,
                     orig_hw: tuple,
                     classes_of_interest: Optional[set]
                     ) -> List[RawDetection]:
        """Decode YOLOv5 outputs → RawDetection list in original-image coords.

        Handles two export shapes:
          • single tensor (1, N, 85) — already-decoded; we just NMS.
          • three tensors per scale  — sigmoid + grid decode + NMS.
        """
        if len(outputs) == 1 and outputs[0].ndim == 3:
            # Already-decoded form: (1, N, 85) where 85 = 4 box + 1 obj + 80 cls
            preds = outputs[0][0]   # (N, 85)
            boxes_xywh, scores, classes = self._decode_flat(preds, min_conf,
                                                             classes_of_interest)
        else:
            # Per-scale form: list of three tensors (1, 3, H, W, 85) or
            # (1, 255, H, W) — depending on the export. Unify to (1,3,H,W,85).
            preds_list = []
            for i, t in enumerate(outputs):
                t = self._to_3hw85(t)   # (1, 3, H, W, 85)
                pred = self._decode_scale(t, i)
                preds_list.append(pred)
            preds = np.concatenate(preds_list, axis=0)   # (M, 85)
            boxes_xywh, scores, classes = self._decode_flat(preds, min_conf,
                                                             classes_of_interest)

        if len(boxes_xywh) == 0:
            return []

        # xywh → xyxy in input-image (letterboxed) space
        x = boxes_xywh[:, 0]
        y = boxes_xywh[:, 1]
        w = boxes_xywh[:, 2]
        h = boxes_xywh[:, 3]
        boxes_xyxy = np.stack(
            [x - w / 2, y - h / 2, x + w / 2, y + h / 2], axis=1)

        # NMS (per-class)
        keep = self._nms(boxes_xyxy, scores, classes, self.iou_thresh)
        boxes_xyxy = boxes_xyxy[keep]
        scores     = scores[keep]
        classes    = classes[keep]

        # Undo letterbox: subtract pad, divide by ratio → original image coords
        pad_x, pad_y = pad_xy
        boxes_xyxy[:, [0, 2]] = (boxes_xyxy[:, [0, 2]] - pad_x) / ratio
        boxes_xyxy[:, [1, 3]] = (boxes_xyxy[:, [1, 3]] - pad_y) / ratio
        h0, w0 = orig_hw
        boxes_xyxy[:, 0] = np.clip(boxes_xyxy[:, 0], 0, w0)
        boxes_xyxy[:, 2] = np.clip(boxes_xyxy[:, 2], 0, w0)
        boxes_xyxy[:, 1] = np.clip(boxes_xyxy[:, 1], 0, h0)
        boxes_xyxy[:, 3] = np.clip(boxes_xyxy[:, 3], 0, h0)

        out = []
        for i in range(len(boxes_xyxy)):
            cid = int(classes[i])
            label = (self.labels[cid] if 0 <= cid < len(self.labels)
                     else f"class_{cid}")
            out.append(RawDetection(
                label=label, class_id=cid,
                confidence=float(scores[i]),
                bbox=boxes_xyxy[i].astype(np.float32),
            ))
        return out

    @staticmethod
    def _to_3hw85(t: np.ndarray) -> np.ndarray:
        """Coerce one scale tensor into shape (1, 3, H, W, 85)."""
        if t.ndim == 5:
            return t
        if t.ndim == 4:
            # (1, 255, H, W) → (1, 3, 85, H, W) → (1, 3, H, W, 85)
            n, c, h, w = t.shape
            assert c == 255, f"unexpected channel count {c}"
            return t.reshape(n, 3, 85, h, w).transpose(0, 1, 3, 4, 2)
        raise ValueError(f"unsupported output shape: {t.shape}")

    def _decode_scale(self, t: np.ndarray, scale_idx: int) -> np.ndarray:
        """Decode one scale's grid into (N, 85) absolute coords."""
        n, na, gh, gw, no = t.shape
        stride = YOLOV5_STRIDES[scale_idx]
        anchors = YOLOV5_ANCHORS[scale_idx]    # (3, 2)

        # Sigmoid the whole output
        t = self._sigmoid(t)

        # Grid (cx, cy)
        key = (scale_idx, gh, gw)
        if key not in self._grid_cache:
            yv, xv = np.meshgrid(np.arange(gh), np.arange(gw), indexing="ij")
            grid = np.stack((xv, yv), axis=-1).astype(np.float32)
            self._grid_cache[key] = grid
        grid = self._grid_cache[key]            # (gh, gw, 2)

        # YOLOv5 box decode
        # xy = (sigmoid(tx) * 2 - 0.5 + cx) * stride
        # wh = (sigmoid(tw) * 2)^2 * anchor
        xy = (t[..., 0:2] * 2 - 0.5 + grid[None, None]) * stride
        wh = (t[..., 2:4] * 2) ** 2 * anchors.reshape(1, na, 1, 1, 2)
        obj = t[..., 4:5]
        cls = t[..., 5:]
        decoded = np.concatenate([xy, wh, obj, cls], axis=-1)
        return decoded.reshape(-1, no)          # (n*na*gh*gw, 85)

    @staticmethod
    def _sigmoid(x: np.ndarray) -> np.ndarray:
        return 1.0 / (1.0 + np.exp(-x))

    @staticmethod
    def _decode_flat(preds: np.ndarray, min_conf: float,
                     classes_of_interest: Optional[set]
                     ) -> tuple:
        """Apply objectness × class confidence threshold."""
        # Objectness gate first (cheap)
        obj = preds[:, 4]
        keep = obj > min_conf
        preds = preds[keep]
        if len(preds) == 0:
            return (np.zeros((0, 4), np.float32),
                    np.zeros((0,), np.float32),
                    np.zeros((0,), np.int32))
        cls_scores = preds[:, 5:] * preds[:, 4:5]
        class_id = np.argmax(cls_scores, axis=1)
        score = cls_scores[np.arange(len(cls_scores)), class_id]
        keep = score > min_conf
        if classes_of_interest is not None:
            keep &= np.isin(class_id, list(classes_of_interest))
        return preds[keep, :4], score[keep], class_id[keep].astype(np.int32)

    @staticmethod
    def _nms(boxes: np.ndarray, scores: np.ndarray, classes: np.ndarray,
             iou_thresh: float) -> np.ndarray:
        """Per-class greedy NMS — pure numpy, no torchvision."""
        if len(boxes) == 0:
            return np.zeros(0, dtype=np.int32)
        # Per-class offset trick: shift each class's boxes far apart so a
        # single global NMS is equivalent to per-class NMS.
        max_coord = boxes.max() if len(boxes) else 0.0
        offsets = classes.astype(np.float32) * (max_coord + 1)
        b = boxes + offsets[:, None]
        x1, y1, x2, y2 = b[:, 0], b[:, 1], b[:, 2], b[:, 3]
        areas = (x2 - x1) * (y2 - y1)
        order = scores.argsort()[::-1]
        keep = []
        while len(order) > 0:
            i = order[0]
            keep.append(i)
            if len(order) == 1:
                break
            xx1 = np.maximum(x1[i], x1[order[1:]])
            yy1 = np.maximum(y1[i], y1[order[1:]])
            xx2 = np.minimum(x2[i], x2[order[1:]])
            yy2 = np.minimum(y2[i], y2[order[1:]])
            w = np.maximum(0.0, xx2 - xx1)
            h = np.maximum(0.0, yy2 - yy1)
            inter = w * h
            iou = inter / (areas[i] + areas[order[1:]] - inter + 1e-9)
            order = order[1:][iou <= iou_thresh]
        return np.array(keep, dtype=np.int32)

    # ────────── Stub (no NPU) ──────────
    @staticmethod
    def _stub_infer(img: np.ndarray, min_conf: float) -> List[RawDetection]:
        return []
