// generated from rosidl_generator_c/resource/idl__struct.h.em
// with input from skyhunter_msgs:msg/LeaderState.idl
// generated code does not contain a copyright notice

#ifndef SKYHUNTER_MSGS__MSG__DETAIL__LEADER_STATE__STRUCT_H_
#define SKYHUNTER_MSGS__MSG__DETAIL__LEADER_STATE__STRUCT_H_

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
// Member 'pose'
// Member 'next_waypoints'
#include "geometry_msgs/msg/detail/pose__struct.h"
// Member 'velocity'
#include "geometry_msgs/msg/detail/twist__struct.h"

/// Struct defined in msg/LeaderState in the package skyhunter_msgs.
typedef struct skyhunter_msgs__msg__LeaderState
{
  std_msgs__msg__Header header;
  geometry_msgs__msg__Pose pose;
  geometry_msgs__msg__Twist velocity;
  geometry_msgs__msg__Pose__Sequence next_waypoints;
  uint8_t formation_mode;
  uint8_t formation_state;
  /// 0: NAVIGATING, 1: GOAL_REACHED, 2: TRANSITIONING
  int8_t swarm_state;
  /// 0=V-Shape, 1=Column, 2=Diamond
  int8_t formation_type;
  int32_t current_waypoint_index;
} skyhunter_msgs__msg__LeaderState;

// Struct for a sequence of skyhunter_msgs__msg__LeaderState.
typedef struct skyhunter_msgs__msg__LeaderState__Sequence
{
  skyhunter_msgs__msg__LeaderState * data;
  /// The number of valid items in data
  size_t size;
  /// The number of allocated items in data
  size_t capacity;
} skyhunter_msgs__msg__LeaderState__Sequence;

#ifdef __cplusplus
}
#endif

#endif  // SKYHUNTER_MSGS__MSG__DETAIL__LEADER_STATE__STRUCT_H_
