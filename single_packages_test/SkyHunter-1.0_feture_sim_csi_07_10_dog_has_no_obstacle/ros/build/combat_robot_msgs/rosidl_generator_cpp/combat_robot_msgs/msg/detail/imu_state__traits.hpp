// generated from rosidl_generator_cpp/resource/idl__traits.hpp.em
// with input from combat_robot_msgs:msg/IMUState.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/imu_state.hpp"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__IMU_STATE__TRAITS_HPP_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__IMU_STATE__TRAITS_HPP_

#include <stdint.h>

#include <sstream>
#include <string>
#include <type_traits>

#include "combat_robot_msgs/msg/detail/imu_state__struct.hpp"
#include "rosidl_runtime_cpp/traits.hpp"

// Include directives for member types
// Member 'header'
#include "std_msgs/msg/detail/header__traits.hpp"
// Member 'angle'
#include "geometry_msgs/msg/detail/vector3__traits.hpp"

namespace combat_robot_msgs
{

namespace msg
{

inline void to_flow_style_yaml(
  const IMUState & msg,
  std::ostream & out)
{
  out << "{";
  // member: header
  {
    out << "header: ";
    to_flow_style_yaml(msg.header, out);
    out << ", ";
  }

  // member: angle
  {
    out << "angle: ";
    to_flow_style_yaml(msg.angle, out);
    out << ", ";
  }

  // member: is_connected
  {
    out << "is_connected: ";
    rosidl_generator_traits::value_to_yaml(msg.is_connected, out);
    out << ", ";
  }

  // member: device_id
  {
    out << "device_id: ";
    rosidl_generator_traits::value_to_yaml(msg.device_id, out);
  }
  out << "}";
}  // NOLINT(readability/fn_size)

inline void to_block_style_yaml(
  const IMUState & msg,
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

  // member: angle
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "angle:\n";
    to_block_style_yaml(msg.angle, out, indentation + 2);
  }

  // member: is_connected
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "is_connected: ";
    rosidl_generator_traits::value_to_yaml(msg.is_connected, out);
    out << "\n";
  }

  // member: device_id
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "device_id: ";
    rosidl_generator_traits::value_to_yaml(msg.device_id, out);
    out << "\n";
  }
}  // NOLINT(readability/fn_size)

inline std::string to_yaml(const IMUState & msg, bool use_flow_style = false)
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
  const combat_robot_msgs::msg::IMUState & msg,
  std::ostream & out, size_t indentation = 0)
{
  combat_robot_msgs::msg::to_block_style_yaml(msg, out, indentation);
}

[[deprecated("use combat_robot_msgs::msg::to_yaml() instead")]]
inline std::string to_yaml(const combat_robot_msgs::msg::IMUState & msg)
{
  return combat_robot_msgs::msg::to_yaml(msg);
}

template<>
inline const char * data_type<combat_robot_msgs::msg::IMUState>()
{
  return "combat_robot_msgs::msg::IMUState";
}

template<>
inline const char * name<combat_robot_msgs::msg::IMUState>()
{
  return "combat_robot_msgs/msg/IMUState";
}

template<>
struct has_fixed_size<combat_robot_msgs::msg::IMUState>
  : std::integral_constant<bool, false> {};

template<>
struct has_bounded_size<combat_robot_msgs::msg::IMUState>
  : std::integral_constant<bool, false> {};

template<>
struct is_message<combat_robot_msgs::msg::IMUState>
  : std::true_type {};

}  // namespace rosidl_generator_traits

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__IMU_STATE__TRAITS_HPP_
