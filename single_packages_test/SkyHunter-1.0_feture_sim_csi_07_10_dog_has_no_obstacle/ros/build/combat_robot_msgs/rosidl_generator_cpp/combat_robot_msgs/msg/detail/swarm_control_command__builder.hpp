// generated from rosidl_generator_cpp/resource/idl__builder.hpp.em
// with input from combat_robot_msgs:msg/SwarmControlCommand.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/swarm_control_command.hpp"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__SWARM_CONTROL_COMMAND__BUILDER_HPP_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__SWARM_CONTROL_COMMAND__BUILDER_HPP_

#include <algorithm>
#include <utility>

#include "combat_robot_msgs/msg/detail/swarm_control_command__struct.hpp"
#include "rosidl_runtime_cpp/message_initialization.hpp"


namespace combat_robot_msgs
{

namespace msg
{

namespace builder
{

class Init_SwarmControlCommand_selected_robot_ids
{
public:
  explicit Init_SwarmControlCommand_selected_robot_ids(::combat_robot_msgs::msg::SwarmControlCommand & msg)
  : msg_(msg)
  {}
  ::combat_robot_msgs::msg::SwarmControlCommand selected_robot_ids(::combat_robot_msgs::msg::SwarmControlCommand::_selected_robot_ids_type arg)
  {
    msg_.selected_robot_ids = std::move(arg);
    return std::move(msg_);
  }

private:
  ::combat_robot_msgs::msg::SwarmControlCommand msg_;
};

class Init_SwarmControlCommand_selected_robot_count
{
public:
  explicit Init_SwarmControlCommand_selected_robot_count(::combat_robot_msgs::msg::SwarmControlCommand & msg)
  : msg_(msg)
  {}
  Init_SwarmControlCommand_selected_robot_ids selected_robot_count(::combat_robot_msgs::msg::SwarmControlCommand::_selected_robot_count_type arg)
  {
    msg_.selected_robot_count = std::move(arg);
    return Init_SwarmControlCommand_selected_robot_ids(msg_);
  }

private:
  ::combat_robot_msgs::msg::SwarmControlCommand msg_;
};

class Init_SwarmControlCommand_grouping_index
{
public:
  explicit Init_SwarmControlCommand_grouping_index(::combat_robot_msgs::msg::SwarmControlCommand & msg)
  : msg_(msg)
  {}
  Init_SwarmControlCommand_selected_robot_count grouping_index(::combat_robot_msgs::msg::SwarmControlCommand::_grouping_index_type arg)
  {
    msg_.grouping_index = std::move(arg);
    return Init_SwarmControlCommand_selected_robot_count(msg_);
  }

private:
  ::combat_robot_msgs::msg::SwarmControlCommand msg_;
};

class Init_SwarmControlCommand_formation_number
{
public:
  explicit Init_SwarmControlCommand_formation_number(::combat_robot_msgs::msg::SwarmControlCommand & msg)
  : msg_(msg)
  {}
  Init_SwarmControlCommand_grouping_index formation_number(::combat_robot_msgs::msg::SwarmControlCommand::_formation_number_type arg)
  {
    msg_.formation_number = std::move(arg);
    return Init_SwarmControlCommand_grouping_index(msg_);
  }

private:
  ::combat_robot_msgs::msg::SwarmControlCommand msg_;
};

class Init_SwarmControlCommand_formation_type
{
public:
  explicit Init_SwarmControlCommand_formation_type(::combat_robot_msgs::msg::SwarmControlCommand & msg)
  : msg_(msg)
  {}
  Init_SwarmControlCommand_formation_number formation_type(::combat_robot_msgs::msg::SwarmControlCommand::_formation_type_type arg)
  {
    msg_.formation_type = std::move(arg);
    return Init_SwarmControlCommand_formation_number(msg_);
  }

private:
  ::combat_robot_msgs::msg::SwarmControlCommand msg_;
};

class Init_SwarmControlCommand_header
{
public:
  Init_SwarmControlCommand_header()
  : msg_(::rosidl_runtime_cpp::MessageInitialization::SKIP)
  {}
  Init_SwarmControlCommand_formation_type header(::combat_robot_msgs::msg::SwarmControlCommand::_header_type arg)
  {
    msg_.header = std::move(arg);
    return Init_SwarmControlCommand_formation_type(msg_);
  }

private:
  ::combat_robot_msgs::msg::SwarmControlCommand msg_;
};

}  // namespace builder

}  // namespace msg

template<typename MessageType>
auto build();

template<>
inline
auto build<::combat_robot_msgs::msg::SwarmControlCommand>()
{
  return combat_robot_msgs::msg::builder::Init_SwarmControlCommand_header();
}

}  // namespace combat_robot_msgs

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__SWARM_CONTROL_COMMAND__BUILDER_HPP_
