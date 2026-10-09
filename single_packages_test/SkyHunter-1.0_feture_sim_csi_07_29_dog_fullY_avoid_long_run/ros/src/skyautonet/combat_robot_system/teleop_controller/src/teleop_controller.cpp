#include <cmath>
#include <algorithm>
#include "teleop_controller.hpp"

namespace teleop_controller {
RobotController::RobotController(const rclcpp::NodeOptions &options)
    : Node("teleop_controller", options) {
  init_ros();
  init_serial();
}

RobotController::~RobotController() {
  // Chassis MCU latches the last commanded wheel speed — explicitly write
  // 0 to both wheel registers before closing modbus so the motors don't
  // keep coasting at the last commanded RPM after the node exits.
  if (modbus_) {
    RCLCPP_INFO(get_logger(), "RobotController shutdown: stopping wheels via modbus");
    modbus_write_register(modbus_, 0x0C, 0);
    modbus_write_register(modbus_, 0x0D, 0);
    // Duplicate writes in case of a dropped frame on the RS-485 bus.
    modbus_write_register(modbus_, 0x0C, 0);
    modbus_write_register(modbus_, 0x0D, 0);
    modbus_close(modbus_);
    modbus_free(modbus_);
    modbus_ = nullptr;
  }
}

void RobotController::init_ros() {
  wheel_base_ = declare_parameter<double>("wheel_base", 0.4);                     // meters
  max_linear_velocity_ = declare_parameter<double>("max_linear_velocity", 2.22);  // m/s
  max_angular_velocity_ = declare_parameter<double>("max_angular_velocity", 6);  // rad/s  
  device_path_ = declare_parameter<std::string>("device_path", "/dev/ttyUSB0");  // Default to real serial port

  // Subscriber
  sub_cmd_vel_ = this->create_subscription<DriveCommand>(
      "/drive_command", rclcpp::QoS(1), std::bind(&RobotController::on_cmd_vel, this, _1));

  // Subscribe to operation state
  sub_operation_state_ = this->create_subscription<OperationState>(
      "/operation_state", rclcpp::QoS(10), std::bind(&RobotController::on_operation_state, this, _1));

  timer_ = rclcpp::create_timer(this, get_clock(), rclcpp::Rate(20.0).period(),
                                std::bind(&RobotController::on_timer, this));
}

void RobotController::on_timer() {
  // read register uint16 for feedback
  uint16_t left_wheel_rpm_raw;
  uint16_t right_wheel_rpm_raw;

  int rc_left = modbus_read_registers(modbus_, 0x36, 1, &left_wheel_rpm_raw);
  int rc_right = modbus_read_registers(modbus_, 0x37, 1, &right_wheel_rpm_raw);

  if (rc_left == -1 || rc_right == -1) {
    //RCLCPP_ERROR(get_logger(), "Failed to read registers from modbus: %s", modbus_strerror(errno));
    return;
  }
  int16_t left_rpm = static_cast<int16_t>(left_wheel_rpm_raw);
  int16_t right_rpm = static_cast<int16_t>(right_wheel_rpm_raw);

  const double MAX_RPM = 3000.0;
  const double MAX_SPEED_MPS = 2.222;  // 8km/h
  const double WHEEL_BASE = 0.4;       // meters

  auto rpm_to_mps = [MAX_RPM, MAX_SPEED_MPS](int16_t rpm) {
    return (rpm / MAX_RPM) * MAX_SPEED_MPS;
  };

  double left_mps = rpm_to_mps(left_rpm);
  double right_mps = rpm_to_mps(right_rpm);

  double linear_vel = (left_mps + right_mps) / 2.0;
  double angular_vel = (right_mps - left_mps) / WHEEL_BASE;

  // Avoid division by zero
  double steering_angle = 0.0;
  if (std::abs(linear_vel) > 1e-1) {  // 더 큰 임계값으로 변경
    steering_angle = std::atan2(angular_vel * WHEEL_BASE, linear_vel);
    // Clamp to [-MAX_STEERING_ANGLE, MAX_STEERING_ANGLE]
    constexpr double MAX_STEERING_ANGLE_RAD = M_PI / 4;  // ±45° != angual velocity
    steering_angle = std::clamp(steering_angle, -MAX_STEERING_ANGLE_RAD, MAX_STEERING_ANGLE_RAD);
  }
  // double steering_angle_deg = steering_angle * (180.0 / M_PI);

  RCLCPP_INFO(get_logger(), "[READ] Linear Velocity: %f [km/h], Angular Velocity: %f [rad/s]", linear_vel*3.6, angular_vel);
}

void RobotController::on_operation_state(const OperationState::SharedPtr msg) {
  // Update local copy of operation state
  current_operation_state_ = msg->state;
}

void RobotController::init_serial() {
  modbus_ = modbus_new_rtu(device_path_.c_str(), 38400, 'N', 8, 1);  // Slave ID
  if (!modbus_) {
        std::cerr << "[Error] Failed to create Modbus context: " << modbus_strerror(errno) << std::endl;
        return;
    }
  modbus_set_slave(modbus_, 0x01);
  modbus_connect(modbus_);
  modbus_write_register(modbus_, 0x50, 0x01);  // set speed control mode
}


double RobotController::speedToRegisterValue(double speed_mps) {
  // Assuming MAX_16BIT_VALUE is 3000
  const int16_t MAX_16BIT_VALUE = 3000;
  double scaled_16bit = (speed_mps / max_linear_velocity_) * MAX_16BIT_VALUE;
  return scaled_16bit;
}

void RobotController::on_cmd_vel(const DriveCommand::ConstSharedPtr &msg) {
  double linear_vel = msg->linear_velocity;
  double angular_vel = msg->angular_velocity;

  if (current_operation_state_ != OperationState::MOVE) { // If not in drive mode, ignore cmd_vel
    linear_vel = 0.0;
    angular_vel = 0.0;
  }
  
  // 제자리 회전 시 허용 범위
  if (std::abs(linear_vel) < 1e-3) {
    angular_vel = std::clamp(angular_vel, -6.0, 6.0);
  } //방향 전환 하지 않을 때 허용 범위
  else if(std::abs(angular_vel) < 1e-3){
    linear_vel =  std::clamp(linear_vel, -2.22, 2.22); 
  } else { // 직진 중 방향 전환 허용 범위
    linear_vel =  std::clamp(linear_vel, -1.5, 1.5); 
    angular_vel = std::clamp(angular_vel, -4.5, 4.5);}

  // Differential drive kinematics
  double target_right_wheel_speed = speedToRegisterValue((linear_vel + (angular_vel * wheel_base_ / 2.0)));
  double target_left_wheel_speed = speedToRegisterValue((linear_vel - (angular_vel * wheel_base_ / 2.0)));

  target_right_wheel_speed = std::clamp(target_right_wheel_speed, -MAX_RPM, MAX_RPM);
  target_left_wheel_speed = std::clamp(target_left_wheel_speed, -MAX_RPM, MAX_RPM);

  int16_t target_left_wheel_speed_scaled = static_cast<int16_t>(std::round(target_left_wheel_speed));
  int16_t target_right_wheel_speed_scaled = static_cast<int16_t>(std::round(target_right_wheel_speed));

  //RCLCPP_INFO(get_logger(), "[Write] Linear Velocity: %.2f [km/h], Angular Velocity: %.2f [rad/s]", linear_vel*3.6, angular_vel);
  //RCLCPP_INFO(this->get_logger(), "[WRITE] Right : %d , Left : %d", (int16_t)target_right_wheel_speed_scaled, (int16_t)target_left_wheel_speed_scaled);

  modbus_write_register(modbus_, 0x0C, target_left_wheel_speed_scaled);
  modbus_write_register(modbus_, 0x0D, target_right_wheel_speed_scaled);
}

}  // namespace teleop_controller
#include <rclcpp_components/register_node_macro.hpp>
RCLCPP_COMPONENTS_REGISTER_NODE(teleop_controller::RobotController)
