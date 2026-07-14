#include "san_costmap/cost_map_publisher.hpp"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/core.hpp>

namespace san_costmap {

CostMapPublisher::CostMapPublisher()
    : master_(static_cast<std::size_t>(DEFAULT_GRID_CELLS)
                * DEFAULT_GRID_CELLS, COST_FREE)
{
    setGeometry(DEFAULT_GRID_CELLS, DEFAULT_GRID_CELLS,
                DEFAULT_RESOLUTION_M);
}

void CostMapPublisher::setGeometry(int width, int height,
                                    float resolution_m,
                                    float origin_x, float origin_y)
{
    width_ = width;
    height_ = height;
    resolution_m_ = resolution_m;
    origin_x_ = origin_x;
    origin_y_ = origin_y;
    master_.assign(static_cast<std::size_t>(width) * height, COST_FREE);
    obstacle_.setGeometry(width, height, resolution_m,
                          origin_x, origin_y);
    traversability_.setGeometry(width, height, resolution_m,
                                origin_x, origin_y);
    inflation_.setGeometry(width, height, resolution_m);
}

void CostMapPublisher::compose() {
    std::fill(master_.begin(), master_.end(), COST_FREE);

    const auto& obs = obstacle_.grid();
    const auto& tra = traversability_.grid();
    for (std::size_t i = 0; i < master_.size(); ++i) {
        uint8_t c = master_[i];
        if (obs[i] > c) c = obs[i];
        if (tra[i] > c) c = tra[i];
        master_[i] = c;
    }
    inflation_.inflate(master_);
}

std::vector<uint8_t> encodePng(const std::vector<uint8_t>& grid,
                                 int width, int height)
{
    cv::Mat img(height, width, CV_8UC1,
                 const_cast<uint8_t*>(grid.data()));
    std::vector<uint8_t> buf;
    cv::imencode(".png", img, buf);
    return buf;
}

combat_robot_msgs::msg::CostMapUpdate
CostMapPublisher::buildMessage(uint32_t seq) const
{
    combat_robot_msgs::msg::CostMapUpdate msg;
    msg.sequence = seq;
    msg.robot_id = robot_id_;
    msg.cost_grid_png = encodePng(master_, width_, height_);
    msg.origin.x = origin_x_;
    msg.origin.y = origin_y_;
    msg.origin.theta = 0.0;
    msg.resolution_m = resolution_m_;
    msg.width_cells = static_cast<uint32_t>(width_);
    msg.height_cells = static_cast<uint32_t>(height_);

    uint32_t lethal = 0;
    uint32_t inflated = 0;
    for (auto c : master_) {
        if (c == COST_LETHAL) ++lethal;
        else if (c >= COST_WARN_LOW && c < COST_LETHAL) ++inflated;
    }
    msg.lethal_count = lethal;
    msg.inflated_count = inflated;
    return msg;
}

}  // namespace san_costmap
