#include "swarm_coordinator/swarm_coordinator.hpp"

namespace swarm_coordinator {

SwarmCoordinator::SwarmCoordinator()
    : SwarmCoordinator(rclcpp::NodeOptions())
{}

SwarmCoordinator::SwarmCoordinator(const rclcpp::NodeOptions& options)
    : rclcpp::Node("swarm_coordinator", options)
{
    status_sub_ = create_subscription<RobotStatus>(
        "/swarm/robot_status", 10,
        std::bind(&SwarmCoordinator::onRobotStatus, this,
                  std::placeholders::_1));
    RCLCPP_INFO(get_logger(),
        "SwarmCoordinator started: leader=%u hub=%u deputy=%u max=%u min=%u",
        LEADER_ROBOT_ID, HUB_ROBOT_ID, DEPUTY_ROBOT_ID,
        MAX_ROBOTS, MIN_ROBOTS);
}

void SwarmCoordinator::onRobotStatus(RobotStatus::SharedPtr msg) {
    if (msg == nullptr) return;
    // PHASE 4: forward Hub UGV status (robot_id == HUB_ROBOT_ID) to
    // the HubHealthMonitor; other robots are observed by
    // san_role_management's BatteryMonitor.
    if (msg->robot_id == HUB_ROBOT_ID) {
        hub_health_.update(*msg, nowMs());
    }
}

uint64_t SwarmCoordinator::nowMs() const {
    return static_cast<uint64_t>(now().nanoseconds() / 1'000'000ll);
}

}  // namespace swarm_coordinator
