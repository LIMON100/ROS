// SAN v1.3 PHASE 1 - master cost-map composition + PNG packaging.
//
// Walks the 4 layers in spec order:
//   static (omitted in PHASE 1; future Hub-shared map)
//   -> obstacle (235/100 mm)
//   -> traversability (slope + ditch)
//   -> inflation
// then packs the resulting uint8 grid into a CostMapUpdate message.

#pragma once

#include <combat_robot_msgs/msg/cost_map_update.hpp>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "san_costmap/inflation_layer_v13.hpp"
#include "san_costmap/obstacle_layer_v13.hpp"
#include "san_costmap/traversability_layer.hpp"

namespace san_costmap {

class CostMapPublisher {
public:
    CostMapPublisher();

    void setGeometry(int width, int height, float resolution_m,
                      float origin_x = 0.0f, float origin_y = 0.0f);
    void setRobotId(const std::string& id) { robot_id_ = id; }

    ObstacleLayerV13&     obstacleLayer()      { return obstacle_; }
    TraversabilityLayer&  traversabilityLayer() { return traversability_; }
    InflationLayerV13&    inflationLayer()     { return inflation_; }

    // Composite master grid from the current per-layer state.
    void compose();

    const std::vector<uint8_t>& master() const { return master_; }
    int width()  const { return width_; }
    int height() const { return height_; }

    // Encode master into a CostMapUpdate. Sets sequence + counts;
    // header.stamp + producer wallclock filled in by the caller.
    combat_robot_msgs::msg::CostMapUpdate buildMessage(uint32_t seq) const;

private:
    int width_  = DEFAULT_GRID_CELLS;
    int height_ = DEFAULT_GRID_CELLS;
    float resolution_m_ = DEFAULT_RESOLUTION_M;
    float origin_x_ = 0.0f;
    float origin_y_ = 0.0f;
    std::string robot_id_ = "0";

    ObstacleLayerV13    obstacle_;
    TraversabilityLayer traversability_;
    InflationLayerV13   inflation_;
    std::vector<uint8_t> master_;
};

// Internal helper - PNG-encode an 8-bit grid. Exposed for tests.
std::vector<uint8_t> encodePng(const std::vector<uint8_t>& grid,
                                 int width, int height);

}  // namespace san_costmap
