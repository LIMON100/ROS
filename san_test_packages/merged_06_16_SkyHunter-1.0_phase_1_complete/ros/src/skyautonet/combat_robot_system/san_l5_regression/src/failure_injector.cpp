// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

#include "san_l5_regression/failure_injector.hpp"

namespace san_l5_regression
{

FailureInjector::FailureInjector(rclcpp::Node * node)
: node_(node)
{
  rclcpp::QoS qos(20);
  qos.reliable();
  status_pub_ = node_->create_publisher<combat_robot_msgs::msg::RobotStatus>(
    kRobotStatusTopic, qos);

  rclcpp::QoS lq_qos(10);
  lq_qos.best_effort();
  lq_pub_ = node_->create_publisher<combat_robot_msgs::msg::LteLinkQuality>(
    kLinkQualityTopic, lq_qos);
}

uint64_t FailureInjector::nowMs() const
{
  return static_cast<uint64_t>(node_->now().nanoseconds() / 1'000'000ll);
}

void FailureInjector::setHealth(uint32_t robot_id, const RobotHealth & h)
{
  std::lock_guard<std::mutex> lock(mu_);
  state_[robot_id] = h;
}

void FailureInjector::killSbc(uint32_t robot_id, bool sbc1, bool sbc2)
{
  std::lock_guard<std::mutex> lock(mu_);
  auto & h = state_[robot_id];
  h.sbc1_healthy = sbc1;
  h.sbc2_healthy = sbc2;
}

void FailureInjector::setBattery(uint32_t robot_id, float percent)
{
  std::lock_guard<std::mutex> lock(mu_);
  state_[robot_id].battery_percent = percent;
}

void FailureInjector::markDeputy(uint32_t robot_id, bool is_deputy)
{
  std::lock_guard<std::mutex> lock(mu_);
  state_[robot_id].is_deputy_ugv = is_deputy;
}

void FailureInjector::markLeader(uint32_t robot_id, bool is_leader)
{
  std::lock_guard<std::mutex> lock(mu_);
  state_[robot_id].is_leader_role_active = is_leader;
}

combat_robot_msgs::msg::RobotStatus
FailureInjector::buildMessage(uint32_t robot_id, const RobotHealth & h) const
{
  combat_robot_msgs::msg::RobotStatus s;
  s.header.stamp = node_->now();
  s.header.frame_id = "synthetic";
  s.robot_id = robot_id;
  // robot_role: follower=0 / leader=1 / hub=2 (legacy enum).
  if (robot_id == 1) {s.robot_role = 1;} else if (robot_id == 2) {s.robot_role = 2;} else {
    s.robot_role = 0;
  }
  s.battery_soc = h.battery_percent / 100.0f;
  s.slam_healthy = h.slam_healthy;
  s.perception_healthy = h.perception_healthy;
  s.comm_healthy = h.comm_healthy;
  s.lte_active = h.lte_active;
  s.is_lte_backup_designated = (robot_id == 3 || robot_id == 5);
  s.is_deputy_ugv = h.is_deputy_ugv;
  s.is_hub_role_active = h.is_hub_role_active;
  s.is_leader_role_active = h.is_leader_role_active;
  s.battery_percent = h.battery_percent;
  s.sbc1_healthy = h.sbc1_healthy;
  s.sbc2_healthy = h.sbc2_healthy;
  s.in_limp_mode = false;
  s.timestamp_ms = nowMs();
  return s;
}

std::size_t FailureInjector::publishAll()
{
  std::vector<combat_robot_msgs::msg::RobotStatus> snapshot;
  {
    std::lock_guard<std::mutex> lock(mu_);
    snapshot.reserve(state_.size());
    for (const auto & [id, h] : state_) {
      if (dead_.count(id)) {continue;}
      snapshot.push_back(buildMessage(id, h));
    }
  }
  for (auto & m : snapshot) {
    status_pub_->publish(m);
  }
  return snapshot.size();
}

void FailureInjector::publishOne(uint32_t robot_id)
{
  combat_robot_msgs::msg::RobotStatus m;
  {
    std::lock_guard<std::mutex> lock(mu_);
    if (dead_.count(robot_id)) {return;}
    auto it = state_.find(robot_id);
    if (it == state_.end()) {return;}
    m = buildMessage(robot_id, it->second);
  }
  status_pub_->publish(m);
}

void FailureInjector::removeRobot(uint32_t robot_id)
{
  std::lock_guard<std::mutex> lock(mu_);
  auto it = state_.find(robot_id);
  if (it != state_.end()) {
    archived_[robot_id] = it->second;
  }
  dead_.insert(robot_id);
}

void FailureInjector::restoreRobot(uint32_t robot_id, const RobotHealth & h)
{
  std::lock_guard<std::mutex> lock(mu_);
  state_[robot_id] = h;
  dead_.erase(robot_id);
  archived_.erase(robot_id);
}

bool FailureInjector::isPublished(uint32_t robot_id) const
{
  std::lock_guard<std::mutex> lock(mu_);
  return state_.find(robot_id) != state_.end() && dead_.count(robot_id) == 0;
}

void FailureInjector::publishLteGrade(
  uint8_t grade, int16_t rsrp_dbm,
  const std::string & iface)
{
  combat_robot_msgs::msg::LteLinkQuality m;
  m.header.stamp = node_->now();
  m.header.frame_id = iface;
  m.source_iface = iface;
  m.rsrp_dbm = rsrp_dbm;
  m.rsrq_db = 0;
  m.sinr_db = 0;
  m.grade = grade;
  m.timestamp_ms = nowMs();
  lq_pub_->publish(m);
}

void FailureInjector::reset()
{
  std::lock_guard<std::mutex> lock(mu_);
  state_.clear();
  archived_.clear();
  dead_.clear();
}

RobotHealth FailureInjector::health(uint32_t robot_id) const
{
  std::lock_guard<std::mutex> lock(mu_);
  auto it = state_.find(robot_id);
  if (it == state_.end()) {return {};}
  return it->second;
}

std::size_t FailureInjector::trackedRobotCount() const
{
  std::lock_guard<std::mutex> lock(mu_);
  return state_.size();
}

}  // namespace san_l5_regression
