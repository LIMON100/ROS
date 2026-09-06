#pragma once
// Rendered Gazebo camera as an ICamera (S-9). Subscribes to a gz-transport
// image topic, converts each RGB888 message to NV12 at this boundary
// (rgb888_to_nv12), and hands the newest one to the seeker.
//
// TEST-ONLY: gated behind RIPOSTE_WITH_GZ, off by default, never built into a
// flight image - the same rule as tools/gz_track_bridge.cpp.
//
// Latest-wins, matching the documented ICamera policy and the V4L2 ring: the
// subscriber thread OVERWRITES the pending frame rather than queueing it, so
// the seeker always sees the newest render and never accumulates backlog.
#include "CameraIngest.h"
#include "riposte/Clock.h"

#include <gz/msgs/image.pb.h>
#include <gz/transport/Node.hh>

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace riposte {

class GzCamera final : public ICamera {
public:
    // Frames must appear within this window of open(), or the topic is wrong or
    // the renderer is not running. A named bound, not a retry policy.
    static constexpr int OPEN_WAIT_MS = 5000;
    
    explicit GzCamera(std::string topic) : topic_(std::move(topic)) {}
    ~GzCamera() override = default;
    GzCamera(const GzCamera&) = delete;
    GzCamera& operator=(const GzCamera&) = delete;
    GzCamera(GzCamera&&) = delete;
    GzCamera& operator=(GzCamera&&) = delete;
    
    bool open() override {
        if (!node_.Subscribe(topic_, &GzCamera::on_image, this)) {
            return false;
        }
        // Dimensions are NEGOTIATED - they come from the first message, never
        // from configuration. Consumers size buffers from width()/height(), so
        // open() must not return true until those are real.
        std::unique_lock<std::mutex> lk(m_);
        return cv_.wait_for(lk, std::chrono::milliseconds(OPEN_WAIT_MS),
                              [this] { return width_ > 0 && height_ > 0; });
      }                       
      
      bool grab(Frame& f, int timeout_ms) override {
          std::unique_lock<std::mutex> lk(m_);
          if (!have_new_ && timeout_ms > 0) {
              cv_.wait_for(lk, std::chrono::milliseconds(timeout_ms),
                           [this] { return have_new_; });
          }                
          if (have_new_) { 
              front_.swap(back_);
              front_ns_ = back_ns_;
              have_new_ = false;
          } else if (front_.empty()) {
              return false; // nothing has ever arrived
          }   
          // A buffered camera keeps handing out its newest frame with the
          // ORIGINAL stamp; SM-7 freshness is judged on mono_ns, not on arrival.
          f.mono_ns = front_ns_;
          f.width = width_;
          f.height = height_;
          f.data = front_.data();
          f.stride = static_cast<std::size_t>(width_);
          f.fourcc = 0x3231564E; // 'NV12'
          f.src_roi = Roi{};
          return true;
      }   
      
      int width() const override { return width_; }
      int height() const override { return height_; }
      const char* name() const override { return "GzCamera"; }
      
private:
    void on_image(const gz::msgs::Image& msg) {
        const int w = static_cast<int>(msg.width());
        const int h = static_cast<int>(msg.height());
        // Refuse anything the conversion contract does not cover rather than
        // producing a plausible-looking frame from it.
        if (msg.pixel_format_type() != gz::msgs::PixelFormatType::RGB_INT8 || w <= 0 ||
            h <= 0 || (w % 2) != 0 || (h % 2) != 0) {
            return;
        }   
        const auto step = static_cast<std::size_t>(msg.step());
        if (msg.data().size() < step * static_cast<std::size_t>(h)) {
            return; // short payload: the frame is not all there
        }   
        std::lock_guard<std::mutex> lk(m_);
        back_.resize(static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 3U / 2U);
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        const auto* src = reinterpret_cast<const uint8_t*>(msg.data().data());
        if (!rgb888_to_nv12(src, step, w, h, back_.data())) {
            return;
        }   
        width_ = w;
        height_ = h;
        // Arrival stamped on CLOCK_MONOTONIC, not the message's sim-time
        // header: SM-7 freshness is judged on the seeker's own clock.
        back_ns_ = mono_now_ns();
        have_new_ = true;
        cv_.notify_one();
    }   
    
    std::string topic_;
    gz::transport::Node node_;
    std::mutex m_;
    std::condition_variable cv_;
    std::vector<uint8_t> back_, front_; // NV12, latest-wins double buffer
    uint64_t back_ns_ = 0;
    uint64_t front_ns_ = 0;
    bool have_new_ = false;
    int width_ = 0;
    int height_ = 0;
}; 
} // namespace riposte