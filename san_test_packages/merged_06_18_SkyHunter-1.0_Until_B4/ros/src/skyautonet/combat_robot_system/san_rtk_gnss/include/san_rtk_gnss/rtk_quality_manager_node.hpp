// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 — RTK Fix→Float mitigation Layer 3: rtk_quality_manager node.
//
// Subscribes the RTK driver's RtkFixStatus and drives RtkQualityFsm on a
// fixed tick (so timeouts fire even if the fix goes completely silent),
// then publishes advisory NavConstraints on ~/nav_constraints. On entering
// RTK_LOST it emits a ThreatAlert (TYPE_RTK_LOST, WARNING) so the mission
// layer can escalate Tier.
//
// Advisory only: nothing changes unless a navigation/formation consumer
// subscribes to nav_constraints.

#ifndef SAN_RTK_GNSS__RTK_QUALITY_MANAGER_NODE_HPP_
#define SAN_RTK_GNSS__RTK_QUALITY_MANAGER_NODE_HPP_

#include <string>

#include <rclcpp/rclcpp.hpp>
#include <combat_robot_msgs/msg/rtk_fix_status.hpp>
#include <combat_robot_msgs/msg/nav_constraints.hpp>
#include <combat_robot_msgs/msg/threat_alert.hpp>

#include "san_rtk_gnss/rtk_quality_fsm.hpp"

namespace san_rtk_gnss
{

class RtkQualityManagerNode : public rclcpp::Node
{
public:
  explicit RtkQualityManagerNode(
    const rclcpp::NodeOptions & opts = rclcpp::NodeOptions());

  // ─── Test hooks (deterministic, no executor) ───────────────────────
  void injectStatusForTest(uint8_t fix_type, double now_sec)
  {
    last_fix_type_ = fix_type;
    last_msg_sec_ = now_sec;
    have_msg_ = true;
  }
  combat_robot_msgs::msg::NavConstraints tickForTest(double now_sec)
  {
    return computeAndPublish(now_sec);
  }
  RtkQualityState fsmState() const {return fsm_.state();}
  uint32_t threatAlertsEmitted() const {return threat_alerts_emitted_;}

private:
  void declareParameters();
  void loadParameters();
  void onStatus(combat_robot_msgs::msg::RtkFixStatus::SharedPtr msg);
  void onTick();
  // Advance the FSM at now_sec, publish NavConstraints (+ ThreatAlert on a
  // fresh RTK_LOST), and return the published constraints.
  combat_robot_msgs::msg::NavConstraints computeAndPublish(double now_sec);

  RtkQualityFsm fsm_;
  RtkQualityState last_state_ = RtkQualityState::Ok;

  std::string robot_id_;
  double sensor_timeout_sec_ = 2.0;
  double tick_hz_ = 5.0;

  uint8_t last_fix_type_ = 0;
  double last_msg_sec_ = 0.0;
  bool have_msg_ = false;
  uint32_t threat_alerts_emitted_ = 0;

  rclcpp::Subscription<combat_robot_msgs::msg::RtkFixStatus>::SharedPtr
    status_sub_;
  rclcpp::Publisher<combat_robot_msgs::msg::NavConstraints>::SharedPtr
    constraints_pub_;
  rclcpp::Publisher<combat_robot_msgs::msg::ThreatAlert>::SharedPtr
    threat_pub_;
  rclcpp::TimerBase::SharedPtr tick_timer_;
};

}  // namespace san_rtk_gnss

#endif  // SAN_RTK_GNSS__RTK_QUALITY_MANAGER_NODE_HPP_
