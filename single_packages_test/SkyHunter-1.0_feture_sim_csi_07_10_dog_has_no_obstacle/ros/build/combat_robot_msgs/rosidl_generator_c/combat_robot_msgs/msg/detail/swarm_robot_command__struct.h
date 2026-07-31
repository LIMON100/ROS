// generated from rosidl_generator_c/resource/idl__struct.h.em
// with input from combat_robot_msgs:msg/SwarmRobotCommand.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/swarm_robot_command.h"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__SWARM_ROBOT_COMMAND__STRUCT_H_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__SWARM_ROBOT_COMMAND__STRUCT_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Constants defined in the message

/// Constant 'COMMAND_NONE'.
enum
{
  combat_robot_msgs__msg__SwarmRobotCommand__COMMAND_NONE = 0
};

/// Constant 'COMMAND_MODE'.
enum
{
  combat_robot_msgs__msg__SwarmRobotCommand__COMMAND_MODE = 1
};

/// Constant 'COMMAND_PATH'.
enum
{
  combat_robot_msgs__msg__SwarmRobotCommand__COMMAND_PATH = 2
};

/// Constant 'COMMAND_FORMATION'.
enum
{
  combat_robot_msgs__msg__SwarmRobotCommand__COMMAND_FORMATION = 3
};

/// Constant 'COMMAND_SYNC'.
enum
{
  combat_robot_msgs__msg__SwarmRobotCommand__COMMAND_SYNC = 4
};

/// Constant 'PATH_CMD_NONE'.
enum
{
  combat_robot_msgs__msg__SwarmRobotCommand__PATH_CMD_NONE = 0
};

/// Constant 'PATH_CMD_START'.
enum
{
  combat_robot_msgs__msg__SwarmRobotCommand__PATH_CMD_START = 1
};

/// Constant 'PATH_CMD_STOP'.
enum
{
  combat_robot_msgs__msg__SwarmRobotCommand__PATH_CMD_STOP = 2
};

/// Constant 'PATH_CMD_PAUSE'.
enum
{
  combat_robot_msgs__msg__SwarmRobotCommand__PATH_CMD_PAUSE = 3
};

/// Constant 'PATH_CMD_RESUME'.
enum
{
  combat_robot_msgs__msg__SwarmRobotCommand__PATH_CMD_RESUME = 4
};

/// Constant 'PATH_CMD_LOAD_PATH'.
enum
{
  combat_robot_msgs__msg__SwarmRobotCommand__PATH_CMD_LOAD_PATH = 5
};

// Include directives for member types
// Member 'header'
#include "std_msgs/msg/detail/header__struct.h"
// Member 'path_id'
// Member 'path_json'
#include "rosidl_runtime_c/string.h"

/// Struct defined in msg/SwarmRobotCommand in the package combat_robot_msgs.
typedef struct combat_robot_msgs__msg__SwarmRobotCommand
{
  std_msgs__msg__Header header;
  uint32_t sequence;
  uint8_t command_type;
  uint32_t leader_robot_id;
  uint32_t target_robot_id;
  uint8_t operation_mode;
  bool estop_requested;
  uint8_t path_command;
  uint16_t num_waypoints;
  rosidl_runtime_c__String path_id;
  rosidl_runtime_c__String path_json;
  uint8_t formation_type;
  uint8_t formation_number;
  uint8_t grouping_index;
  uint8_t slot_index;
  uint8_t selected_robot_count;
  uint32_t selected_robot_ids[8];
} combat_robot_msgs__msg__SwarmRobotCommand;

// Struct for a sequence of combat_robot_msgs__msg__SwarmRobotCommand.
typedef struct combat_robot_msgs__msg__SwarmRobotCommand__Sequence
{
  combat_robot_msgs__msg__SwarmRobotCommand * data;
  /// The number of valid items in data
  size_t size;
  /// The number of allocated items in data
  size_t capacity;
} combat_robot_msgs__msg__SwarmRobotCommand__Sequence;

#ifdef __cplusplus
}
#endif

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__SWARM_ROBOT_COMMAND__STRUCT_H_
