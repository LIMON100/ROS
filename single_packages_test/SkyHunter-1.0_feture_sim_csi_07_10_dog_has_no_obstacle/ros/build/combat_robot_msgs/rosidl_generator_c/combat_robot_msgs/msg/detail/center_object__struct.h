// generated from rosidl_generator_c/resource/idl__struct.h.em
// with input from combat_robot_msgs:msg/CenterObject.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/center_object.h"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__CENTER_OBJECT__STRUCT_H_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__CENTER_OBJECT__STRUCT_H_

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
// Member 'bounding_box'
#include "combat_robot_msgs/msg/detail/bounding_box2d__struct.h"

/// Struct defined in msg/CenterObject in the package combat_robot_msgs.
/**
  * CenterObject.msg
 */
typedef struct combat_robot_msgs__msg__CenterObject
{
  std_msgs__msg__Header header;
  int32_t class_id;
  combat_robot_msgs__msg__BoundingBox2d bounding_box;
  float target_x;
  float target_y;
  float laser_distance;
  float zoom_level;
} combat_robot_msgs__msg__CenterObject;

// Struct for a sequence of combat_robot_msgs__msg__CenterObject.
typedef struct combat_robot_msgs__msg__CenterObject__Sequence
{
  combat_robot_msgs__msg__CenterObject * data;
  /// The number of valid items in data
  size_t size;
  /// The number of allocated items in data
  size_t capacity;
} combat_robot_msgs__msg__CenterObject__Sequence;

#ifdef __cplusplus
}
#endif

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__CENTER_OBJECT__STRUCT_H_
