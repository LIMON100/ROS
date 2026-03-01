// generated from rosidl_generator_cpp/resource/idl__traits.hpp.em
// with input from skyhunter_msgs:msg/LeaderState.idl
// generated code does not contain a copyright notice

#ifndef SKYHUNTER_MSGS__MSG__DETAIL__LEADER_STATE__TRAITS_HPP_
#define SKYHUNTER_MSGS__MSG__DETAIL__LEADER_STATE__TRAITS_HPP_

#include <stdint.h>

#include <sstream>
#include <string>
#include <type_traits>

#include "skyhunter_msgs/msg/detail/leader_state__struct.hpp"
#include "rosidl_runtime_cpp/traits.hpp"

// Include directives for member types
// Member 'header'
#include "std_msgs/msg/detail/header__traits.hpp"
// Member 'pose'
// Member 'next_waypoints'
#include "geometry_msgs/msg/detail/pose__traits.hpp"
// Member 'velocity'
#include "geometry_msgs/msg/detail/twist__traits.hpp"

namespace skyhunter_msgs
{

namespace msg
{

inline void to_flow_style_yaml(
  const LeaderState & msg,
  std::ostream & out)
{
  out << "{";
  // member: header
  {
    out << "header: ";
    to_flow_style_yaml(msg.header, out);
    out << ", ";
  }

  // member: pose
  {
    out << "pose: ";
    to_flow_style_yaml(msg.pose, out);
    out << ", ";
  }

  // member: velocity
  {
    out << "velocity: ";
    to_flow_style_yaml(msg.velocity, out);
    out << ", ";
  }

  // member: next_waypoints
  {
    if (msg.next_waypoints.size() == 0) {
      out << "next_waypoints: []";
    } else {
      out << "next_waypoints: [";
      size_t pending_items = msg.next_waypoints.size();
      for (auto item : msg.next_waypoints) {
        to_flow_style_yaml(item, out);
        if (--pending_items > 0) {
          out << ", ";
        }
      }
      out << "]";
    }
    out << ", ";
  }

  // member: formation_mode
  {
    out << "formation_mode: ";
    rosidl_generator_traits::value_to_yaml(msg.formation_mode, out);
    out << ", ";
  }

  // member: formation_state
  {
    out << "formation_state: ";
    rosidl_generator_traits::value_to_yaml(msg.formation_state, out);
    out << ", ";
  }

  // member: swarm_state
  {
    out << "swarm_state: ";
    rosidl_generator_traits::value_to_yaml(msg.swarm_state, out);
    out << ", ";
  }

  // member: formation_type
  {
    out << "formation_type: ";
    rosidl_generator_traits::value_to_yaml(msg.formation_type, out);
    out << ", ";
  }

  // member: current_waypoint_index
  {
    out << "current_waypoint_index: ";
    rosidl_generator_traits::value_to_yaml(msg.current_waypoint_index, out);
  }
  out << "}";
}  // NOLINT(readability/fn_size)

inline void to_block_style_yaml(
  const LeaderState & msg,
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

  // member: pose
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "pose:\n";
    to_block_style_yaml(msg.pose, out, indentation + 2);
  }

  // member: velocity
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "velocity:\n";
    to_block_style_yaml(msg.velocity, out, indentation + 2);
  }

  // member: next_waypoints
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    if (msg.next_waypoints.size() == 0) {
      out << "next_waypoints: []\n";
    } else {
      out << "next_waypoints:\n";
      for (auto item : msg.next_waypoints) {
        if (indentation > 0) {
          out << std::string(indentation, ' ');
        }
        out << "-\n";
        to_block_style_yaml(item, out, indentation + 2);
      }
    }
  }

  // member: formation_mode
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "formation_mode: ";
    rosidl_generator_traits::value_to_yaml(msg.formation_mode, out);
    out << "\n";
  }

  // member: formation_state
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "formation_state: ";
    rosidl_generator_traits::value_to_yaml(msg.formation_state, out);
    out << "\n";
  }

  // member: swarm_state
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "swarm_state: ";
    rosidl_generator_traits::value_to_yaml(msg.swarm_state, out);
    out << "\n";
  }

  // member: formation_type
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "formation_type: ";
    rosidl_generator_traits::value_to_yaml(msg.formation_type, out);
    out << "\n";
  }

  // member: current_waypoint_index
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "current_waypoint_index: ";
    rosidl_generator_traits::value_to_yaml(msg.current_waypoint_index, out);
    out << "\n";
  }
}  // NOLINT(readability/fn_size)

inline std::string to_yaml(const LeaderState & msg, bool use_flow_style = false)
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
  const skyhunter_msgs::msg::LeaderState & msg,
  std::ostream & out, size_t indentation = 0)
{
  skyhunter_msgs::msg::to_block_style_yaml(msg, out, indentation);
}

[[deprecated("use skyhunter_msgs::msg::to_yaml() instead")]]
inline std::string to_yaml(const skyhunter_msgs::msg::LeaderState & msg)
{
  return skyhunter_msgs::msg::to_yaml(msg);
}

template<>
inline const char * data_type<skyhunter_msgs::msg::LeaderState>()
{
  return "skyhunter_msgs::msg::LeaderState";
}

template<>
inline const char * name<skyhunter_msgs::msg::LeaderState>()
{
  return "skyhunter_msgs/msg/LeaderState";
}

template<>
struct has_fixed_size<skyhunter_msgs::msg::LeaderState>
  : std::integral_constant<bool, false> {};

template<>
struct has_bounded_size<skyhunter_msgs::msg::LeaderState>
  : std::integral_constant<bool, false> {};

template<>
struct is_message<skyhunter_msgs::msg::LeaderState>
  : std::true_type {};

}  // namespace rosidl_generator_traits

#endif  // SKYHUNTER_MSGS__MSG__DETAIL__LEADER_STATE__TRAITS_HPP_
