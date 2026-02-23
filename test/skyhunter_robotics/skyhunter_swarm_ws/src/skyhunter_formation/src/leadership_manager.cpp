#include <chrono>
#include <string>
#include <vector>
#include <map>
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/int8.hpp"
#include "skyhunter_msgs/msg/swarm_heartbeat.hpp"
#include "skyhunter_msgs/msg/election_vote.hpp"
#include "std_msgs/msg/empty.hpp"
#include "skyhunter_msgs/msg/leader_state.hpp"

using namespace std::chrono_literals;

enum class RobotState : int8_t { FOLLOWER = 0, CANDIDATE = 1, LEADER = 2 };

class LeadershipManager : public rclcpp::Node {
public:
  LeadershipManager() : Node("leadership_manager") {
    this->declare_parameter<double>("battery_drain_rate", 0.0001);
    this->declare_parameter<int>("robot_int_id", 1);
    
    my_id_ = std::string(this->get_namespace());
    if (my_id_.length() > 1 && my_id_[0] == '/') my_id_ = my_id_.substr(1);
    
    int_id_ = this->get_parameter("robot_int_id").as_int();
    battery_level_ = 1.0; 

    last_leader_heartbeat_ = this->get_clock()->now();
    last_election_attempt_ = this->get_clock()->now(); // Initialize

    auto qos = rclcpp::SensorDataQoS();
    heartbeat_pub_ = this->create_publisher<skyhunter_msgs::msg::SwarmHeartbeat>("/swarm/heartbeat", 10);
    vote_pub_ = this->create_publisher<skyhunter_msgs::msg::ElectionVote>("/swarm/election_vote", 10);
    state_pub_ = this->create_publisher<std_msgs::msg::Int8>("local_role", 10);

    heartbeat_sub_ = this->create_subscription<skyhunter_msgs::msg::SwarmHeartbeat>(
        "/swarm/heartbeat", qos, std::bind(&LeadershipManager::heartbeat_cb, this, std::placeholders::_1));
    
    vote_sub_ = this->create_subscription<skyhunter_msgs::msg::ElectionVote>(
        "/swarm/election_vote", 10, std::bind(&LeadershipManager::vote_cb, this, std::placeholders::_1));


    sub_chaos_ = this->create_subscription<std_msgs::msg::Empty>(
        "simulate_fail", 10, [this](const std_msgs::msg::Empty::SharedPtr) {
            this->is_failed_ = true;
            RCLCPP_ERROR(this->get_logger(), "!!! CRITICAL HARDWARE FAILURE SIMULATED !!! Heartbeat stopped.");
        });

    // Listen to the current leader's targets to remember the mission
    sub_mission_shadow_ = this->create_subscription<skyhunter_msgs::msg::LeaderState>(
        "/leader_state", 10, [this](const skyhunter_msgs::msg::LeaderState::SharedPtr msg) {
            if (!msg->next_waypoints.empty()) {
                this->mission_buffer_ = msg->next_waypoints; // Shadow the plan
            }
        });
        
    main_timer_ = this->create_wall_timer(500ms, std::bind(&LeadershipManager::logic_loop, this));
  }

private:
  float calculate_fitness() {
    int connectivity = 0;
    auto now = this->get_clock()->now();
    for (auto const& [id, time] : last_seen_swarm_) {
        if ((now - time).seconds() < 5.0) connectivity++;
    }
    return (1.0 * battery_level_) + (0.5 * connectivity) - (0.01 * int_id_);
  }

  void logic_loop() {
    if (is_failed_) {
        // DO NOTHING. No heartbeats sent. Swarm will think I am dead.
        std_msgs::msg::Int8 role_msg;
        role_msg.data = -1; // Special "DEAD" state
        state_pub_->publish(role_msg);
        return; 
    }
    
    battery_level_ -= this->get_parameter("battery_drain_rate").as_double();
    auto now = this->get_clock()->now();

    if (current_role_ == RobotState::FOLLOWER) {
        if ((now - last_leader_heartbeat_).seconds() > 4.0) {
            start_election();
        }
    }

    skyhunter_msgs::msg::SwarmHeartbeat hb;
    hb.header.stamp = now;
    hb.robot_id = my_id_;
    hb.term = current_term_;
    hb.is_leader = (current_role_ == RobotState::LEADER);
    hb.battery_level = battery_level_;
    heartbeat_pub_->publish(hb);

    std_msgs::msg::Int8 role_msg;
    role_msg.data = static_cast<int8_t>(current_role_);
    state_pub_->publish(role_msg);
  }

  void start_election() {
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

  void heartbeat_cb(const skyhunter_msgs::msg::SwarmHeartbeat::SharedPtr msg) {
    if (msg->is_leader) last_leader_heartbeat_ = this->get_clock()->now();
    if (msg->robot_id == my_id_) return;
    last_seen_swarm_[msg->robot_id] = this->get_clock()->now();

    if (msg->is_leader && current_role_ == RobotState::LEADER) {
        if (msg->term > current_term_ || (msg->term == current_term_ && msg->battery_level > battery_level_)) {
            current_role_ = RobotState::FOLLOWER;
            current_term_ = msg->term;
        }
    }
  }

  void vote_cb(const skyhunter_msgs::msg::ElectionVote::SharedPtr msg) {
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

  bool is_failed_ = false; // The Kill Switch
  std::vector<geometry_msgs::msg::Pose> mission_buffer_; 

  RobotState current_role_ = RobotState::FOLLOWER;
  std::string my_id_;
  int int_id_;
  uint32_t current_term_ = 0;
  float battery_level_;
  int votes_received_ = 0;
  rclcpp::Time last_leader_heartbeat_, last_election_attempt_;
  std::map<std::string, rclcpp::Time> last_seen_swarm_;

  rclcpp::Subscription<std_msgs::msg::Empty>::SharedPtr sub_chaos_;
  rclcpp::Subscription<skyhunter_msgs::msg::LeaderState>::SharedPtr sub_mission_shadow_;

  rclcpp::Publisher<skyhunter_msgs::msg::SwarmHeartbeat>::SharedPtr heartbeat_pub_;
  rclcpp::Publisher<skyhunter_msgs::msg::ElectionVote>::SharedPtr vote_pub_;
  rclcpp::Publisher<std_msgs::msg::Int8>::SharedPtr state_pub_;
  rclcpp::Subscription<skyhunter_msgs::msg::SwarmHeartbeat>::SharedPtr heartbeat_sub_;
  rclcpp::Subscription<skyhunter_msgs::msg::ElectionVote>::SharedPtr vote_sub_;
  rclcpp::TimerBase::SharedPtr main_timer_;
};

int main(int argc, char * argv[]) { rclcpp::init(argc, argv); rclcpp::spin(std::make_shared<LeadershipManager>()); rclcpp::shutdown(); return 0; }