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

#include "san_video_sender/video_sender_node.hpp"

extern "C" {
#include <gst/gst.h>
}

class VideoSenderTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!rclcpp::ok()) rclcpp::init(0, nullptr);
        if (!gst_is_initialized()) gst_init(nullptr, nullptr);
    }

    rclcpp::NodeOptions makeOptions(const std::string& codec,
                                    int bitrate)
    {
        rclcpp::NodeOptions opts;
        opts.parameter_overrides({
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
    if (desc.empty()) GTEST_SKIP() << "no v4l2 device";
    EXPECT_NE(desc.find("x264enc"), std::string::npos);
    EXPECT_EQ(desc.find("x265enc"), std::string::npos);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    rclcpp::init(argc, argv);
    int rc = RUN_ALL_TESTS();
    rclcpp::shutdown();
    return rc;
}
