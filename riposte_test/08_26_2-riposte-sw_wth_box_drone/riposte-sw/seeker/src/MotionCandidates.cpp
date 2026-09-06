#include "MotionCandidates.h"

#include "IDetector.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace riposte {

namespace {
// A fit that lost consensus on the guided pass means the scene is not described
// by one background motion; emitting candidates from the earlier, unvalidated
// fit would be exactly the fabricated-target case this guards against.
MotionCandidates::Result r_empty(const MotionCandidates::Result& r) {
    MotionCandidates::Result e;
    e.ego = r.ego;
    e.ego.ok = false;
    return e;
}

// NV12 fourcc, matching what CameraIngest negotiates.
constexpr uint32_t FOURCC_NV12 = 0x3231564E;

// Bresenham circle of radius 3 — the FAST-9 ring. A corner is a run of 9
// contiguous ring pixels all brighter (or all darker) than the centre by the
// threshold; 9 of 16 is what makes the test reject edges, which a plain
// gradient magnitude would happily accept and which would then dominate the
// correspondence set with points that slide along the edge.
constexpr int RING = 16;
constexpr int RING_DX[RING] = {0, 1, 2, 3, 3, 3, 2, 1, 0, -1, -2, -3, -3, -3, -2, -1};
constexpr int RING_DY[RING] = {-3, -3, -2, -1, 0, 1, 2, 3, 3, 3, 2, 1, 0, -1, -2, -3};
constexpr int FAST_ARC = 9;
// Corners are kept one per cell of this size (decimated px) so the set is
// spatially spread: a textured corner of the image would otherwise supply every
// correspondence and leave the global fit unconstrained elsewhere.
constexpr int CELL = 16;
// A match is accepted only if the best patch cost is this much better than the
// best cost outside its neighbourhood — repetitive texture (rooftops, water)
// otherwise yields confident-looking but arbitrary correspondences.
constexpr float MATCH_UNIQUENESS = 0.8F;
// Blob pass ignores this many pixels at the frame edge: the warp source for a
// border pixel can fall outside the previous frame, where the residual is
// undefined rather than moving.
constexpr int BORDER_MARGIN = 4;
constexpr int MIN_BLOB_PIXELS = 4;

} // namespace

// G16.6 deviation: the closed-form solve is a single accumulation followed by
// one division; the parts are not separately meaningful (G16.6)
// NOLINTNEXTLINE(readability-function-size)
bool solve_similarity(const std::vector<float>& src_x, const std::vector<float>& src_y,
                      const std::vector<float>& dst_x, const std::vector<float>& dst_y,
                      float& a, float& b, float& tx, float& ty) {
    const std::size_t n = src_x.size();
    if (n < 2 || src_y.size() != n || dst_x.size() != n || dst_y.size() != n) {
        return false;
    }
    double mpx = 0.0;
    double mpy = 0.0;
    double mqx = 0.0;
    double mqy = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        mpx += src_x[i];
        mpy += src_y[i];
        mqx += dst_x[i];
        mqy += dst_y[i];
    }
    const double inv_n = 1.0 / static_cast<double>(n);
    mpx *= inv_n;
    mpy *= inv_n;
    mqx *= inv_n;
    mqy *= inv_n;

    double sxx = 0.0; // sum p'.q'
    double sxy = 0.0; // sum cross(p', q')
    double spp = 0.0; // sum |p'|^2
    for (std::size_t i = 0; i < n; ++i) {
        const double px = src_x[i] - mpx;
        const double py = src_y[i] - mpy;
        const double qx = dst_x[i] - mqx;
        const double qy = dst_y[i] - mqy;
        sxx += (px * qx) + (py * qy);
        sxy += (px * qy) - (py * qx);
        spp += (px * px) + (py * py);
    }
    // Degenerate: every source point coincides, so rotation and scale are not
    // observable. Returning false is the only honest answer — a normalized
    // "identity" here would silently claim the background did not move.
    if (!(spp > 1e-6)) {
        return false;
    }
    const double ea = sxx / spp;
    const double eb = sxy / spp;
    a = static_cast<float>(ea);
    b = static_cast<float>(eb);
    tx = static_cast<float>(mqx - ((ea * mpx) - (eb * mpy)));
    ty = static_cast<float>(mqy - ((eb * mpx) + (ea * mpy)));
    return std::isfinite(a) && std::isfinite(b) && std::isfinite(tx) && std::isfinite(ty);
}

