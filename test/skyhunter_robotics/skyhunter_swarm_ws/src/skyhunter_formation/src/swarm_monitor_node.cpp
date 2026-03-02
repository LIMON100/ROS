#include <vector>
#include <string>
#include <map>
#include <chrono> 
#include "rclcpp/rclcpp.hpp"
#include "nav_msgs/msg/odometry.hpp"
#include "geometry_msgs/msg/pose_array.hpp"

using namespace std::chrono_literals;

class SwarmMonitorNode : public rclcpp::Node {
public:
  // SwarmMonitorNode() : Node("swarm_monitor_node") {
  //   // We track SH_01 (Global) and SH_02 to SH_07
  //   // Using empty string for SH_01 because its topics are global (/odom)
  //   std::vector<std::string> robot_namespaces = {"", "SH_02", "SH_03", "SH_04", "SH_05", "SH_06", "SH_07"};
    
  //   // Create a QoS that discards old data immediately
  //   auto qos = rclcpp::QoS(rclcpp::KeepLast(1)).best_effort();

  //   for (const auto & ns : robot_namespaces) {
  //       std::string topic = (ns.empty()) ? "/odom" : "/" + ns + "/odom";
        
  //       // LAMBDA TRICK: Static map inside lambda to track time per robot
  //       auto sub = this->create_subscription<nav_msgs::msg::Odometry>(
  //           topic, qos, 
  //           [this, ns](const nav_msgs::msg::Odometry::SharedPtr msg) {
  //               // STATIC VARIABLE to persist across callbacks
  //               static std::map<std::string, rclcpp::Time> last_update_map;
                
  //               auto now = this->get_clock()->now();
                
  //               // THROTTLE: Only process if 100ms (0.1s) has passed (10 Hz cap)
  //               if (last_update_map.find(ns) == last_update_map.end() || 
  //                  (now - last_update_map[ns]).seconds() > 0.1) {
                    
  //                   this->swarm_poses_[ns] = msg->pose.pose;
  //                   last_update_map[ns] = now;
  //               }
  //           });
  //       subscriptions_.push_back(sub);
  //   }

  //   swarm_pub_ = this->create_publisher<geometry_msgs::msg::PoseArray>("/swarm/poses", 10);
    
  //   // Timer to broadcast the array at 10Hz
  //   timer_ = this->create_wall_timer(100ms, std::bind(&SwarmMonitorNode::publish_swarm_state, this));
    
  //   RCLCPP_INFO(this->get_logger(), "Swarm Monitor Active. Listening for all 7 units.");
  // }
  SwarmMonitorNode() : Node("swarm_monitor_node") {
    // We track SH_01 (Global) and SH_02 to SH_07
    // Using empty string for SH_01 because its topics are global (/odom)
    std::vector<std::string> robot_namespaces = {"", "SH_02", "SH_03", "SH_04", "SH_05", "SH_06", "SH_07"};
    
    for (const auto & ns : robot_namespaces) {
        std::string topic = (ns.empty()) ? "/odom" : "/" + ns + "/odom_filtered";
        
        auto sub = this->create_subscription<nav_msgs::msg::Odometry>(
            topic, 10, [this, ns](const nav_msgs::msg::Odometry::SharedPtr msg) {
                this->swarm_poses_[ns] = msg->pose.pose;
            });
        subscriptions_.push_back(sub);
    }

    swarm_pub_ = this->create_publisher<geometry_msgs::msg::PoseArray>("/swarm/poses", 10);
    
    // Timer to broadcast the array at 10Hz
    timer_ = this->create_wall_timer(100ms, std::bind(&SwarmMonitorNode::publish_swarm_state, this));
    
    RCLCPP_INFO(this->get_logger(), "Swarm Monitor Active. Listening for all 7 units.");
  }

private:
  void publish_swarm_state() {
    geometry_msgs::msg::PoseArray msg;
    msg.header.stamp = this->get_clock()->now();
    msg.header.frame_id = "map";

    for (auto const& [ns, pose] : swarm_poses_) {
        msg.poses.push_back(pose);
    }
    swarm_pub_->publish(msg);
  }

  std::vector<rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr> subscriptions_;
  rclcpp::Publisher<geometry_msgs::msg::PoseArray>::SharedPtr swarm_pub_;
  rclcpp::TimerBase::SharedPtr timer_;
  std::map<std::string, geometry_msgs::msg::Pose> swarm_poses_;
};

int main(int argc, char * argv[]) {
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<SwarmMonitorNode>());
  rclcpp::shutdown();
  return 0;
}