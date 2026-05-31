// SAN v1.4 L5 regression - TopicWatcher unit test.

#include <gtest/gtest.h>

#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/string.hpp>

#include <chrono>
#include <future>
#include <thread>

#include "san_l5_regression/topic_watcher.hpp"

using namespace san_l5_regression;
using StrMsg = std_msgs::msg::String;

namespace {

void spin_for(std::shared_ptr<rclcpp::Node> node,
              std::chrono::milliseconds dur)
{
    rclcpp::executors::SingleThreadedExecutor exec;
    exec.add_node(node);
    const auto deadline = std::chrono::steady_clock::now() + dur;
    while (std::chrono::steady_clock::now() < deadline) {
        exec.spin_some();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

}  // namespace

class TopicWatcherTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!rclcpp::ok()) rclcpp::init(0, nullptr);
    }
};

TEST_F(TopicWatcherTest, ReturnsImmediatelyWhenAlreadySatisfied) {
    auto pub_node = std::make_shared<rclcpp::Node>("tw_pub");
    auto sub_node = std::make_shared<rclcpp::Node>("tw_sub");
    auto pub = pub_node->create_publisher<StrMsg>(
        "/tw/test", rclcpp::QoS(10).reliable());
    TopicWatcher<StrMsg> watcher(sub_node.get(), "/tw/test");

    // Publish once and let the watcher absorb it before waitFor().
    StrMsg m; m.data = "hello";
    pub->publish(m);
    rclcpp::executors::SingleThreadedExecutor exec;
    exec.add_node(sub_node);
    exec.spin_some();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    exec.spin_some();

    auto elapsed = watcher.waitFor(
        [](const StrMsg& s) { return s.data == "hello"; },
        std::chrono::milliseconds(50));
    ASSERT_TRUE(elapsed.has_value());
    EXPECT_EQ(*elapsed, 0);
}

TEST_F(TopicWatcherTest, ReturnsTimeoutWhenPredicateNeverMatches) {
    auto sub_node = std::make_shared<rclcpp::Node>("tw_sub_to");
    TopicWatcher<StrMsg> watcher(sub_node.get(), "/tw/empty");

    auto elapsed = watcher.waitFor(
        [](const StrMsg&) { return false; },
        std::chrono::milliseconds(100));
    EXPECT_FALSE(elapsed.has_value());
}

TEST_F(TopicWatcherTest, SignalsWhenMatchingMessageArrives) {
    auto pub_node = std::make_shared<rclcpp::Node>("tw_pub_async");
    auto sub_node = std::make_shared<rclcpp::Node>("tw_sub_async");
    auto pub = pub_node->create_publisher<StrMsg>(
        "/tw/async", rclcpp::QoS(10).reliable());
    TopicWatcher<StrMsg> watcher(sub_node.get(), "/tw/async");

    // Spin the sub node continuously while we publish from another thread.
    std::atomic<bool> done{false};
    std::thread spinner([&] {
        rclcpp::executors::SingleThreadedExecutor exec;
        exec.add_node(sub_node);
        while (!done.load()) {
            exec.spin_some();
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });

    std::thread publisher([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(80));
        StrMsg m; m.data = "target";
        pub->publish(m);
    });

    auto elapsed = watcher.waitFor(
        [](const StrMsg& s) { return s.data == "target"; },
        std::chrono::milliseconds(2000));

    done.store(true);
    if (publisher.joinable()) publisher.join();
    if (spinner.joinable())   spinner.join();

    ASSERT_TRUE(elapsed.has_value())
        << "watcher should have unblocked when 'target' arrived";
    EXPECT_GE(*elapsed, 50);     // we slept 80ms before publishing
    EXPECT_LT(*elapsed, 1500);
}

TEST_F(TopicWatcherTest, ResetClearsLatestAndCount) {
    auto pub_node = std::make_shared<rclcpp::Node>("tw_pub_reset");
    auto sub_node = std::make_shared<rclcpp::Node>("tw_sub_reset");
    auto pub = pub_node->create_publisher<StrMsg>(
        "/tw/reset", rclcpp::QoS(10).reliable());
    TopicWatcher<StrMsg> watcher(sub_node.get(), "/tw/reset");

    StrMsg m; m.data = "a";
    pub->publish(m);
    spin_for(sub_node, std::chrono::milliseconds(150));
    EXPECT_GE(watcher.messageCount(), 1u);

    watcher.reset();
    EXPECT_EQ(watcher.messageCount(), 0u);
    EXPECT_FALSE(watcher.latest().has_value());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    int rc = RUN_ALL_TESTS();
    if (rclcpp::ok()) rclcpp::shutdown();
    return rc;
}
