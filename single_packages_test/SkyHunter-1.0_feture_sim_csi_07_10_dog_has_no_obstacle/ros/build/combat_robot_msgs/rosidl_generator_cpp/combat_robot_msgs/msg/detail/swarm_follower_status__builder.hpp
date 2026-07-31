// generated from rosidl_generator_cpp/resource/idl__builder.hpp.em
// with input from combat_robot_msgs:msg/SwarmFollowerStatus.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/swarm_follower_status.hpp"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__SWARM_FOLLOWER_STATUS__BUILDER_HPP_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__SWARM_FOLLOWER_STATUS__BUILDER_HPP_

#include <algorithm>
#include <utility>

#include "combat_robot_msgs/msg/detail/swarm_follower_status__struct.hpp"
#include "rosidl_runtime_cpp/message_initialization.hpp"


namespace combat_robot_msgs
{

namespace msg
{

namespace builder
{

class Init_SwarmFollowerStatus_ground_speed_mps
{
public:
  explicit Init_SwarmFollowerStatus_ground_speed_mps(::combat_robot_msgs::msg::SwarmFollowerStatus & msg)
  : msg_(msg)
  {}
  ::combat_robot_msgs::msg::SwarmFollowerStatus ground_speed_mps(::combat_robot_msgs::msg::SwarmFollowerStatus::_ground_speed_mps_type arg)
  {
    msg_.ground_speed_mps = std::move(arg);
    return std::move(msg_);
  }

private:
  ::combat_robot_msgs::msg::SwarmFollowerStatus msg_;
};

class Init_SwarmFollowerStatus_heading_deg
{
public:
  explicit Init_SwarmFollowerStatus_heading_deg(::combat_robot_msgs::msg::SwarmFollowerStatus & msg)
  : msg_(msg)
  {}
  Init_SwarmFollowerStatus_ground_speed_mps heading_deg(::combat_robot_msgs::msg::SwarmFollowerStatus::_heading_deg_type arg)
  {
    msg_.heading_deg = std::move(arg);
    return Init_SwarmFollowerStatus_ground_speed_mps(msg_);
  }

private:
  ::combat_robot_msgs::msg::SwarmFollowerStatus msg_;
};

class Init_SwarmFollowerStatus_longitude
{
public:
  explicit Init_SwarmFollowerStatus_longitude(::combat_robot_msgs::msg::SwarmFollowerStatus & msg)
  : msg_(msg)
  {}
  Init_SwarmFollowerStatus_heading_deg longitude(::combat_robot_msgs::msg::SwarmFollowerStatus::_longitude_type arg)
  {
    msg_.longitude = std::move(arg);
    return Init_SwarmFollowerStatus_heading_deg(msg_);
  }

private:
  ::combat_robot_msgs::msg::SwarmFollowerStatus msg_;
};

class Init_SwarmFollowerStatus_latitude
{
public:
  explicit Init_SwarmFollowerStatus_latitude(::combat_robot_msgs::msg::SwarmFollowerStatus & msg)
  : msg_(msg)
  {}
  Init_SwarmFollowerStatus_longitude latitude(::combat_robot_msgs::msg::SwarmFollowerStatus::_latitude_type arg)
  {
    msg_.latitude = std::move(arg);
    return Init_SwarmFollowerStatus_longitude(msg_);
  }

private:
  ::combat_robot_msgs::msg::SwarmFollowerStatus msg_;
};

class Init_SwarmFollowerStatus_last_grouping_index
{
public:
  explicit Init_SwarmFollowerStatus_last_grouping_index(::combat_robot_msgs::msg::SwarmFollowerStatus & msg)
  : msg_(msg)
  {}
  Init_SwarmFollowerStatus_latitude last_grouping_index(::combat_robot_msgs::msg::SwarmFollowerStatus::_last_grouping_index_type arg)
  {
    msg_.last_grouping_index = std::move(arg);
    return Init_SwarmFollowerStatus_latitude(msg_);
  }

private:
  ::combat_robot_msgs::msg::SwarmFollowerStatus msg_;
};

class Init_SwarmFollowerStatus_last_formation_number
{
public:
  explicit Init_SwarmFollowerStatus_last_formation_number(::combat_robot_msgs::msg::SwarmFollowerStatus & msg)
  : msg_(msg)
  {}
  Init_SwarmFollowerStatus_last_grouping_index last_formation_number(::combat_robot_msgs::msg::SwarmFollowerStatus::_last_formation_number_type arg)
  {
    msg_.last_formation_number = std::move(arg);
    return Init_SwarmFollowerStatus_last_grouping_index(msg_);
  }

private:
  ::combat_robot_msgs::msg::SwarmFollowerStatus msg_;
};

class Init_SwarmFollowerStatus_last_formation_type
{
public:
  explicit Init_SwarmFollowerStatus_last_formation_type(::combat_robot_msgs::msg::SwarmFollowerStatus & msg)
  : msg_(msg)
  {}
  Init_SwarmFollowerStatus_last_formation_number last_formation_type(::combat_robot_msgs::msg::SwarmFollowerStatus::_last_formation_type_type arg)
  {
    msg_.last_formation_type = std::move(arg);
    return Init_SwarmFollowerStatus_last_formation_number(msg_);
  }

private:
  ::combat_robot_msgs::msg::SwarmFollowerStatus msg_;
};

class Init_SwarmFollowerStatus_last_operation_mode
{
public:
  explicit Init_SwarmFollowerStatus_last_operation_mode(::combat_robot_msgs::msg::SwarmFollowerStatus & msg)
  : msg_(msg)
  {}
  Init_SwarmFollowerStatus_last_formation_type last_operation_mode(::combat_robot_msgs::msg::SwarmFollowerStatus::_last_operation_mode_type arg)
  {
    msg_.last_operation_mode = std::move(arg);
    return Init_SwarmFollowerStatus_last_formation_type(msg_);
  }

private:
  ::combat_robot_msgs::msg::SwarmFollowerStatus msg_;
};

class Init_SwarmFollowerStatus_heartbeat_age_sec
{
public:
  explicit Init_SwarmFollowerStatus_heartbeat_age_sec(::combat_robot_msgs::msg::SwarmFollowerStatus & msg)
  : msg_(msg)
  {}
  Init_SwarmFollowerStatus_last_operation_mode heartbeat_age_sec(::combat_robot_msgs::msg::SwarmFollowerStatus::_heartbeat_age_sec_type arg)
  {
    msg_.heartbeat_age_sec = std::move(arg);
    return Init_SwarmFollowerStatus_last_operation_mode(msg_);
  }

private:
  ::combat_robot_msgs::msg::SwarmFollowerStatus msg_;
};

class Init_SwarmFollowerStatus_last_heartbeat_sequence
{
public:
  explicit Init_SwarmFollowerStatus_last_heartbeat_sequence(::combat_robot_msgs::msg::SwarmFollowerStatus & msg)
  : msg_(msg)
  {}
  Init_SwarmFollowerStatus_heartbeat_age_sec last_heartbeat_sequence(::combat_robot_msgs::msg::SwarmFollowerStatus::_last_heartbeat_sequence_type arg)
  {
    msg_.last_heartbeat_sequence = std::move(arg);
    return Init_SwarmFollowerStatus_heartbeat_age_sec(msg_);
  }

private:
  ::combat_robot_msgs::msg::SwarmFollowerStatus msg_;
};

class Init_SwarmFollowerStatus_link_status
{
public:
  explicit Init_SwarmFollowerStatus_link_status(::combat_robot_msgs::msg::SwarmFollowerStatus & msg)
  : msg_(msg)
  {}
  Init_SwarmFollowerStatus_last_heartbeat_sequence link_status(::combat_robot_msgs::msg::SwarmFollowerStatus::_link_status_type arg)
  {
    msg_.link_status = std::move(arg);
    return Init_SwarmFollowerStatus_last_heartbeat_sequence(msg_);
  }

private:
  ::combat_robot_msgs::msg::SwarmFollowerStatus msg_;
};

class Init_SwarmFollowerStatus_leader_robot_id
{
public:
  explicit Init_SwarmFollowerStatus_leader_robot_id(::combat_robot_msgs::msg::SwarmFollowerStatus & msg)
  : msg_(msg)
  {}
  Init_SwarmFollowerStatus_link_status leader_robot_id(::combat_robot_msgs::msg::SwarmFollowerStatus::_leader_robot_id_type arg)
  {
    msg_.leader_robot_id = std::move(arg);
    return Init_SwarmFollowerStatus_link_status(msg_);
  }

private:
  ::combat_robot_msgs::msg::SwarmFollowerStatus msg_;
};

class Init_SwarmFollowerStatus_robot_id
{
public:
  explicit Init_SwarmFollowerStatus_robot_id(::combat_robot_msgs::msg::SwarmFollowerStatus & msg)
  : msg_(msg)
  {}
  Init_SwarmFollowerStatus_leader_robot_id robot_id(::combat_robot_msgs::msg::SwarmFollowerStatus::_robot_id_type arg)
  {
    msg_.robot_id = std::move(arg);
    return Init_SwarmFollowerStatus_leader_robot_id(msg_);
  }

private:
  ::combat_robot_msgs::msg::SwarmFollowerStatus msg_;
};

class Init_SwarmFollowerStatus_header
{
public:
  Init_SwarmFollowerStatus_header()
  : msg_(::rosidl_runtime_cpp::MessageInitialization::SKIP)
  {}
  Init_SwarmFollowerStatus_robot_id header(::combat_robot_msgs::msg::SwarmFollowerStatus::_header_type arg)
  {
    msg_.header = std::move(arg);
    return Init_SwarmFollowerStatus_robot_id(msg_);
  }

private:
  ::combat_robot_msgs::msg::SwarmFollowerStatus msg_;
};

}  // namespace builder

}  // namespace msg

template<typename MessageType>
auto build();

template<>
inline
auto build<::combat_robot_msgs::msg::SwarmFollowerStatus>()
{
  return combat_robot_msgs::msg::builder::Init_SwarmFollowerStatus_header();
}

}  // namespace combat_robot_msgs

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__SWARM_FOLLOWER_STATUS__BUILDER_HPP_
