// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SkyHunter v1.5.3 — DCN-2026-017 RTH action server.
//
// Single C++ executable exposing /rth (combat_robot_msgs/action/
// ReturnToHome). Owns:
//   D-080  rclcpp_action server, wraps nav2 navigate_to_pose internally.
//   D-081  Home pose auto-recorded from first /odometry/filtered/global
//          fix, persisted to /run/skyautonet/home_pose.yaml. On reboot
//          we attempt to load the YAML so RTH still works before odom
//          stabilises.
//   D-082  Arrival accuracy verification (±max_distance_m + max_yaw_rad).
//          Fail → result.success=false, termination_reason=ACCURACY_FAIL.
//   D-083  /rtk_gnss_node/heading orientation_covariance[8] watched;
//          if > rtk_loss_threshold for > rtk_loss_threshold_sec, the
//          server enters RTK_LOSS state and continues RTH on best-
//          effort (Nav2 still runs against wheel+IMU odom). Returns
//          GPS_LOSS_DEAD_RECKONING on completion in that state.
//
// Goals execute one-at-a-time. New goals while one is in flight get
// rejected via handleGoal (caller must cancel first).

#include <atomic>
#include <chrono>
#include <cmath>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

#include <rclcpp/rclcpp.hpp>
#include <rclcpp_action/rclcpp_action.hpp>

#include <nav_msgs/msg/odometry.hpp>
#include <nav2_msgs/action/navigate_to_pose.hpp>
#include <sensor_msgs/msg/imu.hpp>
#include <geometry_msgs/msg/pose_stamped.hpp>

#include <combat_robot_msgs/action/return_to_home.hpp>

#include "san_rth/rth_helpers.hpp"

namespace san_rth
{

using ReturnToHome = combat_robot_msgs::action::ReturnToHome;
using NavigateToPose = nav2_msgs::action::NavigateToPose;
using GoalHandle = rclcpp_action::ServerGoalHandle<ReturnToHome>;
using NavGoalHandle = rclcpp_action::ClientGoalHandle<NavigateToPose>;

class RthActionNode : public rclcpp::Node
{
public:
  RthActionNode()
  : rclcpp::Node("rth_action_node")
  {
    // Tuning parameters — all overridable via launch / CLI.
    declare_parameter<double>("accuracy_threshold_m", 2.0);
    declare_parameter<double>("yaw_threshold_deg", 10.0);
    declare_parameter<double>("rtk_loss_threshold", 0.1);
    declare_parameter<double>("rtk_loss_threshold_sec", 5.0);
    declare_parameter<std::string>(
      "home_pose_path",
      "/run/skyautonet/home_pose.yaml");
    declare_parameter<std::string>(
      "odom_topic",
      "/odometry/filtered/global");
    declare_parameter<std::string>(
      "heading_topic",
      "/rtk_gnss_node/heading");
    declare_parameter<std::string>("action_name", "/rth");
    declare_parameter<std::string>("nav2_action", "navigate_to_pose");

    const double yaw_deg = get_parameter("yaw_threshold_deg").as_double();
    thresholds_ = AccuracyThresholds{
      get_parameter("accuracy_threshold_m").as_double(),
      yaw_deg * M_PI / 180.0,
    };
    rtk_cov_threshold_ = get_parameter("rtk_loss_threshold").as_double();
    rtk_hold_sec_ = get_parameter("rtk_loss_threshold_sec").as_double();
    home_pose_path_ = get_parameter("home_pose_path").as_string();

    // Attempt to warm-load a persisted home pose so RTH works before
    // the first /odometry fix lands post-reboot.
    if (auto persisted = readHomePoseYaml(home_pose_path_)) {
      home_pose_ = persisted;
      RCLCPP_INFO(
        get_logger(),
        "[rth] warm-loaded home pose from %s: (%.2f, %.2f)",
        home_pose_path_.c_str(),
        home_pose_->position.x, home_pose_->position.y);
    }

    odom_sub_ = create_subscription<nav_msgs::msg::Odometry>(
      get_parameter("odom_topic").as_string(),
      rclcpp::QoS(10).best_effort(),
      std::bind(&RthActionNode::onOdom, this, std::placeholders::_1));

    heading_sub_ = create_subscription<sensor_msgs::msg::Imu>(
      get_parameter("heading_topic").as_string(),
      rclcpp::QoS(10).best_effort(),
      std::bind(&RthActionNode::onHeading, this, std::placeholders::_1));

    nav_client_ = rclcpp_action::create_client<NavigateToPose>(
      this, get_parameter("nav2_action").as_string());

    rth_server_ = rclcpp_action::create_server<ReturnToHome>(
      this, get_parameter("action_name").as_string(),
      std::bind(
        &RthActionNode::handleGoal, this,
        std::placeholders::_1, std::placeholders::_2),
      std::bind(
        &RthActionNode::handleCancel, this,
        std::placeholders::_1),
      std::bind(
        &RthActionNode::handleAccepted, this,
        std::placeholders::_1));

    RCLCPP_INFO(
      get_logger(),
      "[rth] up — accuracy=±%.2fm/%.1f° rtk_loss=%.2fcov/%.1fs",
      thresholds_.max_distance_m, yaw_deg,
      rtk_cov_threshold_, rtk_hold_sec_);
  }

private:
  // ----------- Subscription callbacks -----------
  void onOdom(nav_msgs::msg::Odometry::SharedPtr msg)
  {
    std::lock_guard<std::mutex> lk(state_mu_);
    current_pose_ = msg->pose.pose;
    have_odom_ = true;
    if (!home_pose_) {
      home_pose_ = current_pose_;
      if (writeHomePoseYaml(home_pose_path_, *home_pose_)) {
        RCLCPP_INFO(
          get_logger(),
          "[rth] home pose locked: (%.2f, %.2f) → %s",
          home_pose_->position.x, home_pose_->position.y,
          home_pose_path_.c_str());
      } else {
        RCLCPP_WARN(
          get_logger(),
          "[rth] home pose lock — failed to write %s "
          "(in-memory only)", home_pose_path_.c_str());
      }
    }
  }

