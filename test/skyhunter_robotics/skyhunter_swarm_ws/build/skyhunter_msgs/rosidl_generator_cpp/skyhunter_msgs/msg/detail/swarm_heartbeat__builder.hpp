// generated from rosidl_generator_cpp/resource/idl__builder.hpp.em
// with input from skyhunter_msgs:msg/SwarmHeartbeat.idl
// generated code does not contain a copyright notice

#ifndef SKYHUNTER_MSGS__MSG__DETAIL__SWARM_HEARTBEAT__BUILDER_HPP_
#define SKYHUNTER_MSGS__MSG__DETAIL__SWARM_HEARTBEAT__BUILDER_HPP_

#include <algorithm>
#include <utility>

#include "skyhunter_msgs/msg/detail/swarm_heartbeat__struct.hpp"
#include "rosidl_runtime_cpp/message_initialization.hpp"


namespace skyhunter_msgs
{

namespace msg
{

namespace builder
{

class Init_SwarmHeartbeat_leader_id_num
{
public:
  explicit Init_SwarmHeartbeat_leader_id_num(::skyhunter_msgs::msg::SwarmHeartbeat & msg)
  : msg_(msg)
  {}
  ::skyhunter_msgs::msg::SwarmHeartbeat leader_id_num(::skyhunter_msgs::msg::SwarmHeartbeat::_leader_id_num_type arg)
  {
    msg_.leader_id_num = std::move(arg);
    return std::move(msg_);
  }

private:
  ::skyhunter_msgs::msg::SwarmHeartbeat msg_;
};

class Init_SwarmHeartbeat_battery_level
{
public:
  explicit Init_SwarmHeartbeat_battery_level(::skyhunter_msgs::msg::SwarmHeartbeat & msg)
  : msg_(msg)
  {}
  Init_SwarmHeartbeat_leader_id_num battery_level(::skyhunter_msgs::msg::SwarmHeartbeat::_battery_level_type arg)
  {
    msg_.battery_level = std::move(arg);
    return Init_SwarmHeartbeat_leader_id_num(msg_);
  }

private:
  ::skyhunter_msgs::msg::SwarmHeartbeat msg_;
};

class Init_SwarmHeartbeat_is_leader
{
public:
  explicit Init_SwarmHeartbeat_is_leader(::skyhunter_msgs::msg::SwarmHeartbeat & msg)
  : msg_(msg)
  {}
  Init_SwarmHeartbeat_battery_level is_leader(::skyhunter_msgs::msg::SwarmHeartbeat::_is_leader_type arg)
  {
    msg_.is_leader = std::move(arg);
    return Init_SwarmHeartbeat_battery_level(msg_);
  }

private:
  ::skyhunter_msgs::msg::SwarmHeartbeat msg_;
};

class Init_SwarmHeartbeat_term
{
public:
  explicit Init_SwarmHeartbeat_term(::skyhunter_msgs::msg::SwarmHeartbeat & msg)
  : msg_(msg)
  {}
  Init_SwarmHeartbeat_is_leader term(::skyhunter_msgs::msg::SwarmHeartbeat::_term_type arg)
  {
    msg_.term = std::move(arg);
    return Init_SwarmHeartbeat_is_leader(msg_);
  }

private:
  ::skyhunter_msgs::msg::SwarmHeartbeat msg_;
};

class Init_SwarmHeartbeat_robot_id
{
public:
  explicit Init_SwarmHeartbeat_robot_id(::skyhunter_msgs::msg::SwarmHeartbeat & msg)
  : msg_(msg)
  {}
  Init_SwarmHeartbeat_term robot_id(::skyhunter_msgs::msg::SwarmHeartbeat::_robot_id_type arg)
  {
    msg_.robot_id = std::move(arg);
    return Init_SwarmHeartbeat_term(msg_);
  }

private:
  ::skyhunter_msgs::msg::SwarmHeartbeat msg_;
};

class Init_SwarmHeartbeat_header
{
public:
  Init_SwarmHeartbeat_header()
  : msg_(::rosidl_runtime_cpp::MessageInitialization::SKIP)
  {}
  Init_SwarmHeartbeat_robot_id header(::skyhunter_msgs::msg::SwarmHeartbeat::_header_type arg)
  {
    msg_.header = std::move(arg);
    return Init_SwarmHeartbeat_robot_id(msg_);
  }

private:
  ::skyhunter_msgs::msg::SwarmHeartbeat msg_;
};

}  // namespace builder

}  // namespace msg

template<typename MessageType>
auto build();

template<>
inline
auto build<::skyhunter_msgs::msg::SwarmHeartbeat>()
{
  return skyhunter_msgs::msg::builder::Init_SwarmHeartbeat_header();
}

}  // namespace skyhunter_msgs

#endif  // SKYHUNTER_MSGS__MSG__DETAIL__SWARM_HEARTBEAT__BUILDER_HPP_
