// generated from rosidl_generator_c/resource/idl__struct.h.em
// with input from combat_robot_msgs:msg/PanTiltState.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/pan_tilt_state.h"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__PAN_TILT_STATE__STRUCT_H_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__PAN_TILT_STATE__STRUCT_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Constants defined in the message

// Include directives for member types
// Member 'stamp'
#include "builtin_interfaces/msg/detail/time__struct.h"

/// Struct defined in msg/PanTiltState in the package combat_robot_msgs.
typedef struct combat_robot_msgs__msg__PanTiltState
{
  builtin_interfaces__msg__Time stamp;
  uint8_t control_mode;
  float horizontal_angle;
  float vertical_angle;
  int32_t pan_speed;
  int32_t tilt_speed;
} combat_robot_msgs__msg__PanTiltState;

// Struct for a sequence of combat_robot_msgs__msg__PanTiltState.
typedef struct combat_robot_msgs__msg__PanTiltState__Sequence
{
  combat_robot_msgs__msg__PanTiltState * data;
  /// The number of valid items in data
  size_t size;
  /// The number of allocated items in data
  size_t capacity;
} combat_robot_msgs__msg__PanTiltState__Sequence;

#ifdef __cplusplus
}
#endif

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__PAN_TILT_STATE__STRUCT_H_
