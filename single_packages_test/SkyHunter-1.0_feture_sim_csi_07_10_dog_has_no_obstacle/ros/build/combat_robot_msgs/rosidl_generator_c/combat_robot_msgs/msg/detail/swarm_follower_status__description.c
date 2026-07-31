// generated from rosidl_generator_c/resource/idl__description.c.em
// with input from combat_robot_msgs:msg/SwarmFollowerStatus.idl
// generated code does not contain a copyright notice

#include "combat_robot_msgs/msg/detail/swarm_follower_status__functions.h"

ROSIDL_GENERATOR_C_PUBLIC_combat_robot_msgs
const rosidl_type_hash_t *
combat_robot_msgs__msg__SwarmFollowerStatus__get_type_hash(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static rosidl_type_hash_t hash = {1, {
      0x47, 0x28, 0xb8, 0xfa, 0x16, 0xac, 0xc6, 0x3e,
      0x73, 0x5a, 0x5f, 0x7a, 0x7c, 0x5c, 0x15, 0x51,
      0xa8, 0x35, 0x11, 0x9d, 0xd0, 0x6d, 0xf5, 0xf8,
      0xc0, 0x57, 0xc2, 0x6a, 0x4e, 0x82, 0xea, 0xec,
    }};
  return &hash;
}

#include <assert.h>
#include <string.h>

// Include directives for referenced types
#include "std_msgs/msg/detail/header__functions.h"
#include "builtin_interfaces/msg/detail/time__functions.h"

// Hashes for external referenced types
#ifndef NDEBUG
static const rosidl_type_hash_t builtin_interfaces__msg__Time__EXPECTED_HASH = {1, {
    0xb1, 0x06, 0x23, 0x5e, 0x25, 0xa4, 0xc5, 0xed,
    0x35, 0x09, 0x8a, 0xa0, 0xa6, 0x1a, 0x3e, 0xe9,
    0xc9, 0xb1, 0x8d, 0x19, 0x7f, 0x39, 0x8b, 0x0e,
    0x42, 0x06, 0xce, 0xa9, 0xac, 0xf9, 0xc1, 0x97,
  }};
static const rosidl_type_hash_t std_msgs__msg__Header__EXPECTED_HASH = {1, {
    0xf4, 0x9f, 0xb3, 0xae, 0x2c, 0xf0, 0x70, 0xf7,
    0x93, 0x64, 0x5f, 0xf7, 0x49, 0x68, 0x3a, 0xc6,
    0xb0, 0x62, 0x03, 0xe4, 0x1c, 0x89, 0x1e, 0x17,
    0x70, 0x1b, 0x1c, 0xb5, 0x97, 0xce, 0x6a, 0x01,
  }};
#endif

static char combat_robot_msgs__msg__SwarmFollowerStatus__TYPE_NAME[] = "combat_robot_msgs/msg/SwarmFollowerStatus";
static char builtin_interfaces__msg__Time__TYPE_NAME[] = "builtin_interfaces/msg/Time";
static char std_msgs__msg__Header__TYPE_NAME[] = "std_msgs/msg/Header";

// Define type names, field names, and default values
static char combat_robot_msgs__msg__SwarmFollowerStatus__FIELD_NAME__header[] = "header";
static char combat_robot_msgs__msg__SwarmFollowerStatus__FIELD_NAME__robot_id[] = "robot_id";
static char combat_robot_msgs__msg__SwarmFollowerStatus__FIELD_NAME__leader_robot_id[] = "leader_robot_id";
static char combat_robot_msgs__msg__SwarmFollowerStatus__FIELD_NAME__link_status[] = "link_status";
static char combat_robot_msgs__msg__SwarmFollowerStatus__FIELD_NAME__last_heartbeat_sequence[] = "last_heartbeat_sequence";
static char combat_robot_msgs__msg__SwarmFollowerStatus__FIELD_NAME__heartbeat_age_sec[] = "heartbeat_age_sec";
static char combat_robot_msgs__msg__SwarmFollowerStatus__FIELD_NAME__last_operation_mode[] = "last_operation_mode";
static char combat_robot_msgs__msg__SwarmFollowerStatus__FIELD_NAME__last_formation_type[] = "last_formation_type";
static char combat_robot_msgs__msg__SwarmFollowerStatus__FIELD_NAME__last_formation_number[] = "last_formation_number";
static char combat_robot_msgs__msg__SwarmFollowerStatus__FIELD_NAME__last_grouping_index[] = "last_grouping_index";
static char combat_robot_msgs__msg__SwarmFollowerStatus__FIELD_NAME__latitude[] = "latitude";
static char combat_robot_msgs__msg__SwarmFollowerStatus__FIELD_NAME__longitude[] = "longitude";
static char combat_robot_msgs__msg__SwarmFollowerStatus__FIELD_NAME__heading_deg[] = "heading_deg";
static char combat_robot_msgs__msg__SwarmFollowerStatus__FIELD_NAME__ground_speed_mps[] = "ground_speed_mps";

