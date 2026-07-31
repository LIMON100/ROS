// generated from rosidl_generator_cpp/resource/idl__traits.hpp.em
// with input from combat_robot_msgs:msg/MissionControlCommand.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/mission_control_command.hpp"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__MISSION_CONTROL_COMMAND__TRAITS_HPP_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__MISSION_CONTROL_COMMAND__TRAITS_HPP_

#include <stdint.h>

#include <sstream>
#include <string>
#include <type_traits>

#include "combat_robot_msgs/msg/detail/mission_control_command__struct.hpp"
#include "rosidl_runtime_cpp/traits.hpp"

// Include directives for member types
// Member 'header'
#include "std_msgs/msg/detail/header__traits.hpp"

namespace combat_robot_msgs
{

namespace msg
{

inline void to_flow_style_yaml(
  const MissionControlCommand & msg,
  std::ostream & out)
{
  out << "{";
  // member: header
  {
    out << "header: ";
    to_flow_style_yaml(msg.header, out);
    out << ", ";
  }

  // member: command_id
  {
    out << "command_id: ";
    rosidl_generator_traits::value_to_yaml(msg.command_id, out);
    out << ", ";
  }

  // member: estop_requested
  {
    out << "estop_requested: ";
    rosidl_generator_traits::value_to_yaml(msg.estop_requested, out);
    out << ", ";
  }

  // member: attack_permission
  {
    out << "attack_permission: ";
    rosidl_generator_traits::value_to_yaml(msg.attack_permission, out);
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

  // member: zoom_command
  {
    out << "zoom_command: ";
    rosidl_generator_traits::value_to_yaml(msg.zoom_command, out);
    out << ", ";
  }

  // member: lateral_wind_speed
  {
    out << "lateral_wind_speed: ";
    rosidl_generator_traits::value_to_yaml(msg.lateral_wind_speed, out);
    out << ", ";
  }

  // member: drone_target_lat
  {
    out << "drone_target_lat: ";
    rosidl_generator_traits::value_to_yaml(msg.drone_target_lat, out);
    out << ", ";
  }

  // member: drone_target_lon
  {
    out << "drone_target_lon: ";
    rosidl_generator_traits::value_to_yaml(msg.drone_target_lon, out);
    out << ", ";
  }

  // member: drone_target_valid
  {
    out << "drone_target_valid: ";
    rosidl_generator_traits::value_to_yaml(msg.drone_target_valid, out);
  }
  out << "}";
}  // NOLINT(readability/fn_size)

inline void to_block_style_yaml(
  const MissionControlCommand & msg,
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

  // member: command_id
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "command_id: ";
    rosidl_generator_traits::value_to_yaml(msg.command_id, out);
    out << "\n";
  }

  // member: estop_requested
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "estop_requested: ";
    rosidl_generator_traits::value_to_yaml(msg.estop_requested, out);
    out << "\n";
  }

  // member: attack_permission
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "attack_permission: ";
    rosidl_generator_traits::value_to_yaml(msg.attack_permission, out);
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

  // member: zoom_command
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "zoom_command: ";
    rosidl_generator_traits::value_to_yaml(msg.zoom_command, out);
    out << "\n";
  }

  // member: lateral_wind_speed
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "lateral_wind_speed: ";
    rosidl_generator_traits::value_to_yaml(msg.lateral_wind_speed, out);
    out << "\n";
  }

  // member: drone_target_lat
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "drone_target_lat: ";
    rosidl_generator_traits::value_to_yaml(msg.drone_target_lat, out);
    out << "\n";
  }

  // member: drone_target_lon
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "drone_target_lon: ";
    rosidl_generator_traits::value_to_yaml(msg.drone_target_lon, out);
    out << "\n";
  }

  // member: drone_target_valid
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "drone_target_valid: ";
    rosidl_generator_traits::value_to_yaml(msg.drone_target_valid, out);
    out << "\n";
  }
}  // NOLINT(readability/fn_size)

inline std::string to_yaml(const MissionControlCommand & msg, bool use_flow_style = false)
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
  const combat_robot_msgs::msg::MissionControlCommand & msg,
  std::ostream & out, size_t indentation = 0)
{
  combat_robot_msgs::msg::to_block_style_yaml(msg, out, indentation);
}

[[deprecated("use combat_robot_msgs::msg::to_yaml() instead")]]
inline std::string to_yaml(const combat_robot_msgs::msg::MissionControlCommand & msg)
{
  return combat_robot_msgs::msg::to_yaml(msg);
}

template<>
inline const char * data_type<combat_robot_msgs::msg::MissionControlCommand>()
{
  return "combat_robot_msgs::msg::MissionControlCommand";
}

template<>
inline const char * name<combat_robot_msgs::msg::MissionControlCommand>()
{
  return "combat_robot_msgs/msg/MissionControlCommand";
}

template<>
struct has_fixed_size<combat_robot_msgs::msg::MissionControlCommand>
  : std::integral_constant<bool, has_fixed_size<std_msgs::msg::Header>::value> {};

template<>
struct has_bounded_size<combat_robot_msgs::msg::MissionControlCommand>
  : std::integral_constant<bool, has_bounded_size<std_msgs::msg::Header>::value> {};

template<>
struct is_message<combat_robot_msgs::msg::MissionControlCommand>
  : std::true_type {};

}  // namespace rosidl_generator_traits

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__MISSION_CONTROL_COMMAND__TRAITS_HPP_
