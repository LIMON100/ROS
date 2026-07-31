// generated from rosidl_generator_cpp/resource/idl__builder.hpp.em
// with input from combat_robot_msgs:msg/IMUState.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/imu_state.hpp"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__IMU_STATE__BUILDER_HPP_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__IMU_STATE__BUILDER_HPP_

#include <algorithm>
#include <utility>

#include "combat_robot_msgs/msg/detail/imu_state__struct.hpp"
#include "rosidl_runtime_cpp/message_initialization.hpp"


namespace combat_robot_msgs
{

namespace msg
{

namespace builder
{

class Init_IMUState_device_id
{
public:
  explicit Init_IMUState_device_id(::combat_robot_msgs::msg::IMUState & msg)
  : msg_(msg)
  {}
  ::combat_robot_msgs::msg::IMUState device_id(::combat_robot_msgs::msg::IMUState::_device_id_type arg)
  {
    msg_.device_id = std::move(arg);
    return std::move(msg_);
  }

private:
  ::combat_robot_msgs::msg::IMUState msg_;
};

class Init_IMUState_is_connected
{
public:
  explicit Init_IMUState_is_connected(::combat_robot_msgs::msg::IMUState & msg)
  : msg_(msg)
  {}
  Init_IMUState_device_id is_connected(::combat_robot_msgs::msg::IMUState::_is_connected_type arg)
  {
    msg_.is_connected = std::move(arg);
    return Init_IMUState_device_id(msg_);
  }

private:
  ::combat_robot_msgs::msg::IMUState msg_;
};

class Init_IMUState_angle
{
public:
  explicit Init_IMUState_angle(::combat_robot_msgs::msg::IMUState & msg)
  : msg_(msg)
  {}
  Init_IMUState_is_connected angle(::combat_robot_msgs::msg::IMUState::_angle_type arg)
  {
    msg_.angle = std::move(arg);
    return Init_IMUState_is_connected(msg_);
  }

private:
  ::combat_robot_msgs::msg::IMUState msg_;
};

class Init_IMUState_header
{
public:
  Init_IMUState_header()
  : msg_(::rosidl_runtime_cpp::MessageInitialization::SKIP)
  {}
  Init_IMUState_angle header(::combat_robot_msgs::msg::IMUState::_header_type arg)
  {
    msg_.header = std::move(arg);
    return Init_IMUState_angle(msg_);
  }

private:
  ::combat_robot_msgs::msg::IMUState msg_;
};

}  // namespace builder

}  // namespace msg

template<typename MessageType>
auto build();

template<>
inline
auto build<::combat_robot_msgs::msg::IMUState>()
{
  return combat_robot_msgs::msg::builder::Init_IMUState_header();
}

}  // namespace combat_robot_msgs

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__IMU_STATE__BUILDER_HPP_
