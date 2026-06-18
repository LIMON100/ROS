// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.3 PHASE 5 - thumbnail-downgrade policy unit test.
//
// Drives the relay node through `processRequestForTest()` so the test
// doesn't need an SRT peer. We assert: <4 streams keep their quality,
// the 4th request triggers thumbnail mode for all, stop transitions
// clear thumbnail when the count drops back below threshold.

#include <gtest/gtest.h>
#include <rclcpp/rclcpp.hpp>

#include <combat_robot_msgs/msg/video_stream_request.hpp>
#include "san_hub_comm/gstreamer_relay_node.hpp"

extern "C" {
#include <gst/gst.h>
}

using Req = combat_robot_msgs::msg::VideoStreamRequest;
using Handle = combat_robot_msgs::msg::VideoStreamHandle;

namespace
{

Req makeStart(uint32_t robot_id, uint8_t quality)
{
  Req r;
  r.target_robot_id = robot_id;
  r.action = Req::ACTION_START;
  r.codec = Req::CODEC_H265;
  r.quality = quality;
  r.protocol = Req::PROTOCOL_SRT;
  r.encryption = true;
  return r;
}

}  // namespace

class ThumbnailDowngradeTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    if (!rclcpp::ok()) {rclcpp::init(0, nullptr);}
    if (!gst_is_initialized()) {gst_init(nullptr, nullptr);}
  }
};

TEST_F(ThumbnailDowngradeTest, ThreeStreamsKeepRequestedQuality) {
  auto node = std::make_shared<san_hub_comm::GStreamerRelayNode>();
  auto h1 = node->processRequestForTest(makeStart(3, Req::QUALITY_HD));
  auto h2 = node->processRequestForTest(makeStart(4, Req::QUALITY_HD));
  auto h3 = node->processRequestForTest(makeStart(5, Req::QUALITY_HD));

  EXPECT_FALSE(node->isThumbnailMode());
  EXPECT_EQ(node->activeStreamCount(), 3u);
  // Pipelines may have failed to PLAY without a real udpsrc peer
  // bound; we only assert the state-machine policy here.
  EXPECT_EQ(h3.quality, Req::QUALITY_HD);
}

TEST_F(ThumbnailDowngradeTest, FourthStreamForcesThumbnailMode) {
  auto node = std::make_shared<san_hub_comm::GStreamerRelayNode>();
  node->processRequestForTest(makeStart(3, Req::QUALITY_HD));
  node->processRequestForTest(makeStart(4, Req::QUALITY_HD));
  node->processRequestForTest(makeStart(5, Req::QUALITY_HD));

  auto h4 = node->processRequestForTest(makeStart(6, Req::QUALITY_HD));
  EXPECT_TRUE(node->isThumbnailMode());
  EXPECT_EQ(h4.quality, Req::QUALITY_THUMBNAIL);
  EXPECT_EQ(node->activeStreamCount(), 4u);
}

TEST_F(ThumbnailDowngradeTest, StopRestoresWhenBelowThreshold) {
  auto node = std::make_shared<san_hub_comm::GStreamerRelayNode>();
  node->processRequestForTest(makeStart(3, Req::QUALITY_HD));
  node->processRequestForTest(makeStart(4, Req::QUALITY_HD));
  node->processRequestForTest(makeStart(5, Req::QUALITY_HD));
  node->processRequestForTest(makeStart(6, Req::QUALITY_HD));
  ASSERT_TRUE(node->isThumbnailMode());

  Req stop = makeStart(6, Req::QUALITY_HD);
  stop.action = Req::ACTION_STOP;
  node->processRequestForTest(stop);

  EXPECT_EQ(node->activeStreamCount(), 3u);
  EXPECT_FALSE(node->isThumbnailMode())
    << "thumbnail mode must clear once concurrent count drops "
    << "below threshold";
}

TEST_F(ThumbnailDowngradeTest, StopUnknownIdIsNoOp) {
  auto node = std::make_shared<san_hub_comm::GStreamerRelayNode>();
  Req stop = makeStart(99, Req::QUALITY_HD);
  stop.action = Req::ACTION_STOP;
  auto h = node->processRequestForTest(stop);
  EXPECT_EQ(h.status, Handle::STATUS_STOPPED);
  EXPECT_EQ(node->activeStreamCount(), 0u);
}

int main(int argc, char ** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  rclcpp::init(argc, argv);
  int rc = RUN_ALL_TESTS();
  rclcpp::shutdown();
  return rc;
}
