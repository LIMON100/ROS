// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 — SurveillanceNode implementation (patched 2026-05-13).

#include "san_surveillance/surveillance_node.hpp"

#include <chrono>
#include <cmath>
#include <algorithm>

namespace san_surveillance
{

using namespace std::chrono_literals;

namespace
{

constexpr uint32_t LEADER_ROBOT_ID = 1;
constexpr uint32_t HUB_ROBOT_ID = 2;

// mode_hint enum (matches sector_allocator + msg field)
constexpr uint8_t MODE_HINT_SWEEP = 0;
constexpr uint8_t MODE_HINT_TRACK = 1;
constexpr uint8_t MODE_HINT_FIXED = 2;

// PanTiltCommand.mode enum (matches PanTiltCommand.msg)
constexpr uint8_t PT_MODE_SWEEP = 0;
constexpr uint8_t PT_MODE_FIXED = 1;
constexpr uint8_t PT_MODE_TRACK = 2;
constexpr uint8_t PT_MODE_ENGAGE = 3;

/// Heuristic role inference from robot_id.
RobotRole inferRole(uint32_t robot_id)
{
  if (robot_id == LEADER_ROBOT_ID) {return RobotRole::Leader;}
  if (robot_id == HUB_ROBOT_ID) {return RobotRole::Hub;}
  return RobotRole::Follower;
}

uint64_t nowMs(const rclcpp::Time & t)
{
  return static_cast<uint64_t>(t.nanoseconds() / 1'000'000ULL);
}

/// Extract yaw (rad) from a quaternion (z-axis only).
float quatToYawRad(double qx, double qy, double qz, double qw)
{
  const double siny_cosp = 2.0 * (qw * qz + qx * qy);
  const double cosy_cosp = 1.0 - 2.0 * (qy * qy + qz * qz);
  return static_cast<float>(std::atan2(siny_cosp, cosy_cosp));
}

}  // namespace

SurveillanceNode::SurveillanceNode(const rclcpp::NodeOptions & opts)
: rclcpp::Node("surveillance_node", opts)
{
  declareParameters();
  loadParameters();

  robot_status_sub_ = create_subscription<combat_robot_msgs::msg::RobotStatus>(
    "/swarm/robot_status",
    rclcpp::QoS(20).best_effort(),
    std::bind(
      &SurveillanceNode::onRobotStatus, this,
      std::placeholders::_1));

  threat_sub_ = create_subscription<combat_robot_msgs::msg::ThreatAlert>(
    "/swarm/threat_alert_raw",
    rclcpp::QoS(10).reliable(),
    std::bind(
      &SurveillanceNode::onThreatAlert, this,
      std::placeholders::_1));

  sector_pub_ = create_publisher<combat_robot_msgs::msg::SurveillanceSectorAssignment>(
    "/swarm/surveillance/sector_assign",
    rclcpp::QoS(20).reliable().transient_local());

  pantilt_pub_ = create_publisher<combat_robot_msgs::msg::PanTiltCommand>(
    "/swarm/cmd/pantilt",
    rclcpp::QoS(20).reliable());
  
  fire_pub_ = create_publisher<combat_robot_msgs::msg::FireSolution>(
      "/swarm/fire_solution", rclcpp::QoS(10).reliable());

  vote_sub_ = create_subscription<std_msgs::msg::UInt32>(
    "/swarm/target_confirmations", rclcpp::QoS(10).reliable(),
    std::bind(&SurveillanceNode::onTargetVote, this, std::placeholders::_1));

  realloc_timer_ = create_wall_timer(
    std::chrono::seconds(realloc_period_sec_),
    std::bind(&SurveillanceNode::onReallocateTick, this));

  RCLCPP_INFO(
    get_logger(),
    "SurveillanceNode UP: realloc_period=%ds robot_timeout=%ds "
    "threat_validity=%ds leader_id=%u default_mode=Recon",
    realloc_period_sec_, robot_timeout_sec_, threat_validity_sec_,
    leader_robot_id_);
}

void SurveillanceNode::declareParameters()
{
  declare_parameter<int>("realloc_period_sec", 10);
  declare_parameter<int>("robot_timeout_sec", 3);
  declare_parameter<int>("threat_validity_sec", 10);
  declare_parameter<int>("leader_robot_id", 1);
  declare_parameter<double>("fire_range_m", 10.0);
  declare_parameter<double>("lead_time_s", 0.5);
  declare_parameter<int>("min_votes", 1);
  declare_parameter<double>("vote_window_s", 1.5);
  declare_parameter<double>("enter_drive_mps", 0.3);
  declare_parameter<double>("exit_drive_mps", 0.1);
  declare_parameter<double>("enter_drive_dps", 5.0);
  declare_parameter<double>("exit_drive_dps", 2.0);
}

void SurveillanceNode::loadParameters()
{
  realloc_period_sec_ = get_parameter("realloc_period_sec").as_int();
  robot_timeout_sec_ = get_parameter("robot_timeout_sec").as_int();
  threat_validity_sec_ = get_parameter("threat_validity_sec").as_int();
  leader_robot_id_ = static_cast<uint32_t>(
    get_parameter("leader_robot_id").as_int());

  fire_range_m_ = get_parameter("fire_range_m").as_double();
  lead_time_s_ = get_parameter("lead_time_s").as_double();

  min_votes_ = get_parameter("min_votes").as_int();
  vote_window_s_ = get_parameter("vote_window_s").as_double();

  DriveClassifier::Config dc;
  dc.enter_drive_mps = static_cast<float>(
    get_parameter("enter_drive_mps").as_double());
  dc.exit_drive_mps = static_cast<float>(
    get_parameter("exit_drive_mps").as_double());
  dc.enter_drive_dps = static_cast<float>(
    get_parameter("enter_drive_dps").as_double());
  dc.exit_drive_dps = static_cast<float>(
    get_parameter("exit_drive_dps").as_double());
  drive_classifier_ = DriveClassifier(dc);
}

// ─── Subscriptions ──────────────────────────────────────────────────────

void SurveillanceNode::onRobotStatus(
  const combat_robot_msgs::msg::RobotStatus::SharedPtr msg)
{
  std::lock_guard<std::recursive_mutex> g(state_mu_);
  auto & s = robots_[msg->robot_id];
  s.role = inferRole(msg->robot_id);
  s.alive = true;
  s.last_seen_ms = nowMs(now());

  // PATCH 2026-05-13: extract world yaw + speed.
  s.yaw_world_deg = quatToYawRad(
    msg->pose.orientation.x, msg->pose.orientation.y,
    msg->pose.orientation.z, msg->pose.orientation.w) * 180.0f / static_cast<float>(M_PI);

  // RobotStatus exposes body-frame velocity via the `twist` field
  // (Twist.linear.{x,y} + angular.z), per combat_robot_msgs/RobotStatus.msg.
  const float vx = static_cast<float>(msg->twist.linear.x);
  const float vy = static_cast<float>(msg->twist.linear.y);
  s.linear_speed_mps = std::sqrt(vx * vx + vy * vy);
  s.angular_speed_dps = std::fabs(
    static_cast<float>(msg->twist.angular.z)) * 180.0f /
    static_cast<float>(M_PI);

  // Drive classifier follows the LEADER's motion (front-anchor).
  if (msg->robot_id == leader_robot_id_) {
    MotionSnapshot m;
    m.linear_speed_mps = s.linear_speed_mps;
    m.angular_speed_dps = s.angular_speed_dps;
    drive_classifier_.update(m);
    current_frame_ = drive_classifier_.recommendedFrame();
  }
}

void SurveillanceNode::onThreatAlert(
  const combat_robot_msgs::msg::ThreatAlert::SharedPtr msg)
{
  // ThreatAlert now exposes the threat localization directly via
  // bearing_deg / elevation_deg (combat_robot_msgs/ThreatAlert.msg).
  // Reporters without a position (e.g. SBC_FAILED) leave has_position=
  // false; treat those as "front" so the alert still steers the sweep
  // forward instead of being silently dropped.
  const float bearing_world =
    msg->has_position ? msg->bearing_deg : 0.0f;
  // PATCH 2026-05-13: scope the lock tightly — DO NOT hold during the
  // reallocateNow() call below. Previously held the lock and called
  // reallocateNow() which re-locked → deadlock with std::mutex.
  {
    std::lock_guard<std::recursive_mutex> g(state_mu_);
    // threat_bearing_world_deg_ = bearing_world;
    // threat_elevation_deg_ = msg->has_position ?
    //   std::optional<float>(msg->elevation_deg) : std::nullopt;
    // threat_timestamp_ms_ = nowMs(now());
    const uint64_t ts = nowMs(now());
    constexpr float kMergeDeg = 15.0f;
    constexpr size_t kMaxThreats = 2;
    int found = -1;
    for (size_t i = 0; i < threats_.size(); ++i) {
      if (std::fabs(angularDifference(threats_[i].bearing_world_deg, bearing_world)) < kMergeDeg) {
        found = static_cast<int>(i); break;
      } 
    } 
    if (found >= 0) {
      threats_[found].bearing_world_deg = bearing_world;
      threats_[found].ts_ms = ts;
    } else if (threats_.size() < kMaxThreats) {
      threats_.push_back({bearing_world, ts});
    } else {
      size_t st = 0;
      for (size_t i = 1; i < threats_.size(); ++i) {
        if (threats_[i].ts_ms < threats_[st].ts_ms) {st = i;}
      } 
      threats_[st] = {bearing_world, ts};
    } 
    // threat_elevation_deg_ = msg->has_position ?
    //   std::optional<float>(msg->elevation_deg) : std::nullopt;
    threat_elevation_deg_ = msg->has_position ?
      std::optional<float>(msg->elevation_deg) : std::nullopt;

    // task6: lead-point fire solution for an in-range target
    fire_sol_valid_ = false; 
    if (msg->has_position && msg->range_m > 0.0f &&
      msg->range_m < static_cast<float>(fire_range_m_))
    { 
      const uint64_t fts = nowMs(now());
      if (fire_prev_ts_ > 0 && fts > fire_prev_ts_) {
        const float dt = static_cast<float>(fts - fire_prev_ts_) / 1000.0f;
        float dB = bearing_world - fire_prev_bearing_;
        while (dB > 180.0f) {dB -= 360.0f;}
        while (dB < -180.0f) {dB += 360.0f;}
        fire_rate_dps_ = (dt > 0.0f) ? dB / dt : 0.0f;
      } else {
        fire_rate_dps_ = 0.0f;
      }
      fire_prev_bearing_ = bearing_world;
      fire_prev_ts_ = fts;
      fire_lead_bearing_ = bearing_world + fire_rate_dps_ * static_cast<float>(lead_time_s_);
      fire_range_val_ = msg->range_m;
      fire_elev_val_ = msg->elevation_deg;
        fire_sol_valid_ = true;
      } else {
        fire_prev_ts_ = 0;
      } 
    } 
    { 
      combat_robot_msgs::msg::FireSolution fs;
      fs.header.stamp = now();
      fs.shooter_robot_id = leader_robot_id_;
      fs.solution_valid = fire_sol_valid_;
      fs.aim_bearing_deg = fire_lead_bearing_;
      fs.aim_elevation_deg = fire_elev_val_;
      fs.range_m = fire_range_val_;
      fs.lead_time_s = static_cast<float>(lead_time_s_);
      fs.target_rate_dps = fire_rate_dps_;
      const uint64_t vnow = nowMs(now());
      const uint64_t vwin = static_cast<uint64_t>(vote_window_s_ * 1000.0);
      uint8_t vc = 0;
      for (const auto & [rid, vts] : votes_) {
        if (vnow - vts <= vwin) {++vc;}
      }
      fs.vote_count = vc;
      fs.engage_ready = fire_sol_valid_ && (static_cast<int>(vc) >= min_votes_);
      fs.timestamp_ms = nowMs(now());
      fire_pub_->publish(fs);
    }
  RCLCPP_INFO(
    get_logger(),
    "Threat alert: bearing %.1f° (world) — immediate sector realloc",
    bearing_world);
  // recursive_mutex makes this safe even if lock is already held by
  // the caller's chain, but we still release before calling for
  // clarity + to avoid surprise contention.
  reallocateNow();
}


void SurveillanceNode::onTargetVote(const std_msgs::msg::UInt32::SharedPtr msg)
{ 
  std::lock_guard<std::recursive_mutex> g(state_mu_);
  votes_[msg->data] = nowMs(now());
}

void SurveillanceNode::onReallocateTick()
{
  reallocateNow();
}

void SurveillanceNode::reallocateNow()
{
  PublishSnapshot snap;
  {
    std::lock_guard<std::recursive_mutex> g(state_mu_);
    const uint64_t now_ms = nowMs(now());
    snap.now_ms = now_ms;

    // Decay stale robots
    for (auto & [id, st] : robots_) {
      if (now_ms - st.last_seen_ms >
        static_cast<uint64_t>(robot_timeout_sec_) * 1000ULL)
      {
        st.alive = false;
      }
    }
    // Decay stale threat (bearing + elevation together)
    // if (threat_bearing_world_deg_ &&
    //   now_ms - threat_timestamp_ms_ >
    //   static_cast<uint64_t>(threat_validity_sec_) * 1000ULL)
    // {
    //   threat_bearing_world_deg_.reset();
    //   threat_elevation_deg_.reset();
    // }

    const uint64_t ttl = static_cast<uint64_t>(threat_validity_sec_) * 1000ULL;
    threats_.erase(
      std::remove_if(threats_.begin(), threats_.end(),
        [&](const ActiveThreat & t) {return now_ms - t.ts_ms > ttl;}),
      threats_.end());
    if (threats_.empty()) {threat_elevation_deg_.reset();}

    AllocatorInput in;
    in.mode = current_mode_;
    in.output_frame = current_frame_;
    snap.frame = current_frame_;

    // Anchor world-frame sectors to the leader's current yaw.
    auto leader_it = robots_.find(leader_robot_id_);
    if (leader_it != robots_.end()) {
      in.leader_yaw_world_deg = leader_it->second.yaw_world_deg;
    }

    // Threat bearing — if World frame, pass through; if Heading frame,
    // transform from world to leader-relative heading.
    // if (threat_bearing_world_deg_) {
    //   if (current_frame_ == SectorFrame::World) {
    //     in.threat_bearing_deg = *threat_bearing_world_deg_;
    //   } else {
    //     in.threat_bearing_deg = worldToHeading(
    //       *threat_bearing_world_deg_, in.leader_yaw_world_deg);
    //   }
    // }

    for (const auto & t : threats_) {
      const float b = (current_frame_ == SectorFrame::World)
        ? t.bearing_world_deg
        : worldToHeading(t.bearing_world_deg, in.leader_yaw_world_deg);
      in.threat_bearings_deg.push_back(b);
    }

    for (const auto & [id, st] : robots_) {
      RobotInfo r;
      r.robot_id = id;
      r.role = st.role;
      r.alive = st.alive;
      r.yaw_world_deg = st.yaw_world_deg;
      in.robots.push_back(r);
      // Capture yaw for per-robot World→Heading transform on publish.
      snap.yaw_by_id[id] = st.yaw_world_deg;
    }

    if (in.robots.empty()) {return;}
    snap.assignments = allocateSectors(in);
    snap.threat_elevation_deg = threat_elevation_deg_;
  }
  // ─── Lock released ──────────────────────────────────────
  publishAssignments(snap);
}

// ─── Publishing — lock NOT held ────────────────────────────────────────

void SurveillanceNode::publishAssignments(const PublishSnapshot & snap)
{
  for (const auto & a : snap.assignments) {
    combat_robot_msgs::msg::SurveillanceSectorAssignment msg;
    msg.header.stamp = now();
    // PATCH 2026-05-13: frame_id reflects the actual sector frame so
    // downstream consumers (operator UI, per-robot pan-tilt driver)
    // know whether to interpret as body or world.
    msg.header.frame_id = (a.frame == SectorFrame::World) ?
      "world" : "robot_heading";
    msg.sequence = ++sequence_counter_;
    msg.robot_id = a.robot_id;
    msg.sector_start_deg = a.sector_start_deg;
    msg.sector_end_deg = a.sector_end_deg;
    msg.valid_period_sec = realloc_period_sec_;
    msg.priority = a.priority;
    msg.mode_hint = a.mode_hint;
    msg.timestamp_ms = snap.now_ms;
    sector_pub_->publish(msg);

    // Emit PanTiltCommand for sectors that should be actively scanned.
    if (a.mode_hint == MODE_HINT_SWEEP || a.mode_hint == MODE_HINT_TRACK) {
      combat_robot_msgs::msg::PanTiltCommand pt;
      pt.header.stamp = now();
      // PanTilt hardware is body-mounted → ALWAYS heading-frame.
      pt.header.frame_id = "robot_heading";
      pt.sequence = sequence_counter_;
      pt.robot_id = a.robot_id;

      // Sector centre (in sector's native frame).
      float centre_native = a.centreDeg();

      // PATCH 2026-05-13: if the sector is World-frame, convert to
      // this robot's heading-frame using its yaw at allocation time.
      float centre_heading = centre_native;
      if (a.frame == SectorFrame::World) {
        const auto it = snap.yaw_by_id.find(a.robot_id);
        const float robot_yaw =
          (it != snap.yaw_by_id.end()) ? it->second : 0.0f;
        centre_heading = worldToHeading(centre_native, robot_yaw);
      }

      pt.target_pan_deg = centre_heading;
      // Tilt: Track uses threat elevation when available; Sweep stays
      // at the horizon (the elevated-drone case is handled by the
      // dedicated Track sector once an alert lands).
      if (a.mode_hint == MODE_HINT_TRACK && snap.threat_elevation_deg) {
        pt.target_tilt_deg = *snap.threat_elevation_deg;
      } else {
        pt.target_tilt_deg = 0.0f;            // horizon default
      }
      pt.speed_dps = 30.0f;                   // default
      pt.mode = (a.mode_hint == MODE_HINT_TRACK) ?
        PT_MODE_TRACK : PT_MODE_SWEEP;
      pt.sweep_range_deg = a.widthDeg();
      pt.timestamp_ms = snap.now_ms;
      pantilt_pub_->publish(pt);
    }
  }
}

// ─── Test accessors ─────────────────────────────────────────────────────

SectorFrame SurveillanceNode::currentFrameForTest() const
{
  std::lock_guard<std::recursive_mutex> g(state_mu_);
  return current_frame_;
}

std::size_t SurveillanceNode::robotCountForTest() const
{
  std::lock_guard<std::recursive_mutex> g(state_mu_);
  return robots_.size();
}

DriveClassifier::State SurveillanceNode::driveStateForTest() const
{
  std::lock_guard<std::recursive_mutex> g(state_mu_);
  return drive_classifier_.state();
}

}  // namespace san_surveillance
