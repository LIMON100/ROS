// generated from rosidl_generator_cpp/resource/idl__builder.hpp.em
// with input from combat_robot_msgs:msg/OperationState.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/operation_state.hpp"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__OPERATION_STATE__BUILDER_HPP_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__OPERATION_STATE__BUILDER_HPP_

#include <algorithm>
#include <utility>

#include "combat_robot_msgs/msg/detail/operation_state__struct.hpp"
#include "rosidl_runtime_cpp/message_initialization.hpp"


namespace combat_robot_msgs
{

namespace msg
{

namespace builder
{

class Init_OperationState_error_code
{
public:
  explicit Init_OperationState_error_code(::combat_robot_msgs::msg::OperationState & msg)
  : msg_(msg)
  {}
  ::combat_robot_msgs::msg::OperationState error_code(::combat_robot_msgs::msg::OperationState::_error_code_type arg)
  {
    msg_.error_code = std::move(arg);
    return std::move(msg_);
  }

private:
  ::combat_robot_msgs::msg::OperationState msg_;
};

class Init_OperationState_distance_to_goal_m
{
public:
  explicit Init_OperationState_distance_to_goal_m(::combat_robot_msgs::msg::OperationState & msg)
  : msg_(msg)
  {}
  Init_OperationState_error_code distance_to_goal_m(::combat_robot_msgs::msg::OperationState::_distance_to_goal_m_type arg)
  {
    msg_.distance_to_goal_m = std::move(arg);
    return Init_OperationState_error_code(msg_);
  }

private:
  ::combat_robot_msgs::msg::OperationState msg_;
};

class Init_OperationState_distance_to_next_wp_m
{
public:
  explicit Init_OperationState_distance_to_next_wp_m(::combat_robot_msgs::msg::OperationState & msg)
  : msg_(msg)
  {}
  Init_OperationState_distance_to_goal_m distance_to_next_wp_m(::combat_robot_msgs::msg::OperationState::_distance_to_next_wp_m_type arg)
  {
    msg_.distance_to_next_wp_m = std::move(arg);
    return Init_OperationState_distance_to_goal_m(msg_);
  }

private:
  ::combat_robot_msgs::msg::OperationState msg_;
};

class Init_OperationState_progress_ratio
{
public:
  explicit Init_OperationState_progress_ratio(::combat_robot_msgs::msg::OperationState & msg)
  : msg_(msg)
  {}
  Init_OperationState_distance_to_next_wp_m progress_ratio(::combat_robot_msgs::msg::OperationState::_progress_ratio_type arg)
  {
    msg_.progress_ratio = std::move(arg);
    return Init_OperationState_distance_to_next_wp_m(msg_);
  }

private:
  ::combat_robot_msgs::msg::OperationState msg_;
};

class Init_OperationState_total_waypoints
{
public:
  explicit Init_OperationState_total_waypoints(::combat_robot_msgs::msg::OperationState & msg)
  : msg_(msg)
  {}
  Init_OperationState_progress_ratio total_waypoints(::combat_robot_msgs::msg::OperationState::_total_waypoints_type arg)
  {
    msg_.total_waypoints = std::move(arg);
    return Init_OperationState_progress_ratio(msg_);
  }

private:
  ::combat_robot_msgs::msg::OperationState msg_;
};

class Init_OperationState_current_waypoint_index
{
public:
  explicit Init_OperationState_current_waypoint_index(::combat_robot_msgs::msg::OperationState & msg)
  : msg_(msg)
  {}
  Init_OperationState_total_waypoints current_waypoint_index(::combat_robot_msgs::msg::OperationState::_current_waypoint_index_type arg)
  {
    msg_.current_waypoint_index = std::move(arg);
    return Init_OperationState_total_waypoints(msg_);
  }

private:
  ::combat_robot_msgs::msg::OperationState msg_;
};

class Init_OperationState_current_speed_mps
{
public:
  explicit Init_OperationState_current_speed_mps(::combat_robot_msgs::msg::OperationState & msg)
  : msg_(msg)
  {}
  Init_OperationState_current_waypoint_index current_speed_mps(::combat_robot_msgs::msg::OperationState::_current_speed_mps_type arg)
  {
    msg_.current_speed_mps = std::move(arg);
    return Init_OperationState_current_waypoint_index(msg_);
  }

private:
  ::combat_robot_msgs::msg::OperationState msg_;
};

class Init_OperationState_gps_heading
{
public:
  explicit Init_OperationState_gps_heading(::combat_robot_msgs::msg::OperationState & msg)
  : msg_(msg)
  {}
  Init_OperationState_current_speed_mps gps_heading(::combat_robot_msgs::msg::OperationState::_gps_heading_type arg)
  {
    msg_.gps_heading = std::move(arg);
    return Init_OperationState_current_speed_mps(msg_);
  }

private:
  ::combat_robot_msgs::msg::OperationState msg_;
};

class Init_OperationState_gps_lon
{
public:
  explicit Init_OperationState_gps_lon(::combat_robot_msgs::msg::OperationState & msg)
  : msg_(msg)
  {}
  Init_OperationState_gps_heading gps_lon(::combat_robot_msgs::msg::OperationState::_gps_lon_type arg)
  {
    msg_.gps_lon = std::move(arg);
    return Init_OperationState_gps_heading(msg_);
  }

private:
  ::combat_robot_msgs::msg::OperationState msg_;
};

class Init_OperationState_gps_lat
{
public:
  explicit Init_OperationState_gps_lat(::combat_robot_msgs::msg::OperationState & msg)
  : msg_(msg)
  {}
  Init_OperationState_gps_lon gps_lat(::combat_robot_msgs::msg::OperationState::_gps_lat_type arg)
  {
    msg_.gps_lat = std::move(arg);
    return Init_OperationState_gps_lon(msg_);
  }

private:
  ::combat_robot_msgs::msg::OperationState msg_;
};

class Init_OperationState_current_zoom_level
{
public:
  explicit Init_OperationState_current_zoom_level(::combat_robot_msgs::msg::OperationState & msg)
  : msg_(msg)
  {}
  Init_OperationState_gps_lat current_zoom_level(::combat_robot_msgs::msg::OperationState::_current_zoom_level_type arg)
  {
    msg_.current_zoom_level = std::move(arg);
    return Init_OperationState_gps_lat(msg_);
  }

private:
  ::combat_robot_msgs::msg::OperationState msg_;
};

class Init_OperationState_crosshair_y
{
public:
  explicit Init_OperationState_crosshair_y(::combat_robot_msgs::msg::OperationState & msg)
  : msg_(msg)
  {}
  Init_OperationState_current_zoom_level crosshair_y(::combat_robot_msgs::msg::OperationState::_crosshair_y_type arg)
  {
    msg_.crosshair_y = std::move(arg);
    return Init_OperationState_current_zoom_level(msg_);
  }

private:
  ::combat_robot_msgs::msg::OperationState msg_;
};

class Init_OperationState_crosshair_x
{
public:
  explicit Init_OperationState_crosshair_x(::combat_robot_msgs::msg::OperationState & msg)
  : msg_(msg)
  {}
  Init_OperationState_crosshair_y crosshair_x(::combat_robot_msgs::msg::OperationState::_crosshair_x_type arg)
  {
    msg_.crosshair_x = std::move(arg);
    return Init_OperationState_crosshair_y(msg_);
  }

private:
  ::combat_robot_msgs::msg::OperationState msg_;
};

class Init_OperationState_permission_request_active
{
public:
  explicit Init_OperationState_permission_request_active(::combat_robot_msgs::msg::OperationState & msg)
  : msg_(msg)
  {}
  Init_OperationState_crosshair_x permission_request_active(::combat_robot_msgs::msg::OperationState::_permission_request_active_type arg)
  {
    msg_.permission_request_active = std::move(arg);
    return Init_OperationState_crosshair_x(msg_);
  }

private:
  ::combat_robot_msgs::msg::OperationState msg_;
};

class Init_OperationState_estop_active
{
public:
  explicit Init_OperationState_estop_active(::combat_robot_msgs::msg::OperationState & msg)
  : msg_(msg)
  {}
  Init_OperationState_permission_request_active estop_active(::combat_robot_msgs::msg::OperationState::_estop_active_type arg)
  {
    msg_.estop_active = std::move(arg);
    return Init_OperationState_permission_request_active(msg_);
  }

private:
  ::combat_robot_msgs::msg::OperationState msg_;
};

class Init_OperationState_mission_status
{
public:
  explicit Init_OperationState_mission_status(::combat_robot_msgs::msg::OperationState & msg)
  : msg_(msg)
  {}
  Init_OperationState_estop_active mission_status(::combat_robot_msgs::msg::OperationState::_mission_status_type arg)
  {
    msg_.mission_status = std::move(arg);
    return Init_OperationState_estop_active(msg_);
  }

private:
  ::combat_robot_msgs::msg::OperationState msg_;
};

class Init_OperationState_active_mode_id
{
public:
  explicit Init_OperationState_active_mode_id(::combat_robot_msgs::msg::OperationState & msg)
  : msg_(msg)
  {}
  Init_OperationState_mission_status active_mode_id(::combat_robot_msgs::msg::OperationState::_active_mode_id_type arg)
  {
    msg_.active_mode_id = std::move(arg);
    return Init_OperationState_mission_status(msg_);
  }

private:
  ::combat_robot_msgs::msg::OperationState msg_;
};

class Init_OperationState_state
{
public:
  Init_OperationState_state()
  : msg_(::rosidl_runtime_cpp::MessageInitialization::SKIP)
  {}
  Init_OperationState_active_mode_id state(::combat_robot_msgs::msg::OperationState::_state_type arg)
  {
    msg_.state = std::move(arg);
    return Init_OperationState_active_mode_id(msg_);
  }

private:
  ::combat_robot_msgs::msg::OperationState msg_;
};

}  // namespace builder

}  // namespace msg

template<typename MessageType>
auto build();

template<>
inline
auto build<::combat_robot_msgs::msg::OperationState>()
{
  return combat_robot_msgs::msg::builder::Init_OperationState_state();
}

}  // namespace combat_robot_msgs

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__OPERATION_STATE__BUILDER_HPP_
