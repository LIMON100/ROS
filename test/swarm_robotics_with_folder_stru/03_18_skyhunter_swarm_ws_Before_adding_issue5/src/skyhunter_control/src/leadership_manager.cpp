// #include <chrono>
// #include <string>
// #include <vector>
// #include <map>
// #include "rclcpp/rclcpp.hpp"
// #include "std_msgs/msg/int8.hpp"
// #include "skyhunter_msgs/msg/swarm_heartbeat.hpp"
// #include "skyhunter_msgs/msg/election_vote.hpp"
// #include "std_msgs/msg/empty.hpp"
// #include "skyhunter_msgs/msg/leader_state.hpp"
// #include <cstdlib> // Needed for the system() function

// using namespace std::chrono_literals;

// enum class RobotState : int8_t { FOLLOWER = 0, CANDIDATE = 1, LEADER = 2 };

// class LeadershipManager : public rclcpp::Node {
// public:
//   LeadershipManager() : Node("leadership_manager") {
//     this->declare_parameter<double>("battery_drain_rate", 0.0001);
//     this->declare_parameter<int>("robot_int_id", 1);
    
//     my_id_ = std::string(this->get_namespace());
//     if (my_id_.length() > 1 && my_id_[0] == '/') my_id_ = my_id_.substr(1);
    
//     int_id_ = this->get_parameter("robot_int_id").as_int();
//     if (int_id_ == 1) {
//         current_role_ = RobotState::LEADER;
//         RCLCPP_INFO(this->get_logger(), "Robot 01 Initialized as PERMANENT LEADER.");
//     } else {
//         current_role_ = RobotState::FOLLOWER;
//     }

//     battery_level_ = 1.0; 

//     last_leader_heartbeat_ = this->get_clock()->now();
//     last_election_attempt_ = this->get_clock()->now(); // Initialize

//     auto qos = rclcpp::SensorDataQoS();
//     heartbeat_pub_ = this->create_publisher<skyhunter_msgs::msg::SwarmHeartbeat>("/swarm/heartbeat", 10);
//     vote_pub_ = this->create_publisher<skyhunter_msgs::msg::ElectionVote>("/swarm/election_vote", 10);
//     state_pub_ = this->create_publisher<std_msgs::msg::Int8>("local_role", 10);

//     heartbeat_sub_ = this->create_subscription<skyhunter_msgs::msg::SwarmHeartbeat>(
//         "/swarm/heartbeat", qos, std::bind(&LeadershipManager::heartbeat_cb, this, std::placeholders::_1));
    
//     vote_sub_ = this->create_subscription<skyhunter_msgs::msg::ElectionVote>(
//         "/swarm/election_vote", 10, std::bind(&LeadershipManager::vote_cb, this, std::placeholders::_1));


//     sub_chaos_ = this->create_subscription<std_msgs::msg::Empty>(
//         "simulate_fail", 10, [this](const std_msgs::msg::Empty::SharedPtr) {
//             this->is_failed_ = true;
//             RCLCPP_ERROR(this->get_logger(), "!!! CRITICAL HARDWARE FAILURE SIMULATED !!! Heartbeat stopped.");
//         });

//     // Listen to the current leader's targets to remember the mission
//     sub_mission_shadow_ = this->create_subscription<skyhunter_msgs::msg::LeaderState>(
//         "/leader_state", 10, [this](const skyhunter_msgs::msg::LeaderState::SharedPtr msg) {
//             if (!msg->next_waypoints.empty()) {
//                 this->mission_buffer_ = msg->next_waypoints; // Shadow the plan
//             }
//         });

//     virtual_leader_pub_ = this->create_publisher<skyhunter_msgs::msg::LeaderState>("/swarm/virtual_leader/state", 10);

//     local_leader_sub_ = this->create_subscription<skyhunter_msgs::msg::LeaderState>(
//         "leader_state", 10, [this](const skyhunter_msgs::msg::LeaderState::SharedPtr msg) {
//             // If I am ID 1 (R1) OR if I have been elected LEADER (R2 taking over)
//             if (this->int_id_ == 1 || this->current_role_ == RobotState::LEADER) {
//                 this->virtual_leader_pub_->publish(*msg);
//             }
//         });
        
