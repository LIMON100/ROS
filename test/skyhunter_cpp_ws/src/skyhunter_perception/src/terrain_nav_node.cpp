#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <geometry_msgs/msg/twist.hpp>
#include <tf2_ros/transform_listener.h>
#include <tf2_ros/buffer.h>
#include <tf2_sensor_msgs/tf2_sensor_msgs.hpp>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/filters/voxel_grid.h>
#include <pcl/filters/statistical_outlier_removal.h>
#include <pcl_conversions/pcl_conversions.h>
#include <grid_map_core/grid_map_core.hpp>
#include <grid_map_ros/grid_map_ros.hpp>
#include <Eigen/Dense>
#include <deque>
#include <limits>
#include <cmath>
#include <algorithm>

using PointT = pcl::PointXYZ;
// extend to PointXYZI if intensity available
static inline double clamp(double x, double a, double b) { return std::max(a, std::min(b, x)); }

struct CellKF {
    float h = std::numeric_limits<float>::quiet_NaN();
    float var = 0.25f; // initial variance (m^2)
    bool valid = false;
};

class TerrainNavNode : public rclcpp::Node {
public:
    TerrainNavNode() : Node("terrain_nav_node"),
                       tf_buffer_(this->get_clock()), tf_listener_(tf_buffer_) {
        // --- Parameters (tune) ---
        base_frame_ = this->declare_parameter<std::string>("base_frame", "base_link");
        cloud_frame_ = this->declare_parameter<std::string>("cloud_frame", "rslidar"); // incoming frame id
        cloud_topic_ = this->declare_parameter<std::string>("cloud_topic", "/rslidar_points");
        odom_topic_ = this->declare_parameter<std::string>("odom_topic", "/odom");
        cmd_topic_ = this->declare_parameter<std::string>("cmd_topic", "/cmd_vel");
        map_length_ = this->declare_parameter<double>("map_length", 10.0);
        map_res_ = this->declare_parameter<double>("map_resolution", 0.05);
        
        // ROI in base_link after transform
        roi_x_min_ = this->declare_parameter<double>("roi_x_min", 0.20);
        roi_x_max_ = this->declare_parameter<double>("roi_x_max", 6.00);
        roi_y_abs_ = this->declare_parameter<double>("roi_y_abs", 3.00);
        roi_z_min_ = this->declare_parameter<double>("roi_z_min", -1.00);
        roi_z_max_ = this->declare_parameter<double>("roi_z_max", 1.50);
        
        voxel_leaf_ = this->declare_parameter<double>("voxel_leaf", 0.05);
        sor_meanK_ = this->declare_parameter<int>("sor_meanK", 20);
        sor_std_ = this->declare_parameter<double>("sor_stddev", 1.2);
        
        // Elevation fusion
        meas_sigma_ = this->declare_parameter<double>("meas_sigma", 0.03); // meters
        max_cell_h_ = this->declare_parameter<double>("max_cell_height", 2.0);
        
        // Traversability thresholds
        slow_slope_deg_ = this->declare_parameter<double>("slow_slope_deg", 25.0);
        stop_slope_deg_ = this->declare_parameter<double>("stop_slope_deg", 30.0);
        rough_r_thr_ = this->declare_parameter<double>("rough_r_thr", 0.08); // tune
        
        // Look-ahead braking model
        t_react_ = this->declare_parameter<double>("t_reaction", 0.2);
        mu_ = this->declare_parameter<double>("mu", 0.35);
        safety_ = this->declare_parameter<double>("safety_factor", 1.5);
        L_min_ = this->declare_parameter<double>("L_min", 1.0);
        L_max_ = this->declare_parameter<double>("L_max", 5.0);
        L_margin_ = this->declare_parameter<double>("L_margin", 0.5);
        
        // DWA params
        v_max_ = this->declare_parameter<double>("v_max", 1.94); // 7 km/h
        w_max_ = this->declare_parameter<double>("w_max", 1.2); // rad/s tune
        v_acc_ = this->declare_parameter<double>("v_acc", 1.0);
        w_acc_ = this->declare_parameter<double>("w_acc", 2.0);
        dt_ = this->declare_parameter<double>("dt", 0.1);
        T_pred_ = this->declare_parameter<double>("T_pred", 2.0);
        n_v_ = this->declare_parameter<int>("n_v", 6);
        n_w_ = this->declare_parameter<int>("n_w", 11);
        lethal_cost_ = this->declare_parameter<double>("lethal_cost", 0.95);
        v_no_stop_min_ = this->declare_parameter<double>("v_no_stop_min", 0.2); // enforce no-stop when possible
        
        // Negative obstacle detection
        neg_gap_len_ = this->declare_parameter<double>("neg_gap_len", 0.4);
        neg_risk_thr_ = this->declare_parameter<double>("neg_risk_thr", 0.8);
        
        // Weights
        w_terrain_ = this->declare_parameter<double>("w_terrain", 1.0);
        w_heading_ = this->declare_parameter<double>("w_heading", 0.2);
        w_speed_ = this->declare_parameter<double>("w_speed", 0.1);
        
        // --- Grid map init ---
        grid_map_.setFrameId(base_frame_);
        grid_map_.setGeometry(grid_map::Length(map_length_, map_length_), map_res_);
        grid_map_.add("elevation", std::numeric_limits<float>::quiet_NaN());
        grid_map_.add("variance", 0.25f);
        grid_map_.add("slope_deg", std::numeric_limits<float>::quiet_NaN());
        grid_map_.add("roughness", std::numeric_limits<float>::quiet_NaN());
        grid_map_.add("traversability", 0.0f);
        grid_map_.add("neg_obs_risk", 0.0f);
        
        kf_.resize(grid_map_.getSize()(0) * grid_map_.getSize()(1));
        
        cloud_sub_ = this->create_subscription<sensor_msgs::msg::PointCloud2>(
            cloud_topic_, rclcpp::SensorDataQoS(),
            std::bind(&TerrainNavNode::onCloud, this, std::placeholders::_1));
            
        odom_sub_ = this->create_subscription<nav_msgs::msg::Odometry>(
            odom_topic_, 20, std::bind(&TerrainNavNode::onOdom, this, std::placeholders::_1));
            
        cmd_pub_ = this->create_publisher<geometry_msgs::msg::Twist>(cmd_topic_, 10);
        
        map_pub_ = this->create_publisher<grid_map_msgs::msg::GridMap>("/terrain_grid_map", 2);
        
        control_timer_ = this->create_wall_timer(
            std::chrono::milliseconds(50), std::bind(&TerrainNavNode::controlTick, this)); // 20 Hz
    }

private:
    // --- ROS ---
    rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr cloud_sub_;
    rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr odom_sub_;
    rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr cmd_pub_;
    rclcpp::Publisher<grid_map_msgs::msg::GridMap>::SharedPtr map_pub_;
    rclcpp::TimerBase::SharedPtr control_timer_;
    tf2_ros::Buffer tf_buffer_;
    tf2_ros::TransformListener tf_listener_;
    
