// generated from rosidl_generator_c/resource/idl__struct.h.em
// with input from combat_robot_msgs:msg/WaypointList.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/waypoint_list.h"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__WAYPOINT_LIST__STRUCT_H_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__WAYPOINT_LIST__STRUCT_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Constants defined in the message

// Include directives for member types
// Member 'waypoints'
#include "combat_robot_msgs/msg/detail/waypoint__struct.h"

/// Struct defined in msg/WaypointList in the package combat_robot_msgs.
typedef struct combat_robot_msgs__msg__WaypointList
{
  int32_t mode;
  int32_t formation;
  int32_t mission_id;
  int32_t mission_status;
  combat_robot_msgs__msg__Waypoint__Sequence waypoints;
} combat_robot_msgs__msg__WaypointList;

// Struct for a sequence of combat_robot_msgs__msg__WaypointList.
typedef struct combat_robot_msgs__msg__WaypointList__Sequence
{
  combat_robot_msgs__msg__WaypointList * data;
  /// The number of valid items in data
  size_t size;
  /// The number of allocated items in data
  size_t capacity;
} combat_robot_msgs__msg__WaypointList__Sequence;

#ifdef __cplusplus
}
#endif

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__WAYPOINT_LIST__STRUCT_H_
