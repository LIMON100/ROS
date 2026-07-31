// generated from rosidl_generator_c/resource/idl__struct.h.em
// with input from combat_robot_msgs:msg/Waypoint.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/waypoint.h"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__WAYPOINT__STRUCT_H_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__WAYPOINT__STRUCT_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Constants defined in the message

/// Struct defined in msg/Waypoint in the package combat_robot_msgs.
typedef struct combat_robot_msgs__msg__Waypoint
{
  int32_t way_id;
  double way_lon;
  double way_lat;
  int32_t way_status;
} combat_robot_msgs__msg__Waypoint;

// Struct for a sequence of combat_robot_msgs__msg__Waypoint.
typedef struct combat_robot_msgs__msg__Waypoint__Sequence
{
  combat_robot_msgs__msg__Waypoint * data;
  /// The number of valid items in data
  size_t size;
  /// The number of allocated items in data
  size_t capacity;
} combat_robot_msgs__msg__Waypoint__Sequence;

#ifdef __cplusplus
}
#endif

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__WAYPOINT__STRUCT_H_
