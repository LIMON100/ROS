// SAN v1.5 Phase 2-E Turn 2 — UnitreeGo2Node implementation.

#include "san_unitree_driver/unitree_go2_node.hpp"

#include <algorithm>
#include <chrono>
#include <stdexcept>

namespace san_unitree_driver {

using namespace std::chrono_literals;

// ─── ctors ──────────────────────────────────────────────────────────────

UnitreeGo2Node::UnitreeGo2Node(const rclcpp::NodeOptions& opts)
    : UnitreeGo2Node(opts, makeRealGo2Sdk()) {
  // Delegate to the test-friendly ctor with the real SDK.
}

UnitreeGo2Node::UnitreeGo2Node(const rclcpp::NodeOptions& opts,
                                std::unique_ptr<Go2SdkInterface> sdk)
    : rclcpp::Node("unitree_go2_node", opts),
      sdk_(std::move(sdk)) {
  if (!sdk_) {
    throw std::runtime_error(
        "UnitreeGo2Node: null SDK injected — fail-closed");
  }

  declareParameters();
  loadParameters();

  // Initialize SDK. Throws on failure — main catches and fails-closed.
  sdk_->init(interface_name_);

  wireSdkAndRos();

  RCLCPP_INFO(get_logger(),
              "UnitreeGo2Node UP on interface=%s",
              interface_name_.c_str());
}

// ─── parameters ─────────────────────────────────────────────────────────

void UnitreeGo2Node::declareParameters() {
  declare_parameter<std::string>("interface_name",    "eth0");
  declare_parameter<std::string>("lidar_frame_id",    "go2_lidar_frame");
  declare_parameter<std::string>("imu_frame_id",      "go2_imu_frame");
  declare_parameter<std::string>("camera_frame_id",   "go2_camera_frame");
  declare_parameter<std::string>("base_frame_id",     "go2_base_link");
  declare_parameter<double>("max_cmd_vel_linear_mps",    1.5);
  declare_parameter<double>("max_cmd_vel_angular_rps",   2.0);
}

void UnitreeGo2Node::loadParameters() {
  interface_name_         = get_parameter("interface_name").as_string();
  lidar_frame_id_         = get_parameter("lidar_frame_id").as_string();
  imu_frame_id_           = get_parameter("imu_frame_id").as_string();
  camera_frame_id_        = get_parameter("camera_frame_id").as_string();
  base_frame_id_          = get_parameter("base_frame_id").as_string();
  max_cmd_vel_linear_mps_ = get_parameter("max_cmd_vel_linear_mps").as_double();
  max_cmd_vel_angular_rps_= get_parameter("max_cmd_vel_angular_rps").as_double();
}

// ─── topic wiring ───────────────────────────────────────────────────────

void UnitreeGo2Node::wireSdkAndRos() {
  // QoS per IDS v1.5 §7:
  //   - Sensor streams (LiDAR/IMU/camera): SensorDataQoS (best_effort,
  //     depth varies; LiDAR small depth, IMU large depth to handle
  //     bursts).
  //   - State: P2 (best_effort, depth 1 — only-latest semantics).
  //   - cmd_vel / goal_pose: P1 (reliable, small depth).

  lidar_pub_  = create_publisher<sensor_msgs::msg::PointCloud2>(
      "~/lidar", rclcpp::SensorDataQoS().keep_last(5));
  imu_pub_    = create_publisher<sensor_msgs::msg::Imu>(
      "~/imu",   rclcpp::SensorDataQoS().keep_last(50));
  camera_pub_ = create_publisher<sensor_msgs::msg::Image>(
      "~/internal_camera",
      rclcpp::SensorDataQoS().keep_last(5));

  cmd_vel_sub_ = create_subscription<geometry_msgs::msg::Twist>(
      "~/cmd_vel",
      rclcpp::QoS(5).reliable(),
      std::bind(&UnitreeGo2Node::onCmdVel, this, std::placeholders::_1));

  goal_pose_sub_ = create_subscription<geometry_msgs::msg::PoseStamped>(
      "~/goal_pose",
      rclcpp::QoS(5).reliable(),
      std::bind(&UnitreeGo2Node::onGoalPose, this, std::placeholders::_1));

  // SDK → ROS callbacks (these run on SDK thread; rclcpp Publisher
  // is thread-safe so direct publish from here is OK).
  sdk_->registerLidarCallback(
      [this](const LidarScan& s) { onSdkLidar(s); });
  sdk_->registerImuCallback(
      [this](const ImuData& i) { onSdkImu(i); });
  sdk_->registerCameraCallback(
      [this](const CameraFrame& f) { onSdkCamera(f); });
  sdk_->registerStateCallback(
      [this](const Go2State& s) { onSdkState(s); });

  health_timer_ = create_wall_timer(
      1s, std::bind(&UnitreeGo2Node::onHealthTick, this));
}

// ─── SDK → ROS ──────────────────────────────────────────────────────────

void UnitreeGo2Node::onSdkLidar(const LidarScan& scan) {
  ++lidar_count_;

  // [v1.5.1 C-7 fix] Pack std::vector<LidarPoint> into PointCloud2.
  // Each point is 4 float32 = 16 bytes. Layout: x, y, z, intensity.
  //
  // We allocate the message as std::unique_ptr and publish() it as a
  // moved unique_ptr. Combined with use_intra_process_comms(true)
  // (set in wireSdkAndRos via PublisherOptions), this enables zero-
  // copy delivery to any intra-process subscriber — saves ~16 MB/s
  // for the LiDAR stream + ~180 MB/s for the camera stream (see
  // onSdkCamera). Inter-process subscribers (e.g. across SBC1↔SBC2)
  // still pay the DDS serialization cost.
  auto msg = std::make_unique<sensor_msgs::msg::PointCloud2>();
  msg->header.stamp     = toRosTime(scan.timestamp_ns);
  msg->header.frame_id  = lidar_frame_id_;
  msg->height           = 1;
  msg->width            = static_cast<uint32_t>(scan.points.size());
  msg->is_bigendian     = false;
  msg->is_dense         = true;
  msg->point_step       = 16;
  msg->row_step         = msg->point_step * msg->width;

  msg->fields.resize(4);
  auto setField = [&](size_t i, const char* name, uint32_t offset) {
    msg->fields[i].name     = name;
    msg->fields[i].offset   = offset;
    msg->fields[i].datatype = sensor_msgs::msg::PointField::FLOAT32;
    msg->fields[i].count    = 1;
  };
  setField(0, "x",         0);
  setField(1, "y",         4);
  setField(2, "z",         8);
  setField(3, "intensity", 12);

  msg->data.resize(msg->row_step);
  std::memcpy(msg->data.data(), scan.points.data(), msg->row_step);

  lidar_pub_->publish(std::move(msg));
}

void UnitreeGo2Node::onSdkImu(const ImuData& imu) {
  ++imu_count_;
  // [v1.5.1 C-7 fix] UniquePtr publish for intra-process zero-copy.
  auto msg = std::make_unique<sensor_msgs::msg::Imu>();
  msg->header.stamp    = toRosTime(imu.timestamp_ns);
  msg->header.frame_id = imu_frame_id_;
  msg->angular_velocity.x         = imu.angular_velocity_x;
  msg->angular_velocity.y         = imu.angular_velocity_y;
  msg->angular_velocity.z         = imu.angular_velocity_z;
  msg->linear_acceleration.x      = imu.linear_acceleration_x;
  msg->linear_acceleration.y      = imu.linear_acceleration_y;
  msg->linear_acceleration.z      = imu.linear_acceleration_z;
  msg->orientation.x              = imu.orientation_x;
  msg->orientation.y              = imu.orientation_y;
  msg->orientation.z              = imu.orientation_z;
  msg->orientation.w              = imu.orientation_w;
  // Covariance fields left at 0 (unknown); downstream nodes apply
  // their own confidence estimate.
  imu_pub_->publish(std::move(msg));
}

void UnitreeGo2Node::onSdkCamera(const CameraFrame& frame) {
  ++camera_count_;
  // [v1.5.1 C-7 fix] UniquePtr publish for intra-process zero-copy.
  //
  // The remaining copy is `msg->data = frame.data` — this is bounded
  // by the SDK interface contract (CameraFrame is delivered by const
  // ref and the SDK may reuse its internal buffer for the next frame).
  // Full zero-copy from SDK requires a separate SDK adapter rev. to
  // deliver `CameraFrame&&` or `std::shared_ptr<std::vector<uint8_t>>`.
  // Tracked as follow-up D-007.A in DCN-2026-004.
  auto msg = std::make_unique<sensor_msgs::msg::Image>();
  msg->header.stamp    = toRosTime(frame.timestamp_ns);
  msg->header.frame_id = camera_frame_id_;
  msg->height          = frame.height;
  msg->width           = frame.width;
  msg->encoding        = frame.encoding;
  msg->is_bigendian    = 0;
  // step = width * bytes-per-pixel; for rgb8 / bgr8 = width * 3.
  // For yuv422 = width * 2. Use encoding hint.
  uint32_t bytes_per_pixel = (frame.encoding == "yuv422") ? 2 : 3;
  msg->step = frame.width * bytes_per_pixel;
  msg->data = frame.data;   // bounded copy — see comment above
  camera_pub_->publish(std::move(msg));
}

void UnitreeGo2Node::onSdkState(const Go2State& /*state*/) {
  ++state_count_;
  // TODO Turn 3+: publish a real Go2State message after we add it to
  // combat_robot_msgs. For now, just count and let the health timer
  // log the rate.
}

// ─── ROS → SDK ──────────────────────────────────────────────────────────

void UnitreeGo2Node::onCmdVel(
    const geometry_msgs::msg::Twist::SharedPtr msg) {
  ++cmd_vel_count_;

  // Safety: clamp magnitudes to configured limits. Logs a warning
  // if any axis is clamped.
  CmdVel cmd;
  cmd.linear_x_mps  = static_cast<float>(std::clamp(
      msg->linear.x,
      -max_cmd_vel_linear_mps_,
       max_cmd_vel_linear_mps_));
  cmd.linear_y_mps  = static_cast<float>(std::clamp(
      msg->linear.y,
      -max_cmd_vel_linear_mps_,
       max_cmd_vel_linear_mps_));
  cmd.angular_z_rps = static_cast<float>(std::clamp(
      msg->angular.z,
      -max_cmd_vel_angular_rps_,
       max_cmd_vel_angular_rps_));

  const bool clamped =
      (cmd.linear_x_mps  != msg->linear.x)  ||
      (cmd.linear_y_mps  != msg->linear.y)  ||
      (cmd.angular_z_rps != msg->angular.z);
  if (clamped) {
    ++cmd_vel_clamped_;
    RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 1000,
        "cmd_vel clamped: in (%.2f, %.2f, %.2f) "
        "limit (%.2f m/s, %.2f rad/s)",
        msg->linear.x, msg->linear.y, msg->angular.z,
        max_cmd_vel_linear_mps_, max_cmd_vel_angular_rps_);
  }

