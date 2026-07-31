// generated from rosidl_generator_cpp/resource/idl__traits.hpp.em
// with input from combat_robot_msgs:msg/LidarStatus.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/lidar_status.hpp"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__LIDAR_STATUS__TRAITS_HPP_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__LIDAR_STATUS__TRAITS_HPP_

#include <stdint.h>

#include <sstream>
#include <string>
#include <type_traits>

#include "combat_robot_msgs/msg/detail/lidar_status__struct.hpp"
#include "rosidl_runtime_cpp/traits.hpp"

// Include directives for member types
// Member 'header'
#include "std_msgs/msg/detail/header__traits.hpp"

namespace combat_robot_msgs
{

namespace msg
{

inline void to_flow_style_yaml(
  const LidarStatus & msg,
  std::ostream & out)
{
  out << "{";
  // member: header
  {
    out << "header: ";
    to_flow_style_yaml(msg.header, out);
    out << ", ";
  }

  // member: status
  {
    out << "status: ";
    rosidl_generator_traits::value_to_yaml(msg.status, out);
    out << ", ";
  }

  // member: last_scan_point_count
  {
    out << "last_scan_point_count: ";
    rosidl_generator_traits::value_to_yaml(msg.last_scan_point_count, out);
    out << ", ";
  }

  // member: scan_rate_hz
  {
    out << "scan_rate_hz: ";
    rosidl_generator_traits::value_to_yaml(msg.scan_rate_hz, out);
    out << ", ";
  }

  // member: obstacle_detected
  {
    out << "obstacle_detected: ";
    rosidl_generator_traits::value_to_yaml(msg.obstacle_detected, out);
    out << ", ";
  }

  // member: min_obstacle_distance_m
  {
    out << "min_obstacle_distance_m: ";
    rosidl_generator_traits::value_to_yaml(msg.min_obstacle_distance_m, out);
  }
  out << "}";
}  // NOLINT(readability/fn_size)

inline void to_block_style_yaml(
  const LidarStatus & msg,
  std::ostream & out, size_t indentation = 0)
{
  // member: header
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "header:\n";
    to_block_style_yaml(msg.header, out, indentation + 2);
  }

  // member: status
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "status: ";
    rosidl_generator_traits::value_to_yaml(msg.status, out);
    out << "\n";
  }

  // member: last_scan_point_count
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "last_scan_point_count: ";
    rosidl_generator_traits::value_to_yaml(msg.last_scan_point_count, out);
    out << "\n";
  }

  // member: scan_rate_hz
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "scan_rate_hz: ";
    rosidl_generator_traits::value_to_yaml(msg.scan_rate_hz, out);
    out << "\n";
  }

  // member: obstacle_detected
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "obstacle_detected: ";
    rosidl_generator_traits::value_to_yaml(msg.obstacle_detected, out);
    out << "\n";
  }

  // member: min_obstacle_distance_m
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "min_obstacle_distance_m: ";
    rosidl_generator_traits::value_to_yaml(msg.min_obstacle_distance_m, out);
    out << "\n";
  }
}  // NOLINT(readability/fn_size)

inline std::string to_yaml(const LidarStatus & msg, bool use_flow_style = false)
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
  const combat_robot_msgs::msg::LidarStatus & msg,
  std::ostream & out, size_t indentation = 0)
{
  combat_robot_msgs::msg::to_block_style_yaml(msg, out, indentation);
}

[[deprecated("use combat_robot_msgs::msg::to_yaml() instead")]]
inline std::string to_yaml(const combat_robot_msgs::msg::LidarStatus & msg)
{
  return combat_robot_msgs::msg::to_yaml(msg);
}

template<>
inline const char * data_type<combat_robot_msgs::msg::LidarStatus>()
{
  return "combat_robot_msgs::msg::LidarStatus";
}

template<>
inline const char * name<combat_robot_msgs::msg::LidarStatus>()
{
  return "combat_robot_msgs/msg/LidarStatus";
}

template<>
struct has_fixed_size<combat_robot_msgs::msg::LidarStatus>
  : std::integral_constant<bool, has_fixed_size<std_msgs::msg::Header>::value> {};

template<>
struct has_bounded_size<combat_robot_msgs::msg::LidarStatus>
  : std::integral_constant<bool, has_bounded_size<std_msgs::msg::Header>::value> {};

template<>
struct is_message<combat_robot_msgs::msg::LidarStatus>
  : std::true_type {};

}  // namespace rosidl_generator_traits

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__LIDAR_STATUS__TRAITS_HPP_
