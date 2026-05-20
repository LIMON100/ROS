// SAN v1.5.1 — VideoDecoderNode smoke test.
//
// On CI without gstreamer1.0-libav or gstreamer1.0-rockchip the
// pipeline cannot build; we still verify the node constructs and
// gracefully reports DecoderBackend::UNINITIALIZED.

#include <gtest/gtest.h>
#include <rclcpp/rclcpp.hpp>

extern "C" {
#include <gst/gst.h>
}

#include "san_video_decoder/video_decoder_node.hpp"

using san_video_decoder::VideoDecoderNode;
using san_video_decoder::DecoderBackend;

namespace {

rclcpp::NodeOptions makeOpts() {
    rclcpp::NodeOptions opts;
    opts.parameter_overrides({
        {"compressed_topic", std::string("/test/h265_in")},
        {"decoded_topic",    std::string("/test/decoded_out")},
        {"pipeline_warmup_ms", 50},
    });
    return opts;
}

}  // namespace

class VideoDecoderTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!gst_is_initialized()) gst_init(nullptr, nullptr);
        if (!rclcpp::ok())          rclcpp::init(0, nullptr);
    }
};

TEST_F(VideoDecoderTest, NodeConstructsAndExposesBackend) {
    auto node = std::make_shared<VideoDecoderNode>(makeOpts());
    // Backend is either MPP_HARDWARE (board), AVDEC_SOFTWARE (CI w/ libav)
    // or UNINITIALIZED (CI without either) — all are acceptable here.
    const auto b = node->backend();
    EXPECT_TRUE(b == DecoderBackend::MPP_HARDWARE
             || b == DecoderBackend::AVDEC_SOFTWARE
             || b == DecoderBackend::UNINITIALIZED);
}

TEST_F(VideoDecoderTest, CountersStartAtZero) {
    auto node = std::make_shared<VideoDecoderNode>(makeOpts());
    EXPECT_EQ(node->framesIn(),       0u);
    EXPECT_EQ(node->framesOut(),      0u);
    EXPECT_EQ(node->framesDropped(),  0u);
}

TEST_F(VideoDecoderTest, BackendStringRoundTrip) {
    EXPECT_STREQ(san_video_decoder::backendToString(
        DecoderBackend::MPP_HARDWARE), "mpp_hardware");
    EXPECT_STREQ(san_video_decoder::backendToString(
        DecoderBackend::AVDEC_SOFTWARE), "avdec_software");
    EXPECT_STREQ(san_video_decoder::backendToString(
        DecoderBackend::UNINITIALIZED), "uninitialized");
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    gst_init(&argc, &argv);
    rclcpp::init(argc, argv);
    int rc = RUN_ALL_TESTS();
    rclcpp::shutdown();
    return rc;
}
