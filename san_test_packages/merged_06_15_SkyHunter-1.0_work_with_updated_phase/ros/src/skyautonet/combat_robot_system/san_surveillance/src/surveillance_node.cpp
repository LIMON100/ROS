// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 — SurveillanceNode implementation (patched 2026-05-13).

#include "san_surveillance/surveillance_node.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>

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
    "/swarm/threat_alert",
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

  // DCN-2026-026 C-3 — 교전 합의 투표 수집 + 사격 제원 발행. The mesh
  // secret may legitimately be absent (sim/dev hosts): voting is then
  // disabled and FireSolution publishes with vote_count 0 / not ready.
  vote_sub_ = create_subscription<combat_robot_msgs::msg::TargetConfirmation>(
    "/swarm/target_confirmations",
    rclcpp::QoS(20).reliable(),
    std::bind(
      &SurveillanceNode::onTargetConfirmation, this,
      std::placeholders::_1));
  fire_solution_pub_ = create_publisher<combat_robot_msgs::msg::FireSolution>(
    "/swarm/fire_solution",
    rclcpp::QoS(10).reliable());
  try {
    vote_auth_ =
      std::make_unique<san_fire_authorization::TargetConfirmationAuth>(
      get_parameter("hmac_secret_path").as_string());
  } catch (const std::exception & e) {
    RCLCPP_WARN(
      get_logger(),
      "TargetConfirmation auth disabled (mesh secret unavailable: %s) — "
      "votes will be ignored, engage_ready stays false", e.what());
  }

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
  declare_parameter<double>("enter_drive_mps", 0.3);
  declare_parameter<double>("exit_drive_mps", 0.1);
  declare_parameter<double>("enter_drive_dps", 5.0);
  declare_parameter<double>("exit_drive_dps", 2.0);
  // DCN-2026-026 C-3 (비준 2026-06-10: k=2 확정)
  declare_parameter<std::string>("hmac_secret_path", "/etc/san/mesh_secret.bin");
  declare_parameter<int>("min_votes", 2);
  declare_parameter<int>("vote_window_ms", 1500);
  declare_parameter<double>("fire_lead_time_s", 0.3);
}

void SurveillanceNode::loadParameters()
{
  realloc_period_sec_ = get_parameter("realloc_period_sec").as_int();
  robot_timeout_sec_ = get_parameter("robot_timeout_sec").as_int();
  threat_validity_sec_ = get_parameter("threat_validity_sec").as_int();
  leader_robot_id_ = static_cast<uint32_t>(
    get_parameter("leader_robot_id").as_int());

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

  min_votes_ = static_cast<int>(get_parameter("min_votes").as_int());
  votes_ = VoteTally(
    static_cast<uint32_t>(get_parameter("vote_window_ms").as_int()));
  fire_lead_time_s_ =
    static_cast<float>(get_parameter("fire_lead_time_s").as_double());
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
  const std::optional<float> elevation = msg->has_position ?
    std::optional<float>(msg->elevation_deg) : std::nullopt;
  // PATCH 2026-05-13: scope the lock tightly — DO NOT hold during the
  // reallocateNow() call below. Previously held the lock and called
  // reallocateNow() which re-locked → deadlock with std::mutex.
  std::size_t n_clusters = 0;
  {
    std::lock_guard<std::recursive_mutex> g(state_mu_);
    const uint64_t now_ms = nowMs(now());
    // DCN-2026-026 C-1: merge into the nearest cluster within 15°,
    // else open a new one (evicting the stalest when at capacity).
    ThreatCluster * merge_into = nullptr;
    float best_d = kThreatMergeDeg;
    for (auto & t : threats_) {
      const float d = angularDifference(t.bearing_world_deg, bearing_world);
      if (d <= best_d) {merge_into = &t; best_d = d;}
    }
    if (merge_into == nullptr) {
      if (threats_.size() < kMaxThreatClusters) {
        threats_.push_back(ThreatCluster{});
        merge_into = &threats_.back();
      } else {
        merge_into = &threats_.front();
        for (auto & t : threats_) {
          if (t.last_ms < merge_into->last_ms) {merge_into = &t;}
        }
      }
    }
    // DCN-2026-026 C-3: keep the previous fix per cluster for the
    // angular-rate estimate (scalar prev state cross-contaminated two
    // threats in the defbb64 original).
    if (merge_into->last_ms != 0) {
      merge_into->prev_bearing_deg = merge_into->bearing_world_deg;
      merge_into->prev_ms = merge_into->last_ms;
    }
    merge_into->bearing_world_deg = bearing_world;   // newest fix wins
    merge_into->elevation_deg = elevation;
    merge_into->range_m = msg->has_position ? msg->range_m : 0.0f;
    merge_into->last_ms = now_ms;
    n_clusters = threats_.size();
  }
  RCLCPP_INFO(
    get_logger(),
    "Threat alert: bearing %.1f° (world), %zu active cluster(s) — "
    "immediate sector realloc",
    bearing_world, n_clusters);
  // recursive_mutex makes this safe even if lock is already held by
  // the caller's chain, but we still release before calling for
  // clarity + to avoid surprise contention.
  reallocateNow();
}

