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
    costmap_ = nullptr;
  }

  virtual void onInitialize()
  {
    current_ = true;
    
    auto node = node_.lock();
    declareParameter("enabled", rclcpp::ParameterValue(true));
    declareParameter("topic", rclcpp::ParameterValue("/elevation_map"));
    
    node->get_parameter(name_ + "." + "enabled", enabled_);
    std::string topic;
    node->get_parameter(name_ + "." + "topic", topic);

    // Subscribe to the GridMap output
    grid_map_sub_ = node->create_subscription<grid_map_msgs::msg::GridMap>(
      topic, rclcpp::SensorDataQoS(),
      std::bind(&TerrainLayer::gridMapCallback, this, std::placeholders::_1));
      
    RCLCPP_INFO(node->get_logger(), "TerrainLayer: Initialized. Listening on %s", topic.c_str());
  }

  // The updateBounds method tells the costmap which area needs to be redrawn
  virtual void updateBounds(
    double /*robot_x*/, double /*robot_y*/, double /*robot_yaw*/, // FIX: Commented out unused params
    double * min_x, double * min_y, double * max_x, double * max_y)
  {
    if (!enabled_ || !grid_map_received_) {
      return;
    }
    
    // We update the area covered by the GridMap
    grid_map::Position center = grid_map_.getPosition();
    double length = grid_map_.getLength().x();
    double width = grid_map_.getLength().y();

    *min_x = std::min(*min_x, center.x() - length / 2.0);
    *min_y = std::min(*min_y, center.y() - width / 2.0);
    *max_x = std::max(*max_x, center.x() + length / 2.0);
    *max_y = std::max(*max_y, center.y() + width / 2.0);
  }

  // The updateCosts method actually writes the lethal/free values into the master costmap
  virtual void updateCosts(
    nav2_costmap_2d::Costmap2D & master_grid,
    int min_i, int min_j, int max_i, int max_j)
  {
    if (!enabled_ || !grid_map_received_) {
      return;
    }

    // Lock to prevent data changing while we read
    std::lock_guard<std::mutex> lock(mutex_);

    // Iterate over the bounds provided by updateBounds
    for (int j = min_j; j < max_j; j++) {
      for (int i = min_i; i < max_i; i++) {
        
        // 1. Get World Coordinates of the current Costmap cell
        double wx, wy;
        master_grid.mapToWorld(i, j, wx, wy);
        
        // 2. Look up this coordinate in our GridMap
        grid_map::Position position(wx, wy);
        grid_map::Index index;
        
        // If the point is inside our GridMap region...
        if (grid_map_.isInside(position)) {
             grid_map_.getIndex(position, index);
             
             // 3. Check if we have valid traversability data here
             if (grid_map_.isValid(index, "traversability")) {
                
                // Get value: 0.0 (Safe) to 1.0 (Lethal)
                float score = grid_map_.at("traversability", index);
                
                unsigned char cost_val;
                
                // Map float 0.0-1.0 to Costmap 0-254
                if (score >= 1.0) {
                    cost_val = nav2_costmap_2d::LETHAL_OBSTACLE; // 254
                } else if (score >= 0.8) {
                    cost_val = 253; // Inscribed inflation
                } else if (score <= 0.0) {
                    cost_val = nav2_costmap_2d::FREE_SPACE; // 0
                } else {
                    // Linear mapping for roughness/slope
                    cost_val = static_cast<unsigned char>(score * 252.0);
                }

                // 4. Update the Master Costmap
                // Only overwrite if our new cost is higher (safer) or if it was unknown
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

  virtual bool isClearable() {
    return false; // Terrain data shouldn't be cleared by recovery behaviors like spin
  }

private:
  void gridMapCallback(const grid_map_msgs::msg::GridMap::SharedPtr msg)
  {
    std::lock_guard<std::mutex> lock(mutex_);
    grid_map::GridMapRosConverter::fromMessage(*msg, grid_map_);
    grid_map_received_ = true;
  }

  rclcpp::Subscription<grid_map_msgs::msg::GridMap>::SharedPtr grid_map_sub_;
  grid_map::GridMap grid_map_;
  std::mutex mutex_;
  bool grid_map_received_ = false;
};

} // namespace skyhunter_costmap

#include "pluginlib/class_list_macros.hpp"
PLUGINLIB_EXPORT_CLASS(skyhunter_costmap::TerrainLayer, nav2_costmap_2d::Layer)