static rosidl_runtime_c__type_description__Field combat_robot_msgs__msg__SwarmFollowerStatus__FIELDS[] = {
  {
    {combat_robot_msgs__msg__SwarmFollowerStatus__FIELD_NAME__header, 6, 6},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_NESTED_TYPE,
      0,
      0,
      {std_msgs__msg__Header__TYPE_NAME, 19, 19},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__SwarmFollowerStatus__FIELD_NAME__robot_id, 8, 8},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_UINT32,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__SwarmFollowerStatus__FIELD_NAME__leader_robot_id, 15, 15},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_UINT32,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__SwarmFollowerStatus__FIELD_NAME__link_status, 11, 11},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_UINT8,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__SwarmFollowerStatus__FIELD_NAME__last_heartbeat_sequence, 23, 23},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_UINT32,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__SwarmFollowerStatus__FIELD_NAME__heartbeat_age_sec, 17, 17},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_FLOAT,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__SwarmFollowerStatus__FIELD_NAME__last_operation_mode, 19, 19},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_UINT8,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__SwarmFollowerStatus__FIELD_NAME__last_formation_type, 19, 19},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_UINT8,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__SwarmFollowerStatus__FIELD_NAME__last_formation_number, 21, 21},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_UINT8,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__SwarmFollowerStatus__FIELD_NAME__last_grouping_index, 19, 19},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_UINT8,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__SwarmFollowerStatus__FIELD_NAME__latitude, 8, 8},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_DOUBLE,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__SwarmFollowerStatus__FIELD_NAME__longitude, 9, 9},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_DOUBLE,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__SwarmFollowerStatus__FIELD_NAME__heading_deg, 11, 11},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_FLOAT,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__SwarmFollowerStatus__FIELD_NAME__ground_speed_mps, 16, 16},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_FLOAT,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
};

static rosidl_runtime_c__type_description__IndividualTypeDescription combat_robot_msgs__msg__SwarmFollowerStatus__REFERENCED_TYPE_DESCRIPTIONS[] = {
  {
    {builtin_interfaces__msg__Time__TYPE_NAME, 27, 27},
    {NULL, 0, 0},
  },
  {
    {std_msgs__msg__Header__TYPE_NAME, 19, 19},
    {NULL, 0, 0},
  },
};

const rosidl_runtime_c__type_description__TypeDescription *
combat_robot_msgs__msg__SwarmFollowerStatus__get_type_description(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static bool constructed = false;
  static const rosidl_runtime_c__type_description__TypeDescription description = {
    {
      {combat_robot_msgs__msg__SwarmFollowerStatus__TYPE_NAME, 41, 41},
      {combat_robot_msgs__msg__SwarmFollowerStatus__FIELDS, 14, 14},
    },
    {combat_robot_msgs__msg__SwarmFollowerStatus__REFERENCED_TYPE_DESCRIPTIONS, 2, 2},
  };
  if (!constructed) {
    assert(0 == memcmp(&builtin_interfaces__msg__Time__EXPECTED_HASH, builtin_interfaces__msg__Time__get_type_hash(NULL), sizeof(rosidl_type_hash_t)));
    description.referenced_type_descriptions.data[0].fields = builtin_interfaces__msg__Time__get_type_description(NULL)->type_description.fields;
    assert(0 == memcmp(&std_msgs__msg__Header__EXPECTED_HASH, std_msgs__msg__Header__get_type_hash(NULL), sizeof(rosidl_type_hash_t)));
    description.referenced_type_descriptions.data[1].fields = std_msgs__msg__Header__get_type_description(NULL)->type_description.fields;
    constructed = true;
  }
  return &description;
}

static char toplevel_type_raw_source[] =
  "std_msgs/Header header\n"
  "\n"
  "uint8 LINK_DISCONNECTED = 0\n"
  "uint8 LINK_CONNECTED = 1\n"
  "\n"
  "uint32 robot_id\n"
  "uint32 leader_robot_id\n"
  "uint8 link_status\n"
  "uint32 last_heartbeat_sequence\n"
  "float32 heartbeat_age_sec\n"
  "\n"
  "uint8 last_operation_mode\n"
  "uint8 last_formation_type\n"
  "uint8 last_formation_number\n"
  "uint8 last_grouping_index\n"
  "\n"
  "# Follower position so the operator-host (leader) command_server can place this\n"
  "# robot on the tablet swarm map. Filled by the follower's own command_server from\n"
  "# its NavSatFix (/sN/fix); 0 until a fix is received.\n"
  "float64 latitude\n"
  "float64 longitude\n"
  "float32 heading_deg\n"
  "float32 ground_speed_mps";

static char msg_encoding[] = "msg";

// Define all individual source functions

const rosidl_runtime_c__type_description__TypeSource *
combat_robot_msgs__msg__SwarmFollowerStatus__get_individual_type_description_source(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static const rosidl_runtime_c__type_description__TypeSource source = {
    {combat_robot_msgs__msg__SwarmFollowerStatus__TYPE_NAME, 41, 41},
    {msg_encoding, 3, 3},
    {toplevel_type_raw_source, 596, 596},
  };
  return &source;
}

const rosidl_runtime_c__type_description__TypeSource__Sequence *
combat_robot_msgs__msg__SwarmFollowerStatus__get_type_description_sources(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static rosidl_runtime_c__type_description__TypeSource sources[3];
  static const rosidl_runtime_c__type_description__TypeSource__Sequence source_sequence = {sources, 3, 3};
  static bool constructed = false;
  if (!constructed) {
    sources[0] = *combat_robot_msgs__msg__SwarmFollowerStatus__get_individual_type_description_source(NULL),
    sources[1] = *builtin_interfaces__msg__Time__get_individual_type_description_source(NULL);
    sources[2] = *std_msgs__msg__Header__get_individual_type_description_source(NULL);
    constructed = true;
  }
  return &source_sequence;
}
