// generated from rosidl_generator_c/resource/idl__struct.h.em
// with input from skyhunter_msgs:msg/SwarmHeartbeat.idl
// generated code does not contain a copyright notice

#ifndef SKYHUNTER_MSGS__MSG__DETAIL__SWARM_HEARTBEAT__STRUCT_H_
#define SKYHUNTER_MSGS__MSG__DETAIL__SWARM_HEARTBEAT__STRUCT_H_

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
// Member 'robot_id'
#include "rosidl_runtime_c/string.h"

/// Struct defined in msg/SwarmHeartbeat in the package skyhunter_msgs.
typedef struct skyhunter_msgs__msg__SwarmHeartbeat
{
  std_msgs__msg__Header header;
  rosidl_runtime_c__String robot_id;
  uint32_t term;
  bool is_leader;
  float battery_level;
  int32_t leader_id_num;
} skyhunter_msgs__msg__SwarmHeartbeat;

// Struct for a sequence of skyhunter_msgs__msg__SwarmHeartbeat.
typedef struct skyhunter_msgs__msg__SwarmHeartbeat__Sequence
{
  skyhunter_msgs__msg__SwarmHeartbeat * data;
  /// The number of valid items in data
  size_t size;
  /// The number of allocated items in data
  size_t capacity;
} skyhunter_msgs__msg__SwarmHeartbeat__Sequence;

#ifdef __cplusplus
}
#endif

#endif  // SKYHUNTER_MSGS__MSG__DETAIL__SWARM_HEARTBEAT__STRUCT_H_
