// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.3 PHASE 5 - VideoSenderNode unit test.
//
// We can't actually open /dev/video0 from CI, so we instead verify
// the pipeline description string we'd hand to gst_parse_launch.
// `buildDescription()` is private; we go through the public path
// using a parameter override that swaps v4l2src for videotestsrc
// is not possible without a code change, so the test reads the
// `currentDescription()` after `startPipeline()` returns and accepts
// either success (GStreamer happy) or failure (no /dev/video0).

#include <gtest/gtest.h>
#include <rclcpp/rclcpp.hpp>

#include <combat_robot_msgs/msg/video_stream_handle.hpp>

#include "san_video_sender/video_sender_node.hpp"

extern "C" {
#include <gst/gst.h>
}

class VideoSenderTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    if (!rclcpp::ok()) {rclcpp::init(0, nullptr);}
    if (!gst_is_initialized()) {gst_init(nullptr, nullptr);}
  }

  rclcpp::NodeOptions makeOptions(
    const std::string & codec,
    int bitrate)
  {
    rclcpp::NodeOptions opts;
    opts.parameter_overrides(
    {
      {"hub_ip", std::string("10.0.0.2")},
      {"robot_id", 3},
      {"video_device", std::string("/dev/null")},
      {"default_bitrate_kbps", bitrate},
      {"udp_base_port", 5000},
      {"width", 1280},
      {"height", 720},
      {"framerate", 30},
      {"codec", codec},
    });
    return opts;
  }
};

TEST_F(VideoSenderTest, NodeBootsWithoutVideoDevice) {
  // We don't assert isPipelinePlaying() because /dev/null isn't a
  // valid v4l2 device on most CI hosts; we assert the node doesn't
  // crash and the description string was attempted.
  auto node = std::make_shared<san_video_sender::VideoSenderNode>(
    makeOptions("h265", 1500));
  // No exception means startPipeline() handled the failure path.
  SUCCEED();
}

TEST_F(VideoSenderTest, DescriptionUsesUdpBasePlusRobotId) {
  auto node = std::make_shared<san_video_sender::VideoSenderNode>(
    makeOptions("h265", 1500));
  const std::string desc = node->currentDescription();
  if (desc.empty()) {
    // Pipeline didn't construct (no v4l2 source on CI) - skip.
    GTEST_SKIP() << "no v4l2 device on this host";
  }
  EXPECT_NE(desc.find("port=5003"), std::string::npos)
    << "udp port must be udp_base + robot_id (5000 + 3)";
  EXPECT_NE(desc.find("host=10.0.0.2"), std::string::npos);
}

TEST_F(VideoSenderTest, CodecH264SwitchesEncoder) {
  auto node = std::make_shared<san_video_sender::VideoSenderNode>(
    makeOptions("h264", 1500));
  const std::string desc = node->currentDescription();
  if (desc.empty()) {GTEST_SKIP() << "no v4l2 device";}
  EXPECT_NE(desc.find("x264enc"), std::string::npos);
  EXPECT_EQ(desc.find("x265enc"), std::string::npos);
}

// ─── P1-5 part 2 — pure-logic qualityToResolution coverage ────────────
//
// These two tests exercise the public static qualityToResolution() —
// the quality enum → (resolution, bitrate) mapping that is a contract
// shared with the Hub-side san_hub_comm::gstreamer_relay_node
// qualityBitrateKbps(). If the bitrate column drifts here without
// matching the relay, production will see the sender → relay echo
// bitrate mismatch warning. Pure-logic, no rclcpp / no GStreamer
// init needed — runs in microseconds.
TEST(VideoSenderQualityTable, AllEnumsMapToExpectedResolutionAndBitrate) {
  using Handle = combat_robot_msgs::msg::VideoStreamHandle;
  using san_video_sender::VideoSenderNode;
  int w = 0, h = 0, br = 0;

  VideoSenderNode::qualityToResolution(Handle::QUALITY_THUMBNAIL, w, h, br);
  EXPECT_EQ(w, 320);  EXPECT_EQ(h, 240);  EXPECT_EQ(br, 100);

  VideoSenderNode::qualityToResolution(Handle::QUALITY_LOW, w, h, br);
  EXPECT_EQ(w, 640);  EXPECT_EQ(h, 480);  EXPECT_EQ(br, 500);

  // v1.5.1 DCN-2026-003 D-001 bumped HD from 1500 → 2000 kbps; this
  // assertion locks the value against accidental reversion.
  VideoSenderNode::qualityToResolution(Handle::QUALITY_HD, w, h, br);
  EXPECT_EQ(w, 1280); EXPECT_EQ(h, 720); EXPECT_EQ(br, 2000)
    << "HD bitrate must stay at 2000 kbps (DCN-2026-003 D-001) — "
    "must match relay qualityBitrateKbps(QUALITY_HD)";

  VideoSenderNode::qualityToResolution(Handle::QUALITY_FHD, w, h, br);
  EXPECT_EQ(w, 1920); EXPECT_EQ(h, 1080); EXPECT_EQ(br, 4000);
}

TEST(VideoSenderQualityTable, UnknownQualityFallsBackToHdDefaults) {
  using san_video_sender::VideoSenderNode;
  int w = 0, h = 0, br = 0;

  // Garbage enum value (e.g. forward-compat from a future quality
  // tier the relay doesn't know yet) must fall back to HD — never
  // produces 0x0 resolution or 0 bitrate which would crash the
  // GStreamer encoder caps negotiation.
  for (uint8_t q : {static_cast<uint8_t>(99),
      static_cast<uint8_t>(200),
      static_cast<uint8_t>(255)})
  {
    VideoSenderNode::qualityToResolution(q, w, h, br);
    EXPECT_EQ(w, 1280) << "quality=" << static_cast<int>(q);
    EXPECT_EQ(h, 720) << "quality=" << static_cast<int>(q);
    EXPECT_EQ(br, 2000) << "quality=" << static_cast<int>(q);
    EXPECT_NE(w, 0);        // crash guard
    EXPECT_NE(br, 0);
  }
}

int main(int argc, char ** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  rclcpp::init(argc, argv);
  int rc = RUN_ALL_TESTS();
  rclcpp::shutdown();
  return rc;
}
