#include "nav2_costmap_2d/costmap_layer.hpp"
#include "nav2_costmap_2d/layer.hpp"
#include "nav2_costmap_2d/costmap_2d.hpp"
#include "rclcpp/rclcpp.hpp"
#include "grid_map_ros/grid_map_ros.hpp"
#include "grid_map_msgs/msg/grid_map.hpp"

namespace skyhunter_costmap
{

class TerrainLayer : public nav2_costmap_2d::CostmapLayer
{
public:
  TerrainLayer()
  {
    last_min_x_ = last_min_y_ = last_max_x_ = last_max_y_ = 0;
  }

  // --- MISSING FUNCTION ADDED HERE ---
  virtual bool isClearable() {
    return false; // Terrain data usually shouldn't be cleared by standard recovery behaviors
  }

  virtual void onInitialize()
  {
    current_ = true;
    
    auto node = node_.lock();
    grid_map_sub_ = node->create_subscription<grid_map_msgs::msg::GridMap>(
      "/elevation_map", rclcpp::QoS(1).best_effort(),
      std::bind(&TerrainLayer::gridMapCallback, this, std::placeholders::_1));
      
    RCLCPP_INFO(node->get_logger(), "TerrainLayer: Initialized.");
  }

  virtual void updateBounds(
    double robot_x, double robot_y, double /*robot_yaw*/, // Comment out unused param to fix warning
    double * min_x, double * min_y, double * max_x, double * max_y)
  {
    if (!enabled_ || !grid_map_received_) {
      return;
    }
    
    double dist = 10.0; 
    *min_x = std::min(*min_x, robot_x - dist);
    *min_y = std::min(*min_y, robot_y - dist);
    *max_x = std::max(*max_x, robot_x + dist);
    *max_y = std::max(*max_y, robot_y + dist);
  }

  virtual void updateCosts(
    nav2_costmap_2d::Costmap2D & master_grid,
    int min_i, int min_j, int max_i, int max_j)
  {
    if (!enabled_ || !grid_map_received_) {
      return;
    }

    for (int j = min_j; j < max_j; j++) {
      for (int i = min_i; i < max_i; i++) {
        
        double wx, wy;
        master_grid.mapToWorld(i, j, wx, wy);
        
        grid_map::Position position(wx, wy);
        grid_map::Index index;
        
        if (grid_map_.getIndex(position, index)) {
          if (grid_map_.isValid(index, "traversability")) {
            
            float score = grid_map_.at("traversability", index);
            
            unsigned char cost_val;
            
            if (score >= 1.0) {
                cost_val = nav2_costmap_2d::LETHAL_OBSTACLE;
            } else if (score <= 0.0) {
                cost_val = nav2_costmap_2d::FREE_SPACE;
            } else {
                cost_val = static_cast<unsigned char>(score * 252.0);
            }

            unsigned char old_cost = master_grid.getCost(i, j);
            if (old_cost == nav2_costmap_2d::NO_INFORMATION || cost_val > old_cost) {
                master_grid.setCost(i, j, cost_val);
            }
          }
        }
      }
    }
  }

  virtual void reset()
  {
    grid_map_received_ = false;
    current_ = false;
  }

private:
  void gridMapCallback(const grid_map_msgs::msg::GridMap::SharedPtr msg)
  {
    grid_map::GridMapRosConverter::fromMessage(*msg, grid_map_);
    grid_map_received_ = true;
  }

  rclcpp::Subscription<grid_map_msgs::msg::GridMap>::SharedPtr grid_map_sub_;
  grid_map::GridMap grid_map_;
  bool grid_map_received_ = false;
  double last_min_x_, last_min_y_, last_max_x_, last_max_y_;
};

} // namespace skyhunter_costmap

#include "pluginlib/class_list_macros.hpp"
PLUGINLIB_EXPORT_CLASS(skyhunter_costmap::TerrainLayer, nav2_costmap_2d::Layer)