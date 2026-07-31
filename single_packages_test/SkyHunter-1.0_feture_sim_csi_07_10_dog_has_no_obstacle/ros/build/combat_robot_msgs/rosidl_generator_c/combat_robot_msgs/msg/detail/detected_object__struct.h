// generated from rosidl_generator_c/resource/idl__struct.h.em
// with input from combat_robot_msgs:msg/DetectedObject.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/detected_object.h"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__DETECTED_OBJECT__STRUCT_H_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__DETECTED_OBJECT__STRUCT_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Constants defined in the message

// Include directives for member types
// Member 'box'
#include "combat_robot_msgs/msg/detail/bounding_box2d__struct.h"

/// Struct defined in msg/DetectedObject in the package combat_robot_msgs.
/**
  * DetectedObject.msg
 */
typedef struct combat_robot_msgs__msg__DetectedObject
{
  int32_t id;
  float prob;
  combat_robot_msgs__msg__BoundingBox2d box;
} combat_robot_msgs__msg__DetectedObject;

// Struct for a sequence of combat_robot_msgs__msg__DetectedObject.
typedef struct combat_robot_msgs__msg__DetectedObject__Sequence
{
  combat_robot_msgs__msg__DetectedObject * data;
  /// The number of valid items in data
  size_t size;
  /// The number of allocated items in data
  size_t capacity;
} combat_robot_msgs__msg__DetectedObject__Sequence;

#ifdef __cplusplus
}
#endif

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__DETECTED_OBJECT__STRUCT_H_
