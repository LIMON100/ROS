#include "skyhunter_navigation/follower_clear_layer.hpp"
#include "nav2_costmap_2d/costmap_math.hpp"
#include "pluginlib/class_list_macros.hpp"

// Register the plugin with ROS 2
PLUGINLIB_EXPORT_CLASS(skyhunter_navigation::FollowerClearLayer, nav2_costmap_2d::Layer)

namespace skyhunter_navigation {

FollowerClearLayer::FollowerClearLayer() {}

void FollowerClearLayer::onInitialize() {
  auto node = node_.lock();
  declareParameter("clear_radius", rclcpp::ParameterValue(1.5));
  node->get_parameter(name_ + "." + "clear_radius", clear_radius_);

  sub_swarm_ = node->create_subscription<geometry_msgs::msg::PoseArray>(
      "/swarm/poses", 10, std::bind(&FollowerClearLayer::swarm_callback, this, std::placeholders::_1));

  current_ = true;
}

void FollowerClearLayer::swarm_callback(const geometry_msgs::msg::PoseArray::SharedPtr msg) {
  std::lock_guard<std::mutex> lock(swarm_mutex_);
  latest_swarm_ = *msg;
}

void FollowerClearLayer::updateBounds(double robot_x, double robot_y, double robot_yaw,
                                      double *min_x, double *min_y, double *max_x, double *max_y) {
  std::lock_guard<std::mutex> lock(swarm_mutex_);
  if (latest_swarm_.poses.empty()) return;

  // Expand the bounds of the map update to include all followers
  for (const auto& pose : latest_swarm_.poses) {
    // Ignore the leader itself (robot is at robot_x, robot_y)
    double dist = std::hypot(pose.position.x - robot_x, pose.position.y - robot_y);
    if (dist < 1.0) continue;

    *min_x = std::min(*min_x, pose.position.x - clear_radius_);
    *min_y = std::min(*min_y, pose.position.y - clear_radius_);
    *max_x = std::max(*max_x, pose.position.x + clear_radius_);
    *max_y = std::max(*max_y, pose.position.y + clear_radius_);
  }
}

void FollowerClearLayer::updateCosts(nav2_costmap_2d::Costmap2D &master_grid,
                                     int min_i, int min_j, int max_i, int max_j) {
  std::lock_guard<std::mutex> lock(swarm_mutex_);
  if (latest_swarm_.poses.empty()) return;

  // For every follower...
  for (const auto& pose : latest_swarm_.poses) {
    // Forcefully erase a circle of pixels around the follower to FREE_SPACE (0)
    unsigned int mx, my;
    if (master_grid.worldToMap(pose.position.x, pose.position.y, mx, my)) {
        // Simple circle drawing algorithm
        int radius_cells = clear_radius_ / master_grid.getResolution();
        for (int dx = -radius_cells; dx <= radius_cells; dx++) {
            for (int dy = -radius_cells; dy <= radius_cells; dy++) {
                if (dx*dx + dy*dy <= radius_cells*radius_cells) {
                    unsigned int px = mx + dx;
                    unsigned int py = my + dy;
                    if (px >= min_i && px < max_i && py >= min_j && py < max_j) {
                        // THE NUCLEAR ERASER: Force pixel to 0 (Free Space)
                        master_grid.setCost(px, py, nav2_costmap_2d::FREE_SPACE);
                    }
                }
            }
        }
    }
  }
}

} // namespace skyhunter_navigation