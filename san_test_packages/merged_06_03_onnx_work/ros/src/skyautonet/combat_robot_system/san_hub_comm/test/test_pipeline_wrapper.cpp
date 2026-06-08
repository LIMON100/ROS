// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.3 PHASE 5 - GStreamerPipeline wrapper unit test.
//
// Uses fakesrc → fakesink (no hardware) so the test passes on a CI
// host that has GStreamer installed but no camera or SRT peer. The
// real pipeline shape is exercised by the integration test running
// against the hub Docker container.

#include <gtest/gtest.h>
#include <rclcpp/rclcpp.hpp>

#include <atomic>
#include <chrono>
#include <thread>

#include "san_hub_comm/gstreamer_pipeline.hpp"

extern "C" {
#include <gst/gst.h>
}

using namespace std::chrono_literals;

class PipelineWrapperTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    if (!gst_is_initialized()) {gst_init(nullptr, nullptr);}
  }
};

TEST_F(PipelineWrapperTest, BuildsFakeSrcPipeline) {
  auto logger = rclcpp::get_logger("test_pipe");
  san_hub_comm::GStreamerPipeline pipe(
    logger, "fakesrc num-buffers=10 ! fakesink");
  EXPECT_TRUE(pipe.play());
  EXPECT_TRUE(pipe.isPlaying());
}

TEST_F(PipelineWrapperTest, BadDescriptionDoesNotCrash) {
  auto logger = rclcpp::get_logger("test_pipe");
  // Plain text that gst_parse_launch will reject.
  san_hub_comm::GStreamerPipeline pipe(
    logger, "this-element-does-not-exist ! fakesink");
  EXPECT_FALSE(pipe.play());
}

TEST_F(PipelineWrapperTest, StopTransitionsToNullWithinFiveSeconds) {
  auto logger = rclcpp::get_logger("test_pipe");
  san_hub_comm::GStreamerPipeline pipe(
    logger,
    "videotestsrc is-live=true ! video/x-raw,width=320,height=240 "
    "! fakesink");
  EXPECT_TRUE(pipe.play());

  const auto start = std::chrono::steady_clock::now();
  EXPECT_TRUE(pipe.stop());
  const auto elapsed = std::chrono::steady_clock::now() - start;
  EXPECT_LT(elapsed, 5s)
    << "stop() must clean up within 5 s for the VideoStreamStop KPP";
  EXPECT_FALSE(pipe.isPlaying());
}

TEST_F(PipelineWrapperTest, EventCallbackFiresOnEos) {
  auto logger = rclcpp::get_logger("test_pipe");
  san_hub_comm::GStreamerPipeline pipe(
    logger, "fakesrc num-buffers=1 ! fakesink");

  std::atomic<bool> saw_eos(false);
  pipe.setEventCallback(
    [&](san_hub_comm::PipelineEvent ev, const std::string &) {
      if (ev == san_hub_comm::PipelineEvent::EOS) {
        saw_eos.store(true);
      }
    });

  ASSERT_TRUE(pipe.play());

  // Allow the pipeline a moment to finish (fakesrc 1 buffer ~ instant).
  const auto deadline = std::chrono::steady_clock::now() + 2s;
  while (std::chrono::steady_clock::now() < deadline) {
    if (saw_eos.load()) {break;}
    std::this_thread::sleep_for(50ms);
  }
  EXPECT_TRUE(saw_eos.load());
}

int main(int argc, char ** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  rclcpp::init(argc, argv);
  int rc = RUN_ALL_TESTS();
  rclcpp::shutdown();
  return rc;
}
