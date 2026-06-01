#include <algorithm>
#include <cmath>
#include <memory>

#include <rclcpp/rclcpp.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <geometry_msgs/msg/twist.hpp>

namespace
{

double yawFromQuat(const geometry_msgs::msg::Quaternion & q)
{
const double siny_cosp = 2.0 * (q.w * q.z + q.x * q.y);
const double cosy_cosp = 1.0 - 2.0 * (q.y * q.y + q.z * q.z);
return std::atan2(siny_cosp, cosy_cosp);
}

double wrapPi(double a)
{
while (a > M_PI) {a -= 2.0 * M_PI;}
while (a <= -M_PI) {a += 2.0 * M_PI;}
return a;
}

}  // namespace

class FollowerPursuitNode : public rclcpp::Node
{
public:
FollowerPursuitNode()
: rclcpp::Node("follower_pursuit_node")
{
    robot_id_ = declare_parameter<int>("robot_id", 2);

    // Formation offset behind the leader. Defaults mirror the sim spawn
    // formula in swarm_sim.launch.py so followers hold roughly where they
    // start. Row/side derived from robot_id; magnitudes are tunable.
    const double offset_back = declare_parameter<double>("offset_back_m", -3.0);
    const double offset_side = declare_parameter<double>("offset_side_m", 2.5);
    const int row = robot_id_ / 2;
    const int side = (robot_id_ % 2 == 0) ? 1 : -1;
    offset_back_ = offset_back * static_cast<double>(row);
    offset_side_ = offset_side * static_cast<double>(side);

    max_linear_ = declare_parameter<double>("max_linear_mps", 1.2);
    max_angular_ = declare_parameter<double>("max_angular_rps", 1.5);
    kp_linear_ = declare_parameter<double>("kp_linear", 0.8);
    kp_angular_ = declare_parameter<double>("kp_angular", 2.0);
    stop_distance_ = declare_parameter<double>("stop_distance_m", 0.4);
    leader_timeout_s_ = declare_parameter<double>("leader_timeout_s", 1.5);
    const std::string leader_topic =
    declare_parameter<std::string>("leader_odom_topic", "/odom");

    leader_sub_ = create_subscription<nav_msgs::msg::Odometry>(
    leader_topic, rclcpp::SensorDataQoS(),
    [this](nav_msgs::msg::Odometry::SharedPtr m) {
        leader_x_ = m->pose.pose.position.x;
        leader_y_ = m->pose.pose.position.y;
        leader_yaw_ = yawFromQuat(m->pose.pose.orientation);
        leader_stamp_ = now();
        have_leader_ = true;
    });

    // Relative — resolves to /robot_N/odom inside the per-robot namespace.
    own_sub_ = create_subscription<nav_msgs::msg::Odometry>(
    "odom", rclcpp::SensorDataQoS(),
    [this](nav_msgs::msg::Odometry::SharedPtr m) {
        own_x_ = m->pose.pose.position.x;
        own_y_ = m->pose.pose.position.y;
        own_yaw_ = yawFromQuat(m->pose.pose.orientation);
        have_own_ = true;
    });

    cmd_pub_ = create_publisher<geometry_msgs::msg::Twist>("cmd_vel", 10);

    timer_ = create_wall_timer(
    std::chrono::milliseconds(50),
    std::bind(&FollowerPursuitNode::onTick, this));

    RCLCPP_INFO(
    get_logger(),
    "FollowerPursuitNode UP: robot_id=%d offset=(back %.1fm, side %.1fm) "
    "leader_topic=%s", robot_id_, offset_back_, offset_side_,
    leader_topic.c_str());
}

private:
void onTick()
{
    geometry_msgs::msg::Twist cmd;  // zero by default

    if (!have_leader_ || !have_own_) {
    cmd_pub_->publish(cmd);  // hold still until both feeds are live
    return;
    }

    // Stop if the leader feed went stale (leader killed / comms loss).
    if ((now() - leader_stamp_).seconds() > leader_timeout_s_) {
    cmd_pub_->publish(cmd);
    return;
    }

    // Formation target in world frame: leader pose + rotated offset.
    const double cy = std::cos(leader_yaw_);
    const double sy = std::sin(leader_yaw_);
    const double tx = leader_x_ + (offset_back_ * cy - offset_side_ * sy);
    const double ty = leader_y_ + (offset_back_ * sy + offset_side_ * cy);

    const double dx = tx - own_x_;
    const double dy = ty - own_y_;
    const double dist = std::hypot(dx, dy);

    if (dist < stop_distance_) {
    // Arrived at slot — align heading to the leader and hold.
    const double yaw_err = wrapPi(leader_yaw_ - own_yaw_);
    cmd.angular.z = std::clamp(kp_angular_ * yaw_err, -max_angular_, max_angular_);
    cmd_pub_->publish(cmd);
    return;
    }

    const double desired_yaw = std::atan2(dy, dx);
    const double yaw_err = wrapPi(desired_yaw - own_yaw_);

    // Turn-in-place priority: scale linear down as heading error grows.
    const double turn_factor = std::max(0.0, 1.0 - std::fabs(yaw_err) / M_PI);
    double linear = std::clamp(kp_linear_ * dist, 0.0, max_linear_) * turn_factor;
    double angular = std::clamp(kp_angular_ * yaw_err, -max_angular_, max_angular_);

    cmd.linear.x = linear;
    cmd.angular.z = angular;
    cmd_pub_->publish(cmd);
}

int robot_id_{2};
double offset_back_{-3.0}, offset_side_{2.5};
double max_linear_{1.2}, max_angular_{1.5};
double kp_linear_{0.8}, kp_angular_{2.0};
double stop_distance_{0.4}, leader_timeout_s_{1.5};

double leader_x_{0.0}, leader_y_{0.0}, leader_yaw_{0.0};
double own_x_{0.0}, own_y_{0.0}, own_yaw_{0.0};
bool have_leader_{false}, have_own_{false};
rclcpp::Time leader_stamp_;

rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr leader_sub_;
rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr own_sub_;
rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr cmd_pub_;
rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char ** argv)
{
rclcpp::init(argc, argv);
try {
    rclcpp::spin(std::make_shared<FollowerPursuitNode>());
} catch (const std::exception & e) {
    RCLCPP_FATAL(
    rclcpp::get_logger("follower_pursuit_main"),
    "FollowerPursuitNode aborted: %s", e.what());
    rclcpp::shutdown();
    return 1;
}
rclcpp::shutdown();
return 0;
}
