#ifndef SWARM_COORDINATOR_HPP_
#define SWARM_COORDINATOR_HPP_

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include <rclcpp/rclcpp.hpp>

#include "combat_robot_msgs/msg/mission_control_command.hpp"
#include "combat_robot_msgs/msg/swarm_follower_status.hpp"
#include "combat_robot_msgs/msg/swarm_leader_heartbeat.hpp"
#include "combat_robot_msgs/msg/swarm_control_command.hpp"
#include "combat_robot_msgs/msg/swarm_path_command.hpp"
#include "combat_robot_msgs/msg/swarm_robot_command.hpp"

namespace swarm_coordinator {

using combat_robot_msgs::msg::MissionControlCommand;
using combat_robot_msgs::msg::SwarmFollowerStatus;
using combat_robot_msgs::msg::SwarmLeaderHeartbeat;
using combat_robot_msgs::msg::SwarmControlCommand;
using combat_robot_msgs::msg::SwarmPathCommand;
using combat_robot_msgs::msg::SwarmRobotCommand;

class SwarmCoordinatorNode : public rclcpp::Node {
public:
  explicit SwarmCoordinatorNode(const rclcpp::NodeOptions& t_options);

private:
  static constexpr uint32_t MAX_ROBOTS = 8;

  enum class Role {
    LEADER,
    FOLLOWER
  };

  void initParameters();
  void initLeader();
  void initFollower();

  void onLeaderMissionControlCommand(const MissionControlCommand::SharedPtr t_msg);
  void onLeaderPathCommand(const SwarmPathCommand::SharedPtr t_msg);
  void onLeaderFormationCommand(const SwarmControlCommand::SharedPtr t_msg);
  void publishLeaderHeartbeat();

  void onFollowerRobotCommand(const SwarmRobotCommand::SharedPtr t_msg);
  void onFollowerHeartbeat(const SwarmLeaderHeartbeat::SharedPtr t_msg);
  void publishFollowerStatus();

  void publishRobotCommand(uint8_t t_command_type, bool t_force_default_targets);
  SwarmRobotCommand buildRobotCommand(uint8_t t_command_type, uint32_t t_target_robot_id);
  SwarmLeaderHeartbeat buildLeaderHeartbeat();
  SwarmFollowerStatus buildFollowerStatus(bool t_link_connected, double t_heartbeat_age_sec);
  std::vector<uint32_t> targetRobotIdsForRelay(bool t_force_default_targets) const;
  void updateSelectedRobotIds(const SwarmControlCommand& t_msg);
  uint8_t slotIndexForRobot(uint32_t t_robot_id) const;
  bool isValidRobotId(uint32_t t_robot_id) const;
  std::string robotTopic(uint32_t t_robot_id, const std::string& t_suffix) const;
  std::string followerTopic(const std::string& t_suffix) const;

  Role m_role{Role::LEADER};
  uint32_t m_robot_id{1};
  uint32_t m_leader_robot_id{1};
  bool m_relay_to_self{false};
  int m_heartbeat_period_ms{200};
  int m_heartbeat_timeout_ms{1000};
  std::string m_robot_topic_prefix{"/swarm"};
  std::string m_follower_output_prefix{"/swarm/follower"};
  std::string m_heartbeat_topic{"/swarm/leader/heartbeat"};

  mutable std::mutex m_selected_robot_mutex;
  std::vector<uint32_t> m_selected_robot_ids;
  std::vector<uint32_t> m_default_target_robot_ids;

  mutable std::mutex m_command_state_mutex;
  MissionControlCommand m_latest_mode_command{};
  SwarmPathCommand m_latest_path_command{};
  SwarmControlCommand m_latest_formation_command{};

  std::atomic<uint32_t> m_next_command_sequence{1};
  std::atomic<uint32_t> m_next_heartbeat_sequence{1};
  std::atomic<int64_t> m_last_heartbeat_ns{0};
  std::atomic<uint32_t> m_last_heartbeat_sequence{0};
  std::atomic<bool> m_leader_connected{false};

  rclcpp::QoS m_command_qos{rclcpp::KeepLast(1)};
  rclcpp::QoS m_heartbeat_qos{rclcpp::KeepLast(5)};

  rclcpp::Subscription<MissionControlCommand>::SharedPtr m_sub_leader_mission_control;
  rclcpp::Subscription<SwarmPathCommand>::SharedPtr m_sub_leader_path_command;
  rclcpp::Subscription<SwarmControlCommand>::SharedPtr m_sub_leader_formation_command;

  std::array<rclcpp::Publisher<SwarmRobotCommand>::SharedPtr, MAX_ROBOTS + 1>
    m_pub_robot_commands{};
  rclcpp::Publisher<SwarmLeaderHeartbeat>::SharedPtr m_pub_leader_heartbeat;
  rclcpp::TimerBase::SharedPtr m_leader_heartbeat_timer;

  rclcpp::Subscription<SwarmRobotCommand>::SharedPtr m_sub_follower_robot_command;
  rclcpp::Subscription<SwarmLeaderHeartbeat>::SharedPtr m_sub_follower_heartbeat;

  rclcpp::Publisher<SwarmRobotCommand>::SharedPtr m_pub_follower_command;
  rclcpp::Publisher<SwarmFollowerStatus>::SharedPtr m_pub_follower_status;
  rclcpp::TimerBase::SharedPtr m_follower_status_timer;
};

}  // namespace swarm_coordinator

#endif  // SWARM_COORDINATOR_HPP_
