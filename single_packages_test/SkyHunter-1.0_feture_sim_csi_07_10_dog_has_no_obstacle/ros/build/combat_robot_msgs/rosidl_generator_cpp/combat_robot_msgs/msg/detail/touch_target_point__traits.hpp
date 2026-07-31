// generated from rosidl_generator_cpp/resource/idl__traits.hpp.em
// with input from combat_robot_msgs:msg/TouchTargetPoint.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/touch_target_point.hpp"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__TOUCH_TARGET_POINT__TRAITS_HPP_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__TOUCH_TARGET_POINT__TRAITS_HPP_

#include <stdint.h>

#include <sstream>
#include <string>
#include <type_traits>

#include "combat_robot_msgs/msg/detail/touch_target_point__struct.hpp"
#include "rosidl_runtime_cpp/traits.hpp"

namespace combat_robot_msgs
{

namespace msg
{

inline void to_flow_style_yaml(
  const TouchTargetPoint & msg,
  std::ostream & out)
{
  out << "{";
  // member: touch_x
  {
    out << "touch_x: ";
    rosidl_generator_traits::value_to_yaml(msg.touch_x, out);
    out << ", ";
  }

  // member: touch_y
  {
    out << "touch_y: ";
    rosidl_generator_traits::value_to_yaml(msg.touch_y, out);
  }
  out << "}";
}  // NOLINT(readability/fn_size)

inline void to_block_style_yaml(
  const TouchTargetPoint & msg,
  std::ostream & out, size_t indentation = 0)
{
  // member: touch_x
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "touch_x: ";
    rosidl_generator_traits::value_to_yaml(msg.touch_x, out);
    out << "\n";
  }

  // member: touch_y
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "touch_y: ";
    rosidl_generator_traits::value_to_yaml(msg.touch_y, out);
    out << "\n";
  }
}  // NOLINT(readability/fn_size)

inline std::string to_yaml(const TouchTargetPoint & msg, bool use_flow_style = false)
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
  const combat_robot_msgs::msg::TouchTargetPoint & msg,
  std::ostream & out, size_t indentation = 0)
{
  combat_robot_msgs::msg::to_block_style_yaml(msg, out, indentation);
}

[[deprecated("use combat_robot_msgs::msg::to_yaml() instead")]]
inline std::string to_yaml(const combat_robot_msgs::msg::TouchTargetPoint & msg)
{
  return combat_robot_msgs::msg::to_yaml(msg);
}

template<>
inline const char * data_type<combat_robot_msgs::msg::TouchTargetPoint>()
{
  return "combat_robot_msgs::msg::TouchTargetPoint";
}

template<>
inline const char * name<combat_robot_msgs::msg::TouchTargetPoint>()
{
  return "combat_robot_msgs/msg/TouchTargetPoint";
}

template<>
struct has_fixed_size<combat_robot_msgs::msg::TouchTargetPoint>
  : std::integral_constant<bool, true> {};

template<>
struct has_bounded_size<combat_robot_msgs::msg::TouchTargetPoint>
  : std::integral_constant<bool, true> {};

template<>
struct is_message<combat_robot_msgs::msg::TouchTargetPoint>
  : std::true_type {};

}  // namespace rosidl_generator_traits

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__TOUCH_TARGET_POINT__TRAITS_HPP_
