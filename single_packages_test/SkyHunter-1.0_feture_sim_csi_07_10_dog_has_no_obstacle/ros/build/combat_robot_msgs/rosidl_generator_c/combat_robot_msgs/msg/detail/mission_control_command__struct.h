// generated from rosidl_generator_c/resource/idl__struct.h.em
// with input from combat_robot_msgs:msg/MissionControlCommand.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/mission_control_command.h"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__MISSION_CONTROL_COMMAND__STRUCT_H_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__MISSION_CONTROL_COMMAND__STRUCT_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Constants defined in the message

/// Constant 'IDLE'.
enum
{
  combat_robot_msgs__msg__MissionControlCommand__IDLE = 0
};

/// Constant 'RECON'.
enum
{
  combat_robot_msgs__msg__MissionControlCommand__RECON = 1
};

/// Constant 'PROTECT_GENERAL'.
enum
{
  combat_robot_msgs__msg__MissionControlCommand__PROTECT_GENERAL = 2
};

/// Constant 'PROTECT_DRONE'.
enum
{
  combat_robot_msgs__msg__MissionControlCommand__PROTECT_DRONE = 3
};

/// Constant 'DEBUG_ATTACK'.
enum
{
  combat_robot_msgs__msg__MissionControlCommand__DEBUG_ATTACK = 4
};

/// Constant 'DEBUG_TRACKING'.
enum
{
  combat_robot_msgs__msg__MissionControlCommand__DEBUG_TRACKING = 5
};

/// Constant 'ASSAULT'.
enum
{
  combat_robot_msgs__msg__MissionControlCommand__ASSAULT = 6
};

/// Constant 'RETURN_TO_HOME'.
enum
{
  combat_robot_msgs__msg__MissionControlCommand__RETURN_TO_HOME = 7
};

/// Constant 'ATTACK_PERMISSION_NONE'.
enum
{
  combat_robot_msgs__msg__MissionControlCommand__ATTACK_PERMISSION_NONE = 0
};

/// Constant 'ATTACK_PERMISSION_APPROVE'.
enum
{
  combat_robot_msgs__msg__MissionControlCommand__ATTACK_PERMISSION_APPROVE = 1
};

/// Constant 'ATTACK_PERMISSION_DENY'.
enum
{
  combat_robot_msgs__msg__MissionControlCommand__ATTACK_PERMISSION_DENY = 2
};

// Include directives for member types
// Member 'header'
#include "std_msgs/msg/detail/header__struct.h"

/// Struct defined in msg/MissionControlCommand in the package combat_robot_msgs.
typedef struct combat_robot_msgs__msg__MissionControlCommand
{
  std_msgs__msg__Header header;
  uint8_t command_id;
  bool estop_requested;
  uint8_t attack_permission;
  int8_t pan_speed;
  int8_t tilt_speed;
  int8_t zoom_command;
  float lateral_wind_speed;
  double drone_target_lat;
  double drone_target_lon;
  bool drone_target_valid;
} combat_robot_msgs__msg__MissionControlCommand;

// Struct for a sequence of combat_robot_msgs__msg__MissionControlCommand.
typedef struct combat_robot_msgs__msg__MissionControlCommand__Sequence
{
  combat_robot_msgs__msg__MissionControlCommand * data;
  /// The number of valid items in data
  size_t size;
  /// The number of allocated items in data
  size_t capacity;
} combat_robot_msgs__msg__MissionControlCommand__Sequence;

#ifdef __cplusplus
}
#endif

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__MISSION_CONTROL_COMMAND__STRUCT_H_
