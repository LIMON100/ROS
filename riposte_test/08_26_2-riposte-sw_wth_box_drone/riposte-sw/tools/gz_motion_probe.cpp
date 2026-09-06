// Runs the wide-channel motion-candidate path (R-13) over a rendered Gazebo
// camera stream, with NO detector and NO AI model of any kind.
//
// MotionCandidates is implemented and unit-tested but is NOT instantiated
// anywhere in the running seeker - test_motion.cpp is its only caller. This
// probe is the first time it sees real rendered pixels, and it answers a
// question the detector gate would otherwise block: can riposte find a moving
// target in a simulated image using pixel motion alone?
//
// TEST-ONLY: gated behind RIPOSTE_WITH_GZ, never built into a flight image.
#include "CameraIngest.h"
#include "MotionCandidates.h"
#include "riposte/Clock.h"

#include <gz/msgs/image.pb.h>
#include <gz/transport/Node.hh>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

std::mutex g_m;
riposte::MotionCandidates g_mc;
std::vector<uint8_t> g_prev, g_cur;
int g_w = 0, g_h = 0;
bool g_have_prev = false;

int g_pairs = 0;      // consecutive pairs processed
int g_ego_ok = 0;     // pairs where the background fit converged
int g_with_cand = 0;  // pairs that produced at least one candidate
int g_cand_total = 0; // candidates summed over all pairs
float g_best = 0.F;   // best score seen
float g_bx = 0.F, g_by = 0.F;

riposte::Frame make_frame(const std::vector<uint8_t>& buf) {
    riposte::Frame f;
    f.mono_ns = riposte::mono_now_ns();
    f.width = g_w;
    f.height = g_h;
    f.data = buf.data();
    f.stride = static_cast<size_t>(g_w);
    f.fourcc = 0x3231564E; // 'NV12'
    return f;
}   

void on_image(const gz::msgs::Image& msg) {
    const int w = static_cast<int>(msg.width());
    const int h = static_cast<int>(msg.height());
    if (msg.pixel_format_type() != gz::msgs::PixelFormatType::RGB_INT8 || w <= 0 ||
        h <= 0 || (w % 2) != 0 || (h % 2) != 0) {
        return;
    }   
    std::lock_guard<std::mutex> lk(g_m);
    if (g_w != w || g_h != h) {
        g_w = w; 
        g_h = h; 
        g_have_prev = false;
        g_prev.assign(static_cast<size_t>(w) * h * 3U / 2U, 0);
        g_cur.assign(static_cast<size_t>(w) * h * 3U / 2U, 0);
    }   
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const auto* src = reinterpret_cast<const uint8_t*>(msg.data().data());
    if (!riposte::rgb888_to_nv12(src, static_cast<size_t>(msg.step()), w, h,
                                g_cur.data())) {
        return;                  
    }
    if (g_have_prev) {
        const riposte::Frame prev = make_frame(g_prev);
        const riposte::Frame cur = make_frame(g_cur);
        const riposte::MotionCandidates::Result r = g_mc.process(prev, cur);
        ++g_pairs;
        g_ego_ok += r.ego.ok ? 1 : 0;
        g_cand_total += static_cast<int>(r.candidates.size());
        if (!r.candidates.empty()) {
            ++g_with_cand;
            const auto& c = r.candidates.front();
            if (c.score > g_best) {
                g_best = c.score; 
                g_bx = c.cx;
                g_by = c.cy;
            }   
        }   
        if (g_pairs <= 6 || (g_pairs % 20) == 0) {
            std::printf("pair %3d: ego ok=%d inl=%d/%d  candidates=%zu", g_pairs,
                        r.ego.ok ? 1 : 0, r.ego.inliers, r.ego.correspondences,
                        r.candidates.size());
            if (!r.candidates.empty()) {
                const auto& c = r.candidates.front();
                std::printf("  best score=%.2f at (%.3f,%.3f) %dpx", c.score, c.cx, c.cy,
                            c.pixels);
            }               
            std::printf("\n");
        }   
    }
    g_prev.swap(g_cur);
    g_have_prev = true;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::printf("usage: gz_motion_probe <image-topic> [seconds]\n");
        return 2;
    }
    const int secs = (argc > 2) ? std::atoi(argv[2]) : 20;
    gz::transport::Node node;
    if (!node.Subscribe(std::string(argv[1]), on_image)) {
        std::printf("cannot subscribe %s\n", argv[1]);
        return 2;
    }
    std::this_thread::sleep_for(std::chrono::seconds(secs));
    
    std::lock_guard<std::mutex> lk(g_m);
    std::printf("\n--- %d s over %dx%d ---\n", secs, g_w, g_h);
    std::printf("pairs=%d ego_ok=%d (%.0f%%) pairs_with_candidate=%d (%.0f%%)\n", g_pairs,
                g_ego_ok, g_pairs ? 100.0 * g_ego_ok / g_pairs : 0.0, g_with_cand,
                g_pairs ? 100.0 * g_with_cand / g_pairs : 0.0);
    std::printf("candidates_total=%d best_score=%.2f at (%.3f,%.3f)\n", g_cand_total,
                g_best, g_bx, g_by);
    if (g_pairs == 0) {
        std::printf("NO_FRAMES\n");
        return 2; 
    }   
    // A pass means the pixel-motion path found an independently-moving region
    // in a meaningful share of frames - not a single lucky blob.
    const bool pass = g_with_cand * 4 >= g_pairs;
    std::printf("%s\n", pass ? "GZ_MOTION_CANDIDATES_PASS" : "GZ_MOTION_NONE");
    return pass ? 0 : 1;
} 
