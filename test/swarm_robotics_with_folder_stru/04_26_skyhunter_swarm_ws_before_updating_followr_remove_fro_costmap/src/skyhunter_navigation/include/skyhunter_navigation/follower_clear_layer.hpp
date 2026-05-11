#ifndef FOLLOWER_CLEAR_LAYER_HPP_
#define FOLLOWER_CLEAR_LAYER_HPP_

#include "nav2_costmap_2d/layer.hpp"
#include "nav2_costmap_2d/costmap_2d.hpp"
#include <rclcpp/rclcpp.hpp>
#include <geometry_msgs/msg/pose_array.hpp>
#include <mutex>

namespace skyhunter_navigation {

class FollowerClearLayer : public nav2_costmap_2d::Layer {
public:
  FollowerClearLayer();
  virtual void onInitialize();
  virtual void updateBounds(double robot_x, double robot_y, double robot_yaw,
                            double *min_x, double *min_y, double *max_x, double *max_y);
  virtual void updateCosts(nav2_costmap_2d::Costmap2D &master_grid,
                           int min_i, int min_j, int max_i, int max_j);
  virtual void reset() {}
  virtual bool isClearable() { return false; }

private:
  void swarm_callback(const geometry_msgs::msg::PoseArray::SharedPtr msg);

  rclcpp::Subscription<geometry_msgs::msg::PoseArray>::SharedPtr sub_swarm_;
  geometry_msgs::msg::PoseArray latest_swarm_;
  std::mutex swarm_mutex_;
  double clear_radius_;
};

} // namespace skyhunter_navigation
#endif