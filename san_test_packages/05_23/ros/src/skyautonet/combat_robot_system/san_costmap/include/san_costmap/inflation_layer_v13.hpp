// SAN v1.3 PHASE 1 - inflation layer.
//
// Spreads lethal cells outward using an exponential decay so the
// planner avoids near-clearance corridors. Radius defaults to 1.0 m
// (sqrt(1.3² + 0.85²)/2 + 0.25 m safety) and the decay constant
// matches Nav2's `cost_scaling_factor` semantics.

#pragma once

#include <cstdint>
#include <vector>

#include "san_costmap/cost_constants.hpp"

namespace san_costmap {

class InflationLayerV13 {
public:
    InflationLayerV13(int width = DEFAULT_GRID_CELLS,
                       int height = DEFAULT_GRID_CELLS,
                       float resolution_m = DEFAULT_RESOLUTION_M);

    void setGeometry(int width, int height, float resolution_m);
    void setInflationRadiusM(float r) { inflation_radius_m_ = r; }
    void setCostScalingFactor(float k) { cost_scaling_factor_ = k; }

    // Apply inflation onto an existing master cost grid. Lethal cells
    // are propagated outward; non-lethal cells already at COST_WARN_*
    // are not raised below their existing level.
    void inflate(std::vector<uint8_t>& master) const;

    float inflationRadiusM() const { return inflation_radius_m_; }

private:
    int width_;
    int height_;
    float resolution_m_;
    float inflation_radius_m_  = 1.0f;
    float cost_scaling_factor_ = 3.0f;
};

}  // namespace san_costmap
