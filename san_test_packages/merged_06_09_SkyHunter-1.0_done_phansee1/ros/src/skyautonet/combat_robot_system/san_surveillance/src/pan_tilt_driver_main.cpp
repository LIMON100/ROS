#include "san_surveillance/pan_tilt_driver_node.hpp"
 
int main(int argc, char ** argv)
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<san_surveillance::PanTiltDriverNode>());
    rclcpp::shutdown();
    return 0;
}