//     main_timer_ = this->create_wall_timer(500ms, std::bind(&LeadershipManager::logic_loop, this));
//   }

// private:
//   float calculate_fitness() {
//     int connectivity = 0;
//     auto now = this->get_clock()->now();
//     for (auto const& [id, time] : last_seen_swarm_) {
//         if ((now - time).seconds() < 5.0) connectivity++;
//     }
//     return (1.0 * battery_level_) + (0.5 * connectivity) - (0.01 * int_id_);
//   }

//   void logic_loop() {
//     if (is_failed_) {
//         std_msgs::msg::Int8 role_msg; role_msg.data = -1;
//         state_pub_->publish(role_msg);
//         return; 
//     }
    
//     auto now = this->get_clock()->now();

//     // 1. Detection of Leader Loss (Only for ID > 1)
//     if (current_role_ == RobotState::FOLLOWER && int_id_ > 1) {
//         double time_since_heartbeat = (now - last_leader_heartbeat_).seconds();
        
//         // ORIGINAL 4.0 TIMEOUT
//         if (time_since_heartbeat > 4.0) { 
//             current_role_ = RobotState::CANDIDATE;
//             takeover_start_time = now;
//         }
//     }

//     if (current_role_ == RobotState::CANDIDATE) {
//         // Reduced wait time to 5.0s to match your python timer logic better
//         if ((now - takeover_start_time).seconds() >= 5.0) {
//             current_role_ = RobotState::LEADER;
//             RCLCPP_WARN(this->get_logger(), "!!! I AM NOW THE LEADER (ID: %d) !!!", int_id_);
            
//             // Force an immediate heartbeat update so listeners know ASAP
//             skyhunter_msgs::msg::SwarmHeartbeat hb;
//             hb.header.stamp = now;
//             hb.robot_id = my_id_;
//             hb.is_leader = true;
//             heartbeat_pub_->publish(hb);
//         }
//     }

//     // 3. Heartbeat
//     skyhunter_msgs::msg::SwarmHeartbeat hb;
//     hb.header.stamp = now;
//     hb.robot_id = (int_id_ == 1) ? "robot_01" : my_id_;
//     hb.is_leader = (current_role_ == RobotState::LEADER);
//     heartbeat_pub_->publish(hb);

//     std_msgs::msg::Int8 role_msg;
//     role_msg.data = static_cast<int8_t>(current_role_);
//     state_pub_->publish(role_msg);
// }

//   void start_election() {
//     auto now = this->get_clock()->now();
//     if ((now - last_election_attempt_).seconds() < 2.0) return;
//     last_election_attempt_ = now;

//     current_term_++;
//     votes_received_ = 1; 
//     current_role_ = RobotState::CANDIDATE;
    
//     skyhunter_msgs::msg::ElectionVote vote;
//     vote.term = current_term_;
//     vote.candidate_id = my_id_;
//     vote.voter_id = my_id_;
//     vote.fitness_score = calculate_fitness();
//     vote_pub_->publish(vote);
//   }

//   void heartbeat_cb(const skyhunter_msgs::msg::SwarmHeartbeat::SharedPtr msg) {
//     if (msg->robot_id == my_id_) return;

//     if (msg->is_leader) {
//         last_leader_heartbeat_ = this->get_clock()->now();

//         // --- ROBUST ID PARSING ---
//         int incoming_id;
//         try {
//             if (msg->robot_id == "robot_01") {
//                 incoming_id = 1;
//             } else {
//                 // Extracts numbers from strings like "SH_02" or "/SH_02"
//                 std::string id_str = msg->robot_id;
//                 // Remove prefix "SH_" or "/SH_"
//                 size_t pos = id_str.find("SH_");
//                 if (pos != std::string::npos) id_str = id_str.substr(pos + 3);
//                 incoming_id = std::stoi(id_str);
//             }

