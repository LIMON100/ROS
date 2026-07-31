// generated from rosidl_generator_c/resource/idl__struct.h.em
// with input from combat_robot_msgs:msg/SwarmControlCommand.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/swarm_control_command.h"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__SWARM_CONTROL_COMMAND__STRUCT_H_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__SWARM_CONTROL_COMMAND__STRUCT_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Constants defined in the message

/// Constant 'FORMATION_NONE'.
enum
{
  combat_robot_msgs__msg__SwarmControlCommand__FORMATION_NONE = 0
};

/// Constant 'FORMATION_RECON'.
enum
{
  combat_robot_msgs__msg__SwarmControlCommand__FORMATION_RECON = 1
};

/// Constant 'FORMATION_PROTECT'.
enum
{
  combat_robot_msgs__msg__SwarmControlCommand__FORMATION_PROTECT = 2
};

/// Constant 'FORMATION_ASSAULT'.
enum
{
  combat_robot_msgs__msg__SwarmControlCommand__FORMATION_ASSAULT = 3
};

// Include directives for member types
// Member 'header'
#include "std_msgs/msg/detail/header__struct.h"

/// Struct defined in msg/SwarmControlCommand in the package combat_robot_msgs.
typedef struct combat_robot_msgs__msg__SwarmControlCommand
{
  std_msgs__msg__Header header;
  uint8_t formation_type;
  uint8_t formation_number;
  uint8_t grouping_index;
  uint8_t selected_robot_count;
  uint32_t selected_robot_ids[8];
} combat_robot_msgs__msg__SwarmControlCommand;

// Struct for a sequence of combat_robot_msgs__msg__SwarmControlCommand.
typedef struct combat_robot_msgs__msg__SwarmControlCommand__Sequence
{
  combat_robot_msgs__msg__SwarmControlCommand * data;
  /// The number of valid items in data
  size_t size;
  /// The number of allocated items in data
  size_t capacity;
} combat_robot_msgs__msg__SwarmControlCommand__Sequence;

#ifdef __cplusplus
}
#endif

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__SWARM_CONTROL_COMMAND__STRUCT_H_
