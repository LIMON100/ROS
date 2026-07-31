// generated from rosidl_generator_c/resource/idl__struct.h.em
// with input from combat_robot_msgs:msg/TargetPoint.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/target_point.h"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__TARGET_POINT__STRUCT_H_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__TARGET_POINT__STRUCT_H_

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
// Member 'box'
#include "combat_robot_msgs/msg/detail/bounding_box2d__struct.h"

/// Struct defined in msg/TargetPoint in the package combat_robot_msgs.
typedef struct combat_robot_msgs__msg__TargetPoint
{
  std_msgs__msg__Header header;
  bool is_locked;
  double x;
  double y;
  float height;
  /// 0: person, 1: drone
  uint8_t class_id;
  combat_robot_msgs__msg__BoundingBox2d box;
  int32_t track_id;
} combat_robot_msgs__msg__TargetPoint;

// Struct for a sequence of combat_robot_msgs__msg__TargetPoint.
typedef struct combat_robot_msgs__msg__TargetPoint__Sequence
{
  combat_robot_msgs__msg__TargetPoint * data;
  /// The number of valid items in data
  size_t size;
  /// The number of allocated items in data
  size_t capacity;
} combat_robot_msgs__msg__TargetPoint__Sequence;

#ifdef __cplusplus
}
#endif

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__TARGET_POINT__STRUCT_H_
