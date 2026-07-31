// generated from rosidl_generator_cpp/resource/idl__builder.hpp.em
// with input from combat_robot_msgs:msg/WaypointList.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/waypoint_list.hpp"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__WAYPOINT_LIST__BUILDER_HPP_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__WAYPOINT_LIST__BUILDER_HPP_

#include <algorithm>
#include <utility>

#include "combat_robot_msgs/msg/detail/waypoint_list__struct.hpp"
#include "rosidl_runtime_cpp/message_initialization.hpp"


namespace combat_robot_msgs
{

namespace msg
{

namespace builder
{

class Init_WaypointList_waypoints
{
public:
  explicit Init_WaypointList_waypoints(::combat_robot_msgs::msg::WaypointList & msg)
  : msg_(msg)
  {}
  ::combat_robot_msgs::msg::WaypointList waypoints(::combat_robot_msgs::msg::WaypointList::_waypoints_type arg)
  {
    msg_.waypoints = std::move(arg);
    return std::move(msg_);
  }

private:
  ::combat_robot_msgs::msg::WaypointList msg_;
};

class Init_WaypointList_mission_status
{
public:
  explicit Init_WaypointList_mission_status(::combat_robot_msgs::msg::WaypointList & msg)
  : msg_(msg)
  {}
  Init_WaypointList_waypoints mission_status(::combat_robot_msgs::msg::WaypointList::_mission_status_type arg)
  {
    msg_.mission_status = std::move(arg);
    return Init_WaypointList_waypoints(msg_);
  }

private:
  ::combat_robot_msgs::msg::WaypointList msg_;
};

class Init_WaypointList_mission_id
{
public:
  explicit Init_WaypointList_mission_id(::combat_robot_msgs::msg::WaypointList & msg)
  : msg_(msg)
  {}
  Init_WaypointList_mission_status mission_id(::combat_robot_msgs::msg::WaypointList::_mission_id_type arg)
  {
    msg_.mission_id = std::move(arg);
    return Init_WaypointList_mission_status(msg_);
  }

private:
  ::combat_robot_msgs::msg::WaypointList msg_;
};

class Init_WaypointList_formation
{
public:
  explicit Init_WaypointList_formation(::combat_robot_msgs::msg::WaypointList & msg)
  : msg_(msg)
  {}
  Init_WaypointList_mission_id formation(::combat_robot_msgs::msg::WaypointList::_formation_type arg)
  {
    msg_.formation = std::move(arg);
    return Init_WaypointList_mission_id(msg_);
  }

private:
  ::combat_robot_msgs::msg::WaypointList msg_;
};

class Init_WaypointList_mode
{
public:
  Init_WaypointList_mode()
  : msg_(::rosidl_runtime_cpp::MessageInitialization::SKIP)
  {}
  Init_WaypointList_formation mode(::combat_robot_msgs::msg::WaypointList::_mode_type arg)
  {
    msg_.mode = std::move(arg);
    return Init_WaypointList_formation(msg_);
  }

private:
  ::combat_robot_msgs::msg::WaypointList msg_;
};

}  // namespace builder

}  // namespace msg

template<typename MessageType>
auto build();

template<>
inline
auto build<::combat_robot_msgs::msg::WaypointList>()
{
  return combat_robot_msgs::msg::builder::Init_WaypointList_mode();
}

}  // namespace combat_robot_msgs

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__WAYPOINT_LIST__BUILDER_HPP_
