// SAN v1.5 Phase 2-E Turn 2 — UnitreeGo2Node unit tests.
//
// Uses MockGo2Sdk (defined here) injected via the test-friendly
// ctor. No live Go2 / SDK needed.
//
// Coverage:
//   T1  ctor succeeds with mock SDK (verifies wireSdkAndRos())
//   T2  ctor throws on null SDK injection (fail-closed)
//   T3  LidarScan from SDK → PointCloud2 published with correct
//       width / fields / data bytes
//   T4  ImuData from SDK → sensor_msgs/Imu published with the same
//       angular/linear/orientation values
//   T5  CameraFrame from SDK → sensor_msgs/Image published with
//       correct dimensions and encoding
//   T6  ROS Twist on ~/cmd_vel → sdk_->sendCmdVel called
//   T7  cmd_vel beyond limits is clamped (warns + clamps)
//   T8  ROS PoseStamped on ~/goal_pose → sdk_->sendGoalPose called
//       with yaw extracted from quaternion

#include "san_unitree_driver/unitree_go2_node.hpp"
#include "san_unitree_driver/go2_sdk_interface.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

namespace san_unitree_driver {
namespace {

using namespace std::chrono_literals;

// ─── MockGo2Sdk ─────────────────────────────────────────────────────────

class MockGo2Sdk : public Go2SdkInterface {
public:
  void init(const std::string& iface) override { last_iface_ = iface; initialized_ = true; }
  void registerLidarCallback(LidarCallback cb) override   { lidar_cb_  = std::move(cb); }
  void registerImuCallback(ImuCallback cb) override       { imu_cb_    = std::move(cb); }
  void registerCameraCallback(CameraCallback cb) override { camera_cb_ = std::move(cb); }
  void registerStateCallback(StateCallback cb) override   { state_cb_  = std::move(cb); }

  bool sendCmdVel(const CmdVel& cmd) override {
    last_cmd_  = cmd;
    cmd_count_++;
    return cmd_send_ok_;
  }
  bool sendGoalPose(const GoalPose& goal) override {
    last_goal_ = goal;
    goal_count_++;
    return goal_send_ok_;
  }

  bool isHealthy() const override { return healthy_; }

  // Test driving — invoke SDK callbacks from test thread.
  void emitLidar(const LidarScan& s)  { if (lidar_cb_)  lidar_cb_(s); }
  void emitImu(const ImuData& i)      { if (imu_cb_)    imu_cb_(i); }
  void emitCamera(const CameraFrame& f){ if (camera_cb_) camera_cb_(f); }
  void emitState(const Go2State& s)   { if (state_cb_)  state_cb_(s); }

  // Inspection
  std::string  last_iface_;
  bool         initialized_   = false;
  bool         healthy_       = true;
  bool         cmd_send_ok_   = true;
  bool         goal_send_ok_  = true;
  CmdVel       last_cmd_{};
  GoalPose     last_goal_{};
  std::atomic<int> cmd_count_{0};
  std::atomic<int> goal_count_{0};

private:
  LidarCallback  lidar_cb_;
  ImuCallback    imu_cb_;
  CameraCallback camera_cb_;
  StateCallback  state_cb_;
};

// ─── test fixture ───────────────────────────────────────────────────────

class UnitreeGo2NodeTest : public ::testing::Test {
protected:
  void SetUp() override {
    if (!rclcpp::ok()) rclcpp::init(0, nullptr);
  }

  void TearDown() override {
    node_.reset();
    helper_.reset();
    executor_.reset();
  }

  /// Build node with mock SDK + helper publisher/subscriber on a
  /// sibling node. Returns the raw mock pointer (Node owns the SDK
  /// via unique_ptr, so we capture the pointer BEFORE move).
  MockGo2Sdk* makeNodeWithMock() {
    auto mock = std::make_unique<MockGo2Sdk>();
    MockGo2Sdk* raw = mock.get();
    rclcpp::NodeOptions opts;
    node_ = std::make_shared<UnitreeGo2Node>(opts, std::move(mock));

    helper_ = std::make_shared<rclcpp::Node>("test_helper");
    executor_ = std::make_shared<rclcpp::executors::SingleThreadedExecutor>();
    executor_->add_node(node_);
    executor_->add_node(helper_);
    return raw;
  }

  void spinFor(std::chrono::milliseconds dur) {
    const auto deadline = std::chrono::steady_clock::now() + dur;
    while (std::chrono::steady_clock::now() < deadline && rclcpp::ok()) {
      executor_->spin_some(20ms);
    }
  }

