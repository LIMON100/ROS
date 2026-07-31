// generated from rosidl_generator_c/resource/idl__description.c.em
// with input from combat_robot_msgs:msg/DriveCommand.idl
// generated code does not contain a copyright notice

#include "combat_robot_msgs/msg/detail/drive_command__functions.h"

ROSIDL_GENERATOR_C_PUBLIC_combat_robot_msgs
const rosidl_type_hash_t *
combat_robot_msgs__msg__DriveCommand__get_type_hash(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static rosidl_type_hash_t hash = {1, {
      0xb4, 0x1c, 0x56, 0x1e, 0xcf, 0x8e, 0x8a, 0x46,
      0xa6, 0x62, 0x59, 0xfa, 0xf9, 0x2c, 0x61, 0x8f,
      0xb1, 0xa2, 0xe1, 0x87, 0xba, 0x3c, 0x43, 0x35,
      0xda, 0x74, 0x69, 0x18, 0x29, 0xcd, 0x9d, 0x32,
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

static char combat_robot_msgs__msg__DriveCommand__TYPE_NAME[] = "combat_robot_msgs/msg/DriveCommand";
static char builtin_interfaces__msg__Time__TYPE_NAME[] = "builtin_interfaces/msg/Time";
static char std_msgs__msg__Header__TYPE_NAME[] = "std_msgs/msg/Header";

// Define type names, field names, and default values
static char combat_robot_msgs__msg__DriveCommand__FIELD_NAME__header[] = "header";
static char combat_robot_msgs__msg__DriveCommand__FIELD_NAME__linear_velocity[] = "linear_velocity";
static char combat_robot_msgs__msg__DriveCommand__FIELD_NAME__angular_velocity[] = "angular_velocity";

static rosidl_runtime_c__type_description__Field combat_robot_msgs__msg__DriveCommand__FIELDS[] = {
  {
    {combat_robot_msgs__msg__DriveCommand__FIELD_NAME__header, 6, 6},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_NESTED_TYPE,
      0,
      0,
      {std_msgs__msg__Header__TYPE_NAME, 19, 19},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__DriveCommand__FIELD_NAME__linear_velocity, 15, 15},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_FLOAT,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__DriveCommand__FIELD_NAME__angular_velocity, 16, 16},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_FLOAT,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
};

static rosidl_runtime_c__type_description__IndividualTypeDescription combat_robot_msgs__msg__DriveCommand__REFERENCED_TYPE_DESCRIPTIONS[] = {
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
combat_robot_msgs__msg__DriveCommand__get_type_description(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static bool constructed = false;
  static const rosidl_runtime_c__type_description__TypeDescription description = {
    {
      {combat_robot_msgs__msg__DriveCommand__TYPE_NAME, 34, 34},
      {combat_robot_msgs__msg__DriveCommand__FIELDS, 3, 3},
    },
    {combat_robot_msgs__msg__DriveCommand__REFERENCED_TYPE_DESCRIPTIONS, 2, 2},
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
  "# combat_robot_msgs/msg/DriveCommand.msg\n"
  "std_msgs/Header header\n"
  "float32 linear_velocity    # m/s, forward(+)/backward(-)\n"
  "float32 angular_velocity   # rad/s, left(+)/right(-)  \\xe2\\x86\\x90 \\xea\\xb8\\xb0\\xec\\xa1\\xb4 \\xeb\\xb6\\x80\\xed\\x98\\xb8 \\xea\\xb7\\x9c\\xec\\xb9\\x99 \\xea\\xb7\\xb8\\xeb\\x8c\\x80\\xeb\\xa1\\x9c \\xec\\x82\\xac\\xec\\x9a\\xa9";

static char msg_encoding[] = "msg";

// Define all individual source functions

const rosidl_runtime_c__type_description__TypeSource *
combat_robot_msgs__msg__DriveCommand__get_individual_type_description_source(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static const rosidl_runtime_c__type_description__TypeSource source = {
    {combat_robot_msgs__msg__DriveCommand__TYPE_NAME, 34, 34},
    {msg_encoding, 3, 3},
    {toplevel_type_raw_source, 193, 193},
  };
  return &source;
}

const rosidl_runtime_c__type_description__TypeSource__Sequence *
combat_robot_msgs__msg__DriveCommand__get_type_description_sources(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static rosidl_runtime_c__type_description__TypeSource sources[3];
  static const rosidl_runtime_c__type_description__TypeSource__Sequence source_sequence = {sources, 3, 3};
  static bool constructed = false;
  if (!constructed) {
    sources[0] = *combat_robot_msgs__msg__DriveCommand__get_individual_type_description_source(NULL),
    sources[1] = *builtin_interfaces__msg__Time__get_individual_type_description_source(NULL);
    sources[2] = *std_msgs__msg__Header__get_individual_type_description_source(NULL);
    constructed = true;
  }
  return &source_sequence;
}
