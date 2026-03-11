#ifndef SKYHUNTER_CONTROL__LEADERSHIP_MANAGER_HPP_
#define SKYHUNTER_CONTROL__LEADERSHIP_MANAGER_HPP_

#include <chrono>
#include <string>
#include <vector>
#include <map>

#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/int8.hpp>
#include <std_msgs/msg/empty.hpp>
#include <skyhunter_msgs/msg/swarm_heartbeat.hpp>
#include <skyhunter_msgs/msg/election_vote.hpp>
#include <skyhunter_msgs/msg/leader_state.hpp>

#include <geometry_msgs/msg/pose.hpp>

using namespace std::chrono_literals;

enum class RobotState : int8_t { FOLLOWER = 0, CANDIDATE = 1, LEADER = 2 };

class LeadershipManager : public rclcpp::Node
{
public:
  explicit LeadershipManager(const rclcpp::NodeOptions & options = rclcpp::NodeOptions());

  ~LeadershipManager() override = default;

private:
  // Timer callback - main logic
  void logic_loop();

  // Callback functions
  void heartbeat_cb(const skyhunter_msgs::msg::SwarmHeartbeat::SharedPtr msg);
  void vote_cb(const skyhunter_msgs::msg::ElectionVote::SharedPtr msg);

  // Helper functions
  float calculate_fitness();
  void start_election();

  // State variables
  bool is_failed_ = false;
  std::vector<geometry_msgs::msg::Pose> mission_buffer_;
  bool takeover_complete = false;

  RobotState current_role_ = RobotState::FOLLOWER;
  std::string my_id_;
  int int_id_;

  uint32_t current_term_ = 0;
  float battery_level_;
  int votes_received_ = 0;

  rclcpp::Time last_leader_heartbeat_;
  rclcpp::Time last_election_attempt_;
  std::map<std::string, rclcpp::Time> last_seen_swarm_;

  int active_leader_id_ = 1;
  bool takeover_in_progress = false;

  rclcpp::Time takeover_start_time;

  // Publishers
  rclcpp::Publisher<skyhunter_msgs::msg::SwarmHeartbeat>::SharedPtr heartbeat_pub_;
  rclcpp::Publisher<skyhunter_msgs::msg::ElectionVote>::SharedPtr vote_pub_;
  rclcpp::Publisher<std_msgs::msg::Int8>::SharedPtr state_pub_;
  rclcpp::Publisher<skyhunter_msgs::msg::LeaderState>::SharedPtr virtual_leader_pub_;

  // Subscribers
  rclcpp::Subscription<skyhunter_msgs::msg::SwarmHeartbeat>::SharedPtr heartbeat_sub_;
  rclcpp::Subscription<skyhunter_msgs::msg::ElectionVote>::SharedPtr vote_sub_;
  rclcpp::Subscription<std_msgs::msg::Empty>::SharedPtr sub_chaos_;
  rclcpp::Subscription<skyhunter_msgs::msg::LeaderState>::SharedPtr sub_mission_shadow_;
  rclcpp::Subscription<skyhunter_msgs::msg::LeaderState>::SharedPtr local_leader_sub_;

  // Timer
  rclcpp::TimerBase::SharedPtr main_timer_;
};

#endif  // SKYHUNTER_CONTROL__LEADERSHIP_MANAGER_HPP_