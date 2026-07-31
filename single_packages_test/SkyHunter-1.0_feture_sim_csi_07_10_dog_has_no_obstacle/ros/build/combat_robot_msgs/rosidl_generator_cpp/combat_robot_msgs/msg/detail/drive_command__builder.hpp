// generated from rosidl_generator_cpp/resource/idl__builder.hpp.em
// with input from combat_robot_msgs:msg/DriveCommand.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/drive_command.hpp"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__DRIVE_COMMAND__BUILDER_HPP_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__DRIVE_COMMAND__BUILDER_HPP_

#include <algorithm>
#include <utility>

#include "combat_robot_msgs/msg/detail/drive_command__struct.hpp"
#include "rosidl_runtime_cpp/message_initialization.hpp"


namespace combat_robot_msgs
{

namespace msg
{

namespace builder
{

class Init_DriveCommand_angular_velocity
{
public:
  explicit Init_DriveCommand_angular_velocity(::combat_robot_msgs::msg::DriveCommand & msg)
  : msg_(msg)
  {}
  ::combat_robot_msgs::msg::DriveCommand angular_velocity(::combat_robot_msgs::msg::DriveCommand::_angular_velocity_type arg)
  {
    msg_.angular_velocity = std::move(arg);
    return std::move(msg_);
  }

private:
  ::combat_robot_msgs::msg::DriveCommand msg_;
};

class Init_DriveCommand_linear_velocity
{
public:
  explicit Init_DriveCommand_linear_velocity(::combat_robot_msgs::msg::DriveCommand & msg)
  : msg_(msg)
  {}
  Init_DriveCommand_angular_velocity linear_velocity(::combat_robot_msgs::msg::DriveCommand::_linear_velocity_type arg)
  {
    msg_.linear_velocity = std::move(arg);
    return Init_DriveCommand_angular_velocity(msg_);
  }

private:
  ::combat_robot_msgs::msg::DriveCommand msg_;
};

class Init_DriveCommand_header
{
public:
  Init_DriveCommand_header()
  : msg_(::rosidl_runtime_cpp::MessageInitialization::SKIP)
  {}
  Init_DriveCommand_linear_velocity header(::combat_robot_msgs::msg::DriveCommand::_header_type arg)
  {
    msg_.header = std::move(arg);
    return Init_DriveCommand_linear_velocity(msg_);
  }

private:
  ::combat_robot_msgs::msg::DriveCommand msg_;
};

}  // namespace builder

}  // namespace msg

template<typename MessageType>
auto build();

template<>
inline
auto build<::combat_robot_msgs::msg::DriveCommand>()
{
  return combat_robot_msgs::msg::builder::Init_DriveCommand_header();
}

}  // namespace combat_robot_msgs

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__DRIVE_COMMAND__BUILDER_HPP_
