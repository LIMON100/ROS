#pragma once
#include "IDetector.h"
#include "ModelIo.h"

#include <cstddef>
#include <cstdint>

namespace riposte {

// Model-input preprocessing: NV12 frame/tile -> letterboxed RGB888 square
// (SDD §4.4.3, S-9). No OpenCV anywhere in this process.
//
// Two implementations share one contract:
//  - CPU reference (Preproc.cpp, always compiled): plain bilinear + BT.601,
//    written for correctness, host unit-tested (test/test_preproc.cpp). This is
//    the accuracy baseline, and the SIL/bring-up fallback.
//  - RGA (PreprocRga.cpp, RIPOSTE_WITH_RGA): RK3588 2D engine does the
//    scale + colour conversion in one pass for the production frame rate. At
//    bring-up its output is compared pixel-wise against the CPU reference.
//
// Contract for both:
//  - `f` must be NV12 (Y plane: height rows of stride; interleaved UV plane at
//    data + height*stride, same stride) and match lb.src_w/src_h.
//  - `dst` is a model_size x model_size x 3 (RGB888) buffer of dst_size bytes.
//  - ONLY the letterbox content rectangle (pad_x, pad_y, content_w, content_h)
//    is written. The caller owns the padding fill: it pre-fills the buffer with
//    LETTERBOX_PAD_VALUE and re-fills it whenever the letterbox geometry
//    changes (wide frame vs search tile), so unchanged geometry costs nothing
//    per frame. See HailoDetector.
//  - Returns false (writing nothing) on any contract violation — a wrong-size
//    input must never produce a plausible-looking tensor.

bool letterbox_nv12_rgb888(const Frame& f, const Letterbox& lb, uint8_t* dst,
                           std::size_t dst_size);

// Plain STRETCH resize NV12 -> dst_w x dst_h RGB888, for ReID crops (TR-B).
// Deliberately not letterboxed: the S-6 aspect argument does not apply here —
// nothing measures the crop geometrically, an embedding only needs every crop
// of the same object distorted the SAME way, and stretch-to-fixed-size is what
// ReID models are trained with. Same contract style as above: writes the whole
// dst buffer, returns false (writing nothing) on any contract violation.
bool resize_nv12_rgb888(const Frame& f, uint8_t* dst, std::size_t dst_size, int dst_w,
                        int dst_h);

#ifdef RIPOSTE_WITH_RGA
bool letterbox_nv12_rgb888_rga(const Frame& f, const Letterbox& lb, uint8_t* dst,
                               std::size_t dst_size);
#endif

} // namespace riposte
