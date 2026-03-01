// generated from rosidl_generator_cpp/resource/idl__traits.hpp.em
// with input from skyhunter_msgs:msg/SwarmHeartbeat.idl
// generated code does not contain a copyright notice

#ifndef SKYHUNTER_MSGS__MSG__DETAIL__SWARM_HEARTBEAT__TRAITS_HPP_
#define SKYHUNTER_MSGS__MSG__DETAIL__SWARM_HEARTBEAT__TRAITS_HPP_

#include <stdint.h>

#include <sstream>
#include <string>
#include <type_traits>

#include "skyhunter_msgs/msg/detail/swarm_heartbeat__struct.hpp"
#include "rosidl_runtime_cpp/traits.hpp"

// Include directives for member types
// Member 'header'
#include "std_msgs/msg/detail/header__traits.hpp"

namespace skyhunter_msgs
{

namespace msg
{

inline void to_flow_style_yaml(
  const SwarmHeartbeat & msg,
  std::ostream & out)
{
  out << "{";
  // member: header
  {
    out << "header: ";
    to_flow_style_yaml(msg.header, out);
    out << ", ";
  }

  // member: robot_id
  {
    out << "robot_id: ";
    rosidl_generator_traits::value_to_yaml(msg.robot_id, out);
    out << ", ";
  }

  // member: term
  {
    out << "term: ";
    rosidl_generator_traits::value_to_yaml(msg.term, out);
    out << ", ";
  }

  // member: is_leader
  {
    out << "is_leader: ";
    rosidl_generator_traits::value_to_yaml(msg.is_leader, out);
    out << ", ";
  }

  // member: battery_level
  {
    out << "battery_level: ";
    rosidl_generator_traits::value_to_yaml(msg.battery_level, out);
    out << ", ";
  }

  // member: leader_id_num
  {
    out << "leader_id_num: ";
    rosidl_generator_traits::value_to_yaml(msg.leader_id_num, out);
  }
  out << "}";
}  // NOLINT(readability/fn_size)

inline void to_block_style_yaml(
  const SwarmHeartbeat & msg,
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

  // member: robot_id
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "robot_id: ";
    rosidl_generator_traits::value_to_yaml(msg.robot_id, out);
    out << "\n";
  }

  // member: term
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "term: ";
    rosidl_generator_traits::value_to_yaml(msg.term, out);
    out << "\n";
  }

  // member: is_leader
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "is_leader: ";
    rosidl_generator_traits::value_to_yaml(msg.is_leader, out);
    out << "\n";
  }

  // member: battery_level
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "battery_level: ";
    rosidl_generator_traits::value_to_yaml(msg.battery_level, out);
    out << "\n";
  }

  // member: leader_id_num
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "leader_id_num: ";
    rosidl_generator_traits::value_to_yaml(msg.leader_id_num, out);
    out << "\n";
  }
}  // NOLINT(readability/fn_size)

inline std::string to_yaml(const SwarmHeartbeat & msg, bool use_flow_style = false)
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

}  // namespace skyhunter_msgs

namespace rosidl_generator_traits
{

[[deprecated("use skyhunter_msgs::msg::to_block_style_yaml() instead")]]
inline void to_yaml(
  const skyhunter_msgs::msg::SwarmHeartbeat & msg,
  std::ostream & out, size_t indentation = 0)
{
  skyhunter_msgs::msg::to_block_style_yaml(msg, out, indentation);
}

[[deprecated("use skyhunter_msgs::msg::to_yaml() instead")]]
inline std::string to_yaml(const skyhunter_msgs::msg::SwarmHeartbeat & msg)
{
  return skyhunter_msgs::msg::to_yaml(msg);
}

template<>
inline const char * data_type<skyhunter_msgs::msg::SwarmHeartbeat>()
{
  return "skyhunter_msgs::msg::SwarmHeartbeat";
}

template<>
inline const char * name<skyhunter_msgs::msg::SwarmHeartbeat>()
{
  return "skyhunter_msgs/msg/SwarmHeartbeat";
}

template<>
struct has_fixed_size<skyhunter_msgs::msg::SwarmHeartbeat>
  : std::integral_constant<bool, false> {};

template<>
struct has_bounded_size<skyhunter_msgs::msg::SwarmHeartbeat>
  : std::integral_constant<bool, false> {};

template<>
struct is_message<skyhunter_msgs::msg::SwarmHeartbeat>
  : std::true_type {};

}  // namespace rosidl_generator_traits

#endif  // SKYHUNTER_MSGS__MSG__DETAIL__SWARM_HEARTBEAT__TRAITS_HPP_
