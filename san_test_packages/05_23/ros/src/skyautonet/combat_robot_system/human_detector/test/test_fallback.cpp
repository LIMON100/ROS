// SAN v1.5.1 (was v1.3 PHASE 6 — see DCN-2026-004 D-011) - auto-fallback unit test.
//
// On a CI host with neither RKNN nor HailoRT installed, requesting
// `hailo8` must produce a working detector that has fallen all the
// way through to the stub. The active backend name lets the node
// surface this in RobotStatus telemetry.

#include <gtest/gtest.h>
#include <rclcpp/rclcpp.hpp>

#include "human_detector/human_detector_node.hpp"

namespace {

rclcpp::NodeOptions makeOptions(const std::string& backend,
                                const std::string& model_path = "")
{
    rclcpp::NodeOptions opts;
    opts.parameter_overrides({
        {"inference_backend", backend},
        {"model_path", model_path},
        {"rk3588_fallback_model_path", model_path},
    });
    return opts;
}

}  // namespace

class FallbackTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!rclcpp::ok()) rclcpp::init(0, nullptr);
    }
};

TEST_F(FallbackTest, Hailo8FallsThroughToReadyBackend) {
    // No real Hailo M.2 on the CI host - the node must end up with
    // a ready backend (either rk3588 if RKNN is available, or stub).
    auto node = std::make_shared<human_detector::HumanDetectorNode>(
        makeOptions("hailo8"));
    EXPECT_TRUE(node->backendIsReady())
        << "fallback must leave the detector in a ready state";
    const std::string active = node->activeBackendName();
    EXPECT_TRUE(active == "rk3588" || active == "stub")
        << "fallback should land on rk3588 or stub, got: " << active;
}

TEST_F(FallbackTest, UnknownBackendNameUsesStub) {
    auto node = std::make_shared<human_detector::HumanDetectorNode>(
        makeOptions("nonexistent-accelerator"));
    EXPECT_TRUE(node->backendIsReady());
    EXPECT_EQ(node->activeBackendName(), "stub")
        << "unknown name must terminate at stub, not retry rk3588";
}

TEST_F(FallbackTest, StubExplicitlySelectedStays) {
    auto node = std::make_shared<human_detector::HumanDetectorNode>(
        makeOptions("stub"));
    EXPECT_TRUE(node->backendIsReady());
    EXPECT_EQ(node->activeBackendName(), "stub");
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    rclcpp::init(argc, argv);
    int rc = RUN_ALL_TESTS();
    rclcpp::shutdown();
    return rc;
}