  void onHeading(sensor_msgs::msg::Imu::SharedPtr msg)
  {
    std::lock_guard<std::mutex> lk(state_mu_);
    rtk_yaw_cov_ = msg->orientation_covariance[8];
    if (rtk_yaw_cov_ > rtk_cov_threshold_) {
      if (!rtk_loss_start_sec_) {
        rtk_loss_start_sec_ = now().seconds();
      }
    } else {
      rtk_loss_start_sec_.reset();
    }
  }

  // ----------- Action server lifecycle -----------
  rclcpp_action::GoalResponse handleGoal(
    const rclcpp_action::GoalUUID & /*uuid*/,
    std::shared_ptr<const ReturnToHome::Goal> goal)
  {
    if (active_goal_.load()) {
      RCLCPP_WARN(
        get_logger(),
        "[rth] rejected — another goal is already in flight");
      return rclcpp_action::GoalResponse::REJECT;
    }
    // D-081 override: lock a NEW home from the latest odom fix.
    if (goal->reset_home_pose) {
      std::lock_guard<std::mutex> lk(state_mu_);
      if (!have_odom_) {
        RCLCPP_WARN(
          get_logger(),
          "[rth] reset_home_pose=true but no odom yet — reject");
        return rclcpp_action::GoalResponse::REJECT;
      }
      home_pose_ = current_pose_;
      writeHomePoseYaml(home_pose_path_, *home_pose_);
      RCLCPP_INFO(
        get_logger(),
        "[rth] home pose RESET to (%.2f, %.2f)",
        home_pose_->position.x, home_pose_->position.y);
    }
    return rclcpp_action::GoalResponse::ACCEPT_AND_EXECUTE;
  }

  rclcpp_action::CancelResponse handleCancel(
    std::shared_ptr<GoalHandle>/*gh*/)
  {
    cancel_requested_.store(true);
    // Snapshot the goal handle under the lock, then issue the cancel
    // outside it so we never hold nav_goal_mu_ across an rmw call.
    std::shared_ptr<NavGoalHandle> nav_goal;
    {
      std::lock_guard<std::mutex> lk(nav_goal_mu_);
      nav_goal = current_nav_goal_;
    }
    if (nav_goal) {
      nav_client_->async_cancel_goal(nav_goal);
    }
    return rclcpp_action::CancelResponse::ACCEPT;
  }