void MotionCandidates::decimate_luma(const Frame& f, std::vector<uint8_t>& out, int& dw,
                                     int& dh) const {
    const int d = std::max(1, p_.decimate);
    dw = f.width / d;
    dh = f.height / d;
    out.assign(static_cast<std::size_t>(dw) * static_cast<std::size_t>(dh), 0);
    const std::size_t stride =
        (f.stride != 0U) ? f.stride : static_cast<std::size_t>(f.width);
    // Box average: decimation without it aliases high-frequency texture into
    // the corner detector, which then reports corners that move with the
    // sampling grid rather than with the scene.
    for (int y = 0; y < dh; ++y) {
        for (int x = 0; x < dw; ++x) {
            uint32_t sum = 0;
            for (int j = 0; j < d; ++j) {
                const uint8_t* row =
                    f.data + (static_cast<std::size_t>((y * d) + j) * stride);
                for (int i = 0; i < d; ++i) {
                    sum += row[(x * d) + i];
                }
            }
            out[(static_cast<std::size_t>(y) * static_cast<std::size_t>(dw)) +
                static_cast<std::size_t>(x)] =
                static_cast<uint8_t>(sum / static_cast<uint32_t>(d * d));
        }
    }
}

void MotionCandidates::find_corners(const std::vector<uint8_t>& img, int w, int h,
                                    std::vector<int>& xs, std::vector<int>& ys) const {
    xs.clear();
    ys.clear();
    const int margin = 3 + p_.patch_radius + p_.search_radius;
    if (w <= 2 * margin || h <= 2 * margin) {
        return; // too small to hold a patch and its search range
    }
    const int cells_x = ((w + CELL) - 1) / CELL;
    const int cells_y = ((h + CELL) - 1) / CELL;
    std::vector<int> best_score(
        static_cast<std::size_t>(cells_x) * static_cast<std::size_t>(cells_y), 0);
    std::vector<int> best_x(best_score.size(), -1);
    std::vector<int> best_y(best_score.size(), -1);

    for (int y = margin; y < h - margin; ++y) {
        for (int x = margin; x < w - margin; ++x) {
            const int c =
                img[(static_cast<std::size_t>(y) * static_cast<std::size_t>(w)) +
                    static_cast<std::size_t>(x)];
            int bright = 0;
            int dark = 0;
            int best_run_b = 0;
            int best_run_d = 0;
            // Two laps so a run wrapping the ring end is still contiguous.
            for (int k = 0; k < 2 * RING; ++k) {
                const int idx = k % RING;
                const int v = img[(static_cast<std::size_t>(y + RING_DY[idx]) *
                                   static_cast<std::size_t>(w)) +
                                  static_cast<std::size_t>(x + RING_DX[idx])];
                bright = (v > c + p_.fast_threshold) ? bright + 1 : 0;
                dark = (v < c - p_.fast_threshold) ? dark + 1 : 0;
                best_run_b = std::max(best_run_b, bright);
                best_run_d = std::max(best_run_d, dark);
            }
            if (best_run_b < FAST_ARC && best_run_d < FAST_ARC) {
                continue;
            }
            int strength = 0;
            for (int k = 0; k < RING; ++k) {
                const int v = img[(static_cast<std::size_t>(y + RING_DY[k]) *
                                   static_cast<std::size_t>(w)) +
                                  static_cast<std::size_t>(x + RING_DX[k])];
                strength += std::abs(v - c);
            }
            const std::size_t cell =
                (static_cast<std::size_t>(y / CELL) * static_cast<std::size_t>(cells_x)) +
                static_cast<std::size_t>(x / CELL);
            if (strength > best_score[cell]) {
                best_score[cell] = strength;
                best_x[cell] = x;
                best_y[cell] = y;
            }
        }
    }
    for (std::size_t i = 0; i < best_x.size(); ++i) {
        if (best_x[i] >= 0) {
            xs.push_back(best_x[i]);
            ys.push_back(best_y[i]);
        }
    }
}

