// generated from rosidl_generator_cpp/resource/idl__traits.hpp.em
// with input from combat_robot_msgs:msg/OperationState.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/operation_state.hpp"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__OPERATION_STATE__TRAITS_HPP_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__OPERATION_STATE__TRAITS_HPP_

#include <stdint.h>

#include <sstream>
#include <string>
#include <type_traits>

#include "combat_robot_msgs/msg/detail/operation_state__struct.hpp"
#include "rosidl_runtime_cpp/traits.hpp"

namespace combat_robot_msgs
{

namespace msg
{

inline void to_flow_style_yaml(
  const OperationState & msg,
  std::ostream & out)
{
  out << "{";
  // member: state
  {
    out << "state: ";
    rosidl_generator_traits::value_to_yaml(msg.state, out);
    out << ", ";
  }

  // member: active_mode_id
  {
    out << "active_mode_id: ";
    rosidl_generator_traits::value_to_yaml(msg.active_mode_id, out);
    out << ", ";
  }

  // member: mission_status
  {
    out << "mission_status: ";
    rosidl_generator_traits::value_to_yaml(msg.mission_status, out);
    out << ", ";
  }

  // member: estop_active
  {
    out << "estop_active: ";
    rosidl_generator_traits::value_to_yaml(msg.estop_active, out);
    out << ", ";
  }

  // member: permission_request_active
  {
    out << "permission_request_active: ";
    rosidl_generator_traits::value_to_yaml(msg.permission_request_active, out);
    out << ", ";
  }

  // member: crosshair_x
  {
    out << "crosshair_x: ";
    rosidl_generator_traits::value_to_yaml(msg.crosshair_x, out);
    out << ", ";
  }

  // member: crosshair_y
  {
    out << "crosshair_y: ";
    rosidl_generator_traits::value_to_yaml(msg.crosshair_y, out);
    out << ", ";
  }

  // member: current_zoom_level
  {
    out << "current_zoom_level: ";
    rosidl_generator_traits::value_to_yaml(msg.current_zoom_level, out);
    out << ", ";
  }

  // member: gps_lat
  {
    out << "gps_lat: ";
    rosidl_generator_traits::value_to_yaml(msg.gps_lat, out);
    out << ", ";
  }

  // member: gps_lon
  {
    out << "gps_lon: ";
    rosidl_generator_traits::value_to_yaml(msg.gps_lon, out);
    out << ", ";
  }

  // member: gps_heading
  {
    out << "gps_heading: ";
    rosidl_generator_traits::value_to_yaml(msg.gps_heading, out);
    out << ", ";
  }

  // member: current_speed_mps
  {
    out << "current_speed_mps: ";
    rosidl_generator_traits::value_to_yaml(msg.current_speed_mps, out);
    out << ", ";
  }

  // member: current_waypoint_index
  {
    out << "current_waypoint_index: ";
    rosidl_generator_traits::value_to_yaml(msg.current_waypoint_index, out);
    out << ", ";
  }

  // member: total_waypoints
  {
    out << "total_waypoints: ";
    rosidl_generator_traits::value_to_yaml(msg.total_waypoints, out);
    out << ", ";
  }

  // member: progress_ratio
  {
    out << "progress_ratio: ";
    rosidl_generator_traits::value_to_yaml(msg.progress_ratio, out);
    out << ", ";
  }

  // member: distance_to_next_wp_m
  {
    out << "distance_to_next_wp_m: ";
    rosidl_generator_traits::value_to_yaml(msg.distance_to_next_wp_m, out);
    out << ", ";
  }

  // member: distance_to_goal_m
  {
    out << "distance_to_goal_m: ";
    rosidl_generator_traits::value_to_yaml(msg.distance_to_goal_m, out);
    out << ", ";
  }

  // member: error_code
  {
    out << "error_code: ";
    rosidl_generator_traits::value_to_yaml(msg.error_code, out);
  }
  out << "}";
}  // NOLINT(readability/fn_size)

inline void to_block_style_yaml(
  const OperationState & msg,
  std::ostream & out, size_t indentation = 0)
{
  // member: state
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "state: ";
    rosidl_generator_traits::value_to_yaml(msg.state, out);
    out << "\n";
  }

  // member: active_mode_id
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "active_mode_id: ";
    rosidl_generator_traits::value_to_yaml(msg.active_mode_id, out);
    out << "\n";
  }