  void handleAccepted(std::shared_ptr<GoalHandle> gh)
  {
    active_goal_.store(true);
    cancel_requested_.store(false);

    // DCN-2026-017 P1-3 fix: track the execute thread instead of
    // detaching, so the destructor can join cleanly on shutdown
    // (avoids dangling `this` access if the node is destroyed while
    // a goal is in flight). Pre-join any previous thread first — by
    // construction it MUST be finished here because handleGoal
    // rejects when active_goal_ is true, so the previous execute()
    // has already set active_goal_=false before returning. The join
    // is therefore a no-op in normal flow but releases the OS handle
    // so the next assignment doesn't terminate() the live thread.
    if (execute_thread_.joinable()) {
      execute_thread_.join();
    }
    execute_thread_ = std::thread{[this, gh]() {execute(gh);}};
  }

  // ----------- Goal execution -----------
  void execute(std::shared_ptr<GoalHandle> gh)
  {
    auto result = std::make_shared<ReturnToHome::Result>();
    auto feedback = std::make_shared<ReturnToHome::Feedback>();

    // Snapshot home + initial state under the mutex.
    geometry_msgs::msg::Pose home_snapshot;
    {
      std::lock_guard<std::mutex> lk(state_mu_);
      if (!home_pose_) {
        result->success = false;
        result->termination_reason = "NO_HOME_POSE";
        gh->abort(result);
        active_goal_.store(false);
        RCLCPP_WARN(
          get_logger(),
          "[rth] aborted — no home pose recorded yet");
        return;
      }
      home_snapshot = *home_pose_;
    }

    feedback->current_state = "PLANNING";
    {
      // rtk_yaw_cov_ is written by onHeading on another executor thread;
      // read it under the lock (was an unlocked data race).
      std::lock_guard<std::mutex> lk(state_mu_);
      feedback->rtk_yaw_covariance = rtk_yaw_cov_;
    }
    gh->publish_feedback(feedback);

    if (!nav_client_->wait_for_action_server(std::chrono::seconds(5))) {
      result->success = false;
      result->termination_reason = "TIMEOUT";
      gh->abort(result);
      active_goal_.store(false);
      RCLCPP_ERROR(
        get_logger(),
        "[rth] nav2 action server not available");
      return;
    }

    NavigateToPose::Goal nav_goal;
    nav_goal.pose.header.frame_id = "map";
    nav_goal.pose.header.stamp = now();
    nav_goal.pose.pose = home_snapshot;

    rclcpp_action::Client<NavigateToPose>::SendGoalOptions opts;
    auto goal_future = nav_client_->async_send_goal(nav_goal, opts);
    if (goal_future.wait_for(std::chrono::seconds(5)) !=
      std::future_status::ready)
    {
      result->success = false;
      result->termination_reason = "TIMEOUT";
      gh->abort(result);
      active_goal_.store(false);
      return;
    }
    auto nav_goal_handle = goal_future.get();
    if (!nav_goal_handle) {
      result->success = false;
      result->termination_reason = "TIMEOUT";
      gh->abort(result);
      active_goal_.store(false);
      RCLCPP_ERROR(get_logger(), "[rth] nav2 rejected our goal");
      return;
    }
    {
      std::lock_guard<std::mutex> lk(nav_goal_mu_);
      current_nav_goal_ = nav_goal_handle;
    }

    feedback->current_state = "MOVING";
    gh->publish_feedback(feedback);

    auto nav_result_future =
      nav_client_->async_get_result(nav_goal_handle);

    // Poll loop — publish feedback every 500 ms, watch for RTK loss
    // transition + cancel request.
    bool entered_rtk_loss = false;
    while (rclcpp::ok()) {
      if (cancel_requested_.load()) {
        result->success = false;
        result->termination_reason = "CANCELLED";
        gh->canceled(result);
        active_goal_.store(false);
        {
          std::lock_guard<std::mutex> lk(nav_goal_mu_);
          current_nav_goal_.reset();
        }
        return;
      }
      const auto status = nav_result_future.wait_for(
        std::chrono::milliseconds(500));
      {
        std::lock_guard<std::mutex> lk(state_mu_);
        const double dx = current_pose_.position.x - home_snapshot.position.x;
        const double dy = current_pose_.position.y - home_snapshot.position.y;
        feedback->distance_remaining_m = std::hypot(dx, dy);
        feedback->rtk_yaw_covariance = rtk_yaw_cov_;
        const bool in_loss =
          rtkLossActive(
          rtk_loss_start_sec_, now().seconds(),
          rtk_hold_sec_);
        if (in_loss && !entered_rtk_loss) {
          entered_rtk_loss = true;
          RCLCPP_WARN(
            get_logger(),
            "[rth] RTK loss sustained — continuing on dead "
            "reckoning (cov=%.3f)", rtk_yaw_cov_);
        }
        feedback->current_state =
          entered_rtk_loss ? "RTK_LOSS" : "MOVING";
      }
      gh->publish_feedback(feedback);
      if (status == std::future_status::ready) {
        break;
      }
    }

    // Verify arrival.
    feedback->current_state = "VERIFYING";
    gh->publish_feedback(feedback);

    AccuracyResult acc;
    {
      std::lock_guard<std::mutex> lk(state_mu_);
      acc = evaluateAccuracy(current_pose_, home_snapshot, thresholds_);
    }
    result->final_distance_m = acc.distance_m;
    result->final_yaw_error_rad = acc.yaw_error_rad;

    if (entered_rtk_loss) {
      result->success = acc.passed;  // best-effort; reason explains.
      result->termination_reason = "GPS_LOSS_DEAD_RECKONING";
    } else if (acc.passed) {
      result->success = true;
      result->termination_reason = "OK";
    } else {
      result->success = false;
      result->termination_reason = "ACCURACY_FAIL";
    }

    // A cancel that landed during the final poll iteration leaves the
    // goal in CANCELING — succeed() would then throw. Honour the cancel.
    if (cancel_requested_.load()) {
      result->success = false;
      result->termination_reason = "CANCELLED";
      gh->canceled(result);
    } else {
      gh->succeed(result);
    }
    active_goal_.store(false);
    {
      std::lock_guard<std::mutex> lk(nav_goal_mu_);
      current_nav_goal_.reset();
    }
    RCLCPP_INFO(
      get_logger(),
      "[rth] done — reason=%s dist=%.2fm yaw=%.3frad",
      result->termination_reason.c_str(),
      result->final_distance_m, result->final_yaw_error_rad);
  }

