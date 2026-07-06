// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5.2 DCN-2026-010 D-028 — DetectionToThreatNode impl.

#include "san_hub_orchestrator/detection_to_threat_node.hpp"

#include <chrono>
#include <stdexcept>

namespace san_hub_orchestrator
{

using namespace std::chrono_literals;

DetectionToThreatNode::DetectionToThreatNode(
  const rclcpp::NodeOptions & opts)
: rclcpp::Node("detection_to_threat_node", opts),
  converter_(DetectionConverterConfig{})
{
  declareParameters();
  loadParameters();

  // Rebuild converter with the loaded thresholds.
  DetectionConverterConfig cfg;
  cfg.confidence_threshold =
    static_cast<float>(get_parameter("confidence_threshold").as_double());
  cfg.rgb_confidence_threshold =
    static_cast<float>(get_parameter("rgb_confidence_threshold").as_double());
  cfg.fused_fallback_window_s =
    get_parameter("fused_fallback_window_s").as_double();
  if (cfg.confidence_threshold < 0.0f || cfg.confidence_threshold > 1.0f) {
    throw std::runtime_error(
            "DetectionToThreatNode: confidence_threshold out of [0,1]");
  }
  if (cfg.rgb_confidence_threshold < 0.0f ||
    cfg.rgb_confidence_threshold > 1.0f)
  {
    throw std::runtime_error(
            "DetectionToThreatNode: rgb_confidence_threshold out of [0,1]");
  }
  if (cfg.fused_fallback_window_s <= 0.0 ||
    cfg.fused_fallback_window_s > 60.0)
  {
    throw std::runtime_error(
            "DetectionToThreatNode: fused_fallback_window_s out of (0, 60]");
  }
  converter_ = DetectionToThreatConverter(cfg);

  threat_pub_ = create_publisher<ThreatAlertMsg>(
    output_topic_, rclcpp::QoS(50).reliable());

  sub_fused_ = create_subscription<DetectionArray>(
    fused_topic_, rclcpp::QoS(10).best_effort(),
    std::bind(
      &DetectionToThreatNode::onDetectionFused,
      this, std::placeholders::_1));
  sub_rgb_ = create_subscription<DetectionArray>(
    rgb_topic_, rclcpp::QoS(10).best_effort(),
    std::bind(
      &DetectionToThreatNode::onDetectionRgb,
      this, std::placeholders::_1));

  tf_buffer_ = std::make_shared<tf2_ros::Buffer>(get_clock());
  tf_listener_ = std::make_shared<tf2_ros::TransformListener>(*tf_buffer_);
  sub_joints_ = create_subscription<sensor_msgs::msg::JointState>(
    "joint_states", rclcpp::QoS(10).best_effort(),
    std::bind(&DetectionToThreatNode::onJointState, this, std::placeholders::_1));

  // DCN-2026-026 C-3 — 교전 합의 투표 발행. Voting silently disables
  // when the mesh secret is absent (sim/dev) or this reporter has no
  // numeric robot id (e.g. "perception") — detection→threat publishing
  // is unaffected either way.
  {
    const std::string & src = source_robot_id_;
    std::size_t p = (src.rfind("robot_", 0) == 0) ? 6 : 0;
    uint32_t id = 0;
    bool ok = p < src.size();
    for (std::size_t i = p; ok && i < src.size(); ++i) {
      if (src[i] < '0' || src[i] > '9') {ok = false; break;}
      id = id * 10u + static_cast<uint32_t>(src[i] - '0');
    }
    numeric_robot_id_ = ok ? id : 0;
  }
  vote_pub_ = create_publisher<combat_robot_msgs::msg::TargetConfirmation>(
    get_parameter("vote_topic").as_string(), rclcpp::QoS(20).reliable());
  if (numeric_robot_id_ != 0) {
    try {
      vote_auth_ =
        std::make_unique<san_fire_authorization::TargetConfirmationAuth>(
        get_parameter("hmac_secret_path").as_string());
    } catch (const std::exception & e) {
      RCLCPP_WARN(
        get_logger(),
        "TargetConfirmation voting disabled (mesh secret unavailable: %s)",
        e.what());
    }
  }

  RCLCPP_INFO(
    get_logger(),
    "DetectionToThreatNode UP: fused=%s rgb=%s out=%s "
    "conf=%.2f rgb_conf=%.2f fallback=%.1fs",
    fused_topic_.c_str(), rgb_topic_.c_str(), output_topic_.c_str(),
    cfg.confidence_threshold, cfg.rgb_confidence_threshold,
    cfg.fused_fallback_window_s);
}

void DetectionToThreatNode::declareParameters()
{
  declare_parameter<std::string>(
    "fused_topic", "/perception_node/detections_fused");
  declare_parameter<std::string>(
    "rgb_topic", "/human_detector_node/detections_rgb");
  declare_parameter<std::string>(
    "output_topic", "/swarm/threat_alert_raw");
  declare_parameter<std::string>("source_robot_id", "perception");
  declare_parameter<double>("confidence_threshold", 0.9);
  declare_parameter<double>("rgb_confidence_threshold", 0.8);
  declare_parameter<double>("fused_fallback_window_s", 1.0);

  // "" = auto: derive "<node namespace>/base_footprint", so followers
  // (/robot_N) resolve robot_N/base_footprint without per-robot config.
  declare_parameter<std::string>("base_frame", "");
  declare_parameter<double>("focal_px", 550.0);
  declare_parameter<double>("img_cx", 320.0);
  declare_parameter<double>("img_cy", 240.0);

  // DCN-2026-026 C-3 (비준 2026-06-10)
  declare_parameter<std::string>("vote_topic", "/swarm/target_confirmations");
  declare_parameter<std::string>(
    "hmac_secret_path", "/etc/san/mesh_secret.bin");
  declare_parameter<double>("vote_min_confidence", 0.9);
  declare_parameter<int>("vote_min_interval_ms", 500);
}

void DetectionToThreatNode::loadParameters()
{
  fused_topic_ = get_parameter("fused_topic").as_string();
  rgb_topic_ = get_parameter("rgb_topic").as_string();
  output_topic_ = get_parameter("output_topic").as_string();
  source_robot_id_ = get_parameter("source_robot_id").as_string();

  base_frame_ = get_parameter("base_frame").as_string();
  if (base_frame_.empty()) {
    // get_namespace() is "/" for the root namespace, "/robot_N" otherwise.
    std::string ns = get_namespace();
    if (!ns.empty() && ns.front() == '/') {ns.erase(0, 1);}
    base_frame_ = ns.empty() ? "base_footprint" : ns + "/base_footprint";
  }
  focal_px_ = get_parameter("focal_px").as_double();
  img_cx_ = get_parameter("img_cx").as_double();
  img_cy_ = get_parameter("img_cy").as_double();
  vote_min_confidence_ = get_parameter("vote_min_confidence").as_double();
  vote_min_interval_ms_ = static_cast<uint64_t>(
    get_parameter("vote_min_interval_ms").as_int());
}

uint64_t DetectionToThreatNode::nowMs() const
{
  return static_cast<uint64_t>(now().nanoseconds() / 1'000'000);
}

void DetectionToThreatNode::onJointState(
  const sensor_msgs::msg::JointState::SharedPtr msg)
{
  for (size_t i = 0; i < msg->name.size() && i < msg->position.size(); ++i) {
    if (msg->name[i] == "gimbal_pan_joint") {
      gimbal_pan_.store(msg->position[i]);
    } else if (msg->name[i] == "gimbal_tilt_joint") {
      gimbal_tilt_.store(msg->position[i]);
    }
  }
}

GeoResult DetectionToThreatNode::geoForDetection(
  const combat_robot_msgs::msg::Detection & det)
{
  double yaw = 0.0;
  try {
    auto tf = tf_buffer_->lookupTransform("map", base_frame_, tf2::TimePointZero);
    const auto & q = tf.transform.rotation;
    yaw = std::atan2(
      2.0 * (q.w * q.z + q.x * q.y),
      1.0 - 2.0 * (q.y * q.y + q.z * q.z));
  } catch (const tf2::TransformException &) {
    return GeoResult{};                 // has_position=false
  }
  const double px = 0.5 * (det.bbox_x1 + det.bbox_x2);
  const double py = 0.5 * (det.bbox_y1 + det.bbox_y2);
  const double h = static_cast<double>(det.bbox_y2) - det.bbox_y1;
  return computeGeo(
    px, py, img_cx_, img_cy_, focal_px_,
    yaw, gimbal_pan_.load(), gimbal_tilt_.load(), det.estimated_depth_m, h,
    classToRealHeightM(det.class_id));
}

void DetectionToThreatNode::maybePublishVote(
  const combat_robot_msgs::msg::Detection & det, const GeoResult & geo)
{
  using Detection = combat_robot_msgs::msg::Detection;
  if (!vote_auth_) {return;}
  if (det.class_id != Detection::CLASS_PERSON &&
    det.class_id != Detection::CLASS_DRONE)
  {
    return;
  }
  if (det.confidence < vote_min_confidence_) {return;}
  if (!geo.has_position || !(geo.range_m > 0.0f)) {return;}

  const uint64_t now_ms = nowMs();
  auto & last = last_vote_ms_by_track_[det.track_id];
  if (last != 0 && now_ms - last < vote_min_interval_ms_) {return;}
  last = now_ms;

  san_fire_authorization::TargetConfirmMessage m;
  m.robot_id = numeric_robot_id_;
  m.track_id = det.track_id;
  m.bearing_deg = geo.bearing_deg;
  m.elevation_deg = geo.elevation_deg;
  m.range_m = geo.range_m;
  // Robot-unique, never-reused nonce: robot id in the top bits + a
  // monotonic sequence (the verifier's sliding window rejects reuse).
  m.nonce = (static_cast<uint64_t>(numeric_robot_id_) << 48) |
    (++vote_nonce_seq_ & 0xFFFFFFFFFFFFULL);
  m.timestamp_ms = now_ms;

  combat_robot_msgs::msg::TargetConfirmation msg;
  msg.header.stamp = now();
  msg.header.frame_id = "world";
  msg.robot_id = m.robot_id;
  msg.track_id = m.track_id;
  msg.bearing_deg = m.bearing_deg;
  msg.elevation_deg = m.elevation_deg;
  msg.range_m = m.range_m;
  msg.nonce = m.nonce;
  msg.timestamp_ms = m.timestamp_ms;
  msg.hmac_hex = vote_auth_->sign(m);
  vote_pub_->publish(msg);
}

void DetectionToThreatNode::onDetectionFused(
  const DetectionArray::SharedPtr msg)
{
  fused_count_.fetch_add(1, std::memory_order_relaxed);
  // Anchor the fused-stream timestamp on this node's clock so the
  // RGB fallback decision is independent of the publisher's clock
  // (same rationale as threat_aggregator's onIncomingThreat).
  converter_.markFusedReceived(nowMs());

  for (const auto & det : msg->detections) {
    auto out = converter_.convert(
      det.class_id, det.confidence,
      det.bbox_x1, det.bbox_y1, det.bbox_x2, det.bbox_y2,
      DetectionSource::Fused);
    if (out) {
      const GeoResult geo = geoForDetection(det);
      publishThreat(*out, DetectionSource::Fused, geo);
      maybePublishVote(det, geo);
    }
  }
}

void DetectionToThreatNode::onDetectionRgb(
  const DetectionArray::SharedPtr msg)
{
  rgb_count_.fetch_add(1, std::memory_order_relaxed);
  if (!converter_.shouldUseRgb(nowMs())) {
    // Fused is authoritative — suppress RGB to avoid double-publish.
    return;
  }
  for (const auto & det : msg->detections) {
    auto out = converter_.convert(
      det.class_id, det.confidence,
      det.bbox_x1, det.bbox_y1, det.bbox_x2, det.bbox_y2,
      DetectionSource::RgbFallback);
    if (out) {
      const GeoResult geo = geoForDetection(det);
      publishThreat(*out, DetectionSource::RgbFallback, geo);
      maybePublishVote(det, geo);
    }
  }
}

void DetectionToThreatNode::publishThreat(
  const ConvertedThreat & t, DetectionSource /*src*/, const GeoResult & geo)
{
  ThreatAlertMsg msg;
  msg.header.stamp = now();
  msg.header.frame_id = "detection_to_threat";
  msg.severity = t.severity;
  msg.threat_type = t.threat_type;
  msg.source_robot_id = source_robot_id_;
  msg.peer_id = "";
  msg.message_ko = t.message_ko;
  msg.detail = t.detail;
  msg.timestamp_ms = nowMs();
  msg.instance_count = 1;

  msg.has_position = geo.has_position;
  msg.bearing_deg = geo.bearing_deg;
  msg.elevation_deg = geo.elevation_deg;
  msg.range_m = geo.range_m;

  threat_pub_->publish(msg);
  publish_count_.fetch_add(1, std::memory_order_relaxed);

  RCLCPP_INFO(
    get_logger(),
    "Threat published: severity=%u type=%u conf=%.2f msg=\"%s\"",
    static_cast<unsigned>(t.severity),
    static_cast<unsigned>(t.threat_type),
    static_cast<double>(t.confidence),
    t.message_ko.c_str());
}

}  // namespace san_hub_orchestrator
