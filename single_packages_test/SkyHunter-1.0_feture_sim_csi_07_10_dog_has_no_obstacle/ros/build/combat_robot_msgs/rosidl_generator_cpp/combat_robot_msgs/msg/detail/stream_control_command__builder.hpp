// generated from rosidl_generator_cpp/resource/idl__builder.hpp.em
// with input from combat_robot_msgs:msg/StreamControlCommand.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/stream_control_command.hpp"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__STREAM_CONTROL_COMMAND__BUILDER_HPP_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__STREAM_CONTROL_COMMAND__BUILDER_HPP_

#include <algorithm>
#include <utility>

#include "combat_robot_msgs/msg/detail/stream_control_command__struct.hpp"
#include "rosidl_runtime_cpp/message_initialization.hpp"


namespace combat_robot_msgs
{

namespace msg
{

namespace builder
{

class Init_StreamControlCommand_stream_target_robot_id
{
public:
  explicit Init_StreamControlCommand_stream_target_robot_id(::combat_robot_msgs::msg::StreamControlCommand & msg)
  : msg_(msg)
  {}
  ::combat_robot_msgs::msg::StreamControlCommand stream_target_robot_id(::combat_robot_msgs::msg::StreamControlCommand::_stream_target_robot_id_type arg)
  {
    msg_.stream_target_robot_id = std::move(arg);
    return std::move(msg_);
  }

private:
  ::combat_robot_msgs::msg::StreamControlCommand msg_;
};

class Init_StreamControlCommand_stream_command
{
public:
  explicit Init_StreamControlCommand_stream_command(::combat_robot_msgs::msg::StreamControlCommand & msg)
  : msg_(msg)
  {}
  Init_StreamControlCommand_stream_target_robot_id stream_command(::combat_robot_msgs::msg::StreamControlCommand::_stream_command_type arg)
  {
    msg_.stream_command = std::move(arg);
    return Init_StreamControlCommand_stream_target_robot_id(msg_);
  }

private:
  ::combat_robot_msgs::msg::StreamControlCommand msg_;
};

class Init_StreamControlCommand_header
{
public:
  Init_StreamControlCommand_header()
  : msg_(::rosidl_runtime_cpp::MessageInitialization::SKIP)
  {}
  Init_StreamControlCommand_stream_command header(::combat_robot_msgs::msg::StreamControlCommand::_header_type arg)
  {
    msg_.header = std::move(arg);
    return Init_StreamControlCommand_stream_command(msg_);
  }

private:
  ::combat_robot_msgs::msg::StreamControlCommand msg_;
};

}  // namespace builder

}  // namespace msg

template<typename MessageType>
auto build();

template<>
inline
auto build<::combat_robot_msgs::msg::StreamControlCommand>()
{
  return combat_robot_msgs::msg::builder::Init_StreamControlCommand_header();
}

}  // namespace combat_robot_msgs

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__STREAM_CONTROL_COMMAND__BUILDER_HPP_
