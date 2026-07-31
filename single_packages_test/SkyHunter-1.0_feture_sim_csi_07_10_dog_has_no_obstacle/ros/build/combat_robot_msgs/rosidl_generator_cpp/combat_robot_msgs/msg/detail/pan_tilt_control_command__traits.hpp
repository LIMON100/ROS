// generated from rosidl_generator_cpp/resource/idl__traits.hpp.em
// with input from combat_robot_msgs:msg/PanTiltControlCommand.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/pan_tilt_control_command.hpp"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__PAN_TILT_CONTROL_COMMAND__TRAITS_HPP_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__PAN_TILT_CONTROL_COMMAND__TRAITS_HPP_

#include <stdint.h>

#include <sstream>
#include <string>
#include <type_traits>

#include "combat_robot_msgs/msg/detail/pan_tilt_control_command__struct.hpp"
#include "rosidl_runtime_cpp/traits.hpp"

// Include directives for member types
// Member 'header'
#include "std_msgs/msg/detail/header__traits.hpp"

namespace combat_robot_msgs
{

namespace msg
{

inline void to_flow_style_yaml(
  const PanTiltControlCommand & msg,
  std::ostream & out)
{
  out << "{";
  // member: header
  {
    out << "header: ";
    to_flow_style_yaml(msg.header, out);
    out << ", ";
  }

  // member: control_mode
  {
    out << "control_mode: ";
    rosidl_generator_traits::value_to_yaml(msg.control_mode, out);
    out << ", ";
  }

  // member: horizontal_angle
  {
    out << "horizontal_angle: ";
    rosidl_generator_traits::value_to_yaml(msg.horizontal_angle, out);
    out << ", ";
  }

  // member: vertical_angle
  {
    out << "vertical_angle: ";
    rosidl_generator_traits::value_to_yaml(msg.vertical_angle, out);
    out << ", ";
  }

  // member: pan_speed
  {
    out << "pan_speed: ";
    rosidl_generator_traits::value_to_yaml(msg.pan_speed, out);
    out << ", ";
  }

  // member: tilt_speed
  {
    out << "tilt_speed: ";
    rosidl_generator_traits::value_to_yaml(msg.tilt_speed, out);
    out << ", ";
  }

  // member: pan_dir
  {
    out << "pan_dir: ";
    rosidl_generator_traits::value_to_yaml(msg.pan_dir, out);
    out << ", ";
  }

  // member: tilt_dir
  {
    out << "tilt_dir: ";
    rosidl_generator_traits::value_to_yaml(msg.tilt_dir, out);
  }
  out << "}";
}  // NOLINT(readability/fn_size)

inline void to_block_style_yaml(
  const PanTiltControlCommand & msg,
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

  // member: control_mode
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "control_mode: ";
    rosidl_generator_traits::value_to_yaml(msg.control_mode, out);
    out << "\n";
  }

  // member: horizontal_angle
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "horizontal_angle: ";
    rosidl_generator_traits::value_to_yaml(msg.horizontal_angle, out);
    out << "\n";
  }

  // member: vertical_angle
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "vertical_angle: ";
    rosidl_generator_traits::value_to_yaml(msg.vertical_angle, out);
    out << "\n";
  }

  // member: pan_speed
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "pan_speed: ";
    rosidl_generator_traits::value_to_yaml(msg.pan_speed, out);
    out << "\n";
  }

  // member: tilt_speed
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "tilt_speed: ";
    rosidl_generator_traits::value_to_yaml(msg.tilt_speed, out);
    out << "\n";
  }

  // member: pan_dir
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "pan_dir: ";
    rosidl_generator_traits::value_to_yaml(msg.pan_dir, out);
    out << "\n";
  }

  // member: tilt_dir
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "tilt_dir: ";
    rosidl_generator_traits::value_to_yaml(msg.tilt_dir, out);
    out << "\n";
  }
}  // NOLINT(readability/fn_size)

inline std::string to_yaml(const PanTiltControlCommand & msg, bool use_flow_style = false)
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
  const combat_robot_msgs::msg::PanTiltControlCommand & msg,
  std::ostream & out, size_t indentation = 0)
{
  combat_robot_msgs::msg::to_block_style_yaml(msg, out, indentation);
}

[[deprecated("use combat_robot_msgs::msg::to_yaml() instead")]]
inline std::string to_yaml(const combat_robot_msgs::msg::PanTiltControlCommand & msg)
{
  return combat_robot_msgs::msg::to_yaml(msg);
}

template<>
inline const char * data_type<combat_robot_msgs::msg::PanTiltControlCommand>()
{
  return "combat_robot_msgs::msg::PanTiltControlCommand";
}

template<>
inline const char * name<combat_robot_msgs::msg::PanTiltControlCommand>()
{
  return "combat_robot_msgs/msg/PanTiltControlCommand";
}

template<>
struct has_fixed_size<combat_robot_msgs::msg::PanTiltControlCommand>
  : std::integral_constant<bool, has_fixed_size<std_msgs::msg::Header>::value> {};

template<>
struct has_bounded_size<combat_robot_msgs::msg::PanTiltControlCommand>
  : std::integral_constant<bool, has_bounded_size<std_msgs::msg::Header>::value> {};

template<>
struct is_message<combat_robot_msgs::msg::PanTiltControlCommand>
  : std::true_type {};

}  // namespace rosidl_generator_traits

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__PAN_TILT_CONTROL_COMMAND__TRAITS_HPP_
