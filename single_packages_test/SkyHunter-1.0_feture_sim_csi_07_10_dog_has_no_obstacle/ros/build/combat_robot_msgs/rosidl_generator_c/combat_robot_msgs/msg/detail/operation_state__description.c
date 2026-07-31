// generated from rosidl_generator_c/resource/idl__description.c.em
// with input from combat_robot_msgs:msg/OperationState.idl
// generated code does not contain a copyright notice

#include "combat_robot_msgs/msg/detail/operation_state__functions.h"

ROSIDL_GENERATOR_C_PUBLIC_combat_robot_msgs
const rosidl_type_hash_t *
combat_robot_msgs__msg__OperationState__get_type_hash(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static rosidl_type_hash_t hash = {1, {
      0xa2, 0x91, 0x5b, 0xb9, 0x5d, 0x1b, 0x8a, 0x45,
      0x36, 0x5d, 0x59, 0x98, 0xbc, 0x41, 0xdc, 0x7d,
      0x45, 0x44, 0x04, 0xac, 0xe0, 0x09, 0xd2, 0xd5,
      0xb0, 0xfc, 0xf8, 0xdf, 0x9b, 0x02, 0x98, 0x17,
    }};
  return &hash;
}

#include <assert.h>
#include <string.h>

// Include directives for referenced types

// Hashes for external referenced types
#ifndef NDEBUG
#endif

static char combat_robot_msgs__msg__OperationState__TYPE_NAME[] = "combat_robot_msgs/msg/OperationState";

// Define type names, field names, and default values
static char combat_robot_msgs__msg__OperationState__FIELD_NAME__state[] = "state";
static char combat_robot_msgs__msg__OperationState__FIELD_NAME__active_mode_id[] = "active_mode_id";
static char combat_robot_msgs__msg__OperationState__FIELD_NAME__mission_status[] = "mission_status";
static char combat_robot_msgs__msg__OperationState__FIELD_NAME__estop_active[] = "estop_active";
static char combat_robot_msgs__msg__OperationState__FIELD_NAME__permission_request_active[] = "permission_request_active";
static char combat_robot_msgs__msg__OperationState__FIELD_NAME__crosshair_x[] = "crosshair_x";
static char combat_robot_msgs__msg__OperationState__FIELD_NAME__crosshair_y[] = "crosshair_y";
static char combat_robot_msgs__msg__OperationState__FIELD_NAME__current_zoom_level[] = "current_zoom_level";
static char combat_robot_msgs__msg__OperationState__FIELD_NAME__gps_lat[] = "gps_lat";
static char combat_robot_msgs__msg__OperationState__FIELD_NAME__gps_lon[] = "gps_lon";
static char combat_robot_msgs__msg__OperationState__FIELD_NAME__gps_heading[] = "gps_heading";
static char combat_robot_msgs__msg__OperationState__FIELD_NAME__current_speed_mps[] = "current_speed_mps";
static char combat_robot_msgs__msg__OperationState__FIELD_NAME__current_waypoint_index[] = "current_waypoint_index";
static char combat_robot_msgs__msg__OperationState__FIELD_NAME__total_waypoints[] = "total_waypoints";
static char combat_robot_msgs__msg__OperationState__FIELD_NAME__progress_ratio[] = "progress_ratio";
static char combat_robot_msgs__msg__OperationState__FIELD_NAME__distance_to_next_wp_m[] = "distance_to_next_wp_m";
static char combat_robot_msgs__msg__OperationState__FIELD_NAME__distance_to_goal_m[] = "distance_to_goal_m";
static char combat_robot_msgs__msg__OperationState__FIELD_NAME__error_code[] = "error_code";

static rosidl_runtime_c__type_description__Field combat_robot_msgs__msg__OperationState__FIELDS[] = {
  {
    {combat_robot_msgs__msg__OperationState__FIELD_NAME__state, 5, 5},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_UINT8,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__OperationState__FIELD_NAME__active_mode_id, 14, 14},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_UINT8,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__OperationState__FIELD_NAME__mission_status, 14, 14},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_UINT8,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__OperationState__FIELD_NAME__estop_active, 12, 12},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_BOOLEAN,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__OperationState__FIELD_NAME__permission_request_active, 25, 25},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_BOOLEAN,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__OperationState__FIELD_NAME__crosshair_x, 11, 11},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_FLOAT,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__OperationState__FIELD_NAME__crosshair_y, 11, 11},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_FLOAT,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__OperationState__FIELD_NAME__current_zoom_level, 18, 18},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_FLOAT,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__OperationState__FIELD_NAME__gps_lat, 7, 7},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_DOUBLE,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__OperationState__FIELD_NAME__gps_lon, 7, 7},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_DOUBLE,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__OperationState__FIELD_NAME__gps_heading, 11, 11},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_FLOAT,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__OperationState__FIELD_NAME__current_speed_mps, 17, 17},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_FLOAT,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__OperationState__FIELD_NAME__current_waypoint_index, 22, 22},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_UINT16,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__OperationState__FIELD_NAME__total_waypoints, 15, 15},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_UINT16,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__OperationState__FIELD_NAME__progress_ratio, 14, 14},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_FLOAT,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__OperationState__FIELD_NAME__distance_to_next_wp_m, 21, 21},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_FLOAT,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__OperationState__FIELD_NAME__distance_to_goal_m, 18, 18},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_FLOAT,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__OperationState__FIELD_NAME__error_code, 10, 10},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_UINT8,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
};

