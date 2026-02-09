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
#include "geometry_msgs/msg/detail/pose__struct.h"
// Member 'velocity'
#include "geometry_msgs/msg/detail/twist__struct.h"

/// Struct defined in msg/LeaderState in the package skyhunter_msgs.
typedef struct skyhunter_msgs__msg__LeaderState
{
  std_msgs__msg__Header header;
  /// Leader's current position and orientation (Global/Odom Frame)
  geometry_msgs__msg__Pose pose;
  /// Leader's current velocity (Linear and Angular)
  geometry_msgs__msg__Twist velocity;
  /// Formation Configuration
  /// 0 = V-Shape, 1 = Diamond, 2 = Column
  uint8_t formation_mode;
  /// Formation Status
  /// 0 = NORMAL (Keep Formation)
  /// 1 = SAFETY_DETOUR (Break Formation / Avoid Obstacles)
  /// 2 = REJOIN (Return to Formation)
  uint8_t formation_state;
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
