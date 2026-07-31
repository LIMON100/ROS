// generated from rosidl_generator_cpp/resource/idl__traits.hpp.em
// with input from combat_robot_msgs:msg/SwarmRobotCommand.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/swarm_robot_command.hpp"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__SWARM_ROBOT_COMMAND__TRAITS_HPP_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__SWARM_ROBOT_COMMAND__TRAITS_HPP_

#include <stdint.h>

#include <sstream>
#include <string>
#include <type_traits>

#include "combat_robot_msgs/msg/detail/swarm_robot_command__struct.hpp"
#include "rosidl_runtime_cpp/traits.hpp"

// Include directives for member types
// Member 'header'
#include "std_msgs/msg/detail/header__traits.hpp"

namespace combat_robot_msgs
{

namespace msg
{

inline void to_flow_style_yaml(
  const SwarmRobotCommand & msg,
  std::ostream & out)
{
  out << "{";
  // member: header
  {
    out << "header: ";
    to_flow_style_yaml(msg.header, out);
    out << ", ";
  }

  // member: sequence
  {
    out << "sequence: ";
    rosidl_generator_traits::value_to_yaml(msg.sequence, out);
    out << ", ";
  }

  // member: command_type
  {
    out << "command_type: ";
    rosidl_generator_traits::value_to_yaml(msg.command_type, out);
    out << ", ";
  }

  // member: leader_robot_id
  {
    out << "leader_robot_id: ";
    rosidl_generator_traits::value_to_yaml(msg.leader_robot_id, out);
    out << ", ";
  }

  // member: target_robot_id
  {
    out << "target_robot_id: ";
    rosidl_generator_traits::value_to_yaml(msg.target_robot_id, out);
    out << ", ";
  }

  // member: operation_mode
  {
    out << "operation_mode: ";
    rosidl_generator_traits::value_to_yaml(msg.operation_mode, out);
    out << ", ";
  }

  // member: estop_requested
  {
    out << "estop_requested: ";
    rosidl_generator_traits::value_to_yaml(msg.estop_requested, out);
    out << ", ";
  }

  // member: path_command
  {
    out << "path_command: ";
    rosidl_generator_traits::value_to_yaml(msg.path_command, out);
    out << ", ";
  }

  // member: num_waypoints
  {
    out << "num_waypoints: ";
    rosidl_generator_traits::value_to_yaml(msg.num_waypoints, out);
    out << ", ";
  }

  // member: path_id
  {
    out << "path_id: ";
    rosidl_generator_traits::value_to_yaml(msg.path_id, out);
    out << ", ";
  }

  // member: path_json
  {
    out << "path_json: ";
    rosidl_generator_traits::value_to_yaml(msg.path_json, out);
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

  // member: slot_index
  {
    out << "slot_index: ";
    rosidl_generator_traits::value_to_yaml(msg.slot_index, out);
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
  const SwarmRobotCommand & msg,
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

  // member: sequence
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "sequence: ";
    rosidl_generator_traits::value_to_yaml(msg.sequence, out);
    out << "\n";
  }

  // member: command_type
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "command_type: ";
    rosidl_generator_traits::value_to_yaml(msg.command_type, out);
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

  // member: target_robot_id
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "target_robot_id: ";
    rosidl_generator_traits::value_to_yaml(msg.target_robot_id, out);
    out << "\n";
  }

  // member: operation_mode
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "operation_mode: ";
    rosidl_generator_traits::value_to_yaml(msg.operation_mode, out);
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

  // member: path_command
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "path_command: ";
    rosidl_generator_traits::value_to_yaml(msg.path_command, out);
    out << "\n";
  }

  // member: num_waypoints
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "num_waypoints: ";
    rosidl_generator_traits::value_to_yaml(msg.num_waypoints, out);
    out << "\n";
  }

  // member: path_id
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "path_id: ";
    rosidl_generator_traits::value_to_yaml(msg.path_id, out);
    out << "\n";
  }

  // member: path_json
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "path_json: ";
    rosidl_generator_traits::value_to_yaml(msg.path_json, out);
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

  // member: slot_index
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "slot_index: ";
    rosidl_generator_traits::value_to_yaml(msg.slot_index, out);
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

inline std::string to_yaml(const SwarmRobotCommand & msg, bool use_flow_style = false)
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
  const combat_robot_msgs::msg::SwarmRobotCommand & msg,
  std::ostream & out, size_t indentation = 0)
{
  combat_robot_msgs::msg::to_block_style_yaml(msg, out, indentation);
}

[[deprecated("use combat_robot_msgs::msg::to_yaml() instead")]]
inline std::string to_yaml(const combat_robot_msgs::msg::SwarmRobotCommand & msg)
{
  return combat_robot_msgs::msg::to_yaml(msg);
}

template<>
inline const char * data_type<combat_robot_msgs::msg::SwarmRobotCommand>()
{
  return "combat_robot_msgs::msg::SwarmRobotCommand";
}

template<>
inline const char * name<combat_robot_msgs::msg::SwarmRobotCommand>()
{
  return "combat_robot_msgs/msg/SwarmRobotCommand";
}

template<>
struct has_fixed_size<combat_robot_msgs::msg::SwarmRobotCommand>
  : std::integral_constant<bool, false> {};

template<>
struct has_bounded_size<combat_robot_msgs::msg::SwarmRobotCommand>
  : std::integral_constant<bool, false> {};

template<>
struct is_message<combat_robot_msgs::msg::SwarmRobotCommand>
  : std::true_type {};

}  // namespace rosidl_generator_traits

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__SWARM_ROBOT_COMMAND__TRAITS_HPP_
