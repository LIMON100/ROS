// generated from rosidl_generator_cpp/resource/idl__builder.hpp.em
// with input from combat_robot_msgs:msg/Waypoint.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/waypoint.hpp"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__WAYPOINT__BUILDER_HPP_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__WAYPOINT__BUILDER_HPP_

#include <algorithm>
#include <utility>

#include "combat_robot_msgs/msg/detail/waypoint__struct.hpp"
#include "rosidl_runtime_cpp/message_initialization.hpp"


namespace combat_robot_msgs
{

namespace msg
{

namespace builder
{

class Init_Waypoint_way_status
{
public:
  explicit Init_Waypoint_way_status(::combat_robot_msgs::msg::Waypoint & msg)
  : msg_(msg)
  {}
  ::combat_robot_msgs::msg::Waypoint way_status(::combat_robot_msgs::msg::Waypoint::_way_status_type arg)
  {
    msg_.way_status = std::move(arg);
    return std::move(msg_);
  }

private:
  ::combat_robot_msgs::msg::Waypoint msg_;
};

class Init_Waypoint_way_lat
{
public:
  explicit Init_Waypoint_way_lat(::combat_robot_msgs::msg::Waypoint & msg)
  : msg_(msg)
  {}
  Init_Waypoint_way_status way_lat(::combat_robot_msgs::msg::Waypoint::_way_lat_type arg)
  {
    msg_.way_lat = std::move(arg);
    return Init_Waypoint_way_status(msg_);
  }

private:
  ::combat_robot_msgs::msg::Waypoint msg_;
};

class Init_Waypoint_way_lon
{
public:
  explicit Init_Waypoint_way_lon(::combat_robot_msgs::msg::Waypoint & msg)
  : msg_(msg)
  {}
  Init_Waypoint_way_lat way_lon(::combat_robot_msgs::msg::Waypoint::_way_lon_type arg)
  {
    msg_.way_lon = std::move(arg);
    return Init_Waypoint_way_lat(msg_);
  }

private:
  ::combat_robot_msgs::msg::Waypoint msg_;
};

class Init_Waypoint_way_id
{
public:
  Init_Waypoint_way_id()
  : msg_(::rosidl_runtime_cpp::MessageInitialization::SKIP)
  {}
  Init_Waypoint_way_lon way_id(::combat_robot_msgs::msg::Waypoint::_way_id_type arg)
  {
    msg_.way_id = std::move(arg);
    return Init_Waypoint_way_lon(msg_);
  }

private:
  ::combat_robot_msgs::msg::Waypoint msg_;
};

}  // namespace builder

}  // namespace msg

template<typename MessageType>
auto build();

template<>
inline
auto build<::combat_robot_msgs::msg::Waypoint>()
{
  return combat_robot_msgs::msg::builder::Init_Waypoint_way_id();
}

}  // namespace combat_robot_msgs

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__WAYPOINT__BUILDER_HPP_