    // --- Parameters ---
    std::string base_frame_, cloud_frame_, cloud_topic_, odom_topic_, cmd_topic_;
    double map_length_{10.0}, map_res_{0.05};
    double roi_x_min_, roi_x_max_, roi_y_abs_, roi_z_min_, roi_z_max_;
    double voxel_leaf_;
    int sor_meanK_;
    double sor_std_;
    double meas_sigma_, max_cell_h_;
    double slow_slope_deg_, stop_slope_deg_, rough_r_thr_;
    double t_react_, mu_, safety_, L_min_, L_max_, L_margin_;
    double v_max_, w_max_, v_acc_, w_acc_, dt_, T_pred_;
    int n_v_, n_w_;
    double lethal_cost_, v_no_stop_min_;
    double neg_gap_len_, neg_risk_thr_;
    double w_terrain_, w_heading_, w_speed_;
    
    // --- State ---
    nav_msgs::msg::Odometry last_odom_;
    bool have_odom_{false};
    grid_map::GridMap grid_map_;
    std::vector<CellKF> kf_;
    
    // --- Helpers ---
    inline int idxOf(const grid_map::Index& index) const {
        return index(0) * grid_map_.getSize()(1) + index(1);
    }
    
    void onOdom(const nav_msgs::msg::Odometry::SharedPtr msg) {
        last_odom_ = *msg;
        have_odom_ = true;
    }
    
