// SAN v1.5.2 DCN-2026-010 D-028 — DetectionToThreatNode impl.

#include "san_hub_orchestrator/detection_to_threat_node.hpp"

#include <chrono>
#include <stdexcept>

namespace san_hub_orchestrator {

using namespace std::chrono_literals;

DetectionToThreatNode::DetectionToThreatNode(
    const rclcpp::NodeOptions& opts)
    : rclcpp::Node("detection_to_threat_node", opts),
      converter_(DetectionConverterConfig{}) {
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
      cfg.rgb_confidence_threshold > 1.0f) {
    throw std::runtime_error(
        "DetectionToThreatNode: rgb_confidence_threshold out of [0,1]");
  }
  if (cfg.fused_fallback_window_s <= 0.0 ||
      cfg.fused_fallback_window_s > 60.0) {
    throw std::runtime_error(
        "DetectionToThreatNode: fused_fallback_window_s out of (0, 60]");
  }
  converter_ = DetectionToThreatConverter(cfg);

  threat_pub_ = create_publisher<ThreatAlertMsg>(
      output_topic_, rclcpp::QoS(50).reliable());

  sub_fused_ = create_subscription<DetectionArray>(
      fused_topic_, rclcpp::QoS(10).best_effort(),
      std::bind(&DetectionToThreatNode::onDetectionFused,
                this, std::placeholders::_1));
  sub_rgb_ = create_subscription<DetectionArray>(
      rgb_topic_, rclcpp::QoS(10).best_effort(),
      std::bind(&DetectionToThreatNode::onDetectionRgb,
                this, std::placeholders::_1));

  RCLCPP_INFO(get_logger(),
      "DetectionToThreatNode UP: fused=%s rgb=%s out=%s "
      "conf=%.2f rgb_conf=%.2f fallback=%.1fs",
      fused_topic_.c_str(), rgb_topic_.c_str(), output_topic_.c_str(),
      cfg.confidence_threshold, cfg.rgb_confidence_threshold,
      cfg.fused_fallback_window_s);
}

void DetectionToThreatNode::declareParameters() {
  declare_parameter<std::string>(
      "fused_topic",   "/perception_node/detections_fused");
  declare_parameter<std::string>(
      "rgb_topic",     "/human_detector_node/detections_rgb");
  declare_parameter<std::string>(
      "output_topic",  "/swarm/threat_alert_raw");
  declare_parameter<std::string>("source_robot_id", "perception");
  declare_parameter<double>("confidence_threshold",     0.9);
  declare_parameter<double>("rgb_confidence_threshold", 0.8);
  declare_parameter<double>("fused_fallback_window_s",  1.0);
}

void DetectionToThreatNode::loadParameters() {
  fused_topic_     = get_parameter("fused_topic").as_string();
  rgb_topic_       = get_parameter("rgb_topic").as_string();
  output_topic_    = get_parameter("output_topic").as_string();
  source_robot_id_ = get_parameter("source_robot_id").as_string();
}

uint64_t DetectionToThreatNode::nowMs() const {
  return static_cast<uint64_t>(now().nanoseconds() / 1'000'000);
}

void DetectionToThreatNode::onDetectionFused(
    const DetectionArray::SharedPtr msg) {
  fused_count_.fetch_add(1, std::memory_order_relaxed);
  // Anchor the fused-stream timestamp on this node's clock so the
  // RGB fallback decision is independent of the publisher's clock
  // (same rationale as threat_aggregator's onIncomingThreat).
  converter_.markFusedReceived(nowMs());

  for (const auto& det : msg->detections) {
    auto out = converter_.convert(
        det.class_id, det.confidence,
        det.bbox_x1, det.bbox_y1, det.bbox_x2, det.bbox_y2,
        DetectionSource::Fused);
    if (out) publishThreat(*out, DetectionSource::Fused);
  }
}

void DetectionToThreatNode::onDetectionRgb(
    const DetectionArray::SharedPtr msg) {
  rgb_count_.fetch_add(1, std::memory_order_relaxed);
  if (!converter_.shouldUseRgb(nowMs())) {
    // Fused is authoritative — suppress RGB to avoid double-publish.
    return;
  }
  for (const auto& det : msg->detections) {
    auto out = converter_.convert(
        det.class_id, det.confidence,
        det.bbox_x1, det.bbox_y1, det.bbox_x2, det.bbox_y2,
        DetectionSource::RgbFallback);
    if (out) publishThreat(*out, DetectionSource::RgbFallback);
  }
}

void DetectionToThreatNode::publishThreat(
    const ConvertedThreat& t, DetectionSource /*src*/) {
  ThreatAlertMsg msg;
  msg.header.stamp        = now();
  msg.header.frame_id     = "detection_to_threat";
  msg.severity            = t.severity;
  msg.threat_type         = t.threat_type;
  msg.source_robot_id     = source_robot_id_;
  msg.peer_id             = "";
  msg.message_ko          = t.message_ko;
  msg.detail              = t.detail;
  msg.timestamp_ms        = nowMs();
  msg.instance_count      = 1;
  threat_pub_->publish(msg);
  publish_count_.fetch_add(1, std::memory_order_relaxed);

  RCLCPP_INFO(get_logger(),
      "Threat published: severity=%u type=%u conf=%.2f msg=\"%s\"",
      static_cast<unsigned>(t.severity),
      static_cast<unsigned>(t.threat_type),
      static_cast<double>(t.confidence),
      t.message_ko.c_str());
}

}  // namespace san_hub_orchestrator
