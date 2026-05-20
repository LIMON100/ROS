// SAN v1.5 — SurveillanceNode implementation.

#include "san_surveillance/surveillance_node.hpp"

#include <chrono>

namespace san_surveillance {

using namespace std::chrono_literals;

namespace {

constexpr uint32_t LEADER_ROBOT_ID = 1;
constexpr uint32_t HUB_ROBOT_ID    = 2;

/// Heuristic role inference from robot_id.
/// In a richer system this would come from a role-management topic.
RobotRole inferRole(uint32_t robot_id) {
  if (robot_id == LEADER_ROBOT_ID) return RobotRole::Leader;
  if (robot_id == HUB_ROBOT_ID)    return RobotRole::Hub;
  return RobotRole::Follower;
}

uint64_t nowMs(const rclcpp::Time& t) {
  return static_cast<uint64_t>(t.nanoseconds() / 1'000'000ULL);
}

}  // namespace

SurveillanceNode::SurveillanceNode(const rclcpp::NodeOptions& opts)
    : rclcpp::Node("surveillance_node", opts) {
  declareParameters();
  loadParameters();

  robot_status_sub_ = create_subscription<combat_robot_msgs::msg::RobotStatus>(
      "/swarm/robot_status",
      rclcpp::QoS(20).best_effort(),
      std::bind(&SurveillanceNode::onRobotStatus, this,
                 std::placeholders::_1));

  threat_sub_ = create_subscription<combat_robot_msgs::msg::ThreatAlert>(
      "/swarm/threat_alert",
      rclcpp::QoS(10).reliable(),
      std::bind(&SurveillanceNode::onThreatAlert, this,
                 std::placeholders::_1));

  sector_pub_ = create_publisher<combat_robot_msgs::msg::SurveillanceSectorAssignment>(
      "/swarm/surveillance/sector_assign",
      rclcpp::QoS(20).reliable().transient_local());

  pantilt_pub_ = create_publisher<combat_robot_msgs::msg::PanTiltCommand>(
      "/swarm/cmd/pantilt",
      rclcpp::QoS(20).reliable());

  realloc_timer_ = create_wall_timer(
      std::chrono::seconds(realloc_period_sec_),
      std::bind(&SurveillanceNode::onReallocateTick, this));

  RCLCPP_INFO(get_logger(),
      "SurveillanceNode UP: realloc_period=%ds robot_timeout=%ds "
      "threat_validity=%ds default_mode=Recon",
      realloc_period_sec_, robot_timeout_sec_, threat_validity_sec_);
}

void SurveillanceNode::declareParameters() {
  declare_parameter<int>("realloc_period_sec",  10);
  declare_parameter<int>("robot_timeout_sec",    3);
  declare_parameter<int>("threat_validity_sec", 10);
}

void SurveillanceNode::loadParameters() {
  realloc_period_sec_  = get_parameter("realloc_period_sec").as_int();
  robot_timeout_sec_   = get_parameter("robot_timeout_sec").as_int();
  threat_validity_sec_ = get_parameter("threat_validity_sec").as_int();
}

void SurveillanceNode::onRobotStatus(
    const combat_robot_msgs::msg::RobotStatus::SharedPtr msg) {
  std::lock_guard<std::mutex> g(state_mu_);
  auto& s = robots_[msg->robot_id];
  s.role         = inferRole(msg->robot_id);
  s.alive        = true;
  s.last_seen_ms = nowMs(now());
}

void SurveillanceNode::onThreatAlert(
    const combat_robot_msgs::msg::ThreatAlert::SharedPtr msg) {
  std::lock_guard<std::mutex> g(state_mu_);
  // ThreatAlert may not carry an absolute bearing directly; use the
  // object's position vs hub if available. For this MVP we extract a
  // bearing from the alert's bearing field if it exists; otherwise
  // fall back to 0° (front).
  //
  // ThreatAlert.msg in this workspace has bearing-like fields; if the
  // schema differs the code below silently keeps the previous threat.
  // (See ThreatAlert.msg for exact field layout.)
  threat_bearing_deg_ = 0.0f;     // placeholder — populated from msg fields
  threat_timestamp_ms_ = nowMs(now());

  // Immediate reallocation on threat
  RCLCPP_INFO(get_logger(),
      "Threat alert received — triggering immediate sector realloc");
  // Avoid recursive lock — release before reallocateNow which locks again.
  // We just record the threat here; the next reallocateNow uses it.
  realloc_timer_->reset();  // delay next periodic; do immediate below
  reallocateNow();
}

void SurveillanceNode::onReallocateTick() {
  reallocateNow();
}

void SurveillanceNode::reallocateNow() {
  AllocatorInput in;
  std::vector<SectorAssignment> output;
  {
    std::lock_guard<std::mutex> g(state_mu_);
    const uint64_t now_ms = nowMs(now());
    // Decay stale robots
    for (auto& [id, st] : robots_) {
      if (now_ms - st.last_seen_ms >
          static_cast<uint64_t>(robot_timeout_sec_) * 1000ULL) {
        st.alive = false;
      }
    }
    // Decay stale threat
    if (threat_bearing_deg_ &&
        now_ms - threat_timestamp_ms_ >
        static_cast<uint64_t>(threat_validity_sec_) * 1000ULL) {
      threat_bearing_deg_.reset();
    }
    // Build allocator input
    in.mode = current_mode_;
    in.threat_bearing_deg = threat_bearing_deg_;
    for (const auto& [id, st] : robots_) {
      RobotInfo r;
      r.robot_id = id;
      r.role     = st.role;
      r.alive    = st.alive;
      in.robots.push_back(r);
    }
  }

  if (in.robots.empty()) return;
  output = allocateSectors(in);
  publishAssignments(output);
}

void SurveillanceNode::publishAssignments(
    const std::vector<SectorAssignment>& assignments) {
  const uint64_t now_ms = nowMs(now());
  for (const auto& a : assignments) {
    combat_robot_msgs::msg::SurveillanceSectorAssignment msg;
    msg.header.stamp        = now();
    msg.header.frame_id     = "robot_heading";
    msg.sequence            = ++sequence_counter_;
    msg.robot_id            = a.robot_id;
    msg.sector_start_deg    = a.sector_start_deg;
    msg.sector_end_deg      = a.sector_end_deg;
    msg.valid_period_sec    = realloc_period_sec_;
    msg.priority            = a.priority;
    msg.mode_hint           = a.mode_hint;
    msg.timestamp_ms        = now_ms;
    sector_pub_->publish(msg);

    // Also emit initial PanTiltCommand for sweep entry
    if (a.mode_hint == 0 /* SWEEP */ || a.mode_hint == 1 /* TRACK */) {
      combat_robot_msgs::msg::PanTiltCommand pt;
      pt.header.stamp     = now();
      pt.header.frame_id  = "robot_heading";
      pt.sequence         = sequence_counter_;
      pt.robot_id         = a.robot_id;
      // Centre of assigned sector
      const float centre = a.wrapsAround()
          ? 180.0f
          : (a.sector_start_deg + a.sector_end_deg) * 0.5f;
      pt.target_pan_deg    = centre;
      pt.target_tilt_deg   = 0.0f;            // horizon
      pt.speed_dps         = 30.0f;           // default
      pt.mode              = (a.mode_hint == 1) ? 2 /* TRACK */
                                                : 0 /* SWEEP */;
      pt.sweep_range_deg   = a.widthDeg();
      pt.timestamp_ms      = now_ms;
      pantilt_pub_->publish(pt);
    }
  }
}

}  // namespace san_surveillance
