// generated from rosidl_generator_cpp/resource/idl__traits.hpp.em
// with input from combat_robot_msgs:msg/SwarmPathCommand.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/swarm_path_command.hpp"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__SWARM_PATH_COMMAND__TRAITS_HPP_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__SWARM_PATH_COMMAND__TRAITS_HPP_

#include <stdint.h>

#include <sstream>
#include <string>
#include <type_traits>

#include "combat_robot_msgs/msg/detail/swarm_path_command__struct.hpp"
#include "rosidl_runtime_cpp/traits.hpp"

// Include directives for member types
// Member 'header'
#include "std_msgs/msg/detail/header__traits.hpp"

namespace combat_robot_msgs
{

namespace msg
{

inline void to_flow_style_yaml(
  const SwarmPathCommand & msg,
  std::ostream & out)
{
  out << "{";
  // member: header
  {
    out << "header: ";
    to_flow_style_yaml(msg.header, out);
    out << ", ";
  }

  // member: command
  {
    out << "command: ";
    rosidl_generator_traits::value_to_yaml(msg.command, out);
    out << ", ";
  }

  // member: num_waypoints
  {
    out << "num_waypoints: ";
    rosidl_generator_traits::value_to_yaml(msg.num_waypoints, out);
    out << ", ";
  }

  // member: path_json
  {
    out << "path_json: ";
    rosidl_generator_traits::value_to_yaml(msg.path_json, out);
  }
  out << "}";
}  // NOLINT(readability/fn_size)

inline void to_block_style_yaml(
  const SwarmPathCommand & msg,
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

  // member: command
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "command: ";
    rosidl_generator_traits::value_to_yaml(msg.command, out);
    out << "\n";
  }

  // member: num_waypoints
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "num_waypoints: ";
    rosidl_generator_traits::value_to_yaml(msg.num_waypoints, out);
    out << "\n";
  }

  // member: path_json
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "path_json: ";
    rosidl_generator_traits::value_to_yaml(msg.path_json, out);
    out << "\n";
  }
}  // NOLINT(readability/fn_size)

inline std::string to_yaml(const SwarmPathCommand & msg, bool use_flow_style = false)
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
  const combat_robot_msgs::msg::SwarmPathCommand & msg,
  std::ostream & out, size_t indentation = 0)
{
  combat_robot_msgs::msg::to_block_style_yaml(msg, out, indentation);
}

[[deprecated("use combat_robot_msgs::msg::to_yaml() instead")]]
inline std::string to_yaml(const combat_robot_msgs::msg::SwarmPathCommand & msg)
{
  return combat_robot_msgs::msg::to_yaml(msg);
}

template<>
inline const char * data_type<combat_robot_msgs::msg::SwarmPathCommand>()
{
  return "combat_robot_msgs::msg::SwarmPathCommand";
}

template<>
inline const char * name<combat_robot_msgs::msg::SwarmPathCommand>()
{
  return "combat_robot_msgs/msg/SwarmPathCommand";
}

template<>
struct has_fixed_size<combat_robot_msgs::msg::SwarmPathCommand>
  : std::integral_constant<bool, false> {};

template<>
struct has_bounded_size<combat_robot_msgs::msg::SwarmPathCommand>
  : std::integral_constant<bool, false> {};

template<>
struct is_message<combat_robot_msgs::msg::SwarmPathCommand>
  : std::true_type {};

}  // namespace rosidl_generator_traits

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__SWARM_PATH_COMMAND__TRAITS_HPP_
