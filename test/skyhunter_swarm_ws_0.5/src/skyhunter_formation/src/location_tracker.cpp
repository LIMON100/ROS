#include <rclcpp/rclcpp.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <cmath>

class GroundTruthTracker : public rclcpp::Node {
public:
    GroundTruthTracker() : Node("location_tracker") {
        auto qos = rclcpp::SensorDataQoS();

        // Leader Position (From AMCL/Odom - Global)
        sub_leader_ = this->create_subscription<nav_msgs::msg::Odometry>(
            "/odom", qos, [this](const nav_msgs::msg::Odometry::SharedPtr msg) {
                lx = msg->pose.pose.position.x;
                ly = msg->pose.pose.position.y;
            });

        // Follower Position (Direct from Gazebo Ground Truth)
        sub_follower_ = this->create_subscription<nav_msgs::msg::Odometry>(
            "/robot_02/ground_truth", qos, [this](const nav_msgs::msg::Odometry::SharedPtr msg) {
                double fx = msg->pose.pose.position.x;
                double fy = msg->pose.pose.position.y;
                double dist = std::hypot(lx - fx, ly - fy);

                static int count = 0;
                if (count++ % 5 == 0) {
                    printf("\033[2J\033[1;1H"); 
                    printf("======= PERFECT GROUND TRUTH MONITOR =======\n");
                    printf("LEADER (World)   : X:%6.2f | Y:%6.2f\n", lx, ly);
                    printf("FOLLOWER (World) : X:%6.2f | Y:%6.2f\n", fx, fy);
                    printf("--------------------------------------------\n");
                    printf("ACTUAL DISTANCE  : %6.2f meters\n", dist);
                    printf("============================================\n");
                }
            });
    }

private:
    double lx = 0, ly = 0;
    rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr sub_leader_, sub_follower_;
};

int main(int argc, char** argv) {
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<GroundTruthTracker>());
    rclcpp::shutdown();
    return 0;
}