// generated from rosidl_generator_c/resource/idl__description.c.em
// with input from combat_robot_msgs:msg/SwarmRobotCommand.idl
// generated code does not contain a copyright notice

#include "combat_robot_msgs/msg/detail/swarm_robot_command__functions.h"

ROSIDL_GENERATOR_C_PUBLIC_combat_robot_msgs
const rosidl_type_hash_t *
combat_robot_msgs__msg__SwarmRobotCommand__get_type_hash(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static rosidl_type_hash_t hash = {1, {
      0x08, 0xc6, 0x5a, 0xa3, 0x9d, 0x4d, 0xff, 0xa2,
      0x60, 0x1c, 0xa1, 0x6d, 0x6a, 0x6e, 0xa7, 0xbb,
      0x96, 0xa3, 0xfb, 0xf5, 0x3d, 0x74, 0xe5, 0x03,
      0xd0, 0x46, 0x38, 0xc6, 0xa2, 0x23, 0x8a, 0xb2,
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

static char combat_robot_msgs__msg__SwarmRobotCommand__TYPE_NAME[] = "combat_robot_msgs/msg/SwarmRobotCommand";
static char builtin_interfaces__msg__Time__TYPE_NAME[] = "builtin_interfaces/msg/Time";
static char std_msgs__msg__Header__TYPE_NAME[] = "std_msgs/msg/Header";

// Define type names, field names, and default values
static char combat_robot_msgs__msg__SwarmRobotCommand__FIELD_NAME__header[] = "header";
static char combat_robot_msgs__msg__SwarmRobotCommand__FIELD_NAME__sequence[] = "sequence";
static char combat_robot_msgs__msg__SwarmRobotCommand__FIELD_NAME__command_type[] = "command_type";
static char combat_robot_msgs__msg__SwarmRobotCommand__FIELD_NAME__leader_robot_id[] = "leader_robot_id";
static char combat_robot_msgs__msg__SwarmRobotCommand__FIELD_NAME__target_robot_id[] = "target_robot_id";
static char combat_robot_msgs__msg__SwarmRobotCommand__FIELD_NAME__operation_mode[] = "operation_mode";
static char combat_robot_msgs__msg__SwarmRobotCommand__FIELD_NAME__estop_requested[] = "estop_requested";
static char combat_robot_msgs__msg__SwarmRobotCommand__FIELD_NAME__path_command[] = "path_command";
static char combat_robot_msgs__msg__SwarmRobotCommand__FIELD_NAME__num_waypoints[] = "num_waypoints";
static char combat_robot_msgs__msg__SwarmRobotCommand__FIELD_NAME__path_id[] = "path_id";
static char combat_robot_msgs__msg__SwarmRobotCommand__FIELD_NAME__path_json[] = "path_json";
static char combat_robot_msgs__msg__SwarmRobotCommand__FIELD_NAME__formation_type[] = "formation_type";
static char combat_robot_msgs__msg__SwarmRobotCommand__FIELD_NAME__formation_number[] = "formation_number";
static char combat_robot_msgs__msg__SwarmRobotCommand__FIELD_NAME__grouping_index[] = "grouping_index";
static char combat_robot_msgs__msg__SwarmRobotCommand__FIELD_NAME__slot_index[] = "slot_index";
static char combat_robot_msgs__msg__SwarmRobotCommand__FIELD_NAME__selected_robot_count[] = "selected_robot_count";
static char combat_robot_msgs__msg__SwarmRobotCommand__FIELD_NAME__selected_robot_ids[] = "selected_robot_ids";

static rosidl_runtime_c__type_description__Field combat_robot_msgs__msg__SwarmRobotCommand__FIELDS[] = {
  {
    {combat_robot_msgs__msg__SwarmRobotCommand__FIELD_NAME__header, 6, 6},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_NESTED_TYPE,
      0,
      0,
      {std_msgs__msg__Header__TYPE_NAME, 19, 19},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__SwarmRobotCommand__FIELD_NAME__sequence, 8, 8},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_UINT32,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__SwarmRobotCommand__FIELD_NAME__command_type, 12, 12},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_UINT8,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__SwarmRobotCommand__FIELD_NAME__leader_robot_id, 15, 15},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_UINT32,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__SwarmRobotCommand__FIELD_NAME__target_robot_id, 15, 15},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_UINT32,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__SwarmRobotCommand__FIELD_NAME__operation_mode, 14, 14},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_UINT8,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__SwarmRobotCommand__FIELD_NAME__estop_requested, 15, 15},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_BOOLEAN,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__SwarmRobotCommand__FIELD_NAME__path_command, 12, 12},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_UINT8,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__SwarmRobotCommand__FIELD_NAME__num_waypoints, 13, 13},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_UINT16,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__SwarmRobotCommand__FIELD_NAME__path_id, 7, 7},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_STRING,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__SwarmRobotCommand__FIELD_NAME__path_json, 9, 9},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_STRING,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__SwarmRobotCommand__FIELD_NAME__formation_type, 14, 14},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_UINT8,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__SwarmRobotCommand__FIELD_NAME__formation_number, 16, 16},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_UINT8,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__SwarmRobotCommand__FIELD_NAME__grouping_index, 14, 14},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_UINT8,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__SwarmRobotCommand__FIELD_NAME__slot_index, 10, 10},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_UINT8,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__SwarmRobotCommand__FIELD_NAME__selected_robot_count, 20, 20},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_UINT8,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__SwarmRobotCommand__FIELD_NAME__selected_robot_ids, 18, 18},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_UINT32_ARRAY,
      8,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
};

