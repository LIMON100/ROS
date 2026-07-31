// generated from rosidl_generator_c/resource/idl__struct.h.em
// with input from combat_robot_msgs:msg/UserCommand.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/user_command.h"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__USER_COMMAND__STRUCT_H_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__USER_COMMAND__STRUCT_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Constants defined in the message

/// Constant 'TABLET'.
enum
{
  combat_robot_msgs__msg__UserCommand__TABLET = 0
};

/// Constant 'IDLE'.
/**
  * Command ID
 */
enum
{
  combat_robot_msgs__msg__UserCommand__IDLE = 0
};

/// Constant 'RECON'.
enum
{
  combat_robot_msgs__msg__UserCommand__RECON = 1
};

/// Constant 'PROTECT_GENERAL'.
enum
{
  combat_robot_msgs__msg__UserCommand__PROTECT_GENERAL = 2
};

/// Constant 'PROTECT_DRONE'.
enum
{
  combat_robot_msgs__msg__UserCommand__PROTECT_DRONE = 3
};

/// Constant 'DEBUG_ATTACK'.
enum
{
  combat_robot_msgs__msg__UserCommand__DEBUG_ATTACK = 4
};

/// Constant 'DEBUG_TRACKING'.
enum
{
  combat_robot_msgs__msg__UserCommand__DEBUG_TRACKING = 5
};

/// Constant 'ASSAULT'.
enum
{
  combat_robot_msgs__msg__UserCommand__ASSAULT = 6
};

/// Constant 'RETURN_TO_HOME'.
enum
{
  combat_robot_msgs__msg__UserCommand__RETURN_TO_HOME = 7
};

/// Constant 'ESTOP'.
enum
{
  combat_robot_msgs__msg__UserCommand__ESTOP = 8
};

/// Constant 'STREAM_NONE'.
/**
  * Stream command
 */
enum
{
  combat_robot_msgs__msg__UserCommand__STREAM_NONE = 0
};

/// Constant 'STREAM_START'.
enum
{
  combat_robot_msgs__msg__UserCommand__STREAM_START = 1
};

/// Constant 'STREAM_STOP'.
enum
{
  combat_robot_msgs__msg__UserCommand__STREAM_STOP = 2
};

// Include directives for member types
// Member 'header'
#include "std_msgs/msg/detail/header__struct.h"

/// Struct defined in msg/UserCommand in the package combat_robot_msgs.
/**
  * Command command_from
 */
typedef struct combat_robot_msgs__msg__UserCommand
{
  /// UserCommand.msg
  /// This message is used to send user commands to the combat robot system.
  std_msgs__msg__Header header;
  /// 0 - tablet / 1 - ble
  uint8_t command_from;
  /// Command ID
  /// 0=Idle, 1=Recon, 2=Protect General, 3=Protect Drone, 4/5=Debug, 6=Assault, 7=Return to Home, 8=Estop
  uint8_t command_id;
  /// Manual Targeting Coordinates
  /// X coordinate in normalized pixels (0.0 - 1.0)
  float target_x;
  /// Y coordinate in normalized pixels (0.0 - 1.0)
  float target_y;
  /// Drone Search Target
  double drone_target_lat;
  double drone_target_lon;
  bool drone_target_valid;
  /// Gun trigger control
  /// 0 - stop / 1 - start
  bool gun_trigger;
  /// 0 - no permission / 1 - permission granted
  bool gun_trigger_permission;
  /// Gimbal / stream control
  int8_t pan_speed;
  int8_t tilt_speed;
  int8_t zoom_command;
  /// 0=None, 1=Start, 2=Stop
  uint8_t stream_command;
} combat_robot_msgs__msg__UserCommand;

// Struct for a sequence of combat_robot_msgs__msg__UserCommand.
typedef struct combat_robot_msgs__msg__UserCommand__Sequence
{
  combat_robot_msgs__msg__UserCommand * data;
  /// The number of valid items in data
  size_t size;
  /// The number of allocated items in data
  size_t capacity;
} combat_robot_msgs__msg__UserCommand__Sequence;

#ifdef __cplusplus
}
#endif

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__USER_COMMAND__STRUCT_H_
