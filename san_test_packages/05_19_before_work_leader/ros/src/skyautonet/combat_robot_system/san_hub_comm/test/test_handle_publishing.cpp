// SAN v1.3 PHASE 5 - VideoStreamHandle shape unit test.
//
// Round-trips a START request through the relay node and checks the
// handle has a valid SRT URI, AES-128 passphrase, and matching
// metadata. We can do this without an SRT peer because publishHandle
// builds the handle independently of pipeline lifecycle success.

#include <gtest/gtest.h>
#include <rclcpp/rclcpp.hpp>

#include <combat_robot_msgs/msg/video_stream_handle.hpp>
#include <combat_robot_msgs/msg/video_stream_request.hpp>
#include "san_hub_comm/gstreamer_relay_node.hpp"

extern "C" {
#include <gst/gst.h>
}

using Req = combat_robot_msgs::msg::VideoStreamRequest;
using Handle = combat_robot_msgs::msg::VideoStreamHandle;

class HandlePublishingTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!rclcpp::ok()) rclcpp::init(0, nullptr);
        if (!gst_is_initialized()) gst_init(nullptr, nullptr);
    }
};

TEST_F(HandlePublishingTest, StartProducesEncryptedSrtUri) {
    auto node = std::make_shared<san_hub_comm::GStreamerRelayNode>();

    Req r;
    r.target_robot_id = 3;
    r.action = Req::ACTION_START;
    r.codec = Req::CODEC_H265;
    r.quality = Req::QUALITY_HD;
    r.protocol = Req::PROTOCOL_SRT;
    r.encryption = true;
    r.operator_ip = "10.0.0.99";

    auto h = node->processRequestForTest(r);
    EXPECT_NE(h.srt_uri.find("srt://"), std::string::npos);
    EXPECT_NE(h.srt_uri.find("mode=listener"), std::string::npos);
    EXPECT_NE(h.srt_uri.find("passphrase="), std::string::npos);
    EXPECT_NE(h.srt_uri.find("pbkeylen=16"), std::string::npos)
        << "AES-128 key length must be advertised on the URI";

    EXPECT_FALSE(h.passphrase.empty());
    EXPECT_GE(h.passphrase.size(), 10u);   // SRT lower bound
    EXPECT_EQ(h.target_robot_id, 3u);
    EXPECT_EQ(h.codec, Req::CODEC_H265);
}

TEST_F(HandlePublishingTest, BitrateMatchesQualityTable) {
    auto node = std::make_shared<san_hub_comm::GStreamerRelayNode>();

    Req r;
    r.target_robot_id = 3;
    r.action = Req::ACTION_START;
    r.codec = Req::CODEC_H265;
    r.quality = Req::QUALITY_HD;
    r.protocol = Req::PROTOCOL_SRT;
    r.encryption = true;
    auto h = node->processRequestForTest(r);
    // HD bitrate raised 1500 → 2000 kbps per DCN-2026-003 D-001 (PR #137).
    EXPECT_EQ(h.actual_bitrate_kbps, 2000u);

    r.target_robot_id = 4;
    r.quality = Req::QUALITY_FHD;
    auto h2 = node->processRequestForTest(r);
    EXPECT_EQ(h2.actual_bitrate_kbps, 4000u);

    r.target_robot_id = 5;
    r.quality = Req::QUALITY_THUMBNAIL;
    auto h3 = node->processRequestForTest(r);
    EXPECT_EQ(h3.actual_bitrate_kbps, 100u);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    rclcpp::init(argc, argv);
    int rc = RUN_ALL_TESTS();
    rclcpp::shutdown();
    return rc;
}
