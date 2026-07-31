// generated from rosidl_generator_cpp/resource/idl__traits.hpp.em
// with input from combat_robot_msgs:msg/SwarmFollowerStatus.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/swarm_follower_status.hpp"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__SWARM_FOLLOWER_STATUS__TRAITS_HPP_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__SWARM_FOLLOWER_STATUS__TRAITS_HPP_

#include <stdint.h>

#include <sstream>
#include <string>
#include <type_traits>

#include "combat_robot_msgs/msg/detail/swarm_follower_status__struct.hpp"
#include "rosidl_runtime_cpp/traits.hpp"

// Include directives for member types
// Member 'header'
#include "std_msgs/msg/detail/header__traits.hpp"

namespace combat_robot_msgs
{

namespace msg
{

inline void to_flow_style_yaml(
  const SwarmFollowerStatus & msg,
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

  // member: leader_robot_id
  {
    out << "leader_robot_id: ";
    rosidl_generator_traits::value_to_yaml(msg.leader_robot_id, out);
    out << ", ";
  }

  // member: link_status
  {
    out << "link_status: ";
    rosidl_generator_traits::value_to_yaml(msg.link_status, out);
    out << ", ";
  }

  // member: last_heartbeat_sequence
  {
    out << "last_heartbeat_sequence: ";
    rosidl_generator_traits::value_to_yaml(msg.last_heartbeat_sequence, out);
    out << ", ";
  }

  // member: heartbeat_age_sec
  {
    out << "heartbeat_age_sec: ";
    rosidl_generator_traits::value_to_yaml(msg.heartbeat_age_sec, out);
    out << ", ";
  }

  // member: last_operation_mode
  {
    out << "last_operation_mode: ";
    rosidl_generator_traits::value_to_yaml(msg.last_operation_mode, out);
    out << ", ";
  }

  // member: last_formation_type
  {
    out << "last_formation_type: ";
    rosidl_generator_traits::value_to_yaml(msg.last_formation_type, out);
    out << ", ";
  }

  // member: last_formation_number
  {
    out << "last_formation_number: ";
    rosidl_generator_traits::value_to_yaml(msg.last_formation_number, out);
    out << ", ";
  }

  // member: last_grouping_index
  {
    out << "last_grouping_index: ";
    rosidl_generator_traits::value_to_yaml(msg.last_grouping_index, out);
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
  }
  out << "}";
}  // NOLINT(readability/fn_size)

inline void to_block_style_yaml(
  const SwarmFollowerStatus & msg,
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

  // member: leader_robot_id
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "leader_robot_id: ";
    rosidl_generator_traits::value_to_yaml(msg.leader_robot_id, out);
    out << "\n";
  }

  // member: link_status
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "link_status: ";
    rosidl_generator_traits::value_to_yaml(msg.link_status, out);
    out << "\n";
  }

  // member: last_heartbeat_sequence
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "last_heartbeat_sequence: ";
    rosidl_generator_traits::value_to_yaml(msg.last_heartbeat_sequence, out);
    out << "\n";
  }

  // member: heartbeat_age_sec
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "heartbeat_age_sec: ";
    rosidl_generator_traits::value_to_yaml(msg.heartbeat_age_sec, out);
    out << "\n";
  }

  // member: last_operation_mode
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "last_operation_mode: ";
    rosidl_generator_traits::value_to_yaml(msg.last_operation_mode, out);
    out << "\n";
  }

  // member: last_formation_type
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "last_formation_type: ";
    rosidl_generator_traits::value_to_yaml(msg.last_formation_type, out);
    out << "\n";
  }

  // member: last_formation_number
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "last_formation_number: ";
    rosidl_generator_traits::value_to_yaml(msg.last_formation_number, out);
    out << "\n";
  }

  // member: last_grouping_index
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "last_grouping_index: ";
    rosidl_generator_traits::value_to_yaml(msg.last_grouping_index, out);
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
}  // NOLINT(readability/fn_size)

inline std::string to_yaml(const SwarmFollowerStatus & msg, bool use_flow_style = false)
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
  const combat_robot_msgs::msg::SwarmFollowerStatus & msg,
  std::ostream & out, size_t indentation = 0)
{
  combat_robot_msgs::msg::to_block_style_yaml(msg, out, indentation);
}

[[deprecated("use combat_robot_msgs::msg::to_yaml() instead")]]
inline std::string to_yaml(const combat_robot_msgs::msg::SwarmFollowerStatus & msg)
{
  return combat_robot_msgs::msg::to_yaml(msg);
}

template<>
inline const char * data_type<combat_robot_msgs::msg::SwarmFollowerStatus>()
{
  return "combat_robot_msgs::msg::SwarmFollowerStatus";
}

template<>
inline const char * name<combat_robot_msgs::msg::SwarmFollowerStatus>()
{
  return "combat_robot_msgs/msg/SwarmFollowerStatus";
}

template<>
struct has_fixed_size<combat_robot_msgs::msg::SwarmFollowerStatus>
  : std::integral_constant<bool, has_fixed_size<std_msgs::msg::Header>::value> {};

template<>
struct has_bounded_size<combat_robot_msgs::msg::SwarmFollowerStatus>
  : std::integral_constant<bool, has_bounded_size<std_msgs::msg::Header>::value> {};

template<>
struct is_message<combat_robot_msgs::msg::SwarmFollowerStatus>
  : std::true_type {};

}  // namespace rosidl_generator_traits

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__SWARM_FOLLOWER_STATUS__TRAITS_HPP_
