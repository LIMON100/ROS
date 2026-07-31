// generated from rosidl_generator_cpp/resource/idl__traits.hpp.em
// with input from combat_robot_msgs:msg/Waypoint.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/waypoint.hpp"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__WAYPOINT__TRAITS_HPP_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__WAYPOINT__TRAITS_HPP_

#include <stdint.h>

#include <sstream>
#include <string>
#include <type_traits>

#include "combat_robot_msgs/msg/detail/waypoint__struct.hpp"
#include "rosidl_runtime_cpp/traits.hpp"

namespace combat_robot_msgs
{

namespace msg
{

inline void to_flow_style_yaml(
  const Waypoint & msg,
  std::ostream & out)
{
  out << "{";
  // member: way_id
  {
    out << "way_id: ";
    rosidl_generator_traits::value_to_yaml(msg.way_id, out);
    out << ", ";
  }

  // member: way_lon
  {
    out << "way_lon: ";
    rosidl_generator_traits::value_to_yaml(msg.way_lon, out);
    out << ", ";
  }

  // member: way_lat
  {
    out << "way_lat: ";
    rosidl_generator_traits::value_to_yaml(msg.way_lat, out);
    out << ", ";
  }

  // member: way_status
  {
    out << "way_status: ";
    rosidl_generator_traits::value_to_yaml(msg.way_status, out);
  }
  out << "}";
}  // NOLINT(readability/fn_size)

inline void to_block_style_yaml(
  const Waypoint & msg,
  std::ostream & out, size_t indentation = 0)
{
  // member: way_id
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "way_id: ";
    rosidl_generator_traits::value_to_yaml(msg.way_id, out);
    out << "\n";
  }

  // member: way_lon
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "way_lon: ";
    rosidl_generator_traits::value_to_yaml(msg.way_lon, out);
    out << "\n";
  }

  // member: way_lat
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "way_lat: ";
    rosidl_generator_traits::value_to_yaml(msg.way_lat, out);
    out << "\n";
  }

  // member: way_status
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "way_status: ";
    rosidl_generator_traits::value_to_yaml(msg.way_status, out);
    out << "\n";
  }
}  // NOLINT(readability/fn_size)

inline std::string to_yaml(const Waypoint & msg, bool use_flow_style = false)
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
  const combat_robot_msgs::msg::Waypoint & msg,
  std::ostream & out, size_t indentation = 0)
{
  combat_robot_msgs::msg::to_block_style_yaml(msg, out, indentation);
}

[[deprecated("use combat_robot_msgs::msg::to_yaml() instead")]]
inline std::string to_yaml(const combat_robot_msgs::msg::Waypoint & msg)
{
  return combat_robot_msgs::msg::to_yaml(msg);
}

template<>
inline const char * data_type<combat_robot_msgs::msg::Waypoint>()
{
  return "combat_robot_msgs::msg::Waypoint";
}

template<>
inline const char * name<combat_robot_msgs::msg::Waypoint>()
{
  return "combat_robot_msgs/msg/Waypoint";
}

template<>
struct has_fixed_size<combat_robot_msgs::msg::Waypoint>
  : std::integral_constant<bool, true> {};

template<>
struct has_bounded_size<combat_robot_msgs::msg::Waypoint>
  : std::integral_constant<bool, true> {};

template<>
struct is_message<combat_robot_msgs::msg::Waypoint>
  : std::true_type {};

}  // namespace rosidl_generator_traits

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__WAYPOINT__TRAITS_HPP_
