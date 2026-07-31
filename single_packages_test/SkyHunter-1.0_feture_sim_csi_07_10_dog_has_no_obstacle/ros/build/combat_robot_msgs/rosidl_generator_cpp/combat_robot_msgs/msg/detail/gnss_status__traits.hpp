// generated from rosidl_generator_cpp/resource/idl__traits.hpp.em
// with input from combat_robot_msgs:msg/GnssStatus.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/gnss_status.hpp"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__GNSS_STATUS__TRAITS_HPP_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__GNSS_STATUS__TRAITS_HPP_

#include <stdint.h>

#include <sstream>
#include <string>
#include <type_traits>

#include "combat_robot_msgs/msg/detail/gnss_status__struct.hpp"
#include "rosidl_runtime_cpp/traits.hpp"

// Include directives for member types
// Member 'header'
#include "std_msgs/msg/detail/header__traits.hpp"

namespace combat_robot_msgs
{

namespace msg
{

inline void to_flow_style_yaml(
  const GnssStatus & msg,
  std::ostream & out)
{
  out << "{";
  // member: header
  {
    out << "header: ";
    to_flow_style_yaml(msg.header, out);
    out << ", ";
  }

  // member: fix_status
  {
    out << "fix_status: ";
    rosidl_generator_traits::value_to_yaml(msg.fix_status, out);
    out << ", ";
  }

  // member: num_satellites
  {
    out << "num_satellites: ";
    rosidl_generator_traits::value_to_yaml(msg.num_satellites, out);
    out << ", ";
  }

  // member: latitude
  {
    out << "latitude: ";
    rosidl_generator_traits::value_to_yaml(msg.latitude, out);
    out << ", ";
  }

  // member: longitude
  {
    out << "longitude: ";
    rosidl_generator_traits::value_to_yaml(msg.longitude, out);
    out << ", ";
  }

  // member: altitude_m
  {
    out << "altitude_m: ";
    rosidl_generator_traits::value_to_yaml(msg.altitude_m, out);
    out << ", ";
  }

  // member: heading_deg
  {
    out << "heading_deg: ";
    rosidl_generator_traits::value_to_yaml(msg.heading_deg, out);
    out << ", ";
  }

  // member: ground_speed_mps
  {
    out << "ground_speed_mps: ";
    rosidl_generator_traits::value_to_yaml(msg.ground_speed_mps, out);
    out << ", ";
  }

  // member: horizontal_accuracy_m
  {
    out << "horizontal_accuracy_m: ";
    rosidl_generator_traits::value_to_yaml(msg.horizontal_accuracy_m, out);
    out << ", ";
  }

  // member: vertical_accuracy_m
  {
    out << "vertical_accuracy_m: ";
    rosidl_generator_traits::value_to_yaml(msg.vertical_accuracy_m, out);
  }
  out << "}";
}  // NOLINT(readability/fn_size)

inline void to_block_style_yaml(
  const GnssStatus & msg,
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

  // member: fix_status
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "fix_status: ";
    rosidl_generator_traits::value_to_yaml(msg.fix_status, out);
    out << "\n";
  }

  // member: num_satellites
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "num_satellites: ";
    rosidl_generator_traits::value_to_yaml(msg.num_satellites, out);
    out << "\n";
  }

  // member: latitude
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "latitude: ";
    rosidl_generator_traits::value_to_yaml(msg.latitude, out);
    out << "\n";
  }

  // member: longitude
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "longitude: ";
    rosidl_generator_traits::value_to_yaml(msg.longitude, out);
    out << "\n";
  }

  // member: altitude_m
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "altitude_m: ";
    rosidl_generator_traits::value_to_yaml(msg.altitude_m, out);
    out << "\n";
  }

  // member: heading_deg
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "heading_deg: ";
    rosidl_generator_traits::value_to_yaml(msg.heading_deg, out);
    out << "\n";
  }

  // member: ground_speed_mps
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "ground_speed_mps: ";
    rosidl_generator_traits::value_to_yaml(msg.ground_speed_mps, out);
    out << "\n";
  }

  // member: horizontal_accuracy_m
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "horizontal_accuracy_m: ";
    rosidl_generator_traits::value_to_yaml(msg.horizontal_accuracy_m, out);
    out << "\n";
  }

  // member: vertical_accuracy_m
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "vertical_accuracy_m: ";
    rosidl_generator_traits::value_to_yaml(msg.vertical_accuracy_m, out);
    out << "\n";
  }
}  // NOLINT(readability/fn_size)

inline std::string to_yaml(const GnssStatus & msg, bool use_flow_style = false)
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
  const combat_robot_msgs::msg::GnssStatus & msg,
  std::ostream & out, size_t indentation = 0)
{
  combat_robot_msgs::msg::to_block_style_yaml(msg, out, indentation);
}

[[deprecated("use combat_robot_msgs::msg::to_yaml() instead")]]
inline std::string to_yaml(const combat_robot_msgs::msg::GnssStatus & msg)
{
  return combat_robot_msgs::msg::to_yaml(msg);
}

template<>
inline const char * data_type<combat_robot_msgs::msg::GnssStatus>()
{
  return "combat_robot_msgs::msg::GnssStatus";
}

template<>
inline const char * name<combat_robot_msgs::msg::GnssStatus>()
{
  return "combat_robot_msgs/msg/GnssStatus";
}

template<>
struct has_fixed_size<combat_robot_msgs::msg::GnssStatus>
  : std::integral_constant<bool, has_fixed_size<std_msgs::msg::Header>::value> {};

template<>
struct has_bounded_size<combat_robot_msgs::msg::GnssStatus>
  : std::integral_constant<bool, has_bounded_size<std_msgs::msg::Header>::value> {};

template<>
struct is_message<combat_robot_msgs::msg::GnssStatus>
  : std::true_type {};

}  // namespace rosidl_generator_traits

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__GNSS_STATUS__TRAITS_HPP_
