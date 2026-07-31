// generated from rosidl_generator_c/resource/idl__struct.h.em
// with input from combat_robot_msgs:msg/DetectedObjects.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/detected_objects.h"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__DETECTED_OBJECTS__STRUCT_H_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__DETECTED_OBJECTS__STRUCT_H_

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
// Member 'objects'
#include "combat_robot_msgs/msg/detail/detected_object__struct.h"

/// Struct defined in msg/DetectedObjects in the package combat_robot_msgs.
/**
  * DetectedObjects.msg
 */
typedef struct combat_robot_msgs__msg__DetectedObjects
{
  std_msgs__msg__Header header;
  int32_t image_width;
  int32_t image_height;
  combat_robot_msgs__msg__DetectedObject__Sequence objects;
} combat_robot_msgs__msg__DetectedObjects;

// Struct for a sequence of combat_robot_msgs__msg__DetectedObjects.
typedef struct combat_robot_msgs__msg__DetectedObjects__Sequence
{
  combat_robot_msgs__msg__DetectedObjects * data;
  /// The number of valid items in data
  size_t size;
  /// The number of allocated items in data
  size_t capacity;
} combat_robot_msgs__msg__DetectedObjects__Sequence;

#ifdef __cplusplus
}
#endif

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__DETECTED_OBJECTS__STRUCT_H_
