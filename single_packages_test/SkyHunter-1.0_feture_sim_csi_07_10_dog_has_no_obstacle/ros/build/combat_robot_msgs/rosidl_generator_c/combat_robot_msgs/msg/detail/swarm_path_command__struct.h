// generated from rosidl_generator_c/resource/idl__struct.h.em
// with input from combat_robot_msgs:msg/SwarmPathCommand.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/swarm_path_command.h"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__SWARM_PATH_COMMAND__STRUCT_H_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__SWARM_PATH_COMMAND__STRUCT_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Constants defined in the message

/// Constant 'CMD_NONE'.
enum
{
  combat_robot_msgs__msg__SwarmPathCommand__CMD_NONE = 0
};

/// Constant 'CMD_START'.
enum
{
  combat_robot_msgs__msg__SwarmPathCommand__CMD_START = 1
};

/// Constant 'CMD_STOP'.
enum
{
  combat_robot_msgs__msg__SwarmPathCommand__CMD_STOP = 2
};

/// Constant 'CMD_PAUSE'.
enum
{
  combat_robot_msgs__msg__SwarmPathCommand__CMD_PAUSE = 3
};

/// Constant 'CMD_RESUME'.
enum
{
  combat_robot_msgs__msg__SwarmPathCommand__CMD_RESUME = 4
};

/// Constant 'CMD_LOAD_PATH'.
enum
{
  combat_robot_msgs__msg__SwarmPathCommand__CMD_LOAD_PATH = 5
};

/// Constant 'CMD_COMPLETE'.
/**
  * path follower / operator signals 'waypoints done'; FSM switches PROTECT modes from drive to engage
 */
enum
{
  combat_robot_msgs__msg__SwarmPathCommand__CMD_COMPLETE = 6
};

// Include directives for member types
// Member 'header'
#include "std_msgs/msg/detail/header__struct.h"
// Member 'path_json'
#include "rosidl_runtime_c/string.h"

/// Struct defined in msg/SwarmPathCommand in the package combat_robot_msgs.
typedef struct combat_robot_msgs__msg__SwarmPathCommand
{
  std_msgs__msg__Header header;
  uint8_t command;
  uint16_t num_waypoints;
  rosidl_runtime_c__String path_json;
} combat_robot_msgs__msg__SwarmPathCommand;

// Struct for a sequence of combat_robot_msgs__msg__SwarmPathCommand.
typedef struct combat_robot_msgs__msg__SwarmPathCommand__Sequence
{
  combat_robot_msgs__msg__SwarmPathCommand * data;
  /// The number of valid items in data
  size_t size;
  /// The number of allocated items in data
  size_t capacity;
} combat_robot_msgs__msg__SwarmPathCommand__Sequence;

#ifdef __cplusplus
}
#endif

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__SWARM_PATH_COMMAND__STRUCT_H_
