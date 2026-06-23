// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5.2 DCN-2026-010 D-028 — DetectionToThreatNode impl.

#include "san_hub_orchestrator/detection_to_threat_node.hpp"

#include <chrono>
#include <stdexcept>
#include <tf2/utils.h>
#include "san_fire_authorization/target_confirmation_auth.hpp"

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
  
  vote_pub_ = create_publisher<combat_robot_msgs::msg::TargetConfirmation>(
      "/swarm/target_confirmations", rclcpp::QoS(10).reliable());
  vote_auth_ = std::make_unique<san_fire_authorization::TargetConfirmationAuth>(
    mesh_secret_path_);

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

  declare_parameter<std::string>("base_frame", "base_footprint");
  declare_parameter<double>("focal_px", 550.0);
  declare_parameter<double>("img_cx", 320.0);
  declare_parameter<double>("img_cy", 240.0);
  declare_parameter<std::string>("mesh_secret_path", "/tmp/mesh_secret.bin");
}

void DetectionToThreatNode::loadParameters()
{
  fused_topic_ = get_parameter("fused_topic").as_string();
  rgb_topic_ = get_parameter("rgb_topic").as_string();
  output_topic_ = get_parameter("output_topic").as_string();
  source_robot_id_ = get_parameter("source_robot_id").as_string();

  base_frame_ = get_parameter("base_frame").as_string();
  focal_px_ = get_parameter("focal_px").as_double();
  img_cx_ = get_parameter("img_cx").as_double();
  img_cy_ = get_parameter("img_cy").as_double();

  mesh_secret_path_ = get_parameter("mesh_secret_path").as_string();
  try { vote_robot_id_ = static_cast<uint32_t>(std::stoul(source_robot_id_)); }
  catch (...) { vote_robot_id_ = 0; }
}

uint64_t DetectionToThreatNode::nowMs() const
{
  return static_cast<uint64_t>(now().nanoseconds() / 1'000'000);
}

void DetectionToThreatNode::onJointState(
    const sensor_msgs::msg::JointState::SharedPtr msg)
{ 
  for (size_t i = 0; i < msg->name.size() && i < msg->position.size(); ++i) {
    if (msg->name[i] == "gimbal_pan_joint")  {gimbal_pan_.store(msg->position[i]);}
    else if (msg->name[i] == "gimbal_tilt_joint") {gimbal_tilt_.store(msg->position[i]);}
  } 
}

GeoResult DetectionToThreatNode::geoForDetection(
    const combat_robot_msgs::msg::Detection & det)
{ 
  double yaw = 0.0;
  try {
    auto tf = tf_buffer_->lookupTransform("map", base_frame_, tf2::TimePointZero);
    const auto & q = tf.transform.rotation;
    yaw = std::atan2(2.0 * (q.w * q.z + q.x * q.y),
                      1.0 - 2.0 * (q.y * q.y + q.z * q.z));
  } catch (const tf2::TransformException &) {
    return GeoResult{};                 // has_position=false
  }
  const double px = 0.5 * (det.bbox_x1 + det.bbox_x2); 
  const double py = 0.5 * (det.bbox_y1 + det.bbox_y2); 
  const double h  = static_cast<double>(det.bbox_y2) - det.bbox_y1;
  return computeGeo(px, py, img_cx_, img_cy_, focal_px_,
    yaw, gimbal_pan_.load(), gimbal_tilt_.load(), det.estimated_depth_m, h);
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
    // if (out) {publishThreat(*out, DetectionSource::Fused);}
    if (out) {publishThreat(*out, DetectionSource::Fused, geoForDetection(det));}
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
    if (out) {publishThreat(*out, DetectionSource::RgbFallback, geoForDetection(det));}
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

  msg.has_position  = geo.has_position;
  msg.bearing_deg   = geo.bearing_deg;
  msg.elevation_deg = geo.elevation_deg;
  msg.range_m       = geo.range_m;

  threat_pub_->publish(msg);
  publish_count_.fetch_add(1, std::memory_order_relaxed);

  if (vote_auth_ && geo.has_position) {
    san_fire_authorization::TargetConfirmMessage vm;
    vm.robot_id = vote_robot_id_;
    vm.track_id = 0;
    vm.bearing_deg = geo.bearing_deg;
    vm.elevation_deg = geo.elevation_deg;
    vm.range_m = geo.range_m;
    vm.nonce = ++vote_nonce_;
    vm.timestamp_ms = nowMs();
    combat_robot_msgs::msg::TargetConfirmation vc;
    vc.header.stamp = now();
    vc.robot_id = vm.robot_id;
    vc.track_id = vm.track_id;
    vc.bearing_deg = vm.bearing_deg;
    vc.elevation_deg = vm.elevation_deg;
    vc.range_m = vm.range_m;
    vc.nonce = vm.nonce;
    vc.timestamp_ms = vm.timestamp_ms;
    vc.hmac_hex = vote_auth_->sign(vm);
    vote_pub_->publish(vc);
  } 

  RCLCPP_INFO(
    get_logger(),
    "Threat published: severity=%u type=%u conf=%.2f msg=\"%s\"",
    static_cast<unsigned>(t.severity),
    static_cast<unsigned>(t.threat_type),
    static_cast<double>(t.confidence),
    t.message_ko.c_str());
}

}  // namespace san_hub_orchestrator
