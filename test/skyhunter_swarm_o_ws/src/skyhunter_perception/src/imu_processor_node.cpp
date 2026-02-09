#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/imu.hpp>
#include <tf2/LinearMath/Quaternion.h>
#include <tf2/LinearMath/Matrix3x3.h>
#include <iostream>
#include <iomanip>

class ImuProcessorNode : public rclcpp::Node
{
public:
    ImuProcessorNode() : Node("imu_processor_node")
    {
        // --- Subscriber ---
        // Use the sensor data QoS profile, which is Best Effort.
        // This is important for compatibility with Gazebo and real sensors.
        subscription_ = this->create_subscription<sensor_msgs::msg::Imu>(
            "/imu/data", rclcpp::SensorDataQoS(),
            std::bind(&ImuProcessorNode::imu_callback, this, std::placeholders::_1));

        RCLCPP_INFO(this->get_logger(), "C++ IMU Processor node started. Waiting for data...");
    }

private:
    void imu_callback(const sensor_msgs::msg::Imu::SharedPtr msg)
    {
        // 1. Get Orientation (Quaternion) from the message
        tf2::Quaternion q(
            msg->orientation.x,
            msg->orientation.y,
            msg->orientation.z,
            msg->orientation.w);

        // 2. Convert Quaternion to Euler Angles (Roll, Pitch, Yaw)
        // The tf2 library handles all the complex math for us.
        tf2::Matrix3x3 m(q);
        double roll, pitch, yaw;
        m.getRPY(roll, pitch, yaw); // angles are in radians

        // 3. Convert Radians to Degrees for readability
        double roll_deg = roll * 180.0 / M_PI;
        double pitch_deg = pitch * 180.0 / M_PI;
        double yaw_deg = yaw * 180.0 / M_PI;

        // 4. Get Angular Velocity
        double turn_rate = msg->angular_velocity.z; // rad/s

        // 5. Get Linear Acceleration
        double accel_x = msg->linear_acceleration.x; // m/s^2

        // Logic to determine a human-readable direction string
        std::string direction = "Unknown";
        if (yaw_deg > -22.5 && yaw_deg <= 22.5) direction = "East (+X)";
        else if (yaw_deg > 22.5 && yaw_deg <= 67.5) direction = "North-East";
        else if (yaw_deg > 67.5 && yaw_deg <= 112.5) direction = "North (+Y)";
        else if (yaw_deg > 112.5 && yaw_deg <= 157.5) direction = "North-West";
        else if (yaw_deg > 157.5 || yaw_deg <= -157.5) direction = "West (-X)";
        else if (yaw_deg > -157.5 && yaw_deg <= -112.5) direction = "South-West";
        else if (yaw_deg > -112.5 && yaw_deg <= -67.5) direction = "South (-Y)";
        else if (yaw_deg > -67.5 && yaw_deg <= -22.5) direction = "South-East";

        // Print Output to the console
        // Using std::cout for cleaner, unbuffered output in this simple case
        std::cout << "--- IMU STATUS ---" << std::endl;
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "Heading (Yaw): " << yaw_deg << "° [" << direction << "]" << std::endl;
        std::cout << "Pitch (Tilt):  " << pitch_deg << "°" << std::endl;
        std::cout << "Turning Speed: " << std::setprecision(3) << turn_rate << " rad/s" << std::endl;
        std::cout << "Acceleration:  " << std::setprecision(3) << accel_x << " m/s^2" << std::endl;
        std::cout << "------------------" << std::endl;
    }

    rclcpp::Subscription<sensor_msgs::msg::Imu>::SharedPtr subscription_;
};

int main(int argc, char *argv[])
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<ImuProcessorNode>());
    rclcpp::shutdown();
    return 0;
}