// G16.6 deviation: one correspondence search per corner — cost map, winner,
// uniqueness and sub-pixel refinement are one measurement (G16.6)
// NOLINTNEXTLINE(readability-function-size)
void MotionCandidates::match_corners(const std::vector<uint8_t>& a,
                                     const std::vector<uint8_t>& b, int w, int h,
                                     const std::vector<int>& xs,
                                     const std::vector<int>& ys, const EgoMotion* guide) {
    matches_.clear();
    const int pr = p_.patch_radius;
    const int sr = (guide != nullptr) ? std::max(1, p_.guided_radius) : p_.search_radius;
    const int span = (2 * sr) + 1;
    cost_map_.assign(static_cast<std::size_t>(span) * static_cast<std::size_t>(span), 0);
    for (std::size_t i = 0; i < xs.size(); ++i) {
        const int x0 = xs[i];
        const int y0 = ys[i];
        // Centre of the search: zero on the first pass, the model's prediction
        // on the guided one.
        int gx = 0;
        int gy = 0;
        if (guide != nullptr) {
            const float qx = ((guide->a * static_cast<float>(x0)) -
                              (guide->b * static_cast<float>(y0))) +
                             guide->tx;
            const float qy = ((guide->b * static_cast<float>(x0)) +
                              (guide->a * static_cast<float>(y0))) +
                             guide->ty;
            gx = static_cast<int>(std::lround(qx - static_cast<float>(x0)));
            gy = static_cast<int>(std::lround(qy - static_cast<float>(y0)));
        }
        // The patch must sit inside BOTH frames for every offset scanned.
        if (x0 - pr < 0 || y0 - pr < 0 || x0 + pr >= w || y0 + pr >= h ||
            x0 + gx - pr - sr < 0 || y0 + gy - pr - sr < 0 || x0 + gx + pr + sr >= w ||
            y0 + gy + pr + sr >= h) {
            continue;
        }
        // The whole cost map first. Judging the winner, the runner-up and the
        // sub-pixel fit during a PARTIAL scan was wrong: the runner-up was
        // measured against whichever offset happened to lead at that moment, so
        // the ambiguity test both rejected good matches and admitted ambiguous
        // ones depending on scan order.
        int best = INT32_MAX;
        int best_dx = 0;
        int best_dy = 0;
        for (int dy = -sr; dy <= sr; ++dy) {
            for (int dx = -sr; dx <= sr; ++dx) {
                int cost = 0;
                for (int py = -pr; py <= pr; ++py) {
                    const std::size_t arow =
                        (static_cast<std::size_t>(y0 + py) * static_cast<std::size_t>(w));
                    const std::size_t brow =
                        (static_cast<std::size_t>(y0 + gy + py + dy) *
                         static_cast<std::size_t>(w));
                    for (int px = -pr; px <= pr; ++px) {
                        const int av = a[arow + static_cast<std::size_t>(x0 + px)];
                        const int bv =
                            b[brow + static_cast<std::size_t>(x0 + gx + px + dx)];
                        cost += std::abs(av - bv);
                    }
                }
                cost_map_[(static_cast<std::size_t>(dy + sr) *
                           static_cast<std::size_t>(span)) +
                          static_cast<std::size_t>(dx + sr)] = cost;
                if (cost < best) {
                    best = cost;
                    best_dx = dx;
                    best_dy = dy;
                }
            }
        }
        int runner_up = INT32_MAX;
        for (int dy = -sr; dy <= sr; ++dy) {
            for (int dx = -sr; dx <= sr; ++dx) {
                if (std::abs(dx - best_dx) <= 2 && std::abs(dy - best_dy) <= 2) {
                    continue; // the winner's own basin, not a rival alignment
                }
                runner_up =
                    std::min(runner_up, cost_map_[(static_cast<std::size_t>(dy + sr) *
                                                   static_cast<std::size_t>(span)) +
                                                  static_cast<std::size_t>(dx + sr)]);
            }
        }
        // Ambiguous match (repetitive texture): a wrong correspondence biases
        // the global fit, and a biased alignment turns the background itself
        // into residual — it invents candidates everywhere.
        if (runner_up != INT32_MAX &&
            static_cast<float>(best) > MATCH_UNIQUENESS * static_cast<float>(runner_up)) {
            continue;
        }
        // Sub-pixel refinement (parabola fit on the SAD costs). Integer-only
        // correspondences quantize the ego-motion to the decimation step — half
        // a decimated pixel is 2 full-resolution px at decimate=4. The residual
        // pass compares interpolated intensities, so that misalignment produces
        // a residual on textured ground comparable to a real moving object:
        // measured before this refinement, a rotating-and-scaling scene with
        // NOTHING moving in it reported 16 candidates.
        float rx = static_cast<float>(best_dx);
        float ry = static_cast<float>(best_dy);
        if (best_dx > -sr && best_dx < sr && best_dy > -sr && best_dy < sr) {
            const auto at = [&](int dx, int dy) {
                return static_cast<float>(cost_map_[(static_cast<std::size_t>(dy + sr) *
                                                     static_cast<std::size_t>(span)) +
                                                    static_cast<std::size_t>(dx + sr)]);
            };
            const float c0 = at(best_dx, best_dy);
            const float xl = at(best_dx - 1, best_dy);
            const float xr = at(best_dx + 1, best_dy);
            const float denx = (xl - (2.F * c0)) + xr;
            if (denx > 1e-3F) {
                rx += std::max(-0.5F, std::min(0.5F, (0.5F * (xl - xr)) / denx));
            }
            const float yu = at(best_dx, best_dy - 1);
            const float yd = at(best_dx, best_dy + 1);
            const float deny = (yu - (2.F * c0)) + yd;
            if (deny > 1e-3F) {
                ry += std::max(-0.5F, std::min(0.5F, (0.5F * (yu - yd)) / deny));
            }
        }
        Match m;
        m.px = static_cast<float>(x0);
        m.py = static_cast<float>(y0);
        m.cx = static_cast<float>(x0 + gx) + rx;
        m.cy = static_cast<float>(y0 + gy) + ry;
        matches_.push_back(m);
    }
}

