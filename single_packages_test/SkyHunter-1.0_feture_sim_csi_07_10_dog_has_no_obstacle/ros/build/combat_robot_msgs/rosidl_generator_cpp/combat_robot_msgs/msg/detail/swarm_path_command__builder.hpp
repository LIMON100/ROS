// generated from rosidl_generator_cpp/resource/idl__builder.hpp.em
// with input from combat_robot_msgs:msg/SwarmPathCommand.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/swarm_path_command.hpp"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__SWARM_PATH_COMMAND__BUILDER_HPP_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__SWARM_PATH_COMMAND__BUILDER_HPP_

#include <algorithm>
#include <utility>

#include "combat_robot_msgs/msg/detail/swarm_path_command__struct.hpp"
#include "rosidl_runtime_cpp/message_initialization.hpp"


namespace combat_robot_msgs
{

namespace msg
{

namespace builder
{

class Init_SwarmPathCommand_path_json
{
public:
  explicit Init_SwarmPathCommand_path_json(::combat_robot_msgs::msg::SwarmPathCommand & msg)
  : msg_(msg)
  {}
  ::combat_robot_msgs::msg::SwarmPathCommand path_json(::combat_robot_msgs::msg::SwarmPathCommand::_path_json_type arg)
  {
    msg_.path_json = std::move(arg);
    return std::move(msg_);
  }

private:
  ::combat_robot_msgs::msg::SwarmPathCommand msg_;
};

class Init_SwarmPathCommand_num_waypoints
{
public:
  explicit Init_SwarmPathCommand_num_waypoints(::combat_robot_msgs::msg::SwarmPathCommand & msg)
  : msg_(msg)
  {}
  Init_SwarmPathCommand_path_json num_waypoints(::combat_robot_msgs::msg::SwarmPathCommand::_num_waypoints_type arg)
  {
    msg_.num_waypoints = std::move(arg);
    return Init_SwarmPathCommand_path_json(msg_);
  }

private:
  ::combat_robot_msgs::msg::SwarmPathCommand msg_;
};

class Init_SwarmPathCommand_command
{
public:
  explicit Init_SwarmPathCommand_command(::combat_robot_msgs::msg::SwarmPathCommand & msg)
  : msg_(msg)
  {}
  Init_SwarmPathCommand_num_waypoints command(::combat_robot_msgs::msg::SwarmPathCommand::_command_type arg)
  {
    msg_.command = std::move(arg);
    return Init_SwarmPathCommand_num_waypoints(msg_);
  }

private:
  ::combat_robot_msgs::msg::SwarmPathCommand msg_;
};

class Init_SwarmPathCommand_header
{
public:
  Init_SwarmPathCommand_header()
  : msg_(::rosidl_runtime_cpp::MessageInitialization::SKIP)
  {}
  Init_SwarmPathCommand_command header(::combat_robot_msgs::msg::SwarmPathCommand::_header_type arg)
  {
    msg_.header = std::move(arg);
    return Init_SwarmPathCommand_command(msg_);
  }

private:
  ::combat_robot_msgs::msg::SwarmPathCommand msg_;
};

}  // namespace builder

}  // namespace msg

template<typename MessageType>
auto build();

template<>
inline
auto build<::combat_robot_msgs::msg::SwarmPathCommand>()
{
  return combat_robot_msgs::msg::builder::Init_SwarmPathCommand_header();
}

}  // namespace combat_robot_msgs

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__SWARM_PATH_COMMAND__BUILDER_HPP_