  if (!sdk_->sendCmdVel(cmd)) {
    RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 1000,
        "sendCmdVel failed — SDK unhealthy?");
  }
}

void UnitreeGo2Node::onGoalPose(
    const geometry_msgs::msg::PoseStamped::SharedPtr msg) {
  ++goal_count_;
  GoalPose goal;
  goal.position_x_m = msg->pose.position.x;
  goal.position_y_m = msg->pose.position.y;
  // Convert quaternion → yaw (Z-axis rotation).
  const auto& q = msg->pose.orientation;
  goal.yaw_rad = std::atan2(2.0 * (q.w * q.z + q.x * q.y),
                             1.0 - 2.0 * (q.y * q.y + q.z * q.z));
  if (!sdk_->sendGoalPose(goal)) {
    RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 1000,
        "sendGoalPose failed — SDK unhealthy?");
  }
}

// ─── health ─────────────────────────────────────────────────────────────

void UnitreeGo2Node::onHealthTick() {
  RCLCPP_INFO(get_logger(),
      "go2 healthy=%d  lidar=%zu imu=%zu cam=%zu state=%zu | "
      "cmd_tx=%zu (clamped=%zu) goal_tx=%zu",
      static_cast<int>(sdk_->isHealthy()),
      lidar_count_, imu_count_, camera_count_, state_count_,
      cmd_vel_count_, cmd_vel_clamped_, goal_count_);

  if (!sdk_->isHealthy()) {
    RCLCPP_ERROR(get_logger(),
        "Go2 SDK link UNHEALTHY — downstream sensors may stale");
  }
}

// ─── helpers ────────────────────────────────────────────────────────────

rclcpp::Time UnitreeGo2Node::toRosTime(uint64_t timestamp_ns) const {
  // SDK timestamps are unix epoch nanoseconds. rclcpp::Time accepts
  // nanos directly.
  return rclcpp::Time(static_cast<int64_t>(timestamp_ns), RCL_ROS_TIME);
}

}  // namespace san_unitree_driver
