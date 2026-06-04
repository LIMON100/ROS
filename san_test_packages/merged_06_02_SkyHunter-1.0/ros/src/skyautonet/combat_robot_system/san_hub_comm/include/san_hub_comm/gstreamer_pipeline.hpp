// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.3 PHASE 5 - GStreamer pipeline lifecycle wrapper.
//
// Owns a single GstElement* pipeline built from a gst_parse_launch
// description, plus the bus callback that surfaces ERROR/EOS/WARNING
// back to the relay node. No `system()`, no `gst-launch-1.0` shell.
//
// Thread-safety: each GStreamerPipeline owns one pipeline; the bus
// callback runs on the main GLib context thread the relay node sets
// up at startup. Public API is meant to be called only from the rclcpp
// node thread.

#pragma once

#include <rclcpp/logger.hpp>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

extern "C" {
#include <gst/gst.h>
}

namespace san_hub_comm
{

enum class PipelineEvent
{
  PLAYING,
  PAUSED,
  EOS,
  WARNING,
  ERROR_RECOVERABLE,
  ERROR_FATAL,
};

class GStreamerPipeline
{
public:
  using EventCallback =
    std::function<void (PipelineEvent ev, const std::string & detail)>;

  GStreamerPipeline(
    rclcpp::Logger logger,
    const std::string & description);
  ~GStreamerPipeline();

  GStreamerPipeline(const GStreamerPipeline &) = delete;
  GStreamerPipeline & operator=(const GStreamerPipeline &) = delete;

  // Transition to PLAYING. Returns false if the state change fails
  // synchronously - caller should destroy the pipeline.
  bool play();

  // Transition to NULL synchronously. Safe to call multiple times.
  // Returns true once the pipeline reports NULL state.
  bool stop();

  bool isPlaying() const {return playing_;}

  void setEventCallback(EventCallback cb);

  // Rebuild the pipeline from a new description in-place. Calls
  // stop() then constructs a fresh element graph. Used by the
  // change_quality path so the relay can swap an x265enc bitrate
  // without tearing down the SRT listener half.
  bool rebuild(const std::string & description);

  const std::string & description() const {return description_;}

private:
  rclcpp::Logger logger_;
  std::string description_;
  GstElement * pipeline_ = nullptr;
  bool playing_ = false;
  std::mutex callback_mutex_;
  EventCallback event_cb_;

  bool build(const std::string & description);
  void teardown();

  static GstBusSyncReply onBusMessageTrampoline(
    GstBus * bus,
    GstMessage * msg,
    gpointer user);
  GstBusSyncReply onBusMessage(GstBus * bus, GstMessage * msg);

  void fireEvent(PipelineEvent ev, const std::string & detail);
};

}  // namespace san_hub_comm
