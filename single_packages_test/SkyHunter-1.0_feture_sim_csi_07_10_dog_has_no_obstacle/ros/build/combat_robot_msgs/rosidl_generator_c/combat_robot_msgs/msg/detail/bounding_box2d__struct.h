// generated from rosidl_generator_c/resource/idl__struct.h.em
// with input from combat_robot_msgs:msg/BoundingBox2d.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/bounding_box2d.h"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__BOUNDING_BOX2D__STRUCT_H_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__BOUNDING_BOX2D__STRUCT_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Constants defined in the message

/// Struct defined in msg/BoundingBox2d in the package combat_robot_msgs.
/**
  * BoundingBox2d.msg
 */
typedef struct combat_robot_msgs__msg__BoundingBox2d
{
  int32_t x;
  int32_t y;
  int32_t width;
  int32_t height;
} combat_robot_msgs__msg__BoundingBox2d;

// Struct for a sequence of combat_robot_msgs__msg__BoundingBox2d.
typedef struct combat_robot_msgs__msg__BoundingBox2d__Sequence
{
  combat_robot_msgs__msg__BoundingBox2d * data;
  /// The number of valid items in data
  size_t size;
  /// The number of allocated items in data
  size_t capacity;
} combat_robot_msgs__msg__BoundingBox2d__Sequence;

#ifdef __cplusplus
}
#endif

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__BOUNDING_BOX2D__STRUCT_H_