    void onCloud(const sensor_msgs::msg::PointCloud2::SharedPtr msg) {
        // Transform to base_link
        sensor_msgs::msg::PointCloud2 cloud_bl;
        try {
            auto tf = tf_buffer_.lookupTransform(base_frame_, msg->header.frame_id, msg->header.stamp);
            tf2::doTransform(*msg, cloud_bl, tf);
        } catch (const std::exception& e) {
            RCLCPP_WARN_THROTTLE(get_logger(), get_clock(), 2000, "TF transform failed: %s", e.what());
            return;
        }
        
        pcl::PointCloud<PointT>::Ptr cloud(new pcl::PointCloud<PointT>());
        pcl::fromROSMsg(cloud_bl, *cloud);
        
        // ROI crop (manual filter for speed)
        pcl::PointCloud<PointT>::Ptr cropped(new pcl::PointCloud<PointT>());
        cropped->reserve(cloud->size());
        for (const auto& p : cloud->points) {
            if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z))
                continue;
            if (p.x < roi_x_min_ || p.x > roi_x_max_) continue;
            if (std::fabs(p.y) > roi_y_abs_) continue;
            if (p.z < roi_z_min_ || p.z > roi_z_max_) continue;
            cropped->push_back(p);
        }
        
        // Voxel
        pcl::VoxelGrid<PointT> vg;
        vg.setInputCloud(cropped);
        vg.setLeafSize((float)voxel_leaf_, (float)voxel_leaf_, (float)voxel_leaf_);
        pcl::PointCloud<PointT>::Ptr vox(new pcl::PointCloud<PointT>());
        vg.filter(*vox);
        
        // SOR
        pcl::StatisticalOutlierRemoval<PointT> sor;
        sor.setInputCloud(vox);
        sor.setMeanK(sor_meanK_);
        sor.setStddevMulThresh(sor_std_);
        pcl::PointCloud<PointT>::Ptr filt(new pcl::PointCloud<PointT>());
        sor.filter(*filt);
        
        // Update elevation map with filtered points
        updateElevation(*filt);
        
        // Recompute slope/roughness/traversability/neg risk
        computeTerrainLayers();
        
        // Publish grid map
        auto out = grid_map::GridMapRosConverter::toMessage(grid_map_);
        out->header.stamp = msg->header.stamp;
        out->header.frame_id = base_frame_;
        map_pub_->publish(*out);
    }
    
    void updateElevation(const pcl::PointCloud<PointT>& cloud) {
        const float R = (float)(meas_sigma_ * meas_sigma_);
        for (const auto& p : cloud.points) {
            if (p.z < -max_cell_h_ || p.z > max_cell_h_) continue;
            grid_map::Position pos(p.x, p.y);
            grid_map::Index index;
            if (!grid_map_.getIndex(pos, index)) continue;
            auto& cell = kf_[idxOf(index)];
            float z = (float)p.z;
            if (!cell.valid || !std::isfinite(cell.h)) {
                cell.h = z;
                cell.var = 0.25f;
                cell.valid = true;
            } else {
                float K = cell.var / (cell.var + R);
                cell.h = cell.h + K * (z - cell.h);
                cell.var = (1.0f - K) * cell.var;
            }
            grid_map_.at("elevation", index) = cell.h;
            grid_map_.at("variance", index) = cell.var;
        }
    }
    
    void computeTerrainLayers() {
        // For each cell: gather neighbor elevations -> PCA -> slope, roughness.
        const int rad_cells = (int)std::round(0.25 / map_res_); // 25cm neighborhood
        const Eigen::Vector3f z_axis(0.f, 0.f, 1.f);
        for (grid_map::GridMapIterator it(grid_map_); !it.isPastEnd(); ++it) {
            const grid_map::Index center = *it;
            float hc = grid_map_.at("elevation", center);
            if (!std::isfinite(hc)) {
                grid_map_.at("slope_deg", center) = std::numeric_limits<float>::quiet_NaN();
                grid_map_.at("roughness", center) = std::numeric_limits<float>::quiet_NaN();
                grid_map_.at("traversability", center) = 0.0f;
                continue;
            }
            
            std::vector<Eigen::Vector3f> pts;
            pts.reserve((2*rad_cells+1)*(2*rad_cells+1));
            for (int dx=-rad_cells; dx<=rad_cells; ++dx) {
                for (int dy=-rad_cells; dy<=rad_cells; ++dy) {
                    grid_map::Index ni(center(0)+dx, center(1)+dy);
                    if (ni(0)<0 || ni(1)<0 || ni(0)>=grid_map_.getSize()(0) || ni(1)>=grid_map_.getSize()(1)) continue;
                    float h = grid_map_.at("elevation", ni);
                    if (!std::isfinite(h)) continue;
                    grid_map::Position pxy;
                    grid_map_.getPosition(ni, pxy);
                    pts.emplace_back((float)pxy.x(), (float)pxy.y(), h);
                }
            }
            
            if (pts.size() < 8) {
                grid_map_.at("slope_deg", center) = std::numeric_limits<float>::quiet_NaN();
                grid_map_.at("roughness", center) = std::numeric_limits<float>::quiet_NaN();
                grid_map_.at("traversability", center) = 0.0f;
                continue;
            }
            
            // PCA
            Eigen::Vector3f mean(0,0,0);
            for (auto& p : pts) mean += p;
            mean /= (float)pts.size();
            Eigen::Matrix3f cov = Eigen::Matrix3f::Zero();
            for (auto& p : pts) {
                Eigen::Vector3f d = p - mean;
                cov += d * d.transpose();
            }
            cov /= (float)pts.size();
            Eigen::SelfAdjointEigenSolver<Eigen::Matrix3f> es(cov);
            Eigen::Vector3f evals = es.eigenvalues(); // ascending for self-adjoint
            Eigen::Matrix3f evecs = es.eigenvectors();
            float l1 = evals(2), l2 = evals(1), l3 = evals(0);
            Eigen::Vector3f normal = evecs.col(0); // smallest eigenvalue -> normal
            if (normal.dot(z_axis) < 0) normal = -normal;
            float slope = std::acos(clamp(normal.dot(z_axis), -1.0f, 1.0f)) * 180.0f / (float)M_PI;
            float rough = l3 / (l1 + l2 + l3 + 1e-6f);
            grid_map_.at("slope_deg", center) = slope;
            grid_map_.at("roughness", center) = rough;
            
            // Traversability (0..1 good)
            float trav = 1.0f;
            if (slope >= stop_slope_deg_) {
                // Steep: rock vs vegetation allowed
                if (rough < (float)rough_r_thr_) trav = 0.0f;
                else trav = 0.4f; // vegetation-ish: slow
            } else if (slope >= slow_slope_deg_) {
                trav = 0.6f;
            } else {
                trav = 1.0f;
            }
            grid_map_.at("traversability", center) = trav;
        }
        
        // Negative obstacle risk from map ray-casting
        computeNegativeObstacleRisk();
    }
    
    void computeNegativeObstacleRisk() {
        grid_map_["neg_obs_risk"].setZero(); // reset
        if (!have_odom_) return;
        double v = std::fabs(last_odom_.twist.twist.linear.x);
        double L = lookAheadDistance(v, 0.0); // downhill
        
        // Rays: -20..+20 deg
        for (double ang = -0.35; ang <= 0.35; ang += 0.05) {
            bool have_last_valid = false;
            grid_map::Index last_valid;
            double gap = 0.0;
            for (double d=0.3; d<=L; d+=map_res_) {
                double x = d * std::cos(ang);
                double y = d * std::sin(ang);
                grid_map::Position pos(x,y);
                grid_map::Index idx;
                if (!grid_map_.getIndex(pos, idx)) break;
                float h = grid_map_.at("elevation", idx);
                bool valid = std::isfinite(h);
                
                if (valid) {
                    have_last_valid = true;
                    last_valid = idx;
                    gap = 0.0;
                } else {
                    if (have_last_valid) {
                        gap += map_res_;
                        if (gap >= neg_gap_len_) {
                            // mark cliff edge at last_valid
                            float& risk = grid_map_.at("neg_obs_risk", last_valid);
                            risk = std::max(risk, 1.0f);
                            break;
                        }
                    }
                }
            }
        }
    }
    
    double lookAheadDistance(double v, double downhill_rad) const {
        // a_eff = mu*g*cos - g*sin (conservative downhill)
        const double g = 9.81;
        double a_eff = mu_ * g * std::cos(downhill_rad) - g * std::sin(downhill_rad);
        a_eff = std::max(0.5, a_eff); // prevent division blow-up
        double L = v * t_react_ + (v*v) / (2.0 * a_eff) * safety_ + L_margin_;
        return clamp(L, L_min_, L_max_);
    }
    
    float cellCostAt(double x, double y) {
        grid_map::Position pos(x,y);
        grid_map::Index idx;
        if (!grid_map_.getIndex(pos, idx)) return 1.0f; // outside -> lethal
        float trav = grid_map_.at("traversability", idx);
        float neg = grid_map_.at("neg_obs_risk", idx);
        if (neg > 0.5f) return 1.0f; // hard avoid
        
        // cost = 1 - traversability (simple)
        float cost = 1.0f - trav;
        return clamp(cost, 0.0f, 1.0f);
    }
    
    void controlTick() {
        if (!have_odom_) return;
        
        // DWA-like sampling around current cmd (odom twist)
        double v0 = last_odom_.twist.twist.linear.x;
        double w0 = last_odom_.twist.twist.angular.z;
        double v_min = std::max(0.0, v0 - v_acc_ * 0.05);
        double v_max = std::min(v_max_, v0 + v_acc_ * 0.05);
        double w_min = std::max(-w_max_, w0 - w_acc_ * 0.05);
        double w_max = std::min( w_max_, w0 + w_acc_ * 0.05);
        
        // Ensure exploration range
        v_min = 0.0;
        v_max = v_max_;
        w_min = -w_max_;
        w_max = w_max_;
        
        // Compute look-ahead
        double v_abs = std::fabs(v0);
        double L = lookAheadDistance(v_abs, 0.0);
        
        // Sample candidates
        double best_v = 0.0, best_w = 0.0;
        double best_J = 1e9;
        bool found_safe_nonstop = false;
        
        for (int i=0; i<n_v_; ++i) {
            double v = v_min + (v_max - v_min) * (double)i / (double)std::max(1, n_v_-1);
            for (int j=0; j<n_w_; ++j) {
                double w = w_min + (w_max - w_min) * (double)j / (double)std::max(1, n_w_-1);
                
                // Rollout
                double x=0, y=0, th=0;
                double terrain_sum = 0.0;
                double max_cost = 0.0;
                bool safe = true;
                
                for (double t=0; t<=T_pred_; t+=dt_) {
                    x += v * std::cos(th) * dt_;
                    y += v * std::sin(th) * dt_;
                    th += w * dt_;
                    
                    // Only evaluate within forward horizon
                    if (x < 0.0 || x > L) continue;
                    
                    float c = cellCostAt(x,y);
                    terrain_sum += c;
                    max_cost = std::max(max_cost, (double)c);
                    if (c >= lethal_cost_) {
                        safe = false;
                        break;
                    }
                }
                
                // Enforce "no-stop" if there exists any safe moving candidate
                if (safe && v >= v_no_stop_min_) found_safe_nonstop = true;
                
                // Score: terrain + speed preference
                double J = w_terrain_ * terrain_sum - w_speed_ * v;
                
                if (safe && J < best_J) {
                    best_J = J;
                    best_v = v;
                    best_w = w;
                }
            }
        }
        
        // If we found at least one safe moving option, forbid full stop.
        if (found_safe_nonstop && best_v < v_no_stop_min_) {
            best_v = v_no_stop_min_;
        }
        
        // Failsafe: if immediate neg obstacle risk ahead, stop
        // (You can add direct checking at x in [0.3..0.8], y~0)
        float c0 = cellCostAt(0.6, 0.0);
        if (c0 >= 1.0f) {
            best_v = 0.0;
            best_w = 0.0;
        }
        
        geometry_msgs::msg::Twist cmd;
        cmd.linear.x = best_v;
        cmd.angular.z = best_w;
        cmd_pub_->publish(cmd);
    }
};

int main(int argc, char* argv[]) {
    rclcpp::init(argc, argv);
    auto node = std::make_shared<TerrainNavNode>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}