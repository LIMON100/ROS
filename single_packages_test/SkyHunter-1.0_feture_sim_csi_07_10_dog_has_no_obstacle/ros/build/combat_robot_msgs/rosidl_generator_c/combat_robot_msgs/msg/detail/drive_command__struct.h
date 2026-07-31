// NOLINT: This file starts with a BOM since it contain non-ASCII characters
// generated from rosidl_generator_c/resource/idl__struct.h.em
// with input from combat_robot_msgs:msg/DriveCommand.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/drive_command.h"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__DRIVE_COMMAND__STRUCT_H_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__DRIVE_COMMAND__STRUCT_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Constants defined in the message

// Include directives for member types
// Member 'header'
#include "std_msgs/msg/detail/header__struct.h"

/// Struct defined in msg/DriveCommand in the package combat_robot_msgs.
/**
  * combat_robot_msgs/msg/DriveCommand.msg
 */
typedef struct combat_robot_msgs__msg__DriveCommand
{
  std_msgs__msg__Header header;
  /// m/s, forward(+)/backward(-)
  float linear_velocity;
  /// rad/s, left(+)/right(-)  ← 기존 부호 규칙 그대로 사용
  float angular_velocity;
} combat_robot_msgs__msg__DriveCommand;

// Struct for a sequence of combat_robot_msgs__msg__DriveCommand.
typedef struct combat_robot_msgs__msg__DriveCommand__Sequence
{
  combat_robot_msgs__msg__DriveCommand * data;
  /// The number of valid items in data
  size_t size;
  /// The number of allocated items in data
  size_t capacity;
} combat_robot_msgs__msg__DriveCommand__Sequence;

#ifdef __cplusplus
}
#endif

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__DRIVE_COMMAND__STRUCT_H_
