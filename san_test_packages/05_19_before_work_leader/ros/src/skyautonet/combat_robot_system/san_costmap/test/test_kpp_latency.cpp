// SAN v1.3 PHASE 1 - KPP latency test (cost_map_latency_p99 <= 5 s).
//
// We run 50 build-one-shot cycles with a realistic cloud and assert
// the p99 latency stays well below the 5 s budget. The CostMapNode's
// recordLatency() already logs an error when the per-cycle time
// exceeds the KPP; this test just verifies the measured distribution.

#include <gtest/gtest.h>
#include <rclcpp/rclcpp.hpp>

#include <algorithm>
#include <chrono>
#include <vector>

#include "san_costmap/cost_map_node.hpp"
#include "test_helpers.hpp"

class CostMapKppLatencyTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!rclcpp::ok()) rclcpp::init(0, nullptr);
    }
};

TEST_F(CostMapKppLatencyTest, P99UnderFiveSeconds) {
    auto node = std::make_shared<san_costmap::CostMapNode>();

    auto obstacle = san_costmap_test::makeBlock(0.30f, 0.20f, 0.20f);
    auto ground   = san_costmap_test::makeFlatGroundFull();

    std::vector<double> latencies;
    latencies.reserve(50);
    for (int i = 0; i < 50; ++i) {
        const auto t0 = std::chrono::steady_clock::now();
        (void)node->buildOneShotForTest(obstacle, ground);
        const auto t1 = std::chrono::steady_clock::now();
        latencies.push_back(
            std::chrono::duration<double>(t1 - t0).count());
    }
    std::sort(latencies.begin(), latencies.end());
    const double p50 = latencies[latencies.size() / 2];
    const double p99 = latencies[
        static_cast<std::size_t>(latencies.size() * 0.99) - 1];

    EXPECT_LT(p99, 5.0)
        << "Cost map latency p99 = " << p99
        << " s exceeds KPP (target 5 s)";
    // Sanity: report p50 too so a regression shows up in CI logs.
    SUCCEED() << "Latency p50=" << p50 << "s p99=" << p99 << "s";
}

TEST_F(CostMapKppLatencyTest, RecordLatencyExposedToNode) {
    auto node = std::make_shared<san_costmap::CostMapNode>();
    auto obstacle = san_costmap_test::makeBlock(0.30f);
    auto ground   = san_costmap_test::makeFlatGroundFull();
    (void)node->buildOneShotForTest(obstacle, ground);
    EXPECT_GE(node->lastLatencySec(), 0.0);
    EXPECT_LT(node->lastLatencySec(), 5.0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    rclcpp::init(argc, argv);
    int rc = RUN_ALL_TESTS();
    rclcpp::shutdown();
    return rc;
}
