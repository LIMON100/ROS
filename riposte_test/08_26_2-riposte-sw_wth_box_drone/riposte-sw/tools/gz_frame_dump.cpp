// One-shot rendered-frame check for the Gazebo camera path (S-9).
//
// Takes ONE image off a gz-transport image topic, converts it with the seeker's
// OWN rgb888_to_nv12(), and answers the question the seeker log cannot: is this
// a real picture, or a plausible-looking buffer of nothing? SyntheticDetector
// ignores pixel content, so an all-black frame produces an identical log.
//
// TEST-ONLY: gated behind RIPOSTE_WITH_GZ, never built into a flight image.
#include "CameraIngest.h"

#include <gz/msgs/image.pb.h>
#include <gz/transport/Node.hh>

#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string>
#include <vector>

namespace {

std::mutex g_m;
std::condition_variable g_cv;
bool g_done = false;
int g_rc = 2; 

void finish(int rc) {
    std::lock_guard<std::mutex> lk(g_m);
    g_rc = rc;
    g_done = true;
    g_cv.notify_all();
}   

// 64x24 luma preview: a headless container has no image viewer, and "is there a
// picture in there" is answerable by eye.
void preview(const std::vector<uint8_t>& nv12, int w, int h) {
    const char* ramp = " .:-=+*#%@";
    for (int r = 0; r < 24; ++r) {
        std::string line;
        for (int c = 0; c < 64; ++c) {
            const int v = nv12[(static_cast<size_t>((r * h) / 24) * w) + ((c * w) / 64)];
            int i = ((v - 16) * 9) / 219; // studio swing: 16..235 is the range
            i = i < 0 ? 0 : (i > 9 ? 9 : i);
            line += ramp[i];
            }
        std::printf("  |%s|\n", line.c_str());
    }
}

void on_image(const gz::msgs::Image& msg) {
    {
        std::lock_guard<std::mutex> lk(g_m);
        if (g_done) {
            return; // one frame only
        }
    }
    const int w = static_cast<int>(msg.width());
    const int h = static_cast<int>(msg.height());
    std::printf("frame: %dx%d step=%u fmt=%d bytes=%zu\n", w, h, msg.step(),
                static_cast<int>(msg.pixel_format_type()), msg.data().size());
    if (msg.pixel_format_type() != gz::msgs::PixelFormatType::RGB_INT8 || w <= 0 ||
        h <= 0 || (w % 2) != 0 || (h % 2) != 0) {
        std::printf("REJECT: not an even-sized RGB_INT8 image\n");
        finish(3);
        return;
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const auto* src = reinterpret_cast<const uint8_t*>(msg.data().data());
    std::vector<uint8_t> nv12(static_cast<size_t>(w) * static_cast<size_t>(h) * 3U / 2U);
    if (!riposte::rgb888_to_nv12(src, static_cast<size_t>(msg.step()), w, h,
                                   nv12.data())) {
          std::printf("REJECT: rgb888_to_nv12 refused the frame\n");
          finish(4);
          return;
      }   
      
      const size_t npix = static_cast<size_t>(w) * static_cast<size_t>(h);
      int lo = 255;
      int hi = 0;
      double sum = 0.0;
      double sum2 = 0.0;
      std::vector<int> hist(256, 0);
      for (size_t i = 0; i < npix; ++i) {
          const int v = nv12[i];
          lo = v < lo ? v : lo;
          hi = v > hi ? v : hi;
          sum += v;
          sum2 += static_cast<double>(v) * v;
          ++hist[static_cast<size_t>(v)];
      }   
      const double mean = sum / static_cast<double>(npix);
      const double var = (sum2 / static_cast<double>(npix)) - (mean * mean);
      int distinct = 0;
      for (int v = 0; v < 256; ++v) {
          distinct += (hist[static_cast<size_t>(v)] > 0) ? 1 : 0;
      } 
      std::printf("luma: min=%d max=%d mean=%.1f sd=%.1f distinct=%d/256\n", lo, hi, mean,
                  var > 0.0 ? std::sqrt(var) : 0.0, distinct);
      preview(nv12, w, h);
      
      // A constant plane is the failure this tool exists to catch; a nearly
      // constant one is the same failure with dither on top.
      const bool real = (lo != hi) && (var >= 1.0) && (distinct >= 8);
      std::printf("%s\n", real ? "GZ_FRAME_REAL_PASS" : "GZ_FRAME_BLANK_FAIL");
      finish(real ? 0 : 1);
}   

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::printf("usage: gz_frame_dump <image-topic> [wait_s]\n");
        return 2;
    }   
    const int wait_s = (argc > 2) ? std::atoi(argv[2]) : 10;
    gz::transport::Node node;
    if (!node.Subscribe(std::string(argv[1]), on_image)) {
        std::printf("cannot subscribe %s\n", argv[1]);
        return 2;
    }   
    std::unique_lock<std::mutex> lk(g_m);
    if (!g_cv.wait_for(lk, std::chrono::seconds(wait_s), [] { return g_done; })) {
        std::printf("NO_FRAME within %ds - is the renderer running?\n", wait_s);
        return 2;
    }   
    return g_rc;
}   



