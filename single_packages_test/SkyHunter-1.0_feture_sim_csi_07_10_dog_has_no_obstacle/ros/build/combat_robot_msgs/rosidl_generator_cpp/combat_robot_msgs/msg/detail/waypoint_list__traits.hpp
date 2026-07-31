// generated from rosidl_generator_cpp/resource/idl__traits.hpp.em
// with input from combat_robot_msgs:msg/WaypointList.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/waypoint_list.hpp"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__WAYPOINT_LIST__TRAITS_HPP_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__WAYPOINT_LIST__TRAITS_HPP_

#include <stdint.h>

#include <sstream>
#include <string>
#include <type_traits>

#include "combat_robot_msgs/msg/detail/waypoint_list__struct.hpp"
#include "rosidl_runtime_cpp/traits.hpp"

// Include directives for member types
// Member 'waypoints'
#include "combat_robot_msgs/msg/detail/waypoint__traits.hpp"

namespace combat_robot_msgs
{

namespace msg
{

inline void to_flow_style_yaml(
  const WaypointList & msg,
  std::ostream & out)
{
  out << "{";
  // member: mode
  {
    out << "mode: ";
    rosidl_generator_traits::value_to_yaml(msg.mode, out);
    out << ", ";
  }

  // member: formation
  {
    out << "formation: ";
    rosidl_generator_traits::value_to_yaml(msg.formation, out);
    out << ", ";
  }

  // member: mission_id
  {
    out << "mission_id: ";
    rosidl_generator_traits::value_to_yaml(msg.mission_id, out);
    out << ", ";
  }

  // member: mission_status
  {
    out << "mission_status: ";
    rosidl_generator_traits::value_to_yaml(msg.mission_status, out);
    out << ", ";
  }

  // member: waypoints
  {
    if (msg.waypoints.size() == 0) {
      out << "waypoints: []";
    } else {
      out << "waypoints: [";
      size_t pending_items = msg.waypoints.size();
      for (auto item : msg.waypoints) {
        to_flow_style_yaml(item, out);
        if (--pending_items > 0) {
          out << ", ";
        }
      }
      out << "]";
    }
  }
  out << "}";
}  // NOLINT(readability/fn_size)

inline void to_block_style_yaml(
  const WaypointList & msg,
  std::ostream & out, size_t indentation = 0)
{
  // member: mode
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "mode: ";
    rosidl_generator_traits::value_to_yaml(msg.mode, out);
    out << "\n";
  }

  // member: formation
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "formation: ";
    rosidl_generator_traits::value_to_yaml(msg.formation, out);
    out << "\n";
  }

  // member: mission_id
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "mission_id: ";
    rosidl_generator_traits::value_to_yaml(msg.mission_id, out);
    out << "\n";
  }

  // member: mission_status
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "mission_status: ";
    rosidl_generator_traits::value_to_yaml(msg.mission_status, out);
    out << "\n";
  }

  // member: waypoints
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    if (msg.waypoints.size() == 0) {
      out << "waypoints: []\n";
    } else {
      out << "waypoints:\n";
      for (auto item : msg.waypoints) {
        if (indentation > 0) {
          out << std::string(indentation, ' ');
        }
        out << "-\n";
        to_block_style_yaml(item, out, indentation + 2);
      }
    }
  }
}  // NOLINT(readability/fn_size)

inline std::string to_yaml(const WaypointList & msg, bool use_flow_style = false)
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
  const combat_robot_msgs::msg::WaypointList & msg,
  std::ostream & out, size_t indentation = 0)
{
  combat_robot_msgs::msg::to_block_style_yaml(msg, out, indentation);
}

[[deprecated("use combat_robot_msgs::msg::to_yaml() instead")]]
inline std::string to_yaml(const combat_robot_msgs::msg::WaypointList & msg)
{
  return combat_robot_msgs::msg::to_yaml(msg);
}

template<>
inline const char * data_type<combat_robot_msgs::msg::WaypointList>()
{
  return "combat_robot_msgs::msg::WaypointList";
}

template<>
inline const char * name<combat_robot_msgs::msg::WaypointList>()
{
  return "combat_robot_msgs/msg/WaypointList";
}

template<>
struct has_fixed_size<combat_robot_msgs::msg::WaypointList>
  : std::integral_constant<bool, false> {};

template<>
struct has_bounded_size<combat_robot_msgs::msg::WaypointList>
  : std::integral_constant<bool, false> {};

template<>
struct is_message<combat_robot_msgs::msg::WaypointList>
  : std::true_type {};

}  // namespace rosidl_generator_traits

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__WAYPOINT_LIST__TRAITS_HPP_
