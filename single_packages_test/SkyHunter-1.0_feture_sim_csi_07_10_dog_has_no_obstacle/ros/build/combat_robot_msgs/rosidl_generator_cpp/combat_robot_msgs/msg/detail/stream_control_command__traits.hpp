// generated from rosidl_generator_cpp/resource/idl__traits.hpp.em
// with input from combat_robot_msgs:msg/StreamControlCommand.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/stream_control_command.hpp"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__STREAM_CONTROL_COMMAND__TRAITS_HPP_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__STREAM_CONTROL_COMMAND__TRAITS_HPP_

#include <stdint.h>

#include <sstream>
#include <string>
#include <type_traits>

#include "combat_robot_msgs/msg/detail/stream_control_command__struct.hpp"
#include "rosidl_runtime_cpp/traits.hpp"

// Include directives for member types
// Member 'header'
#include "std_msgs/msg/detail/header__traits.hpp"

namespace combat_robot_msgs
{

namespace msg
{

inline void to_flow_style_yaml(
  const StreamControlCommand & msg,
  std::ostream & out)
{
  out << "{";
  // member: header
  {
    out << "header: ";
    to_flow_style_yaml(msg.header, out);
    out << ", ";
  }

  // member: stream_command
  {
    out << "stream_command: ";
    rosidl_generator_traits::value_to_yaml(msg.stream_command, out);
    out << ", ";
  }

  // member: stream_target_robot_id
  {
    out << "stream_target_robot_id: ";
    rosidl_generator_traits::value_to_yaml(msg.stream_target_robot_id, out);
  }
  out << "}";
}  // NOLINT(readability/fn_size)

inline void to_block_style_yaml(
  const StreamControlCommand & msg,
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

  // member: stream_command
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "stream_command: ";
    rosidl_generator_traits::value_to_yaml(msg.stream_command, out);
    out << "\n";
  }

  // member: stream_target_robot_id
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "stream_target_robot_id: ";
    rosidl_generator_traits::value_to_yaml(msg.stream_target_robot_id, out);
    out << "\n";
  }
}  // NOLINT(readability/fn_size)

inline std::string to_yaml(const StreamControlCommand & msg, bool use_flow_style = false)
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
  const combat_robot_msgs::msg::StreamControlCommand & msg,
  std::ostream & out, size_t indentation = 0)
{
  combat_robot_msgs::msg::to_block_style_yaml(msg, out, indentation);
}

[[deprecated("use combat_robot_msgs::msg::to_yaml() instead")]]
inline std::string to_yaml(const combat_robot_msgs::msg::StreamControlCommand & msg)
{
  return combat_robot_msgs::msg::to_yaml(msg);
}

template<>
inline const char * data_type<combat_robot_msgs::msg::StreamControlCommand>()
{
  return "combat_robot_msgs::msg::StreamControlCommand";
}

template<>
inline const char * name<combat_robot_msgs::msg::StreamControlCommand>()
{
  return "combat_robot_msgs/msg/StreamControlCommand";
}

template<>
struct has_fixed_size<combat_robot_msgs::msg::StreamControlCommand>
  : std::integral_constant<bool, has_fixed_size<std_msgs::msg::Header>::value> {};

template<>
struct has_bounded_size<combat_robot_msgs::msg::StreamControlCommand>
  : std::integral_constant<bool, has_bounded_size<std_msgs::msg::Header>::value> {};

template<>
struct is_message<combat_robot_msgs::msg::StreamControlCommand>
  : std::true_type {};

}  // namespace rosidl_generator_traits

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__STREAM_CONTROL_COMMAND__TRAITS_HPP_
