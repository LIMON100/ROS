// generated from rosidl_generator_cpp/resource/idl__builder.hpp.em
// with input from combat_robot_msgs:msg/PanTiltState.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/pan_tilt_state.hpp"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__PAN_TILT_STATE__BUILDER_HPP_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__PAN_TILT_STATE__BUILDER_HPP_

#include <algorithm>
#include <utility>

#include "combat_robot_msgs/msg/detail/pan_tilt_state__struct.hpp"
#include "rosidl_runtime_cpp/message_initialization.hpp"


namespace combat_robot_msgs
{

namespace msg
{

namespace builder
{

class Init_PanTiltState_tilt_speed
{
public:
  explicit Init_PanTiltState_tilt_speed(::combat_robot_msgs::msg::PanTiltState & msg)
  : msg_(msg)
  {}
  ::combat_robot_msgs::msg::PanTiltState tilt_speed(::combat_robot_msgs::msg::PanTiltState::_tilt_speed_type arg)
  {
    msg_.tilt_speed = std::move(arg);
    return std::move(msg_);
  }

private:
  ::combat_robot_msgs::msg::PanTiltState msg_;
};

class Init_PanTiltState_pan_speed
{
public:
  explicit Init_PanTiltState_pan_speed(::combat_robot_msgs::msg::PanTiltState & msg)
  : msg_(msg)
  {}
  Init_PanTiltState_tilt_speed pan_speed(::combat_robot_msgs::msg::PanTiltState::_pan_speed_type arg)
  {
    msg_.pan_speed = std::move(arg);
    return Init_PanTiltState_tilt_speed(msg_);
  }

private:
  ::combat_robot_msgs::msg::PanTiltState msg_;
};

class Init_PanTiltState_vertical_angle
{
public:
  explicit Init_PanTiltState_vertical_angle(::combat_robot_msgs::msg::PanTiltState & msg)
  : msg_(msg)
  {}
  Init_PanTiltState_pan_speed vertical_angle(::combat_robot_msgs::msg::PanTiltState::_vertical_angle_type arg)
  {
    msg_.vertical_angle = std::move(arg);
    return Init_PanTiltState_pan_speed(msg_);
  }

private:
  ::combat_robot_msgs::msg::PanTiltState msg_;
};

class Init_PanTiltState_horizontal_angle
{
public:
  explicit Init_PanTiltState_horizontal_angle(::combat_robot_msgs::msg::PanTiltState & msg)
  : msg_(msg)
  {}
  Init_PanTiltState_vertical_angle horizontal_angle(::combat_robot_msgs::msg::PanTiltState::_horizontal_angle_type arg)
  {
    msg_.horizontal_angle = std::move(arg);
    return Init_PanTiltState_vertical_angle(msg_);
  }

private:
  ::combat_robot_msgs::msg::PanTiltState msg_;
};

class Init_PanTiltState_control_mode
{
public:
  explicit Init_PanTiltState_control_mode(::combat_robot_msgs::msg::PanTiltState & msg)
  : msg_(msg)
  {}
  Init_PanTiltState_horizontal_angle control_mode(::combat_robot_msgs::msg::PanTiltState::_control_mode_type arg)
  {
    msg_.control_mode = std::move(arg);
    return Init_PanTiltState_horizontal_angle(msg_);
  }

private:
  ::combat_robot_msgs::msg::PanTiltState msg_;
};

class Init_PanTiltState_stamp
{
public:
  Init_PanTiltState_stamp()
  : msg_(::rosidl_runtime_cpp::MessageInitialization::SKIP)
  {}
  Init_PanTiltState_control_mode stamp(::combat_robot_msgs::msg::PanTiltState::_stamp_type arg)
  {
    msg_.stamp = std::move(arg);
    return Init_PanTiltState_control_mode(msg_);
  }

private:
  ::combat_robot_msgs::msg::PanTiltState msg_;
};

}  // namespace builder

}  // namespace msg

template<typename MessageType>
auto build();

template<>
inline
auto build<::combat_robot_msgs::msg::PanTiltState>()
{
  return combat_robot_msgs::msg::builder::Init_PanTiltState_stamp();
}

}  // namespace combat_robot_msgs

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__PAN_TILT_STATE__BUILDER_HPP_
