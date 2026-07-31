// generated from rosidl_generator_c/resource/idl__struct.h.em
// with input from combat_robot_msgs:msg/IMUState.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/imu_state.h"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__IMU_STATE__STRUCT_H_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__IMU_STATE__STRUCT_H_

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
// Member 'angle'
#include "geometry_msgs/msg/detail/vector3__struct.h"
// Member 'device_id'
#include "rosidl_runtime_c/string.h"

/// Struct defined in msg/IMUState in the package combat_robot_msgs.
typedef struct combat_robot_msgs__msg__IMUState
{
  std_msgs__msg__Header header;
  /// Roll Pitch Yaw
  geometry_msgs__msg__Vector3 angle;
  bool is_connected;
  /// "gun" or "car"
  rosidl_runtime_c__String device_id;
} combat_robot_msgs__msg__IMUState;

// Struct for a sequence of combat_robot_msgs__msg__IMUState.
typedef struct combat_robot_msgs__msg__IMUState__Sequence
{
  combat_robot_msgs__msg__IMUState * data;
  /// The number of valid items in data
  size_t size;
  /// The number of allocated items in data
  size_t capacity;
} combat_robot_msgs__msg__IMUState__Sequence;

#ifdef __cplusplus
}
#endif

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__IMU_STATE__STRUCT_H_
