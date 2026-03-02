// generated from rosidl_generator_cpp/resource/idl__builder.hpp.em
// with input from skyhunter_msgs:msg/LeaderState.idl
// generated code does not contain a copyright notice

#ifndef SKYHUNTER_MSGS__MSG__DETAIL__LEADER_STATE__BUILDER_HPP_
#define SKYHUNTER_MSGS__MSG__DETAIL__LEADER_STATE__BUILDER_HPP_

#include <algorithm>
#include <utility>

#include "skyhunter_msgs/msg/detail/leader_state__struct.hpp"
#include "rosidl_runtime_cpp/message_initialization.hpp"


namespace skyhunter_msgs
{

namespace msg
{

namespace builder
{

class Init_LeaderState_current_waypoint_index
{
public:
  explicit Init_LeaderState_current_waypoint_index(::skyhunter_msgs::msg::LeaderState & msg)
  : msg_(msg)
  {}
  ::skyhunter_msgs::msg::LeaderState current_waypoint_index(::skyhunter_msgs::msg::LeaderState::_current_waypoint_index_type arg)
  {
    msg_.current_waypoint_index = std::move(arg);
    return std::move(msg_);
  }

private:
  ::skyhunter_msgs::msg::LeaderState msg_;
};

class Init_LeaderState_formation_type
{
public:
  explicit Init_LeaderState_formation_type(::skyhunter_msgs::msg::LeaderState & msg)
  : msg_(msg)
  {}
  Init_LeaderState_current_waypoint_index formation_type(::skyhunter_msgs::msg::LeaderState::_formation_type_type arg)
  {
    msg_.formation_type = std::move(arg);
    return Init_LeaderState_current_waypoint_index(msg_);
  }

private:
  ::skyhunter_msgs::msg::LeaderState msg_;
};

class Init_LeaderState_swarm_state
{
public:
  explicit Init_LeaderState_swarm_state(::skyhunter_msgs::msg::LeaderState & msg)
  : msg_(msg)
  {}
  Init_LeaderState_formation_type swarm_state(::skyhunter_msgs::msg::LeaderState::_swarm_state_type arg)
  {
    msg_.swarm_state = std::move(arg);
    return Init_LeaderState_formation_type(msg_);
  }

private:
  ::skyhunter_msgs::msg::LeaderState msg_;
};

class Init_LeaderState_formation_state
{
public:
  explicit Init_LeaderState_formation_state(::skyhunter_msgs::msg::LeaderState & msg)
  : msg_(msg)
  {}
  Init_LeaderState_swarm_state formation_state(::skyhunter_msgs::msg::LeaderState::_formation_state_type arg)
  {
    msg_.formation_state = std::move(arg);
    return Init_LeaderState_swarm_state(msg_);
  }

private:
  ::skyhunter_msgs::msg::LeaderState msg_;
};

class Init_LeaderState_formation_mode
{
public:
  explicit Init_LeaderState_formation_mode(::skyhunter_msgs::msg::LeaderState & msg)
  : msg_(msg)
  {}
  Init_LeaderState_formation_state formation_mode(::skyhunter_msgs::msg::LeaderState::_formation_mode_type arg)
  {
    msg_.formation_mode = std::move(arg);
    return Init_LeaderState_formation_state(msg_);
  }

private:
  ::skyhunter_msgs::msg::LeaderState msg_;
};

class Init_LeaderState_next_waypoints
{
public:
  explicit Init_LeaderState_next_waypoints(::skyhunter_msgs::msg::LeaderState & msg)
  : msg_(msg)
  {}
  Init_LeaderState_formation_mode next_waypoints(::skyhunter_msgs::msg::LeaderState::_next_waypoints_type arg)
  {
    msg_.next_waypoints = std::move(arg);
    return Init_LeaderState_formation_mode(msg_);
  }

private:
  ::skyhunter_msgs::msg::LeaderState msg_;
};

class Init_LeaderState_velocity
{
public:
  explicit Init_LeaderState_velocity(::skyhunter_msgs::msg::LeaderState & msg)
  : msg_(msg)
  {}
  Init_LeaderState_next_waypoints velocity(::skyhunter_msgs::msg::LeaderState::_velocity_type arg)
  {
    msg_.velocity = std::move(arg);
    return Init_LeaderState_next_waypoints(msg_);
  }

private:
  ::skyhunter_msgs::msg::LeaderState msg_;
};

class Init_LeaderState_pose
{
public:
  explicit Init_LeaderState_pose(::skyhunter_msgs::msg::LeaderState & msg)
  : msg_(msg)
  {}
  Init_LeaderState_velocity pose(::skyhunter_msgs::msg::LeaderState::_pose_type arg)
  {
    msg_.pose = std::move(arg);
    return Init_LeaderState_velocity(msg_);
  }

private:
  ::skyhunter_msgs::msg::LeaderState msg_;
};

class Init_LeaderState_header
{
public:
  Init_LeaderState_header()
  : msg_(::rosidl_runtime_cpp::MessageInitialization::SKIP)
  {}
  Init_LeaderState_pose header(::skyhunter_msgs::msg::LeaderState::_header_type arg)
  {
    msg_.header = std::move(arg);
    return Init_LeaderState_pose(msg_);
  }

private:
  ::skyhunter_msgs::msg::LeaderState msg_;
};

}  // namespace builder

}  // namespace msg

template<typename MessageType>
auto build();

template<>
inline
auto build<::skyhunter_msgs::msg::LeaderState>()
{
  return skyhunter_msgs::msg::builder::Init_LeaderState_header();
}

}  // namespace skyhunter_msgs

#endif  // SKYHUNTER_MSGS__MSG__DETAIL__LEADER_STATE__BUILDER_HPP_