const rosidl_runtime_c__type_description__TypeDescription *
combat_robot_msgs__msg__OperationState__get_type_description(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static bool constructed = false;
  static const rosidl_runtime_c__type_description__TypeDescription description = {
    {
      {combat_robot_msgs__msg__OperationState__TYPE_NAME, 36, 36},
      {combat_robot_msgs__msg__OperationState__FIELDS, 18, 18},
    },
    {NULL, 0, 0},
  };
  if (!constructed) {
    constructed = true;
  }
  return &description;
}

static char toplevel_type_raw_source[] =
  "# Internal ROS operation state values\n"
  "uint8 INIT = 0\n"
  "uint8 IDLE = 1\n"
  "uint8 MOVE = 2\n"
  "uint8 SURVEILLANCE = 3\n"
  "uint8 DRONE_SURVEILLANCE = 4\n"
  "uint8 MANUAL_ATTACK = 5\n"
  "uint8 ASSAULT = 6\n"
  "uint8 TRACKING = 7\n"
  "uint8 EMERGENCY_STOP = 8\n"
  "uint8 ERROR = 9\n"
  "\n"
  "# App-facing active mode IDs\n"
  "uint8 ACTIVE_MODE_IDLE = 0\n"
  "uint8 ACTIVE_MODE_RECON = 1\n"
  "uint8 ACTIVE_MODE_PROTECT_GENERAL = 2\n"
  "uint8 ACTIVE_MODE_PROTECT_DRONE = 3\n"
  "uint8 ACTIVE_MODE_ASSAULT = 6\n"
  "uint8 ACTIVE_MODE_RETURN_TO_HOME = 7\n"
  "uint8 ACTIVE_MODE_ESTOP = 8\n"
  "\n"
  "# Common mission status IDs\n"
  "uint8 MISSION_NONE = 0\n"
  "uint8 MISSION_READY = 1\n"
  "uint8 MISSION_MOVING = 2\n"
  "uint8 MISSION_PAUSED = 3\n"
  "uint8 MISSION_REACHED = 4\n"
  "uint8 MISSION_SURVEILLING = 5\n"
  "uint8 MISSION_ERROR = 6\n"
  "\n"
  "uint8 state\n"
  "uint8 active_mode_id\n"
  "uint8 mission_status\n"
  "bool estop_active\n"
  "\n"
  "bool permission_request_active\n"
  "float32 crosshair_x\n"
  "float32 crosshair_y\n"
  "float32 current_zoom_level\n"
  "\n"
  "# Robot navigation status\n"
  "float64 gps_lat\n"
  "float64 gps_lon\n"
  "float32 gps_heading\n"
  "float32 current_speed_mps\n"
  "\n"
  "# Common mission placeholder fields for app integration\n"
  "uint16 current_waypoint_index\n"
  "uint16 total_waypoints\n"
  "float32 progress_ratio\n"
  "float32 distance_to_next_wp_m\n"
  "float32 distance_to_goal_m\n"
  "uint8 error_code";

static char msg_encoding[] = "msg";

// Define all individual source functions

const rosidl_runtime_c__type_description__TypeSource *
combat_robot_msgs__msg__OperationState__get_individual_type_description_source(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static const rosidl_runtime_c__type_description__TypeSource source = {
    {combat_robot_msgs__msg__OperationState__TYPE_NAME, 36, 36},
    {msg_encoding, 3, 3},
    {toplevel_type_raw_source, 1181, 1181},
  };
  return &source;
}

const rosidl_runtime_c__type_description__TypeSource__Sequence *
combat_robot_msgs__msg__OperationState__get_type_description_sources(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static rosidl_runtime_c__type_description__TypeSource sources[1];
  static const rosidl_runtime_c__type_description__TypeSource__Sequence source_sequence = {sources, 1, 1};
  static bool constructed = false;
  if (!constructed) {
    sources[0] = *combat_robot_msgs__msg__OperationState__get_individual_type_description_source(NULL),
    constructed = true;
  }
  return &source_sequence;
}