  // member: mission_status
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "mission_status: ";
    rosidl_generator_traits::value_to_yaml(msg.mission_status, out);
    out << "\n";
  }

  // member: estop_active
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "estop_active: ";
    rosidl_generator_traits::value_to_yaml(msg.estop_active, out);
    out << "\n";
  }

  // member: permission_request_active
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "permission_request_active: ";
    rosidl_generator_traits::value_to_yaml(msg.permission_request_active, out);
    out << "\n";
  }

  // member: crosshair_x
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "crosshair_x: ";
    rosidl_generator_traits::value_to_yaml(msg.crosshair_x, out);
    out << "\n";
  }

  // member: crosshair_y
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "crosshair_y: ";
    rosidl_generator_traits::value_to_yaml(msg.crosshair_y, out);
    out << "\n";
  }

  // member: current_zoom_level
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "current_zoom_level: ";
    rosidl_generator_traits::value_to_yaml(msg.current_zoom_level, out);
    out << "\n";
  }

  // member: gps_lat
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "gps_lat: ";
    rosidl_generator_traits::value_to_yaml(msg.gps_lat, out);
    out << "\n";
  }

  // member: gps_lon
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "gps_lon: ";
    rosidl_generator_traits::value_to_yaml(msg.gps_lon, out);
    out << "\n";
  }

  // member: gps_heading
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "gps_heading: ";
    rosidl_generator_traits::value_to_yaml(msg.gps_heading, out);
    out << "\n";
  }

  // member: current_speed_mps
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "current_speed_mps: ";
    rosidl_generator_traits::value_to_yaml(msg.current_speed_mps, out);
    out << "\n";
  }

  // member: current_waypoint_index
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "current_waypoint_index: ";
    rosidl_generator_traits::value_to_yaml(msg.current_waypoint_index, out);
    out << "\n";
  }

  // member: total_waypoints
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "total_waypoints: ";
    rosidl_generator_traits::value_to_yaml(msg.total_waypoints, out);
    out << "\n";
  }

  // member: progress_ratio
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "progress_ratio: ";
    rosidl_generator_traits::value_to_yaml(msg.progress_ratio, out);
    out << "\n";
  }

  // member: distance_to_next_wp_m
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "distance_to_next_wp_m: ";
    rosidl_generator_traits::value_to_yaml(msg.distance_to_next_wp_m, out);
    out << "\n";
  }

  // member: distance_to_goal_m
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "distance_to_goal_m: ";
    rosidl_generator_traits::value_to_yaml(msg.distance_to_goal_m, out);
    out << "\n";
  }

  // member: error_code
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "error_code: ";
    rosidl_generator_traits::value_to_yaml(msg.error_code, out);
    out << "\n";
  }
}  // NOLINT(readability/fn_size)

inline std::string to_yaml(const OperationState & msg, bool use_flow_style = false)
{
  std::ostringstream out;
  if (use_flow_style) {
    to_flow_style_yaml(msg, out);
  } else {
    to_block_style_yaml(msg, out);
  }
  return out.str();
}

}  // namespace msg

}  // namespace combat_robot_msgs

namespace rosidl_generator_traits
{

[[deprecated("use combat_robot_msgs::msg::to_block_style_yaml() instead")]]
inline void to_yaml(
  const combat_robot_msgs::msg::OperationState & msg,
  std::ostream & out, size_t indentation = 0)
{
  combat_robot_msgs::msg::to_block_style_yaml(msg, out, indentation);
}

[[deprecated("use combat_robot_msgs::msg::to_yaml() instead")]]
inline std::string to_yaml(const combat_robot_msgs::msg::OperationState & msg)
{
  return combat_robot_msgs::msg::to_yaml(msg);
}

template<>
inline const char * data_type<combat_robot_msgs::msg::OperationState>()
{
  return "combat_robot_msgs::msg::OperationState";
}

template<>
inline const char * name<combat_robot_msgs::msg::OperationState>()
{
  return "combat_robot_msgs/msg/OperationState";
}

template<>
struct has_fixed_size<combat_robot_msgs::msg::OperationState>
  : std::integral_constant<bool, true> {};

template<>
struct has_bounded_size<combat_robot_msgs::msg::OperationState>
  : std::integral_constant<bool, true> {};

template<>
struct is_message<combat_robot_msgs::msg::OperationState>
  : std::true_type {};

}  // namespace rosidl_generator_traits

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__OPERATION_STATE__TRAITS_HPP_
