// generated from rosidl_generator_c/resource/idl__struct.h.em
// with input from combat_robot_msgs:msg/OperationState.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/operation_state.h"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__OPERATION_STATE__STRUCT_H_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__OPERATION_STATE__STRUCT_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Constants defined in the message

/// Constant 'INIT'.
enum
{
  combat_robot_msgs__msg__OperationState__INIT = 0
};

/// Constant 'IDLE'.
enum
{
  combat_robot_msgs__msg__OperationState__IDLE = 1
};

/// Constant 'MOVE'.
enum
{
  combat_robot_msgs__msg__OperationState__MOVE = 2
};

/// Constant 'SURVEILLANCE'.
enum
{
  combat_robot_msgs__msg__OperationState__SURVEILLANCE = 3
};

/// Constant 'DRONE_SURVEILLANCE'.
enum
{
  combat_robot_msgs__msg__OperationState__DRONE_SURVEILLANCE = 4
};

/// Constant 'MANUAL_ATTACK'.
enum
{
  combat_robot_msgs__msg__OperationState__MANUAL_ATTACK = 5
};

/// Constant 'ASSAULT'.
enum
{
  combat_robot_msgs__msg__OperationState__ASSAULT = 6
};

/// Constant 'TRACKING'.
enum
{
  combat_robot_msgs__msg__OperationState__TRACKING = 7
};

/// Constant 'EMERGENCY_STOP'.
enum
{
  combat_robot_msgs__msg__OperationState__EMERGENCY_STOP = 8
};

/// Constant 'ERROR'.
enum
{
  combat_robot_msgs__msg__OperationState__ERROR = 9
};

/// Constant 'ACTIVE_MODE_IDLE'.
/**
  * App-facing active mode IDs
 */
enum
{
  combat_robot_msgs__msg__OperationState__ACTIVE_MODE_IDLE = 0
};

/// Constant 'ACTIVE_MODE_RECON'.
enum
{
  combat_robot_msgs__msg__OperationState__ACTIVE_MODE_RECON = 1
};

/// Constant 'ACTIVE_MODE_PROTECT_GENERAL'.
enum
{
  combat_robot_msgs__msg__OperationState__ACTIVE_MODE_PROTECT_GENERAL = 2
};

/// Constant 'ACTIVE_MODE_PROTECT_DRONE'.
enum
{
  combat_robot_msgs__msg__OperationState__ACTIVE_MODE_PROTECT_DRONE = 3
};

/// Constant 'ACTIVE_MODE_ASSAULT'.
enum
{
  combat_robot_msgs__msg__OperationState__ACTIVE_MODE_ASSAULT = 6
};

/// Constant 'ACTIVE_MODE_RETURN_TO_HOME'.
enum
{
  combat_robot_msgs__msg__OperationState__ACTIVE_MODE_RETURN_TO_HOME = 7
};

/// Constant 'ACTIVE_MODE_ESTOP'.
enum
{
  combat_robot_msgs__msg__OperationState__ACTIVE_MODE_ESTOP = 8
};

/// Constant 'MISSION_NONE'.
/**
  * Common mission status IDs
 */
enum
{
  combat_robot_msgs__msg__OperationState__MISSION_NONE = 0
};

/// Constant 'MISSION_READY'.
enum
{
  combat_robot_msgs__msg__OperationState__MISSION_READY = 1
};

/// Constant 'MISSION_MOVING'.
enum
{
  combat_robot_msgs__msg__OperationState__MISSION_MOVING = 2
};

/// Constant 'MISSION_PAUSED'.
enum
{
  combat_robot_msgs__msg__OperationState__MISSION_PAUSED = 3
};

/// Constant 'MISSION_REACHED'.
enum
{
  combat_robot_msgs__msg__OperationState__MISSION_REACHED = 4
};

/// Constant 'MISSION_SURVEILLING'.
enum
{
  combat_robot_msgs__msg__OperationState__MISSION_SURVEILLING = 5
};

/// Constant 'MISSION_ERROR'.
enum
{
  combat_robot_msgs__msg__OperationState__MISSION_ERROR = 6
};

/// Struct defined in msg/OperationState in the package combat_robot_msgs.
/**
  * Internal ROS operation state values
 */
typedef struct combat_robot_msgs__msg__OperationState
{
  uint8_t state;
  uint8_t active_mode_id;
  uint8_t mission_status;
  bool estop_active;
  bool permission_request_active;
  float crosshair_x;
  float crosshair_y;
  float current_zoom_level;
  /// Robot navigation status
  double gps_lat;
  double gps_lon;
  float gps_heading;
  float current_speed_mps;
  /// Common mission placeholder fields for app integration
  uint16_t current_waypoint_index;
  uint16_t total_waypoints;
  float progress_ratio;
  float distance_to_next_wp_m;
  float distance_to_goal_m;
  uint8_t error_code;
} combat_robot_msgs__msg__OperationState;

// Struct for a sequence of combat_robot_msgs__msg__OperationState.
typedef struct combat_robot_msgs__msg__OperationState__Sequence
{
  combat_robot_msgs__msg__OperationState * data;
  /// The number of valid items in data
  size_t size;
  /// The number of allocated items in data
  size_t capacity;
} combat_robot_msgs__msg__OperationState__Sequence;

#ifdef __cplusplus
}
#endif

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__OPERATION_STATE__STRUCT_H_
