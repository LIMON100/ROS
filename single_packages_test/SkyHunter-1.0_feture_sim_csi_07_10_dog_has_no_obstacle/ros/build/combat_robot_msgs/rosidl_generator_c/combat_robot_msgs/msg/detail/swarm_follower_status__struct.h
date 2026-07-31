// generated from rosidl_generator_c/resource/idl__struct.h.em
// with input from combat_robot_msgs:msg/SwarmFollowerStatus.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/swarm_follower_status.h"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__SWARM_FOLLOWER_STATUS__STRUCT_H_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__SWARM_FOLLOWER_STATUS__STRUCT_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Constants defined in the message

/// Constant 'LINK_DISCONNECTED'.
enum
{
  combat_robot_msgs__msg__SwarmFollowerStatus__LINK_DISCONNECTED = 0
};

/// Constant 'LINK_CONNECTED'.
enum
{
  combat_robot_msgs__msg__SwarmFollowerStatus__LINK_CONNECTED = 1
};

// Include directives for member types
// Member 'header'
#include "std_msgs/msg/detail/header__struct.h"

/// Struct defined in msg/SwarmFollowerStatus in the package combat_robot_msgs.
typedef struct combat_robot_msgs__msg__SwarmFollowerStatus
{
  std_msgs__msg__Header header;
  uint32_t robot_id;
  uint32_t leader_robot_id;
  uint8_t link_status;
  uint32_t last_heartbeat_sequence;
  float heartbeat_age_sec;
  uint8_t last_operation_mode;
  uint8_t last_formation_type;
  uint8_t last_formation_number;
  uint8_t last_grouping_index;
  /// Follower position so the operator-host (leader) command_server can place this
  /// robot on the tablet swarm map. Filled by the follower's own command_server from
  /// its NavSatFix (/sN/fix); 0 until a fix is received.
  double latitude;
  double longitude;
  float heading_deg;
  float ground_speed_mps;
} combat_robot_msgs__msg__SwarmFollowerStatus;

// Struct for a sequence of combat_robot_msgs__msg__SwarmFollowerStatus.
typedef struct combat_robot_msgs__msg__SwarmFollowerStatus__Sequence
{
  combat_robot_msgs__msg__SwarmFollowerStatus * data;
  /// The number of valid items in data
  size_t size;
  /// The number of allocated items in data
  size_t capacity;
} combat_robot_msgs__msg__SwarmFollowerStatus__Sequence;

#ifdef __cplusplus
}
#endif

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__SWARM_FOLLOWER_STATUS__STRUCT_H_