  std::shared_ptr<UnitreeGo2Node> node_;
  std::shared_ptr<rclcpp::Node>   helper_;
  std::shared_ptr<rclcpp::executors::SingleThreadedExecutor> executor_;
};

// ─── T1: ctor succeeds with mock SDK ────────────────────────────────────

TEST_F(UnitreeGo2NodeTest, T1_CtorSucceedsWithMockSdk) {
  auto* mock = makeNodeWithMock();
  EXPECT_TRUE(mock->initialized_);
  EXPECT_FALSE(mock->last_iface_.empty());  // got the parameter default "eth0"
}

// ─── T2: ctor throws on null SDK injection ──────────────────────────────

TEST_F(UnitreeGo2NodeTest, T2_CtorThrowsOnNullSdk) {
  rclcpp::NodeOptions opts;
  EXPECT_THROW({
    UnitreeGo2Node node(opts, /*sdk=*/nullptr);
  }, std::runtime_error);
}

// ─── T3: LiDAR scan → PointCloud2 ───────────────────────────────────────

TEST_F(UnitreeGo2NodeTest, T3_LidarScanPublishedAsPointCloud2) {
  auto* mock = makeNodeWithMock();

  // Helper subscriber to capture the published PointCloud2.
  std::vector<sensor_msgs::msg::PointCloud2> recv;
  auto sub = helper_->create_subscription<sensor_msgs::msg::PointCloud2>(
      "/unitree_go2_node/lidar",
      rclcpp::SensorDataQoS().keep_last(5),
      [&recv](sensor_msgs::msg::PointCloud2::SharedPtr m) {
        recv.push_back(*m);
      });

  // Emit a 3-point scan via the mock.
  LidarScan scan;
  scan.timestamp_ns = 1'700'000'000'000'000'000ULL;
  scan.seq = 1;
  scan.points = {
      {1.0f, 2.0f, 3.0f, 100.0f},
      {4.0f, 5.0f, 6.0f, 110.0f},
      {7.0f, 8.0f, 9.0f, 120.0f},
  };
  mock->emitLidar(scan);

  spinFor(200ms);
  ASSERT_FALSE(recv.empty()) << "no PointCloud2 received";
  const auto& msg = recv.front();
  EXPECT_EQ(msg.width, 3u);
  EXPECT_EQ(msg.height, 1u);
  EXPECT_EQ(msg.point_step, 16u);
  EXPECT_EQ(msg.row_step, 48u);
  EXPECT_EQ(msg.fields.size(), 4u);
  EXPECT_EQ(msg.fields[0].name, "x");
  EXPECT_EQ(msg.fields[3].name, "intensity");
  EXPECT_EQ(msg.data.size(), 48u);
}

// ─── T4: IMU data → sensor_msgs/Imu ─────────────────────────────────────

TEST_F(UnitreeGo2NodeTest, T4_ImuDataPublishedAsSensorMsgs) {
  auto* mock = makeNodeWithMock();
  std::vector<sensor_msgs::msg::Imu> recv;
  auto sub = helper_->create_subscription<sensor_msgs::msg::Imu>(
      "/unitree_go2_node/imu",
      rclcpp::SensorDataQoS().keep_last(50),
      [&recv](sensor_msgs::msg::Imu::SharedPtr m) {
        recv.push_back(*m);
      });

  ImuData imu;
  imu.timestamp_ns = 100;
  imu.seq = 5;
  imu.angular_velocity_x      = 0.1;
  imu.angular_velocity_y      = 0.2;
  imu.angular_velocity_z      = 0.3;
  imu.linear_acceleration_x   = 1.1;
  imu.linear_acceleration_y   = 1.2;
  imu.linear_acceleration_z   = 9.8;
  imu.orientation_x = 0.0; imu.orientation_y = 0.0;
  imu.orientation_z = 0.0; imu.orientation_w = 1.0;
  mock->emitImu(imu);

  spinFor(200ms);
  ASSERT_FALSE(recv.empty());
  const auto& msg = recv.front();
  EXPECT_DOUBLE_EQ(msg.angular_velocity.x,    0.1);
  EXPECT_DOUBLE_EQ(msg.angular_velocity.y,    0.2);
  EXPECT_DOUBLE_EQ(msg.angular_velocity.z,    0.3);
  EXPECT_DOUBLE_EQ(msg.linear_acceleration.x, 1.1);
  EXPECT_DOUBLE_EQ(msg.linear_acceleration.z, 9.8);
  EXPECT_DOUBLE_EQ(msg.orientation.w, 1.0);
}

// ─── T5: CameraFrame → sensor_msgs/Image ────────────────────────────────

TEST_F(UnitreeGo2NodeTest, T5_CameraFramePublishedAsImage) {
  auto* mock = makeNodeWithMock();
  std::vector<sensor_msgs::msg::Image> recv;
  auto sub = helper_->create_subscription<sensor_msgs::msg::Image>(
      "/unitree_go2_node/internal_camera",
      rclcpp::SensorDataQoS().keep_last(5),
      [&recv](sensor_msgs::msg::Image::SharedPtr m) {
        recv.push_back(*m);
      });

  CameraFrame frame;
  frame.timestamp_ns = 200;
  frame.seq = 10;
  frame.width  = 4;
  frame.height = 2;
  frame.encoding = "rgb8";
  frame.data.assign(4 * 2 * 3, 0xAB);   // 24 bytes
  mock->emitCamera(frame);

  spinFor(200ms);
  ASSERT_FALSE(recv.empty());
  const auto& msg = recv.front();
  EXPECT_EQ(msg.width, 4u);
  EXPECT_EQ(msg.height, 2u);
  EXPECT_EQ(msg.encoding, "rgb8");
  EXPECT_EQ(msg.step, 12u);   // width * 3 bytes per pixel
  EXPECT_EQ(msg.data.size(), 24u);
  EXPECT_EQ(msg.data[0], 0xAB);
}

// ─── T6: ROS cmd_vel → SDK sendCmdVel ───────────────────────────────────

TEST_F(UnitreeGo2NodeTest, T6_CmdVelForwardedToSdk) {
  auto* mock = makeNodeWithMock();
  auto pub = helper_->create_publisher<geometry_msgs::msg::Twist>(
      "/unitree_go2_node/cmd_vel", rclcpp::QoS(5).reliable());

  geometry_msgs::msg::Twist cmd;
  cmd.linear.x  = 0.5;
  cmd.linear.y  = 0.2;
  cmd.angular.z = 0.7;
  pub->publish(cmd);

  spinFor(200ms);
  EXPECT_EQ(mock->cmd_count_.load(), 1);
  EXPECT_FLOAT_EQ(mock->last_cmd_.linear_x_mps,  0.5f);
  EXPECT_FLOAT_EQ(mock->last_cmd_.linear_y_mps,  0.2f);
  EXPECT_FLOAT_EQ(mock->last_cmd_.angular_z_rps, 0.7f);
}

// ─── T7: cmd_vel clamping ───────────────────────────────────────────────

TEST_F(UnitreeGo2NodeTest, T7_CmdVelClampedToParamLimits) {
  auto* mock = makeNodeWithMock();
  // Defaults: linear 1.5 m/s, angular 2.0 rad/s.
  auto pub = helper_->create_publisher<geometry_msgs::msg::Twist>(
      "/unitree_go2_node/cmd_vel", rclcpp::QoS(5).reliable());

  geometry_msgs::msg::Twist cmd;
  cmd.linear.x  = 5.0;   // exceeds 1.5
  cmd.linear.y  = -3.0;  // exceeds -1.5
  cmd.angular.z = 10.0;  // exceeds 2.0
  pub->publish(cmd);

  spinFor(200ms);
  EXPECT_EQ(mock->cmd_count_.load(), 1);
  EXPECT_FLOAT_EQ(mock->last_cmd_.linear_x_mps,  1.5f);
  EXPECT_FLOAT_EQ(mock->last_cmd_.linear_y_mps,  -1.5f);
  EXPECT_FLOAT_EQ(mock->last_cmd_.angular_z_rps, 2.0f);
}

// ─── T8: goal_pose → sendGoalPose with yaw extraction ───────────────────

TEST_F(UnitreeGo2NodeTest, T8_GoalPoseYawExtractedFromQuaternion) {
  auto* mock = makeNodeWithMock();
  auto pub = helper_->create_publisher<geometry_msgs::msg::PoseStamped>(
      "/unitree_go2_node/goal_pose", rclcpp::QoS(5).reliable());

  geometry_msgs::msg::PoseStamped goal;
  goal.header.frame_id = "map";
  goal.pose.position.x = 10.0;
  goal.pose.position.y = -5.0;
  // 90° about Z: q = [0, 0, sin(45°), cos(45°)] ≈ [0, 0, 0.7071, 0.7071]
  goal.pose.orientation.x = 0.0;
  goal.pose.orientation.y = 0.0;
  goal.pose.orientation.z = 0.7071067811865475;
  goal.pose.orientation.w = 0.7071067811865475;
  pub->publish(goal);

  spinFor(200ms);
  EXPECT_EQ(mock->goal_count_.load(), 1);
  EXPECT_DOUBLE_EQ(mock->last_goal_.position_x_m, 10.0);
  EXPECT_DOUBLE_EQ(mock->last_goal_.position_y_m, -5.0);
  // π/2 = 1.5707963...; tolerate float error
  EXPECT_NEAR(mock->last_goal_.yaw_rad, 1.5707963, 1e-6);
}

}  // namespace
}  // namespace san_unitree_driver

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  const int rc = RUN_ALL_TESTS();
  if (rclcpp::ok()) rclcpp::shutdown();
  return rc;
}
