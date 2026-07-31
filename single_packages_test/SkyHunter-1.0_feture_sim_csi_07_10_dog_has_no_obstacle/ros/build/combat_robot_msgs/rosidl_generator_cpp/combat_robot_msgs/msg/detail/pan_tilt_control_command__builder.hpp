// generated from rosidl_generator_cpp/resource/idl__builder.hpp.em
// with input from combat_robot_msgs:msg/PanTiltControlCommand.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/pan_tilt_control_command.hpp"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__PAN_TILT_CONTROL_COMMAND__BUILDER_HPP_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__PAN_TILT_CONTROL_COMMAND__BUILDER_HPP_

#include <algorithm>
#include <utility>

#include "combat_robot_msgs/msg/detail/pan_tilt_control_command__struct.hpp"
#include "rosidl_runtime_cpp/message_initialization.hpp"


namespace combat_robot_msgs
{

namespace msg
{

namespace builder
{

class Init_PanTiltControlCommand_tilt_dir
{
public:
  explicit Init_PanTiltControlCommand_tilt_dir(::combat_robot_msgs::msg::PanTiltControlCommand & msg)
  : msg_(msg)
  {}
  ::combat_robot_msgs::msg::PanTiltControlCommand tilt_dir(::combat_robot_msgs::msg::PanTiltControlCommand::_tilt_dir_type arg)
  {
    msg_.tilt_dir = std::move(arg);
    return std::move(msg_);
  }

private:
  ::combat_robot_msgs::msg::PanTiltControlCommand msg_;
};

class Init_PanTiltControlCommand_pan_dir
{
public:
  explicit Init_PanTiltControlCommand_pan_dir(::combat_robot_msgs::msg::PanTiltControlCommand & msg)
  : msg_(msg)
  {}
  Init_PanTiltControlCommand_tilt_dir pan_dir(::combat_robot_msgs::msg::PanTiltControlCommand::_pan_dir_type arg)
  {
    msg_.pan_dir = std::move(arg);
    return Init_PanTiltControlCommand_tilt_dir(msg_);
  }

private:
  ::combat_robot_msgs::msg::PanTiltControlCommand msg_;
};

class Init_PanTiltControlCommand_tilt_speed
{
public:
  explicit Init_PanTiltControlCommand_tilt_speed(::combat_robot_msgs::msg::PanTiltControlCommand & msg)
  : msg_(msg)
  {}
  Init_PanTiltControlCommand_pan_dir tilt_speed(::combat_robot_msgs::msg::PanTiltControlCommand::_tilt_speed_type arg)
  {
    msg_.tilt_speed = std::move(arg);
    return Init_PanTiltControlCommand_pan_dir(msg_);
  }

private:
  ::combat_robot_msgs::msg::PanTiltControlCommand msg_;
};

class Init_PanTiltControlCommand_pan_speed
{
public:
  explicit Init_PanTiltControlCommand_pan_speed(::combat_robot_msgs::msg::PanTiltControlCommand & msg)
  : msg_(msg)
  {}
  Init_PanTiltControlCommand_tilt_speed pan_speed(::combat_robot_msgs::msg::PanTiltControlCommand::_pan_speed_type arg)
  {
    msg_.pan_speed = std::move(arg);
    return Init_PanTiltControlCommand_tilt_speed(msg_);
  }

private:
  ::combat_robot_msgs::msg::PanTiltControlCommand msg_;
};

class Init_PanTiltControlCommand_vertical_angle
{
public:
  explicit Init_PanTiltControlCommand_vertical_angle(::combat_robot_msgs::msg::PanTiltControlCommand & msg)
  : msg_(msg)
  {}
  Init_PanTiltControlCommand_pan_speed vertical_angle(::combat_robot_msgs::msg::PanTiltControlCommand::_vertical_angle_type arg)
  {
    msg_.vertical_angle = std::move(arg);
    return Init_PanTiltControlCommand_pan_speed(msg_);
  }

private:
  ::combat_robot_msgs::msg::PanTiltControlCommand msg_;
};

class Init_PanTiltControlCommand_horizontal_angle
{
public:
  explicit Init_PanTiltControlCommand_horizontal_angle(::combat_robot_msgs::msg::PanTiltControlCommand & msg)
  : msg_(msg)
  {}
  Init_PanTiltControlCommand_vertical_angle horizontal_angle(::combat_robot_msgs::msg::PanTiltControlCommand::_horizontal_angle_type arg)
  {
    msg_.horizontal_angle = std::move(arg);
    return Init_PanTiltControlCommand_vertical_angle(msg_);
  }

private:
  ::combat_robot_msgs::msg::PanTiltControlCommand msg_;
};

class Init_PanTiltControlCommand_control_mode
{
public:
  explicit Init_PanTiltControlCommand_control_mode(::combat_robot_msgs::msg::PanTiltControlCommand & msg)
  : msg_(msg)
  {}
  Init_PanTiltControlCommand_horizontal_angle control_mode(::combat_robot_msgs::msg::PanTiltControlCommand::_control_mode_type arg)
  {
    msg_.control_mode = std::move(arg);
    return Init_PanTiltControlCommand_horizontal_angle(msg_);
  }

private:
  ::combat_robot_msgs::msg::PanTiltControlCommand msg_;
};

class Init_PanTiltControlCommand_header
{
public:
  Init_PanTiltControlCommand_header()
  : msg_(::rosidl_runtime_cpp::MessageInitialization::SKIP)
  {}
  Init_PanTiltControlCommand_control_mode header(::combat_robot_msgs::msg::PanTiltControlCommand::_header_type arg)
  {
    msg_.header = std::move(arg);
    return Init_PanTiltControlCommand_control_mode(msg_);
  }

private:
  ::combat_robot_msgs::msg::PanTiltControlCommand msg_;
};

}  // namespace builder

}  // namespace msg

template<typename MessageType>
auto build();

template<>
inline
auto build<::combat_robot_msgs::msg::PanTiltControlCommand>()
{
  return combat_robot_msgs::msg::builder::Init_PanTiltControlCommand_header();
}

}  // namespace combat_robot_msgs

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__PAN_TILT_CONTROL_COMMAND__BUILDER_HPP_
