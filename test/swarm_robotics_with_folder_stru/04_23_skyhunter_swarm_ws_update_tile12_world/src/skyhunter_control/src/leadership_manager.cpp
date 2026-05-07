#include "skyhunter_control/leadership_manager.hpp"

#include <cstdlib>

LeadershipManager::LeadershipManager(const rclcpp::NodeOptions & options)
: Node("leadership_manager", options)
{
  this->declare_parameter<double>("battery_drain_rate", 0.0001);
  this->declare_parameter<int>("robot_int_id", 1);

  my_id_ = std::string(this->get_namespace());
  if (my_id_.length() > 1 && my_id_[0] == '/') my_id_ = my_id_.substr(1);

  int_id_ = this->get_parameter("robot_int_id").as_int();

  if (int_id_ == 1) {
    current_role_ = RobotState::LEADER;
    RCLCPP_INFO(this->get_logger(), "Robot 01 Initialized as PERMANENT LEADER.");
  } else {
    current_role_ = RobotState::FOLLOWER;
  }

  battery_level_ = 1.0;

  last_leader_heartbeat_ = this->get_clock()->now();
  last_election_attempt_ = this->get_clock()->now();

  auto qos = rclcpp::SensorDataQoS();

  heartbeat_pub_ = this->create_publisher<skyhunter_msgs::msg::SwarmHeartbeat>("/swarm/heartbeat", 10);
  vote_pub_      = this->create_publisher<skyhunter_msgs::msg::ElectionVote>("/swarm/election_vote", 10);
  state_pub_     = this->create_publisher<std_msgs::msg::Int8>("local_role", 10);

  heartbeat_sub_ = this->create_subscription<skyhunter_msgs::msg::SwarmHeartbeat>(
    "/swarm/heartbeat", qos,
    std::bind(&LeadershipManager::heartbeat_cb, this, std::placeholders::_1));

  vote_sub_ = this->create_subscription<skyhunter_msgs::msg::ElectionVote>(
    "/swarm/election_vote", 10,
    std::bind(&LeadershipManager::vote_cb, this, std::placeholders::_1));

  sub_chaos_ = this->create_subscription<std_msgs::msg::Empty>(
    "simulate_fail", 10,
    [this](const std_msgs::msg::Empty::SharedPtr) {
      this->is_failed_ = true;
      RCLCPP_ERROR(this->get_logger(), "!!! CRITICAL HARDWARE FAILURE SIMULATED !!! Heartbeat stopped.");
    });

  sub_mission_shadow_ = this->create_subscription<skyhunter_msgs::msg::LeaderState>(
    "/leader_state", 10,
    [this](const skyhunter_msgs::msg::LeaderState::SharedPtr msg) {
      if (!msg->next_waypoints.empty()) {
        this->mission_buffer_ = msg->next_waypoints;
      }
    });

  virtual_leader_pub_ = this->create_publisher<skyhunter_msgs::msg::LeaderState>("/swarm/virtual_leader/state", 10);

  local_leader_sub_ = this->create_subscription<skyhunter_msgs::msg::LeaderState>(
    "leader_state", 10,
    [this](const skyhunter_msgs::msg::LeaderState::SharedPtr msg) {
      if (this->int_id_ == 1 || this->current_role_ == RobotState::LEADER) {
        this->virtual_leader_pub_->publish(*msg);
      }
    });

  main_timer_ = this->create_wall_timer(500ms, std::bind(&LeadershipManager::logic_loop, this));
}

float LeadershipManager::calculate_fitness()
{
  int connectivity = 0;
  auto now = this->get_clock()->now();
  for (auto const& [id, time] : last_seen_swarm_) {
    if ((now - time).seconds() < 5.0) connectivity++;
  }
  return (1.0 * battery_level_) + (0.5 * connectivity) - (0.01 * int_id_);
}