void SurveillanceNode::onTargetConfirmation(
  const combat_robot_msgs::msg::TargetConfirmation::SharedPtr msg)
{
  if (!vote_auth_) {return;}            // secret not provisioned
  san_fire_authorization::TargetConfirmMessage m;
  m.robot_id = msg->robot_id;
  m.track_id = msg->track_id;
  m.bearing_deg = msg->bearing_deg;
  m.elevation_deg = msg->elevation_deg;
  m.range_m = msg->range_m;
  m.nonce = msg->nonce;
  m.timestamp_ms = msg->timestamp_ms;
  const uint64_t now_ms = nowMs(now());
  const auto result = vote_auth_->verify(m, msg->hmac_hex, now_ms);
  if (result != san_fire_authorization::AuthResult::Granted) {
    RCLCPP_WARN_THROTTLE(
      get_logger(), *get_clock(), 5000,
      "TargetConfirmation from robot %u REJECTED (auth result %u)",
      msg->robot_id, static_cast<unsigned>(result));
    return;
  }
  std::lock_guard<std::recursive_mutex> g(state_mu_);
  votes_.record(msg->robot_id, msg->track_id, msg->bearing_deg, now_ms);
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
    // Decay stale threat clusters (DCN-2026-026 C-1 — each decays
    // independently after threat_validity_sec_).
    threats_.erase(
      std::remove_if(
        threats_.begin(), threats_.end(),
        [&](const ThreatCluster & t) {
          return now_ms - t.last_ms >
          static_cast<uint64_t>(threat_validity_sec_) * 1000ULL;
        }),
      threats_.end());

    AllocatorInput in;
    in.mode = current_mode_;
    in.output_frame = current_frame_;
    snap.frame = current_frame_;

    // Anchor world-frame sectors to the leader's current yaw.
    auto leader_it = robots_.find(leader_robot_id_);
    if (leader_it != robots_.end()) {
      in.leader_yaw_world_deg = leader_it->second.yaw_world_deg;
    }

    // Threat bearings — if World frame, pass through; if Heading
    // frame, transform from world to leader-relative heading.
    // DCN-2026-026 C-1: every active cluster feeds the allocator.
    for (const auto & t : threats_) {
      const float bearing_native =
        (current_frame_ == SectorFrame::World) ?
        t.bearing_world_deg :
        worldToHeading(t.bearing_world_deg, in.leader_yaw_world_deg);
      in.threat_bearings_deg.push_back(bearing_native);
      ThreatRef ref;
      ref.bearing_deg = bearing_native;
      ref.elevation_deg = t.elevation_deg;
      ref.bearing_world_deg = t.bearing_world_deg;
      ref.range_m = t.range_m;
      // DCN-2026-026 C-3 — per-cluster angular rate (deg/s) from the
      // last two fixes; 0 until two fixes exist.
      if (t.prev_ms != 0 && t.last_ms > t.prev_ms) {
        const float dt_s =
          static_cast<float>(t.last_ms - t.prev_ms) / 1000.0f;
        const float db = normalizeAngle(
          t.bearing_world_deg - t.prev_bearing_deg);
        ref.rate_dps = db / dt_s;
      }
      ref.vote_count = votes_.countFor(t.bearing_world_deg, now_ms);
      snap.threats.push_back(ref);
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
      // Tilt: Track uses the elevation of the threat cluster nearest
      // this sector's centre (DCN-2026-026 C-1 — up to 2 concurrent
      // clusters may carry different elevations); Sweep stays at the
      // horizon.
      pt.target_tilt_deg = 0.0f;              // horizon default
      if (a.mode_hint == MODE_HINT_TRACK && !snap.threats.empty()) {
        const ThreatRef * nearest = nullptr;
        float best_d = 360.0f;
        for (const auto & t : snap.threats) {
          const float d = angularDifference(t.bearing_deg, centre_native);
          if (d < best_d) {nearest = &t; best_d = d;}
        }
        if (nearest != nullptr && nearest->elevation_deg) {
          pt.target_tilt_deg = *nearest->elevation_deg;
        }
      }
      pt.speed_dps = 30.0f;                   // default
      pt.mode = (a.mode_hint == MODE_HINT_TRACK) ?
        PT_MODE_TRACK : PT_MODE_SWEEP;
      pt.sweep_range_deg = a.widthDeg();
      pt.timestamp_ms = snap.now_ms;
      pantilt_pub_->publish(pt);
    }
  }

  // ─── DCN-2026-026 C-3 — FireSolution per active threat cluster ──
  // engage_ready 는 advisory: 사격 개시 권한은 Two-key + HMAC 의
  // fire-authorization 체인 단독 (FireSolution.msg 권원 헤더 참조).
  for (const auto & t : snap.threats) {
    combat_robot_msgs::msg::FireSolution fs;
    fs.header.stamp = now();
    fs.header.frame_id = "world";
    // Shooter = the TRACK-mode (threat-focus) robot whose sector
    // centre sits closest to this cluster.
    uint32_t shooter = 0;
    float best_d = 360.0f;
    for (const auto & a : snap.assignments) {
      if (a.mode_hint != MODE_HINT_TRACK) {continue;}
      const float d = angularDifference(a.centreDeg(), t.bearing_deg);
      if (d < best_d) {shooter = a.robot_id; best_d = d;}
    }
    fs.shooter_robot_id = shooter;
    fs.solution_valid = (shooter != 0);
    fs.lead_time_s = fire_lead_time_s_;
    fs.target_rate_dps = t.rate_dps;
    fs.aim_bearing_deg = normalizeAngle(
      t.bearing_world_deg + t.rate_dps * fire_lead_time_s_);
    fs.aim_elevation_deg = t.elevation_deg.value_or(0.0f);
    fs.range_m = t.range_m;
    fs.vote_count = t.vote_count;
    fs.min_votes = static_cast<uint8_t>(min_votes_);
    fs.engage_ready = fs.solution_valid &&
      t.vote_count >= static_cast<uint8_t>(min_votes_);
    fs.timestamp_ms = snap.now_ms;
    fire_solution_pub_->publish(fs);
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
