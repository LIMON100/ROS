// generated from rosidl_generator_c/resource/idl__description.c.em
// with input from combat_robot_msgs:msg/UserCommand.idl
// generated code does not contain a copyright notice

#include "combat_robot_msgs/msg/detail/user_command__functions.h"

ROSIDL_GENERATOR_C_PUBLIC_combat_robot_msgs
const rosidl_type_hash_t *
combat_robot_msgs__msg__UserCommand__get_type_hash(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static rosidl_type_hash_t hash = {1, {
      0x64, 0x51, 0xb7, 0x9c, 0x1d, 0x9f, 0x9d, 0x5a,
      0x5f, 0x48, 0x1f, 0x0f, 0x4f, 0xde, 0x38, 0x0b,
      0xab, 0x9f, 0xc8, 0xed, 0x95, 0xb7, 0x36, 0xeb,
      0x2e, 0x88, 0xcd, 0x40, 0xfc, 0xfc, 0x1f, 0xdd,
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

static char combat_robot_msgs__msg__UserCommand__TYPE_NAME[] = "combat_robot_msgs/msg/UserCommand";
static char builtin_interfaces__msg__Time__TYPE_NAME[] = "builtin_interfaces/msg/Time";
static char std_msgs__msg__Header__TYPE_NAME[] = "std_msgs/msg/Header";

// Define type names, field names, and default values
static char combat_robot_msgs__msg__UserCommand__FIELD_NAME__header[] = "header";
static char combat_robot_msgs__msg__UserCommand__FIELD_NAME__command_from[] = "command_from";
static char combat_robot_msgs__msg__UserCommand__FIELD_NAME__command_id[] = "command_id";
static char combat_robot_msgs__msg__UserCommand__FIELD_NAME__target_x[] = "target_x";
static char combat_robot_msgs__msg__UserCommand__FIELD_NAME__target_y[] = "target_y";
static char combat_robot_msgs__msg__UserCommand__FIELD_NAME__drone_target_lat[] = "drone_target_lat";
static char combat_robot_msgs__msg__UserCommand__FIELD_NAME__drone_target_lon[] = "drone_target_lon";
static char combat_robot_msgs__msg__UserCommand__FIELD_NAME__drone_target_valid[] = "drone_target_valid";
static char combat_robot_msgs__msg__UserCommand__FIELD_NAME__gun_trigger[] = "gun_trigger";
static char combat_robot_msgs__msg__UserCommand__FIELD_NAME__gun_trigger_permission[] = "gun_trigger_permission";
static char combat_robot_msgs__msg__UserCommand__FIELD_NAME__pan_speed[] = "pan_speed";
static char combat_robot_msgs__msg__UserCommand__FIELD_NAME__tilt_speed[] = "tilt_speed";
static char combat_robot_msgs__msg__UserCommand__FIELD_NAME__zoom_command[] = "zoom_command";
static char combat_robot_msgs__msg__UserCommand__FIELD_NAME__stream_command[] = "stream_command";

static rosidl_runtime_c__type_description__Field combat_robot_msgs__msg__UserCommand__FIELDS[] = {
  {
    {combat_robot_msgs__msg__UserCommand__FIELD_NAME__header, 6, 6},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_NESTED_TYPE,
      0,
      0,
      {std_msgs__msg__Header__TYPE_NAME, 19, 19},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__UserCommand__FIELD_NAME__command_from, 12, 12},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_UINT8,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__UserCommand__FIELD_NAME__command_id, 10, 10},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_UINT8,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__UserCommand__FIELD_NAME__target_x, 8, 8},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_FLOAT,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__UserCommand__FIELD_NAME__target_y, 8, 8},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_FLOAT,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__UserCommand__FIELD_NAME__drone_target_lat, 16, 16},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_DOUBLE,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__UserCommand__FIELD_NAME__drone_target_lon, 16, 16},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_DOUBLE,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__UserCommand__FIELD_NAME__drone_target_valid, 18, 18},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_BOOLEAN,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__UserCommand__FIELD_NAME__gun_trigger, 11, 11},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_BOOLEAN,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__UserCommand__FIELD_NAME__gun_trigger_permission, 22, 22},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_BOOLEAN,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__UserCommand__FIELD_NAME__pan_speed, 9, 9},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_INT8,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__UserCommand__FIELD_NAME__tilt_speed, 10, 10},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_INT8,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__UserCommand__FIELD_NAME__zoom_command, 12, 12},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_INT8,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__UserCommand__FIELD_NAME__stream_command, 14, 14},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_UINT8,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
};