void LeadershipManager::logic_loop()
{
  if (is_failed_) {
    std_msgs::msg::Int8 role_msg;
    role_msg.data = -1;
    state_pub_->publish(role_msg);
    return;
  }

  auto now = this->get_clock()->now();

  // 1. Detection of Leader Loss (Only for ID > 1)
  if (current_role_ == RobotState::FOLLOWER && int_id_ > 1) {
    double time_since_heartbeat = (now - last_leader_heartbeat_).seconds();

    if (time_since_heartbeat > 4.0) {
      current_role_ = RobotState::CANDIDATE;
      takeover_start_time = now;
    }
  }

  if (current_role_ == RobotState::CANDIDATE) {
    if ((now - takeover_start_time).seconds() >= 5.0) {
      current_role_ = RobotState::LEADER;
      RCLCPP_WARN(this->get_logger(), "!!! I AM NOW THE LEADER (ID: %d) !!!", int_id_);

      skyhunter_msgs::msg::SwarmHeartbeat hb;
      hb.header.stamp = now;
      hb.robot_id = my_id_;
      hb.is_leader = true;
      heartbeat_pub_->publish(hb);
    }
  }

  // 3. Heartbeat
  skyhunter_msgs::msg::SwarmHeartbeat hb;
  hb.header.stamp = now;
  hb.robot_id = (int_id_ == 1) ? "robot_01" : my_id_;
  hb.is_leader = (current_role_ == RobotState::LEADER);
  heartbeat_pub_->publish(hb);

  std_msgs::msg::Int8 role_msg;
  role_msg.data = static_cast<int8_t>(current_role_);
  state_pub_->publish(role_msg);
}

void LeadershipManager::start_election()
{
  auto now = this->get_clock()->now();
  if ((now - last_election_attempt_).seconds() < 2.0) return;
  last_election_attempt_ = now;

  current_term_++;
  votes_received_ = 1;
  current_role_ = RobotState::CANDIDATE;

  skyhunter_msgs::msg::ElectionVote vote;
  vote.term = current_term_;
  vote.candidate_id = my_id_;
  vote.voter_id = my_id_;
  vote.fitness_score = calculate_fitness();
  vote_pub_->publish(vote);
}

void LeadershipManager::heartbeat_cb(const skyhunter_msgs::msg::SwarmHeartbeat::SharedPtr msg)
{
  if (msg->robot_id == my_id_) return;

  if (msg->is_leader) {
    last_leader_heartbeat_ = this->get_clock()->now();

    int incoming_id;
    try {
      if (msg->robot_id == "robot_01") {
        incoming_id = 1;
      } else {
        std::string id_str = msg->robot_id;
        size_t pos = id_str.find("SH_");
        if (pos != std::string::npos) id_str = id_str.substr(pos + 3);
        incoming_id = std::stoi(id_str);
      }

      if (current_role_ == RobotState::CANDIDATE && incoming_id < int_id_) {
        RCLCPP_INFO(this->get_logger(), "Higher priority unit %d is leading. Aborting takeover.", incoming_id);
        current_role_ = RobotState::FOLLOWER;
      }
    } catch (...) {
      return;
    }
  }
}

void LeadershipManager::vote_cb(const skyhunter_msgs::msg::ElectionVote::SharedPtr msg)
{
  if (msg->voter_id == my_id_) return;

  if (msg->term > current_term_) {
    current_term_ = msg->term;
    current_role_ = RobotState::FOLLOWER;
    votes_received_ = 0;
  }

  if (current_role_ == RobotState::FOLLOWER && msg->candidate_id == msg->voter_id) {
    if (msg->fitness_score > calculate_fitness()) {
      skyhunter_msgs::msg::ElectionVote my_vote;
      my_vote.term = current_term_;
      my_vote.candidate_id = msg->candidate_id;
      my_vote.voter_id = my_id_;
      my_vote.fitness_score = calculate_fitness();
      vote_pub_->publish(my_vote);
    }
  }

  if (current_role_ == RobotState::CANDIDATE && msg->candidate_id == my_id_ && msg->voter_id != my_id_) {
    votes_received_++;
    if (votes_received_ >= 2) current_role_ = RobotState::LEADER;
  }
}

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<LeadershipManager>());
  rclcpp::shutdown();
  return 0;
}