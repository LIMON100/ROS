#pragma once
#include "IDetector.h"

#include <cstddef>
#include <vector>

namespace riposte {

// Model input/output plumbing: fitting an arbitrary image rectangle into the
// network's square input, decoding raw output tensors into Detections, and
// suppressing duplicates.
//
// None of this touches HailoRT — it is the layout maths AROUND the accelerator,
// so it compiles and unit-tests on a dev PC and leaves the HailoRT adapter with
// nothing but device calls. That split is deliberate: the parts that can be
// wrong in a way tests can catch are all on this side.

// ---------------------------------------------------------------------------
// Letterbox: fit src_w x src_h into a model_size x model_size input WITHOUT
// changing the aspect ratio, padding the leftover strips.
//
// A plain stretch would be simpler and is wrong here. The tiles the search
// scheduler cuts are not square — a 3x3 tile of 1280x720 is 426x240 — so
// stretching one to 640x640 scales width and height by different factors. Every
// bbox then comes back with a distorted width/height ratio, and the tracker's
// width-normalized `size` feeds the estimator's monocular range: the distortion
// lands directly in the target's reported distance, which is the one number the
// whole 300 m control session geometry rests on.
// ---------------------------------------------------------------------------
struct Letterbox {
    float scale = 1.F; // source pixels -> model pixels
    int pad_x = 0;     // model-pixel offset of the image content
    int pad_y = 0;
    int content_w = 0; // model pixels actually covered by the image
    int content_h = 0;
    int model_size = 0;
    int src_w = 0;
    int src_h = 0;

    bool valid() const { return model_size > 0 && content_w > 0 && content_h > 0; }
};

// Returns an invalid Letterbox on non-positive inputs.
Letterbox letterbox_fit(int src_w, int src_h, int model_size);

// Source-normalized point -> model-input-normalized point.
void letterbox_forward(const Letterbox& lb, float src_nx, float src_ny, float& mx,
                       float& my);

// Maps a detection from MODEL-INPUT normalized coordinates back to SOURCE-image
// normalized coordinates, undoing both the scale and the padding. Returns false
// when the detection's centre lies in the padding strips — that is not a target,
// it is the network firing on the letterbox fill.
bool letterbox_undo(const Letterbox& lb, Detection& d);

// ---------------------------------------------------------------------------
// Non-maximum suppression, per class, greedy by descending score.
//
// Runs in MODEL space, where the input is square so width and height share a
// divisor and IoU is meaningful. Running it after letterbox_undo on a
// non-square source would compare boxes in two different normalizations and
// systematically mis-measure overlap along one axis.
// ---------------------------------------------------------------------------
void nms(std::vector<Detection>& dets, float iou_threshold);

// ---------------------------------------------------------------------------
// Decodes a YOLOv8-style detection head into MODEL-INPUT normalized detections.
//
// Expected tensor layout: [4 + num_classes, num_anchors], row-major, so element
// (r, a) is tensor[(r * num_anchors) + a]. Rows 0..3 are cx, cy, w, h in model
// PIXELS; rows 4.. are per-class scores already in [0,1]. Each anchor
// contributes at most one detection, for its highest-scoring class.
//
// ASSUMPTION: this is the standard YOLOv8 export layout. The real .hef output
// layout must be confirmed at bring-up — a transposed head, or one with NMS
// already baked in, is just as common. Isolating the assumption here means a
// layout surprise is a change to this function and nowhere else.
// ---------------------------------------------------------------------------
void decode_yolov8(const float* tensor, std::size_t element_count, int num_classes,
                   int num_anchors, int model_size, float score_threshold,
                   std::vector<Detection>& out);

} // namespace riposte
