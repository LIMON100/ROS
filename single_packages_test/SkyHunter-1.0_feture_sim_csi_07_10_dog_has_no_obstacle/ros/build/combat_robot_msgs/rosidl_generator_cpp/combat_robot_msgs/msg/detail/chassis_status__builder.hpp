// generated from rosidl_generator_cpp/resource/idl__builder.hpp.em
// with input from combat_robot_msgs:msg/ChassisStatus.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/chassis_status.hpp"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__CHASSIS_STATUS__BUILDER_HPP_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__CHASSIS_STATUS__BUILDER_HPP_

#include <algorithm>
#include <utility>

#include "combat_robot_msgs/msg/detail/chassis_status__struct.hpp"
#include "rosidl_runtime_cpp/message_initialization.hpp"


namespace combat_robot_msgs
{

namespace msg
{

namespace builder
{

class Init_ChassisStatus_motor_temp_c
{
public:
  explicit Init_ChassisStatus_motor_temp_c(::combat_robot_msgs::msg::ChassisStatus & msg)
  : msg_(msg)
  {}
  ::combat_robot_msgs::msg::ChassisStatus motor_temp_c(::combat_robot_msgs::msg::ChassisStatus::_motor_temp_c_type arg)
  {
    msg_.motor_temp_c = std::move(arg);
    return std::move(msg_);
  }

private:
  ::combat_robot_msgs::msg::ChassisStatus msg_;
};

class Init_ChassisStatus_fault_flags
{
public:
  explicit Init_ChassisStatus_fault_flags(::combat_robot_msgs::msg::ChassisStatus & msg)
  : msg_(msg)
  {}
  Init_ChassisStatus_motor_temp_c fault_flags(::combat_robot_msgs::msg::ChassisStatus::_fault_flags_type arg)
  {
    msg_.fault_flags = std::move(arg);
    return Init_ChassisStatus_motor_temp_c(msg_);
  }

private:
  ::combat_robot_msgs::msg::ChassisStatus msg_;
};

class Init_ChassisStatus_angular_velocity_rps
{
public:
  explicit Init_ChassisStatus_angular_velocity_rps(::combat_robot_msgs::msg::ChassisStatus & msg)
  : msg_(msg)
  {}
  Init_ChassisStatus_fault_flags angular_velocity_rps(::combat_robot_msgs::msg::ChassisStatus::_angular_velocity_rps_type arg)
  {
    msg_.angular_velocity_rps = std::move(arg);
    return Init_ChassisStatus_fault_flags(msg_);
  }

private:
  ::combat_robot_msgs::msg::ChassisStatus msg_;
};

class Init_ChassisStatus_linear_velocity_mps
{
public:
  explicit Init_ChassisStatus_linear_velocity_mps(::combat_robot_msgs::msg::ChassisStatus & msg)
  : msg_(msg)
  {}
  Init_ChassisStatus_angular_velocity_rps linear_velocity_mps(::combat_robot_msgs::msg::ChassisStatus::_linear_velocity_mps_type arg)
  {
    msg_.linear_velocity_mps = std::move(arg);
    return Init_ChassisStatus_angular_velocity_rps(msg_);
  }

private:
  ::combat_robot_msgs::msg::ChassisStatus msg_;
};

class Init_ChassisStatus_battery_current_a
{
public:
  explicit Init_ChassisStatus_battery_current_a(::combat_robot_msgs::msg::ChassisStatus & msg)
  : msg_(msg)
  {}
  Init_ChassisStatus_linear_velocity_mps battery_current_a(::combat_robot_msgs::msg::ChassisStatus::_battery_current_a_type arg)
  {
    msg_.battery_current_a = std::move(arg);
    return Init_ChassisStatus_linear_velocity_mps(msg_);
  }

private:
  ::combat_robot_msgs::msg::ChassisStatus msg_;
};

class Init_ChassisStatus_battery_voltage_v
{
public:
  explicit Init_ChassisStatus_battery_voltage_v(::combat_robot_msgs::msg::ChassisStatus & msg)
  : msg_(msg)
  {}
  Init_ChassisStatus_battery_current_a battery_voltage_v(::combat_robot_msgs::msg::ChassisStatus::_battery_voltage_v_type arg)
  {
    msg_.battery_voltage_v = std::move(arg);
    return Init_ChassisStatus_battery_current_a(msg_);
  }

private:
  ::combat_robot_msgs::msg::ChassisStatus msg_;
};

class Init_ChassisStatus_battery_pct
{
public:
  explicit Init_ChassisStatus_battery_pct(::combat_robot_msgs::msg::ChassisStatus & msg)
  : msg_(msg)
  {}
  Init_ChassisStatus_battery_voltage_v battery_pct(::combat_robot_msgs::msg::ChassisStatus::_battery_pct_type arg)
  {
    msg_.battery_pct = std::move(arg);
    return Init_ChassisStatus_battery_voltage_v(msg_);
  }

private:
  ::combat_robot_msgs::msg::ChassisStatus msg_;
};

class Init_ChassisStatus_drive_state
{
public:
  explicit Init_ChassisStatus_drive_state(::combat_robot_msgs::msg::ChassisStatus & msg)
  : msg_(msg)
  {}
  Init_ChassisStatus_battery_pct drive_state(::combat_robot_msgs::msg::ChassisStatus::_drive_state_type arg)
  {
    msg_.drive_state = std::move(arg);
    return Init_ChassisStatus_battery_pct(msg_);
  }

private:
  ::combat_robot_msgs::msg::ChassisStatus msg_;
};

class Init_ChassisStatus_header
{
public:
  Init_ChassisStatus_header()
  : msg_(::rosidl_runtime_cpp::MessageInitialization::SKIP)
  {}
  Init_ChassisStatus_drive_state header(::combat_robot_msgs::msg::ChassisStatus::_header_type arg)
  {
    msg_.header = std::move(arg);
    return Init_ChassisStatus_drive_state(msg_);
  }

private:
  ::combat_robot_msgs::msg::ChassisStatus msg_;
};

}  // namespace builder

}  // namespace msg

template<typename MessageType>
auto build();

template<>
inline
auto build<::combat_robot_msgs::msg::ChassisStatus>()
{
  return combat_robot_msgs::msg::builder::Init_ChassisStatus_header();
}

}  // namespace combat_robot_msgs

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__CHASSIS_STATUS__BUILDER_HPP_
