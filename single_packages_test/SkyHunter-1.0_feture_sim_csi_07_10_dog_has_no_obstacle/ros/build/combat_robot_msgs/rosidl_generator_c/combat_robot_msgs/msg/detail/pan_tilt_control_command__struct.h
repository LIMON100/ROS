// generated from rosidl_generator_c/resource/idl__struct.h.em
// with input from combat_robot_msgs:msg/PanTiltControlCommand.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/pan_tilt_control_command.h"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__PAN_TILT_CONTROL_COMMAND__STRUCT_H_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__PAN_TILT_CONTROL_COMMAND__STRUCT_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Constants defined in the message

/// Constant 'CONTROL_BRAKE'.
enum
{
  combat_robot_msgs__msg__PanTiltControlCommand__CONTROL_BRAKE = 0
};

/// Constant 'CONTROL_HOR_POS'.
enum
{
  combat_robot_msgs__msg__PanTiltControlCommand__CONTROL_HOR_POS = 1
};

/// Constant 'CONTROL_VER_POS'.
enum
{
  combat_robot_msgs__msg__PanTiltControlCommand__CONTROL_VER_POS = 2
};

/// Constant 'CONTROL_DIR'.
enum
{
  combat_robot_msgs__msg__PanTiltControlCommand__CONTROL_DIR = 3
};

// Include directives for member types
// Member 'header'
#include "std_msgs/msg/detail/header__struct.h"

/// Struct defined in msg/PanTiltControlCommand in the package combat_robot_msgs.
typedef struct combat_robot_msgs__msg__PanTiltControlCommand
{
  /// Pan-Tilt Control Command message
  std_msgs__msg__Header header;
  uint8_t control_mode;
  float horizontal_angle;
  float vertical_angle;
  uint8_t pan_speed;
  uint8_t tilt_speed;
  /// 0 - stop / 1 - right / 2 - left
  uint8_t pan_dir;
  /// 0 - stop / 1 - up / 2 - down
  uint8_t tilt_dir;
} combat_robot_msgs__msg__PanTiltControlCommand;

// Struct for a sequence of combat_robot_msgs__msg__PanTiltControlCommand.
typedef struct combat_robot_msgs__msg__PanTiltControlCommand__Sequence
{
  combat_robot_msgs__msg__PanTiltControlCommand * data;
  /// The number of valid items in data
  size_t size;
  /// The number of allocated items in data
  size_t capacity;
} combat_robot_msgs__msg__PanTiltControlCommand__Sequence;

#ifdef __cplusplus
}
#endif

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__PAN_TILT_CONTROL_COMMAND__STRUCT_H_
