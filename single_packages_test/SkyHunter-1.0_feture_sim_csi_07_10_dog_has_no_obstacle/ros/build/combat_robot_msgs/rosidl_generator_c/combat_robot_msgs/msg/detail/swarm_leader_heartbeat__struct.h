// generated from rosidl_generator_c/resource/idl__struct.h.em
// with input from combat_robot_msgs:msg/SwarmLeaderHeartbeat.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/swarm_leader_heartbeat.h"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__SWARM_LEADER_HEARTBEAT__STRUCT_H_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__SWARM_LEADER_HEARTBEAT__STRUCT_H_

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

/// Struct defined in msg/SwarmLeaderHeartbeat in the package combat_robot_msgs.
typedef struct combat_robot_msgs__msg__SwarmLeaderHeartbeat
{
  std_msgs__msg__Header header;
  uint32_t sequence;
  uint32_t leader_robot_id;
  uint8_t operation_mode;
  bool estop_active;
  uint8_t formation_type;
  uint8_t formation_number;
  uint8_t grouping_index;
  uint8_t selected_robot_count;
  uint32_t selected_robot_ids[8];
} combat_robot_msgs__msg__SwarmLeaderHeartbeat;

// Struct for a sequence of combat_robot_msgs__msg__SwarmLeaderHeartbeat.
typedef struct combat_robot_msgs__msg__SwarmLeaderHeartbeat__Sequence
{
  combat_robot_msgs__msg__SwarmLeaderHeartbeat * data;
  /// The number of valid items in data
  size_t size;
  /// The number of allocated items in data
  size_t capacity;
} combat_robot_msgs__msg__SwarmLeaderHeartbeat__Sequence;

#ifdef __cplusplus
}
#endif

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__SWARM_LEADER_HEARTBEAT__STRUCT_H_
