// generated from rosidl_generator_cpp/resource/idl__traits.hpp.em
// with input from combat_robot_msgs:msg/ChassisStatus.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/chassis_status.hpp"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__CHASSIS_STATUS__TRAITS_HPP_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__CHASSIS_STATUS__TRAITS_HPP_

#include <stdint.h>

#include <sstream>
#include <string>
#include <type_traits>

#include "combat_robot_msgs/msg/detail/chassis_status__struct.hpp"
#include "rosidl_runtime_cpp/traits.hpp"

// Include directives for member types
// Member 'header'
#include "std_msgs/msg/detail/header__traits.hpp"

namespace combat_robot_msgs
{

namespace msg
{

inline void to_flow_style_yaml(
  const ChassisStatus & msg,
  std::ostream & out)
{
  out << "{";
  // member: header
  {
    out << "header: ";
    to_flow_style_yaml(msg.header, out);
    out << ", ";
  }

  // member: drive_state
  {
    out << "drive_state: ";
    rosidl_generator_traits::value_to_yaml(msg.drive_state, out);
    out << ", ";
  }

  // member: battery_pct
  {
    out << "battery_pct: ";
    rosidl_generator_traits::value_to_yaml(msg.battery_pct, out);
    out << ", ";
  }

  // member: battery_voltage_v
  {
    out << "battery_voltage_v: ";
    rosidl_generator_traits::value_to_yaml(msg.battery_voltage_v, out);
    out << ", ";
  }

  // member: battery_current_a
  {
    out << "battery_current_a: ";
    rosidl_generator_traits::value_to_yaml(msg.battery_current_a, out);
    out << ", ";
  }

  // member: linear_velocity_mps
  {
    out << "linear_velocity_mps: ";
    rosidl_generator_traits::value_to_yaml(msg.linear_velocity_mps, out);
    out << ", ";
  }

  // member: angular_velocity_rps
  {
    out << "angular_velocity_rps: ";
    rosidl_generator_traits::value_to_yaml(msg.angular_velocity_rps, out);
    out << ", ";
  }

  // member: fault_flags
  {
    out << "fault_flags: ";
    rosidl_generator_traits::value_to_yaml(msg.fault_flags, out);
    out << ", ";
  }

  // member: motor_temp_c
  {
    out << "motor_temp_c: ";
    rosidl_generator_traits::value_to_yaml(msg.motor_temp_c, out);
  }
  out << "}";
}  // NOLINT(readability/fn_size)

inline void to_block_style_yaml(
  const ChassisStatus & msg,
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

  // member: drive_state
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "drive_state: ";
    rosidl_generator_traits::value_to_yaml(msg.drive_state, out);
    out << "\n";
  }

  // member: battery_pct
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "battery_pct: ";
    rosidl_generator_traits::value_to_yaml(msg.battery_pct, out);
    out << "\n";
  }

  // member: battery_voltage_v
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "battery_voltage_v: ";
    rosidl_generator_traits::value_to_yaml(msg.battery_voltage_v, out);
    out << "\n";
  }

  // member: battery_current_a
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "battery_current_a: ";
    rosidl_generator_traits::value_to_yaml(msg.battery_current_a, out);
    out << "\n";
  }

  // member: linear_velocity_mps
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "linear_velocity_mps: ";
    rosidl_generator_traits::value_to_yaml(msg.linear_velocity_mps, out);
    out << "\n";
  }

  // member: angular_velocity_rps
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "angular_velocity_rps: ";
    rosidl_generator_traits::value_to_yaml(msg.angular_velocity_rps, out);
    out << "\n";
  }

  // member: fault_flags
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "fault_flags: ";
    rosidl_generator_traits::value_to_yaml(msg.fault_flags, out);
    out << "\n";
  }

  // member: motor_temp_c
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "motor_temp_c: ";
    rosidl_generator_traits::value_to_yaml(msg.motor_temp_c, out);
    out << "\n";
  }
}  // NOLINT(readability/fn_size)

inline std::string to_yaml(const ChassisStatus & msg, bool use_flow_style = false)
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
  const combat_robot_msgs::msg::ChassisStatus & msg,
  std::ostream & out, size_t indentation = 0)
{
  combat_robot_msgs::msg::to_block_style_yaml(msg, out, indentation);
}

[[deprecated("use combat_robot_msgs::msg::to_yaml() instead")]]
inline std::string to_yaml(const combat_robot_msgs::msg::ChassisStatus & msg)
{
  return combat_robot_msgs::msg::to_yaml(msg);
}

template<>
inline const char * data_type<combat_robot_msgs::msg::ChassisStatus>()
{
  return "combat_robot_msgs::msg::ChassisStatus";
}

template<>
inline const char * name<combat_robot_msgs::msg::ChassisStatus>()
{
  return "combat_robot_msgs/msg/ChassisStatus";
}

template<>
struct has_fixed_size<combat_robot_msgs::msg::ChassisStatus>
  : std::integral_constant<bool, has_fixed_size<std_msgs::msg::Header>::value> {};

template<>
struct has_bounded_size<combat_robot_msgs::msg::ChassisStatus>
  : std::integral_constant<bool, has_bounded_size<std_msgs::msg::Header>::value> {};

template<>
struct is_message<combat_robot_msgs::msg::ChassisStatus>
  : std::true_type {};

}  // namespace rosidl_generator_traits

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__CHASSIS_STATUS__TRAITS_HPP_