  // ----------- State -----------
  rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr odom_sub_;
  rclcpp::Subscription<sensor_msgs::msg::Imu>::SharedPtr heading_sub_;
  rclcpp_action::Client<NavigateToPose>::SharedPtr nav_client_;
  rclcpp_action::Server<ReturnToHome>::SharedPtr rth_server_;
  // Guards current_nav_goal_: written/reset by the execute() thread,
  // read by handleCancel() on an executor thread (MultiThreadedExecutor).
  // Concurrent access to the shared_ptr without this lock is a data race.
  std::mutex nav_goal_mu_;
  std::shared_ptr<NavGoalHandle> current_nav_goal_;

  AccuracyThresholds thresholds_{};
  double rtk_cov_threshold_ = 0.1;
  double rtk_hold_sec_ = 5.0;
  std::string home_pose_path_;

  std::mutex state_mu_;
  std::optional<geometry_msgs::msg::Pose> home_pose_;
  geometry_msgs::msg::Pose current_pose_{};
  bool have_odom_ = false;
  double rtk_yaw_cov_ = 0.0;
  std::optional<double> rtk_loss_start_sec_;

  std::atomic<bool> active_goal_{false};
  std::atomic<bool> cancel_requested_{false};

  // DCN-2026-017 P1-3 fix: tracked goal-execution thread (previously
  // detached). Joined in handleAccepted (before reassignment) and in
  // the destructor (on node shutdown).
  std::thread execute_thread_;

public:
  // DCN-2026-017 P1-3 fix: destructor signals cancel + joins the
  // execute thread so the node tears down cleanly. Without this an
  // in-flight goal's thread would survive node destruction and
  // dereference a dangling `this` pointer when its next callback
  // fires (logs via get_logger() touch the node, etc.). In
  // production this is mitigated by systemd Type=simple +
  // RestartSec, but the contract should be correct regardless.
  ~RthActionNode() override
  {
    cancel_requested_.store(true);    // accelerate execute() exit
    if (execute_thread_.joinable()) {
      execute_thread_.join();
    }
  }
};

}  // namespace san_rth

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  // Multi-threaded executor so the action callbacks don't starve the
  // subscription callbacks (odom + heading must keep flowing while a
  // goal is mid-execution).
  rclcpp::executors::MultiThreadedExecutor exec;
  auto node = std::make_shared<san_rth::RthActionNode>();
  exec.add_node(node);
  exec.spin();
  rclcpp::shutdown();
  return 0;
}