// G16.6 deviation: RANSAC is one algorithm — sample, score, refit on the
// consensus set; splitting it would hide the inlier set's provenance (G16.6)
// NOLINTNEXTLINE(readability-function-size)
MotionCandidates::EgoMotion MotionCandidates::fit_similarity() const {
    EgoMotion ego;
    ego.correspondences = static_cast<int>(matches_.size());
    if (ego.correspondences < p_.min_correspondences) {
        return ego; // not enough evidence to claim a background motion at all
    }
    // Deterministic sampler: the same frames must always produce the same fit,
    // or a failing case cannot be reproduced in a regression test.
    uint32_t rng = 0x9E3779B9U;
    auto next = [&rng]() {
        rng = (rng * 1664525U) + 1013904223U;
        return rng >> 8U;
    };
    const std::size_t n = matches_.size();
    const float tol2 = p_.ransac_inlier_px * p_.ransac_inlier_px;
    int best_inliers = 0;
    float ba = 1.F;
    float bb = 0.F;
    float btx = 0.F;
    float bty = 0.F;
    for (int iter = 0; iter < p_.ransac_iters; ++iter) {
        const std::size_t i = next() % n;
        std::size_t j = next() % n;
        if (i == j) {
            j = (j + 1) % n;
        }
        const Match& m1 = matches_[i];
        const Match& m2 = matches_[j];
        const float dpx = m2.px - m1.px;
        const float dpy = m2.py - m1.py;
        const float den = (dpx * dpx) + (dpy * dpy);
        if (den < 4.F) {
            continue; // the two samples are too close to fix a rotation
        }
        const float dqx = m2.cx - m1.cx;
        const float dqy = m2.cy - m1.cy;
        const float a = ((dqx * dpx) + (dqy * dpy)) / den;
        const float b = ((dqy * dpx) - (dqx * dpy)) / den;
        const float tx = m1.cx - ((a * m1.px) - (b * m1.py));
        const float ty = m1.cy - ((b * m1.px) + (a * m1.py));
        int inliers = 0;
        for (const auto& m : matches_) {
            const float ex = (((a * m.px) - (b * m.py)) + tx) - m.cx;
            const float ey = (((b * m.px) + (a * m.py)) + ty) - m.cy;
            if ((ex * ex) + (ey * ey) <= tol2) {
                ++inliers;
            }
        }
        if (inliers > best_inliers) {
            best_inliers = inliers;
            ba = a;
            bb = b;
            btx = tx;
            bty = ty;
        }
    }
    const float frac =
        static_cast<float>(best_inliers) / static_cast<float>(matches_.size());
    if (frac < p_.min_inlier_frac) {
        // No dominant background motion: either the scene has no usable
        // structure or everything moves independently. Emitting candidates from
        // an alignment nobody agrees with would fabricate targets.
        return ego;
    }
    // Refit on the consensus set — the 2-point model that won is exact but
    // noisy, and the residual pass is sensitive to sub-pixel alignment error.
    std::vector<float> sx;
    std::vector<float> sy;
    std::vector<float> dx;
    std::vector<float> dy;
    sx.reserve(matches_.size());
    sy.reserve(matches_.size());
    dx.reserve(matches_.size());
    dy.reserve(matches_.size());
    for (const auto& m : matches_) {
        const float ex = (((ba * m.px) - (bb * m.py)) + btx) - m.cx;
        const float ey = (((bb * m.px) + (ba * m.py)) + bty) - m.cy;
        if ((ex * ex) + (ey * ey) <= tol2) {
            sx.push_back(m.px);
            sy.push_back(m.py);
            dx.push_back(m.cx);
            dy.push_back(m.cy);
        }
    }
    float a = ba;
    float b = bb;
    float tx = btx;
    float ty = bty;
    if (!solve_similarity(sx, sy, dx, dy, a, b, tx, ty)) {
        return ego;
    }
    ego.ok = true;
    ego.a = a;
    ego.b = b;
    ego.tx = tx;
    ego.ty = ty;
    ego.inliers = best_inliers;
    return ego;
}

