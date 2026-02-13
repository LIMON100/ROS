#include <rclcpp/rclcpp.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <cmath>

class GroundTruthTracker : public rclcpp::Node {
public:
    GroundTruthTracker() : Node("location_tracker") {
        auto qos = rclcpp::SensorDataQoS();

        sub_l_ = this->create_subscription<nav_msgs::msg::Odometry>(
            "/odom", qos, [this](const nav_msgs::msg::Odometry::SharedPtr msg) {
                lx = msg->pose.pose.position.x; ly = msg->pose.pose.position.y;
                l_received = true;
            });

        sub_f_ = this->create_subscription<nav_msgs::msg::Odometry>(
            "/robot_02/odom", qos, [this](const nav_msgs::msg::Odometry::SharedPtr msg) {
                fx = msg->pose.pose.position.x; fy = msg->pose.pose.position.y;
                f_received = true;
            });

        // This timer runs even if no data is received
        timer_ = this->create_wall_timer(std::chrono::seconds(1), [this]() {
            if (!l_received || !f_received) {
                RCLCPP_INFO(this->get_logger(), "WAITING FOR DATA... (Leader: %s | Follower: %s)", 
                    l_received ? "OK" : "MISSING", f_received ? "OK" : "MISSING");
                return;
            }

            double dist = std::hypot(lx - fx, ly - fy);
            printf("\033[2J\033[1;1H"); 
            printf("======= TRACKING MONITOR =======\n");
            printf("LEADER   : X: %6.2f | Y: %6.2f\n", lx, ly);
            printf("FOLLOWER : X: %6.2f | Y: %6.2f\n", fx, fy);
            printf("--------------------------------\n");
            printf("DISTANCE : %6.2f meters\n", dist);
            printf("================================\n");
        });
    }
private:
    double lx=0, ly=0, fx=0, fy=0;
    bool l_received=false, f_received=false;
    rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr sub_l_, sub_f_;
    rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char** argv) {
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<GroundTruthTracker>());
    rclcpp::shutdown();
    return 0;
}