static rosidl_runtime_c__type_description__IndividualTypeDescription combat_robot_msgs__msg__SwarmRobotCommand__REFERENCED_TYPE_DESCRIPTIONS[] = {
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
combat_robot_msgs__msg__SwarmRobotCommand__get_type_description(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static bool constructed = false;
  static const rosidl_runtime_c__type_description__TypeDescription description = {
    {
      {combat_robot_msgs__msg__SwarmRobotCommand__TYPE_NAME, 39, 39},
      {combat_robot_msgs__msg__SwarmRobotCommand__FIELDS, 17, 17},
    },
    {combat_robot_msgs__msg__SwarmRobotCommand__REFERENCED_TYPE_DESCRIPTIONS, 2, 2},
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
  "uint8 COMMAND_NONE = 0\n"
  "uint8 COMMAND_MODE = 1\n"
  "uint8 COMMAND_PATH = 2\n"
  "uint8 COMMAND_FORMATION = 3\n"
  "uint8 COMMAND_SYNC = 4\n"
  "\n"
  "uint8 PATH_CMD_NONE = 0\n"
  "uint8 PATH_CMD_START = 1\n"
  "uint8 PATH_CMD_STOP = 2\n"
  "uint8 PATH_CMD_PAUSE = 3\n"
  "uint8 PATH_CMD_RESUME = 4\n"
  "uint8 PATH_CMD_LOAD_PATH = 5\n"
  "\n"
  "uint32 sequence\n"
  "uint8 command_type\n"
  "uint32 leader_robot_id\n"
  "uint32 target_robot_id\n"
  "\n"
  "uint8 operation_mode\n"
  "bool estop_requested\n"
  "\n"
  "uint8 path_command\n"
  "uint16 num_waypoints\n"
  "string path_id\n"
  "string path_json\n"
  "\n"
  "uint8 formation_type\n"
  "uint8 formation_number\n"
  "uint8 grouping_index\n"
  "uint8 slot_index\n"
  "uint8 selected_robot_count\n"
  "uint32[8] selected_robot_ids";

static char msg_encoding[] = "msg";

// Define all individual source functions

const rosidl_runtime_c__type_description__TypeSource *
combat_robot_msgs__msg__SwarmRobotCommand__get_individual_type_description_source(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static const rosidl_runtime_c__type_description__TypeSource source = {
    {combat_robot_msgs__msg__SwarmRobotCommand__TYPE_NAME, 39, 39},
    {msg_encoding, 3, 3},
    {toplevel_type_raw_source, 635, 635},
  };
  return &source;
}

const rosidl_runtime_c__type_description__TypeSource__Sequence *
combat_robot_msgs__msg__SwarmRobotCommand__get_type_description_sources(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static rosidl_runtime_c__type_description__TypeSource sources[3];
  static const rosidl_runtime_c__type_description__TypeSource__Sequence source_sequence = {sources, 3, 3};
  static bool constructed = false;
  if (!constructed) {
    sources[0] = *combat_robot_msgs__msg__SwarmRobotCommand__get_individual_type_description_source(NULL),
    sources[1] = *builtin_interfaces__msg__Time__get_individual_type_description_source(NULL);
    sources[2] = *std_msgs__msg__Header__get_individual_type_description_source(NULL);
    constructed = true;
  }
  return &source_sequence;
}
