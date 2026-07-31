// generated from rosidl_generator_cpp/resource/idl__builder.hpp.em
// with input from combat_robot_msgs:msg/SwarmRobotCommand.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/swarm_robot_command.hpp"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__SWARM_ROBOT_COMMAND__BUILDER_HPP_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__SWARM_ROBOT_COMMAND__BUILDER_HPP_

#include <algorithm>
#include <utility>

#include "combat_robot_msgs/msg/detail/swarm_robot_command__struct.hpp"
#include "rosidl_runtime_cpp/message_initialization.hpp"


namespace combat_robot_msgs
{

namespace msg
{

namespace builder
{

class Init_SwarmRobotCommand_selected_robot_ids
{
public:
  explicit Init_SwarmRobotCommand_selected_robot_ids(::combat_robot_msgs::msg::SwarmRobotCommand & msg)
  : msg_(msg)
  {}
  ::combat_robot_msgs::msg::SwarmRobotCommand selected_robot_ids(::combat_robot_msgs::msg::SwarmRobotCommand::_selected_robot_ids_type arg)
  {
    msg_.selected_robot_ids = std::move(arg);
    return std::move(msg_);
  }

private:
  ::combat_robot_msgs::msg::SwarmRobotCommand msg_;
};

class Init_SwarmRobotCommand_selected_robot_count
{
public:
  explicit Init_SwarmRobotCommand_selected_robot_count(::combat_robot_msgs::msg::SwarmRobotCommand & msg)
  : msg_(msg)
  {}
  Init_SwarmRobotCommand_selected_robot_ids selected_robot_count(::combat_robot_msgs::msg::SwarmRobotCommand::_selected_robot_count_type arg)
  {
    msg_.selected_robot_count = std::move(arg);
    return Init_SwarmRobotCommand_selected_robot_ids(msg_);
  }

private:
  ::combat_robot_msgs::msg::SwarmRobotCommand msg_;
};

class Init_SwarmRobotCommand_slot_index
{
public:
  explicit Init_SwarmRobotCommand_slot_index(::combat_robot_msgs::msg::SwarmRobotCommand & msg)
  : msg_(msg)
  {}
  Init_SwarmRobotCommand_selected_robot_count slot_index(::combat_robot_msgs::msg::SwarmRobotCommand::_slot_index_type arg)
  {
    msg_.slot_index = std::move(arg);
    return Init_SwarmRobotCommand_selected_robot_count(msg_);
  }

private:
  ::combat_robot_msgs::msg::SwarmRobotCommand msg_;
};

class Init_SwarmRobotCommand_grouping_index
{
public:
  explicit Init_SwarmRobotCommand_grouping_index(::combat_robot_msgs::msg::SwarmRobotCommand & msg)
  : msg_(msg)
  {}
  Init_SwarmRobotCommand_slot_index grouping_index(::combat_robot_msgs::msg::SwarmRobotCommand::_grouping_index_type arg)
  {
    msg_.grouping_index = std::move(arg);
    return Init_SwarmRobotCommand_slot_index(msg_);
  }

private:
  ::combat_robot_msgs::msg::SwarmRobotCommand msg_;
};

class Init_SwarmRobotCommand_formation_number
{
public:
  explicit Init_SwarmRobotCommand_formation_number(::combat_robot_msgs::msg::SwarmRobotCommand & msg)
  : msg_(msg)
  {}
  Init_SwarmRobotCommand_grouping_index formation_number(::combat_robot_msgs::msg::SwarmRobotCommand::_formation_number_type arg)
  {
    msg_.formation_number = std::move(arg);
    return Init_SwarmRobotCommand_grouping_index(msg_);
  }

private:
  ::combat_robot_msgs::msg::SwarmRobotCommand msg_;
};

class Init_SwarmRobotCommand_formation_type
{
public:
  explicit Init_SwarmRobotCommand_formation_type(::combat_robot_msgs::msg::SwarmRobotCommand & msg)
  : msg_(msg)
  {}
  Init_SwarmRobotCommand_formation_number formation_type(::combat_robot_msgs::msg::SwarmRobotCommand::_formation_type_type arg)
  {
    msg_.formation_type = std::move(arg);
    return Init_SwarmRobotCommand_formation_number(msg_);
  }

private:
  ::combat_robot_msgs::msg::SwarmRobotCommand msg_;
};

class Init_SwarmRobotCommand_path_json
{
public:
  explicit Init_SwarmRobotCommand_path_json(::combat_robot_msgs::msg::SwarmRobotCommand & msg)
  : msg_(msg)
  {}
  Init_SwarmRobotCommand_formation_type path_json(::combat_robot_msgs::msg::SwarmRobotCommand::_path_json_type arg)
  {
    msg_.path_json = std::move(arg);
    return Init_SwarmRobotCommand_formation_type(msg_);
  }

private:
  ::combat_robot_msgs::msg::SwarmRobotCommand msg_;
};

class Init_SwarmRobotCommand_path_id
{
public:
  explicit Init_SwarmRobotCommand_path_id(::combat_robot_msgs::msg::SwarmRobotCommand & msg)
  : msg_(msg)
  {}
  Init_SwarmRobotCommand_path_json path_id(::combat_robot_msgs::msg::SwarmRobotCommand::_path_id_type arg)
  {
    msg_.path_id = std::move(arg);
    return Init_SwarmRobotCommand_path_json(msg_);
  }

private:
  ::combat_robot_msgs::msg::SwarmRobotCommand msg_;
};

class Init_SwarmRobotCommand_num_waypoints
{
public:
  explicit Init_SwarmRobotCommand_num_waypoints(::combat_robot_msgs::msg::SwarmRobotCommand & msg)
  : msg_(msg)
  {}
  Init_SwarmRobotCommand_path_id num_waypoints(::combat_robot_msgs::msg::SwarmRobotCommand::_num_waypoints_type arg)
  {
    msg_.num_waypoints = std::move(arg);
    return Init_SwarmRobotCommand_path_id(msg_);
  }

private:
  ::combat_robot_msgs::msg::SwarmRobotCommand msg_;
};

class Init_SwarmRobotCommand_path_command
{
public:
  explicit Init_SwarmRobotCommand_path_command(::combat_robot_msgs::msg::SwarmRobotCommand & msg)
  : msg_(msg)
  {}
  Init_SwarmRobotCommand_num_waypoints path_command(::combat_robot_msgs::msg::SwarmRobotCommand::_path_command_type arg)
  {
    msg_.path_command = std::move(arg);
    return Init_SwarmRobotCommand_num_waypoints(msg_);
  }

private:
  ::combat_robot_msgs::msg::SwarmRobotCommand msg_;
};

class Init_SwarmRobotCommand_estop_requested
{
public:
  explicit Init_SwarmRobotCommand_estop_requested(::combat_robot_msgs::msg::SwarmRobotCommand & msg)
  : msg_(msg)
  {}
  Init_SwarmRobotCommand_path_command estop_requested(::combat_robot_msgs::msg::SwarmRobotCommand::_estop_requested_type arg)
  {
    msg_.estop_requested = std::move(arg);
    return Init_SwarmRobotCommand_path_command(msg_);
  }

private:
  ::combat_robot_msgs::msg::SwarmRobotCommand msg_;
};

class Init_SwarmRobotCommand_operation_mode
{
public:
  explicit Init_SwarmRobotCommand_operation_mode(::combat_robot_msgs::msg::SwarmRobotCommand & msg)
  : msg_(msg)
  {}
  Init_SwarmRobotCommand_estop_requested operation_mode(::combat_robot_msgs::msg::SwarmRobotCommand::_operation_mode_type arg)
  {
    msg_.operation_mode = std::move(arg);
    return Init_SwarmRobotCommand_estop_requested(msg_);
  }

private:
  ::combat_robot_msgs::msg::SwarmRobotCommand msg_;
};

class Init_SwarmRobotCommand_target_robot_id
{
public:
  explicit Init_SwarmRobotCommand_target_robot_id(::combat_robot_msgs::msg::SwarmRobotCommand & msg)
  : msg_(msg)
  {}
  Init_SwarmRobotCommand_operation_mode target_robot_id(::combat_robot_msgs::msg::SwarmRobotCommand::_target_robot_id_type arg)
  {
    msg_.target_robot_id = std::move(arg);
    return Init_SwarmRobotCommand_operation_mode(msg_);
  }

private:
  ::combat_robot_msgs::msg::SwarmRobotCommand msg_;
};

class Init_SwarmRobotCommand_leader_robot_id
{
public:
  explicit Init_SwarmRobotCommand_leader_robot_id(::combat_robot_msgs::msg::SwarmRobotCommand & msg)
  : msg_(msg)
  {}
  Init_SwarmRobotCommand_target_robot_id leader_robot_id(::combat_robot_msgs::msg::SwarmRobotCommand::_leader_robot_id_type arg)
  {
    msg_.leader_robot_id = std::move(arg);
    return Init_SwarmRobotCommand_target_robot_id(msg_);
  }

private:
  ::combat_robot_msgs::msg::SwarmRobotCommand msg_;
};

class Init_SwarmRobotCommand_command_type
{
public:
  explicit Init_SwarmRobotCommand_command_type(::combat_robot_msgs::msg::SwarmRobotCommand & msg)
  : msg_(msg)
  {}
  Init_SwarmRobotCommand_leader_robot_id command_type(::combat_robot_msgs::msg::SwarmRobotCommand::_command_type_type arg)
  {
    msg_.command_type = std::move(arg);
    return Init_SwarmRobotCommand_leader_robot_id(msg_);
  }

private:
  ::combat_robot_msgs::msg::SwarmRobotCommand msg_;
};

class Init_SwarmRobotCommand_sequence
{
public:
  explicit Init_SwarmRobotCommand_sequence(::combat_robot_msgs::msg::SwarmRobotCommand & msg)
  : msg_(msg)
  {}
  Init_SwarmRobotCommand_command_type sequence(::combat_robot_msgs::msg::SwarmRobotCommand::_sequence_type arg)
  {
    msg_.sequence = std::move(arg);
    return Init_SwarmRobotCommand_command_type(msg_);
  }

private:
  ::combat_robot_msgs::msg::SwarmRobotCommand msg_;
};

class Init_SwarmRobotCommand_header
{
public:
  Init_SwarmRobotCommand_header()
  : msg_(::rosidl_runtime_cpp::MessageInitialization::SKIP)
  {}
  Init_SwarmRobotCommand_sequence header(::combat_robot_msgs::msg::SwarmRobotCommand::_header_type arg)
  {
    msg_.header = std::move(arg);
    return Init_SwarmRobotCommand_sequence(msg_);
  }

private:
  ::combat_robot_msgs::msg::SwarmRobotCommand msg_;
};

}  // namespace builder

}  // namespace msg

template<typename MessageType>
auto build();

template<>
inline
auto build<::combat_robot_msgs::msg::SwarmRobotCommand>()
{
  return combat_robot_msgs::msg::builder::Init_SwarmRobotCommand_header();
}

}  // namespace combat_robot_msgs

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__SWARM_ROBOT_COMMAND__BUILDER_HPP_
