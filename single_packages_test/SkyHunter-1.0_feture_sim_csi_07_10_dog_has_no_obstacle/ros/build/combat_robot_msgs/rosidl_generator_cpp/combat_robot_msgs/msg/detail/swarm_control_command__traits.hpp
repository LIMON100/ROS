// generated from rosidl_generator_cpp/resource/idl__traits.hpp.em
// with input from combat_robot_msgs:msg/SwarmControlCommand.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/swarm_control_command.hpp"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__SWARM_CONTROL_COMMAND__TRAITS_HPP_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__SWARM_CONTROL_COMMAND__TRAITS_HPP_

#include <stdint.h>

#include <sstream>
#include <string>
#include <type_traits>

#include "combat_robot_msgs/msg/detail/swarm_control_command__struct.hpp"
#include "rosidl_runtime_cpp/traits.hpp"

// Include directives for member types
// Member 'header'
#include "std_msgs/msg/detail/header__traits.hpp"

namespace combat_robot_msgs
{

namespace msg
{

inline void to_flow_style_yaml(
  const SwarmControlCommand & msg,
  std::ostream & out)
{
  out << "{";
  // member: header
  {
    out << "header: ";
    to_flow_style_yaml(msg.header, out);
    out << ", ";
  }

  // member: formation_type
  {
    out << "formation_type: ";
    rosidl_generator_traits::value_to_yaml(msg.formation_type, out);
    out << ", ";
  }

  // member: formation_number
  {
    out << "formation_number: ";
    rosidl_generator_traits::value_to_yaml(msg.formation_number, out);
    out << ", ";
  }

  // member: grouping_index
  {
    out << "grouping_index: ";
    rosidl_generator_traits::value_to_yaml(msg.grouping_index, out);
    out << ", ";
  }

  // member: selected_robot_count
  {
    out << "selected_robot_count: ";
    rosidl_generator_traits::value_to_yaml(msg.selected_robot_count, out);
    out << ", ";
  }

  // member: selected_robot_ids
  {
    if (msg.selected_robot_ids.size() == 0) {
      out << "selected_robot_ids: []";
    } else {
      out << "selected_robot_ids: [";
      size_t pending_items = msg.selected_robot_ids.size();
      for (auto item : msg.selected_robot_ids) {
        rosidl_generator_traits::value_to_yaml(item, out);
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
  const SwarmControlCommand & msg,
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

  // member: formation_type
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "formation_type: ";
    rosidl_generator_traits::value_to_yaml(msg.formation_type, out);
    out << "\n";
  }

  // member: formation_number
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "formation_number: ";
    rosidl_generator_traits::value_to_yaml(msg.formation_number, out);
    out << "\n";
  }

  // member: grouping_index
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "grouping_index: ";
    rosidl_generator_traits::value_to_yaml(msg.grouping_index, out);
    out << "\n";
  }

  // member: selected_robot_count
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "selected_robot_count: ";
    rosidl_generator_traits::value_to_yaml(msg.selected_robot_count, out);
    out << "\n";
  }

  // member: selected_robot_ids
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    if (msg.selected_robot_ids.size() == 0) {
      out << "selected_robot_ids: []\n";
    } else {
      out << "selected_robot_ids:\n";
      for (auto item : msg.selected_robot_ids) {
        if (indentation > 0) {
          out << std::string(indentation, ' ');
        }
        out << "- ";
        rosidl_generator_traits::value_to_yaml(item, out);
        out << "\n";
      }
    }
  }
}  // NOLINT(readability/fn_size)

inline std::string to_yaml(const SwarmControlCommand & msg, bool use_flow_style = false)
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
  const combat_robot_msgs::msg::SwarmControlCommand & msg,
  std::ostream & out, size_t indentation = 0)
{
  combat_robot_msgs::msg::to_block_style_yaml(msg, out, indentation);
}

[[deprecated("use combat_robot_msgs::msg::to_yaml() instead")]]
inline std::string to_yaml(const combat_robot_msgs::msg::SwarmControlCommand & msg)
{
  return combat_robot_msgs::msg::to_yaml(msg);
}

template<>
inline const char * data_type<combat_robot_msgs::msg::SwarmControlCommand>()
{
  return "combat_robot_msgs::msg::SwarmControlCommand";
}

template<>
inline const char * name<combat_robot_msgs::msg::SwarmControlCommand>()
{
  return "combat_robot_msgs/msg/SwarmControlCommand";
}

template<>
struct has_fixed_size<combat_robot_msgs::msg::SwarmControlCommand>
  : std::integral_constant<bool, has_fixed_size<std_msgs::msg::Header>::value> {};

template<>
struct has_bounded_size<combat_robot_msgs::msg::SwarmControlCommand>
  : std::integral_constant<bool, has_bounded_size<std_msgs::msg::Header>::value> {};

template<>
struct is_message<combat_robot_msgs::msg::SwarmControlCommand>
  : std::true_type {};

}  // namespace rosidl_generator_traits

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__SWARM_CONTROL_COMMAND__TRAITS_HPP_
