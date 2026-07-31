// generated from rosidl_generator_c/resource/idl__struct.h.em
// with input from combat_robot_msgs:msg/StreamControlCommand.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/stream_control_command.h"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__STREAM_CONTROL_COMMAND__STRUCT_H_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__STREAM_CONTROL_COMMAND__STRUCT_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Constants defined in the message

/// Constant 'STREAM_NONE'.
enum
{
  combat_robot_msgs__msg__StreamControlCommand__STREAM_NONE = 0
};

/// Constant 'STREAM_START'.
enum
{
  combat_robot_msgs__msg__StreamControlCommand__STREAM_START = 1
};

/// Constant 'STREAM_STOP'.
enum
{
  combat_robot_msgs__msg__StreamControlCommand__STREAM_STOP = 2
};

// Include directives for member types
// Member 'header'
#include "std_msgs/msg/detail/header__struct.h"

/// Struct defined in msg/StreamControlCommand in the package combat_robot_msgs.
typedef struct combat_robot_msgs__msg__StreamControlCommand
{
  std_msgs__msg__Header header;
  uint8_t stream_command;
  uint32_t stream_target_robot_id;
} combat_robot_msgs__msg__StreamControlCommand;

// Struct for a sequence of combat_robot_msgs__msg__StreamControlCommand.
typedef struct combat_robot_msgs__msg__StreamControlCommand__Sequence
{
  combat_robot_msgs__msg__StreamControlCommand * data;
  /// The number of valid items in data
  size_t size;
  /// The number of allocated items in data
  size_t capacity;
} combat_robot_msgs__msg__StreamControlCommand__Sequence;

#ifdef __cplusplus
}
#endif

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__STREAM_CONTROL_COMMAND__STRUCT_H_
