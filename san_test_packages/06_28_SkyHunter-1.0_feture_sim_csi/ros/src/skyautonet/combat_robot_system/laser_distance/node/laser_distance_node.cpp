#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/float64.hpp>
#include "laser_distance.hpp"

using namespace std::chrono_literals;

class DistancePublisher : public rclcpp::Node {
public:
    DistancePublisher()
    : Node("laser_distance_node")
    {
        // 파라미터 선언
        declare_parameter("port", "/dev/ttyUSB0");
        declare_parameter("baudrate", 115200);
        declare_parameter("frequency", 10.0); // Hz
        
        // 파라미터 가져오기
        std::string port = get_parameter("port").as_string();
        int baudrate = get_parameter("baudrate").as_int();
        double frequency = get_parameter("frequency").as_double();
        
        RCLCPP_INFO(this->get_logger(), "Using port: %s, baud: %d, freq: %.1f Hz", port.c_str(), baudrate, frequency);
        
        laser_ = LaserDistance(port.c_str(), baudrate);
        publisher_ = this->create_publisher<std_msgs::msg::Float64>("distance", 10);
        
        if (!laser_.openPort()) {
            RCLCPP_ERROR(this->get_logger(), "시리얼 포트 열기 실패: %s", port.c_str());
            rclcpp::shutdown();
            return;
        }

        // 주기를 frequency 파라미터에 맞춰 설정
        auto period = std::chrono::duration<double>(1.0 / frequency);
        timer_ = this->create_wall_timer(period, std::bind(&DistancePublisher::timer_callback, this));
    }
    ~DistancePublisher() { laser_.closePort(); }

private:
    void timer_callback() {
        double distance;
        uint8_t status;

        if (laser_.measure(distance, status))
        {
            auto msg = std_msgs::msg::Float64();
            if (status == 0x00) {
                msg.data = distance;                
                // 너무 잦은 로그 출력을 방지하려면 아래 줄 주석 처리 또는 RCLCPP_DEBUG 사용 권장
                // RCLCPP_INFO(this->get_logger(), "측정 거리: %.2f m", distance);
            }
            else {                
                msg.data = -1.0; // out of range
                RCLCPP_WARN(this->get_logger(), "센서 측정 범위 초과 또는 에러 (status: 0x%02X)", status);
            }
            publisher_->publish(msg);
        } else {           
            RCLCPP_ERROR(this->get_logger(), "센서 응답 없음 (Timeout or Error)");
        }
    }

    rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr publisher_;
    rclcpp::TimerBase::SharedPtr timer_;
    LaserDistance laser_;
};

int main(int argc, char* argv[]) {
    rclcpp::init(argc, argv);
    auto node = std::make_shared<DistancePublisher>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
