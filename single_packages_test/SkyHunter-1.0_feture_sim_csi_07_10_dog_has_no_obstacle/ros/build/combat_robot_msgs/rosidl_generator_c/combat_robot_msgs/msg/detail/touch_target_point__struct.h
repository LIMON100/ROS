// generated from rosidl_generator_c/resource/idl__struct.h.em
// with input from combat_robot_msgs:msg/TouchTargetPoint.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/touch_target_point.h"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__TOUCH_TARGET_POINT__STRUCT_H_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__TOUCH_TARGET_POINT__STRUCT_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Constants defined in the message

/// Struct defined in msg/TouchTargetPoint in the package combat_robot_msgs.
typedef struct combat_robot_msgs__msg__TouchTargetPoint
{
  float touch_x;
  float touch_y;
} combat_robot_msgs__msg__TouchTargetPoint;

// Struct for a sequence of combat_robot_msgs__msg__TouchTargetPoint.
typedef struct combat_robot_msgs__msg__TouchTargetPoint__Sequence
{
  combat_robot_msgs__msg__TouchTargetPoint * data;
  /// The number of valid items in data
  size_t size;
  /// The number of allocated items in data
  size_t capacity;
} combat_robot_msgs__msg__TouchTargetPoint__Sequence;

#ifdef __cplusplus
}
#endif

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__TOUCH_TARGET_POINT__STRUCT_H_
