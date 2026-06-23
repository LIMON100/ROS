// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5.2 DCN-2026-010 D-028 — DetectionToThreatNode (rclcpp).
//
// Subscribes:
//   * /perception_node/detections_fused      (EO+IR, primary)
//   * /human_detector_node/detections_rgb    (RGB only, fallback)
//
// Publishes:
//   * /swarm/threat_alert_raw                (combat_robot_msgs/ThreatAlert)
//
// Flow:
//   1. Fused detections at conf ≥ confidence_threshold → publish.
//   2. Fused timestamps are tracked. RGB detections suppress unless
//      no fused has arrived within fused_fallback_window_s.
//   3. RGB fallback uses a lower (rgb_confidence_threshold) gate.

#ifndef SAN_HUB_ORCHESTRATOR__DETECTION_TO_THREAT_NODE_HPP_
#define SAN_HUB_ORCHESTRATOR__DETECTION_TO_THREAT_NODE_HPP_

#include <atomic>
#include <memory>
#include <string>

#include <rclcpp/rclcpp.hpp>
#include <combat_robot_msgs/msg/detection_array.hpp>
#include <combat_robot_msgs/msg/threat_alert.hpp>

#include <combat_robot_msgs/msg/detection.hpp>
#include <sensor_msgs/msg/joint_state.hpp>
#include <tf2_ros/buffer.h>
#include <tf2_ros/transform_listener.h>

#include "san_hub_orchestrator/detection_to_threat.hpp"
#include <combat_robot_msgs/msg/target_confirmation.hpp>
#include "san_fire_authorization/target_confirmation_auth.hpp"

namespace san_hub_orchestrator
{

class DetectionToThreatNode : public rclcpp::Node
{
public:
  explicit DetectionToThreatNode(
    const rclcpp::NodeOptions & opts = rclcpp::NodeOptions());
  ~DetectionToThreatNode() override = default;

  // Test accessors
  uint64_t publishCount() const {return publish_count_.load();}
  uint64_t fusedCount()   const {return fused_count_.load();}
  uint64_t rgbCount()     const {return rgb_count_.load();}

private:
  using DetectionArray = combat_robot_msgs::msg::DetectionArray;
  using ThreatAlertMsg = combat_robot_msgs::msg::ThreatAlert;

  void declareParameters();
  void loadParameters();
  void onDetectionFused(const DetectionArray::SharedPtr msg);
  void onDetectionRgb(const DetectionArray::SharedPtr msg);
  // void publishThreat(const ConvertedThreat & t, DetectionSource src);
  void publishThreat(const ConvertedThreat & t, DetectionSource src, const GeoResult & geo);

  uint64_t nowMs() const;

  // Params
  std::string fused_topic_;
  std::string rgb_topic_;
  std::string output_topic_;
  std::string source_robot_id_;

  DetectionToThreatConverter converter_;

  rclcpp::Subscription<DetectionArray>::SharedPtr sub_fused_;
  rclcpp::Subscription<DetectionArray>::SharedPtr sub_rgb_;
  rclcpp::Publisher<ThreatAlertMsg>::SharedPtr threat_pub_;

  rclcpp::Publisher<combat_robot_msgs::msg::TargetConfirmation>::SharedPtr vote_pub_;
  std::unique_ptr<san_fire_authorization::TargetConfirmationAuth> vote_auth_;
  std::string mesh_secret_path_;
  uint32_t vote_robot_id_{0};
  uint64_t vote_nonce_{0};

  std::atomic<uint64_t> fused_count_  {0};
  std::atomic<uint64_t> rgb_count_    {0};
  std::atomic<uint64_t> publish_count_{0};

  void onJointState(const sensor_msgs::msg::JointState::SharedPtr msg);
  GeoResult geoForDetection(const combat_robot_msgs::msg::Detection & det);

  std::string base_frame_;
  double focal_px_{550.0}, img_cx_{320.0}, img_cy_{240.0};
  std::atomic<double> gimbal_pan_{0.0};
  std::atomic<double> gimbal_tilt_{0.0};
  std::shared_ptr<tf2_ros::Buffer> tf_buffer_;
  std::shared_ptr<tf2_ros::TransformListener> tf_listener_;
  rclcpp::Subscription<sensor_msgs::msg::JointState>::SharedPtr sub_joints_;

};

}  // namespace san_hub_orchestrator

#endif  // SAN_HUB_ORCHESTRATOR__DETECTION_TO_THREAT_NODE_HPP_