//             // SUPPRESSION: If a unit with a LOWER ID (higher priority) is leading, 
//             // I must back down and remain a follower.
//             if (current_role_ == RobotState::CANDIDATE && incoming_id < int_id_) {
//                 RCLCPP_INFO(this->get_logger(), "Higher priority unit %d is leading. Aborting takeover.", incoming_id);
//                 current_role_ = RobotState::FOLLOWER;
//             }
//         } catch (...) {
//             // If ID is unparseable, ignore it for safety
//             return;
//         }
//     }
// }

//   void vote_cb(const skyhunter_msgs::msg::ElectionVote::SharedPtr msg) {
//     if (msg->voter_id == my_id_) return;
//     if (msg->term > current_term_) {
//         current_term_ = msg->term;
//         current_role_ = RobotState::FOLLOWER;
//         votes_received_ = 0;
//     }
//     if (current_role_ == RobotState::FOLLOWER && msg->candidate_id == msg->voter_id) {
//         if (msg->fitness_score > calculate_fitness()) {
//             skyhunter_msgs::msg::ElectionVote my_vote;
//             my_vote.term = current_term_;
//             my_vote.candidate_id = msg->candidate_id;
//             my_vote.voter_id = my_id_;
//             my_vote.fitness_score = calculate_fitness();
//             vote_pub_->publish(my_vote);
//         }
//     }
//     if (current_role_ == RobotState::CANDIDATE && msg->candidate_id == my_id_ && msg->voter_id != my_id_) {
//         votes_received_++;
//         if (votes_received_ >= 2) current_role_ = RobotState::LEADER;
//     }
//   }



//   bool is_failed_ = false; // The Kill Switch
//   std::vector<geometry_msgs::msg::Pose> mission_buffer_; 
//   bool takeover_complete = false;

//   RobotState current_role_ = RobotState::FOLLOWER;
//   std::string my_id_;
//   int int_id_;
//   uint32_t current_term_ = 0;
//   float battery_level_;
//   int votes_received_ = 0;
//   rclcpp::Time last_leader_heartbeat_, last_election_attempt_;
//   std::map<std::string, rclcpp::Time> last_seen_swarm_;

//   int active_leader_id_ = 1; // Start with 1 as leader
//   bool takeover_in_progress = false;

// // Publishers/Subscribers for the Relay
//   rclcpp::Publisher<skyhunter_msgs::msg::LeaderState>::SharedPtr virtual_leader_pub_;
//   rclcpp::Subscription<skyhunter_msgs::msg::LeaderState>::SharedPtr local_leader_sub_;

//   rclcpp::Subscription<std_msgs::msg::Empty>::SharedPtr sub_chaos_;
//   rclcpp::Subscription<skyhunter_msgs::msg::LeaderState>::SharedPtr sub_mission_shadow_;

//   rclcpp::Publisher<skyhunter_msgs::msg::SwarmHeartbeat>::SharedPtr heartbeat_pub_;
//   rclcpp::Publisher<skyhunter_msgs::msg::ElectionVote>::SharedPtr vote_pub_;
//   rclcpp::Publisher<std_msgs::msg::Int8>::SharedPtr state_pub_;
//   rclcpp::Subscription<skyhunter_msgs::msg::SwarmHeartbeat>::SharedPtr heartbeat_sub_;
//   rclcpp::Subscription<skyhunter_msgs::msg::ElectionVote>::SharedPtr vote_sub_;
//   rclcpp::TimerBase::SharedPtr main_timer_;

//   rclcpp::Subscription<skyhunter_msgs::msg::LeaderState>::SharedPtr sub_local_leader_;
//     // Publisher for the global virtual leader state
//   rclcpp::Publisher<skyhunter_msgs::msg::LeaderState>::SharedPtr pub_virtual_leader_;

//   rclcpp::Time takeover_start_time;
// };

// int main(int argc, char * argv[]) { rclcpp::init(argc, argv); rclcpp::spin(std::make_shared<LeadershipManager>()); rclcpp::shutdown(); return 0; }



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