static rosidl_runtime_c__type_description__IndividualTypeDescription combat_robot_msgs__msg__UserCommand__REFERENCED_TYPE_DESCRIPTIONS[] = {
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
combat_robot_msgs__msg__UserCommand__get_type_description(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static bool constructed = false;
  static const rosidl_runtime_c__type_description__TypeDescription description = {
    {
      {combat_robot_msgs__msg__UserCommand__TYPE_NAME, 33, 33},
      {combat_robot_msgs__msg__UserCommand__FIELDS, 14, 14},
    },
    {combat_robot_msgs__msg__UserCommand__REFERENCED_TYPE_DESCRIPTIONS, 2, 2},
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
  "# Command command_from\n"
  "uint8 TABLET = 0x00\n"
  "\n"
  "# Command ID\n"
  "uint8 IDLE = 0\n"
  "uint8 RECON = 1\n"
  "uint8 PROTECT_GENERAL = 2\n"
  "uint8 PROTECT_DRONE = 3\n"
  "uint8 DEBUG_ATTACK = 4\n"
  "uint8 DEBUG_TRACKING = 5\n"
  "uint8 ASSAULT = 6\n"
  "uint8 RETURN_TO_HOME = 7\n"
  "uint8 ESTOP = 8\n"
  "\n"
  "# Stream command\n"
  "uint8 STREAM_NONE = 0\n"
  "uint8 STREAM_START = 1\n"
  "uint8 STREAM_STOP = 2\n"
  "\n"
  "# UserCommand.msg\n"
  "# This message is used to send user commands to the combat robot system.\n"
  "std_msgs/Header header\n"
  "\n"
  "uint8 command_from # 0 - tablet / 1 - ble\n"
  "\n"
  "# Command ID\n"
  "uint8 command_id # 0=Idle, 1=Recon, 2=Protect General, 3=Protect Drone, 4/5=Debug, 6=Assault, 7=Return to Home, 8=Estop\n"
  "\n"
  "# Manual Targeting Coordinates\n"
  "float32 target_x # X coordinate in normalized pixels (0.0 - 1.0)\n"
  "float32 target_y # Y coordinate in normalized pixels (0.0 - 1.0)\n"
  "\n"
  "# Drone Search Target\n"
  "float64 drone_target_lat\n"
  "float64 drone_target_lon\n"
  "bool    drone_target_valid\n"
  "\n"
  "# Gun trigger control\n"
  "bool gun_trigger # 0 - stop / 1 - start\n"
  "bool gun_trigger_permission # 0 - no permission / 1 - permission granted\n"
  "\n"
  "# Gimbal / stream control\n"
  "int8 pan_speed\n"
  "int8 tilt_speed\n"
  "int8 zoom_command\n"
  "uint8 stream_command # 0=None, 1=Start, 2=Stop";

static char msg_encoding[] = "msg";

// Define all individual source functions

const rosidl_runtime_c__type_description__TypeSource *
combat_robot_msgs__msg__UserCommand__get_individual_type_description_source(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static const rosidl_runtime_c__type_description__TypeSource source = {
    {combat_robot_msgs__msg__UserCommand__TYPE_NAME, 33, 33},
    {msg_encoding, 3, 3},
    {toplevel_type_raw_source, 1143, 1143},
  };
  return &source;
}

const rosidl_runtime_c__type_description__TypeSource__Sequence *
combat_robot_msgs__msg__UserCommand__get_type_description_sources(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static rosidl_runtime_c__type_description__TypeSource sources[3];
  static const rosidl_runtime_c__type_description__TypeSource__Sequence source_sequence = {sources, 3, 3};
  static bool constructed = false;
  if (!constructed) {
    sources[0] = *combat_robot_msgs__msg__UserCommand__get_individual_type_description_source(NULL),
    sources[1] = *builtin_interfaces__msg__Time__get_individual_type_description_source(NULL);
    sources[2] = *std_msgs__msg__Header__get_individual_type_description_source(NULL);
    constructed = true;
  }
  return &source_sequence;
}
