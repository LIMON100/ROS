// generated from rosidl_generator_cpp/resource/idl__builder.hpp.em
// with input from combat_robot_msgs:msg/SwarmLeaderHeartbeat.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/swarm_leader_heartbeat.hpp"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__SWARM_LEADER_HEARTBEAT__BUILDER_HPP_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__SWARM_LEADER_HEARTBEAT__BUILDER_HPP_

#include <algorithm>
#include <utility>

#include "combat_robot_msgs/msg/detail/swarm_leader_heartbeat__struct.hpp"
#include "rosidl_runtime_cpp/message_initialization.hpp"


namespace combat_robot_msgs
{

namespace msg
{

namespace builder
{

class Init_SwarmLeaderHeartbeat_selected_robot_ids
{
public:
  explicit Init_SwarmLeaderHeartbeat_selected_robot_ids(::combat_robot_msgs::msg::SwarmLeaderHeartbeat & msg)
  : msg_(msg)
  {}
  ::combat_robot_msgs::msg::SwarmLeaderHeartbeat selected_robot_ids(::combat_robot_msgs::msg::SwarmLeaderHeartbeat::_selected_robot_ids_type arg)
  {
    msg_.selected_robot_ids = std::move(arg);
    return std::move(msg_);
  }

private:
  ::combat_robot_msgs::msg::SwarmLeaderHeartbeat msg_;
};

class Init_SwarmLeaderHeartbeat_selected_robot_count
{
public:
  explicit Init_SwarmLeaderHeartbeat_selected_robot_count(::combat_robot_msgs::msg::SwarmLeaderHeartbeat & msg)
  : msg_(msg)
  {}
  Init_SwarmLeaderHeartbeat_selected_robot_ids selected_robot_count(::combat_robot_msgs::msg::SwarmLeaderHeartbeat::_selected_robot_count_type arg)
  {
    msg_.selected_robot_count = std::move(arg);
    return Init_SwarmLeaderHeartbeat_selected_robot_ids(msg_);
  }

private:
  ::combat_robot_msgs::msg::SwarmLeaderHeartbeat msg_;
};

class Init_SwarmLeaderHeartbeat_grouping_index
{
public:
  explicit Init_SwarmLeaderHeartbeat_grouping_index(::combat_robot_msgs::msg::SwarmLeaderHeartbeat & msg)
  : msg_(msg)
  {}
  Init_SwarmLeaderHeartbeat_selected_robot_count grouping_index(::combat_robot_msgs::msg::SwarmLeaderHeartbeat::_grouping_index_type arg)
  {
    msg_.grouping_index = std::move(arg);
    return Init_SwarmLeaderHeartbeat_selected_robot_count(msg_);
  }

private:
  ::combat_robot_msgs::msg::SwarmLeaderHeartbeat msg_;
};

class Init_SwarmLeaderHeartbeat_formation_number
{
public:
  explicit Init_SwarmLeaderHeartbeat_formation_number(::combat_robot_msgs::msg::SwarmLeaderHeartbeat & msg)
  : msg_(msg)
  {}
  Init_SwarmLeaderHeartbeat_grouping_index formation_number(::combat_robot_msgs::msg::SwarmLeaderHeartbeat::_formation_number_type arg)
  {
    msg_.formation_number = std::move(arg);
    return Init_SwarmLeaderHeartbeat_grouping_index(msg_);
  }

private:
  ::combat_robot_msgs::msg::SwarmLeaderHeartbeat msg_;
};

class Init_SwarmLeaderHeartbeat_formation_type
{
public:
  explicit Init_SwarmLeaderHeartbeat_formation_type(::combat_robot_msgs::msg::SwarmLeaderHeartbeat & msg)
  : msg_(msg)
  {}
  Init_SwarmLeaderHeartbeat_formation_number formation_type(::combat_robot_msgs::msg::SwarmLeaderHeartbeat::_formation_type_type arg)
  {
    msg_.formation_type = std::move(arg);
    return Init_SwarmLeaderHeartbeat_formation_number(msg_);
  }

private:
  ::combat_robot_msgs::msg::SwarmLeaderHeartbeat msg_;
};

class Init_SwarmLeaderHeartbeat_estop_active
{
public:
  explicit Init_SwarmLeaderHeartbeat_estop_active(::combat_robot_msgs::msg::SwarmLeaderHeartbeat & msg)
  : msg_(msg)
  {}
  Init_SwarmLeaderHeartbeat_formation_type estop_active(::combat_robot_msgs::msg::SwarmLeaderHeartbeat::_estop_active_type arg)
  {
    msg_.estop_active = std::move(arg);
    return Init_SwarmLeaderHeartbeat_formation_type(msg_);
  }

private:
  ::combat_robot_msgs::msg::SwarmLeaderHeartbeat msg_;
};

class Init_SwarmLeaderHeartbeat_operation_mode
{
public:
  explicit Init_SwarmLeaderHeartbeat_operation_mode(::combat_robot_msgs::msg::SwarmLeaderHeartbeat & msg)
  : msg_(msg)
  {}
  Init_SwarmLeaderHeartbeat_estop_active operation_mode(::combat_robot_msgs::msg::SwarmLeaderHeartbeat::_operation_mode_type arg)
  {
    msg_.operation_mode = std::move(arg);
    return Init_SwarmLeaderHeartbeat_estop_active(msg_);
  }

private:
  ::combat_robot_msgs::msg::SwarmLeaderHeartbeat msg_;
};

class Init_SwarmLeaderHeartbeat_leader_robot_id
{
public:
  explicit Init_SwarmLeaderHeartbeat_leader_robot_id(::combat_robot_msgs::msg::SwarmLeaderHeartbeat & msg)
  : msg_(msg)
  {}
  Init_SwarmLeaderHeartbeat_operation_mode leader_robot_id(::combat_robot_msgs::msg::SwarmLeaderHeartbeat::_leader_robot_id_type arg)
  {
    msg_.leader_robot_id = std::move(arg);
    return Init_SwarmLeaderHeartbeat_operation_mode(msg_);
  }

private:
  ::combat_robot_msgs::msg::SwarmLeaderHeartbeat msg_;
};

class Init_SwarmLeaderHeartbeat_sequence
{
public:
  explicit Init_SwarmLeaderHeartbeat_sequence(::combat_robot_msgs::msg::SwarmLeaderHeartbeat & msg)
  : msg_(msg)
  {}
  Init_SwarmLeaderHeartbeat_leader_robot_id sequence(::combat_robot_msgs::msg::SwarmLeaderHeartbeat::_sequence_type arg)
  {
    msg_.sequence = std::move(arg);
    return Init_SwarmLeaderHeartbeat_leader_robot_id(msg_);
  }

private:
  ::combat_robot_msgs::msg::SwarmLeaderHeartbeat msg_;
};

class Init_SwarmLeaderHeartbeat_header
{
public:
  Init_SwarmLeaderHeartbeat_header()
  : msg_(::rosidl_runtime_cpp::MessageInitialization::SKIP)
  {}
  Init_SwarmLeaderHeartbeat_sequence header(::combat_robot_msgs::msg::SwarmLeaderHeartbeat::_header_type arg)
  {
    msg_.header = std::move(arg);
    return Init_SwarmLeaderHeartbeat_sequence(msg_);
  }

private:
  ::combat_robot_msgs::msg::SwarmLeaderHeartbeat msg_;
};

}  // namespace builder

}  // namespace msg

template<typename MessageType>
auto build();

template<>
inline
auto build<::combat_robot_msgs::msg::SwarmLeaderHeartbeat>()
{
  return combat_robot_msgs::msg::builder::Init_SwarmLeaderHeartbeat_header();
}

}  // namespace combat_robot_msgs

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__SWARM_LEADER_HEARTBEAT__BUILDER_HPP_