// G16.6 deviation: one data flow — warp, difference, group, score — whose stages
// share the residual buffer (G16.6) NOLINTNEXTLINE(readability-function-size)
MotionCandidates::Result MotionCandidates::process(const Frame& prev, const Frame& cur) {
    Result r;
    if (prev.data == nullptr || cur.data == nullptr || prev.fourcc != FOURCC_NV12 ||
        cur.fourcc != FOURCC_NV12 || prev.width != cur.width ||
        prev.height != cur.height || cur.width <= 0 || cur.height <= 0) {
        return r; // unusable pair: no ego-motion, no candidates
    }
    int dw = 0;
    int dh = 0;
    decimate_luma(prev, prev_small_, dw, dh);
    int cw = 0;
    int ch = 0;
    decimate_luma(cur, cur_small_, cw, ch);
    if (dw != cw || dh != ch || dw <= 0 || dh <= 0) {
        return r;
    }
    find_corners(prev_small_, dw, dh, corner_x_, corner_y_);
    match_corners(prev_small_, cur_small_, dw, dh, corner_x_, corner_y_, nullptr);
    r.ego = fit_similarity();
    if (!r.ego.ok) {
        return r;
    }
    // Guided second pass. A zero-centred window cannot cover the whole frame
    // under rotation or scale — displacement grows with distance from the
    // transform's fixed point — so the first fit is carried by the corners near
    // that point while the far field goes unmatched. Re-matching around the
    // PREDICTED displacement recovers those, and a fit that then loses
    // consensus is reported as no fit rather than aligning the frame badly:
    // measured, the unguided version aligned a rotating scene well enough to
    // pass the consensus test and then reported 16 candidates in a scene with
    // nothing moving in it.
    match_corners(prev_small_, cur_small_, dw, dh, corner_x_, corner_y_, &r.ego);
    const EgoMotion refined = fit_similarity();
    if (!refined.ok) {
        return r_empty(r);
    }
    // Report the transform in FULL-RESOLUTION pixels, which is the frame every
    // caller (and the residual pass below) works in. Rotation and scale are
    // decimation-invariant; the translation is not, and it carries one further
    // term: a box-averaged decimated sample sits at the CENTRE of its d x d
    // box, i.e. at full-resolution offset c = (d-1)/2, so
    //   p_full = d * p_dec + c   =>   t_full = d * t_dec + c * (I - M) * 1.
    // Dropping that term biases the alignment by ~0.1 px under rotation, which
    // is small but systematic and directly feeds the residual.
    const int dfac = std::max(1, p_.decimate);
    const float cofs = 0.5F * static_cast<float>(dfac - 1);
    r.ego = refined;
    r.ego.tx = (refined.tx * static_cast<float>(dfac)) +
               (cofs * ((1.F - refined.a) + refined.b));
    r.ego.ty = (refined.ty * static_cast<float>(dfac)) +
               (cofs * ((1.F - refined.a) - refined.b));

    const float a = r.ego.a;
    const float b = r.ego.b;
    const float tx = r.ego.tx; // already full-resolution (see above)
    const float ty = r.ego.ty;
    const float det = (a * a) + (b * b);
    if (!(det > 1e-6F)) {
        return r; // degenerate transform: nothing to align against
    }
    // Inverse map (cur -> prev), applied incrementally along each row so the
    // per-pixel cost is two adds rather than a matrix multiply.
    const float inv = 1.F / det;
    const float ia = a * inv;
    const float ib = b * inv;

    const int w = cur.width;
    const int h = cur.height;
    const std::size_t cstride =
        (cur.stride != 0U) ? cur.stride : static_cast<std::size_t>(w);
    const std::size_t pstride =
        (prev.stride != 0U) ? prev.stride : static_cast<std::size_t>(w);
    residual_.assign(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), 0);
    for (int y = BORDER_MARGIN; y < h - BORDER_MARGIN; ++y) {
        const float qy = static_cast<float>(y) - ty;
        const float qx = static_cast<float>(BORDER_MARGIN) - tx;
        // p = M^-1 q, M^-1 = [[a, b], [-b, a]] / det
        const float sx0 = (ia * qx) + (ib * qy);
        const float sy0 = (-ib * qx) + (ia * qy);
        const uint8_t* crow = cur.data + (static_cast<std::size_t>(y) * cstride);
        for (int x = BORDER_MARGIN; x < w - BORDER_MARGIN; ++x) {
            // The source position advances by a constant vector per column, so
            // it is stepped rather than re-multiplied; the LOOP variable stays
            // integral (a float induction variable would drift over a 2472-px
            // row and misalign the right-hand side of the frame).
            const float sx = sx0 + (static_cast<float>(x - BORDER_MARGIN) * ia);
            const float sy = sy0 - (static_cast<float>(x - BORDER_MARGIN) * ib);
            const int px = static_cast<int>(std::floor(sx));
            const int py = static_cast<int>(std::floor(sy));
            if (px < 0 || py < 0 || px + 1 >= w || py + 1 >= h) {
                continue; // source outside the previous frame: undefined, not motion
            }
            // BILINEAR sampling, not nearest: the fit is good to a fraction of
            // a pixel, and on textured ground a half-pixel snap alone produces
            // a residual comparable to a real moving object — the whole scene
            // would then read as candidates.
            const float fx = sx - static_cast<float>(px);
            const float fy = sy - static_cast<float>(py);
            const std::size_t row0 =
                (static_cast<std::size_t>(py) * pstride) + static_cast<std::size_t>(px);
            const std::size_t row1 = row0 + pstride;
            const float p00 = prev.data[row0];
            const float p10 = prev.data[row0 + 1];
            const float p01 = prev.data[row1];
            const float p11 = prev.data[row1 + 1];
            const float top = p00 + ((p10 - p00) * fx);
            const float bot = p01 + ((p11 - p01) * fx);
            const int pv = static_cast<int>(std::lround(top + ((bot - top) * fy)));
            const int diff = std::abs(static_cast<int>(crow[x]) - pv);
            if (diff >= p_.residual_threshold) {
                residual_[(static_cast<std::size_t>(y) * static_cast<std::size_t>(w)) +
                          static_cast<std::size_t>(x)] =
                    static_cast<uint8_t>(std::min(255, diff));
            }
        }
    }

    // Connected components over the residual. Claimed pixels are zeroed as they
    // are consumed, so the residual buffer doubles as the visited mask.
    const int min_side =
        std::max(2, static_cast<int>(p_.blob_min_frac * static_cast<float>(w)));
    const int max_side =
        std::max(min_side, static_cast<int>(p_.blob_max_frac * static_cast<float>(w)));
    for (int y = BORDER_MARGIN; y < h - BORDER_MARGIN; ++y) {
        for (int x = BORDER_MARGIN; x < w - BORDER_MARGIN; ++x) {
            const std::size_t seed =
                (static_cast<std::size_t>(y) * static_cast<std::size_t>(w)) +
                static_cast<std::size_t>(x);
            if (residual_[seed] == 0U) {
                continue;
            }
            stack_.clear();
            stack_.push_back(static_cast<int32_t>(seed));
            uint64_t sum = residual_[seed];
            residual_[seed] = 0;
            int pixels = 0;
            int x0 = x;
            int x1 = x;
            int y0 = y;
            int y1 = y;
            while (!stack_.empty()) {
                const int32_t idx = stack_.back();
                stack_.pop_back();
                ++pixels;
                const int cxp = idx % w;
                const int cyp = idx / w;
                x0 = std::min(x0, cxp);
                x1 = std::max(x1, cxp);
                y0 = std::min(y0, cyp);
                y1 = std::max(y1, cyp);
                const int nx[4] = {cxp - 1, cxp + 1, cxp, cxp};
                const int ny[4] = {cyp, cyp, cyp - 1, cyp + 1};
                for (int k = 0; k < 4; ++k) {
                    if (nx[k] < BORDER_MARGIN || ny[k] < BORDER_MARGIN ||
                        nx[k] >= w - BORDER_MARGIN || ny[k] >= h - BORDER_MARGIN) {
                        continue;
                    }
                    const std::size_t nidx =
                        (static_cast<std::size_t>(ny[k]) * static_cast<std::size_t>(w)) +
                        static_cast<std::size_t>(nx[k]);
                    if (residual_[nidx] != 0U) {
                        sum += residual_[nidx];
                        residual_[nidx] = 0;
                        stack_.push_back(static_cast<int32_t>(nidx));
                    }
                }
            }
            const int bw = (x1 - x0) + 1;
            const int bh = (y1 - y0) + 1;
            const int side = std::max(bw, bh);
            // Too small is sensor noise; too large is parallax on near
            // structure, which is exactly what a size band exists to reject.
            if (pixels < MIN_BLOB_PIXELS || side < min_side || side > max_side) {
                continue;
            }
            Candidate c;
            c.cx = (static_cast<float>(x0) + (static_cast<float>(bw) * 0.5F)) /
                   static_cast<float>(w);
            c.cy = (static_cast<float>(y0) + (static_cast<float>(bh) * 0.5F)) /
                   static_cast<float>(h);
            c.w = static_cast<float>(bw) / static_cast<float>(w);
            c.h = static_cast<float>(bh) / static_cast<float>(h);
            c.pixels = pixels;
            const float mean =
                static_cast<float>(sum) / (static_cast<float>(pixels) * 255.F);
            const float fill = static_cast<float>(pixels) / static_cast<float>(bw * bh);
            c.score = std::min(1.F, mean * fill);
            r.candidates.push_back(c);
        }
    }
    std::sort(r.candidates.begin(), r.candidates.end(),
              [](const Candidate& l, const Candidate& rr) { return l.score > rr.score; });
    if (static_cast<int>(r.candidates.size()) > p_.max_candidates) {
        r.candidates.resize(static_cast<std::size_t>(p_.max_candidates));
    }
    return r;
}

} // namespace riposte
