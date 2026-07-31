// generated from rosidl_generator_c/resource/idl__description.c.em
// with input from combat_robot_msgs:msg/PanTiltState.idl
// generated code does not contain a copyright notice

#include "combat_robot_msgs/msg/detail/pan_tilt_state__functions.h"

ROSIDL_GENERATOR_C_PUBLIC_combat_robot_msgs
const rosidl_type_hash_t *
combat_robot_msgs__msg__PanTiltState__get_type_hash(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static rosidl_type_hash_t hash = {1, {
      0xb8, 0x97, 0x5a, 0xf0, 0xa7, 0x5c, 0x05, 0x32,
      0xbf, 0x70, 0x01, 0x0e, 0x00, 0xf9, 0xb1, 0x5f,
      0x7b, 0x85, 0x4b, 0x0c, 0xbf, 0x46, 0x31, 0xdc,
      0xc5, 0xcc, 0x80, 0x2d, 0x14, 0xde, 0x1e, 0x1c,
    }};
  return &hash;
}

#include <assert.h>
#include <string.h>

// Include directives for referenced types
#include "builtin_interfaces/msg/detail/time__functions.h"

// Hashes for external referenced types
#ifndef NDEBUG
static const rosidl_type_hash_t builtin_interfaces__msg__Time__EXPECTED_HASH = {1, {
    0xb1, 0x06, 0x23, 0x5e, 0x25, 0xa4, 0xc5, 0xed,
    0x35, 0x09, 0x8a, 0xa0, 0xa6, 0x1a, 0x3e, 0xe9,
    0xc9, 0xb1, 0x8d, 0x19, 0x7f, 0x39, 0x8b, 0x0e,
    0x42, 0x06, 0xce, 0xa9, 0xac, 0xf9, 0xc1, 0x97,
  }};
#endif

static char combat_robot_msgs__msg__PanTiltState__TYPE_NAME[] = "combat_robot_msgs/msg/PanTiltState";
static char builtin_interfaces__msg__Time__TYPE_NAME[] = "builtin_interfaces/msg/Time";

// Define type names, field names, and default values
static char combat_robot_msgs__msg__PanTiltState__FIELD_NAME__stamp[] = "stamp";
static char combat_robot_msgs__msg__PanTiltState__FIELD_NAME__control_mode[] = "control_mode";
static char combat_robot_msgs__msg__PanTiltState__FIELD_NAME__horizontal_angle[] = "horizontal_angle";
static char combat_robot_msgs__msg__PanTiltState__FIELD_NAME__vertical_angle[] = "vertical_angle";
static char combat_robot_msgs__msg__PanTiltState__FIELD_NAME__pan_speed[] = "pan_speed";
static char combat_robot_msgs__msg__PanTiltState__FIELD_NAME__tilt_speed[] = "tilt_speed";

static rosidl_runtime_c__type_description__Field combat_robot_msgs__msg__PanTiltState__FIELDS[] = {
  {
    {combat_robot_msgs__msg__PanTiltState__FIELD_NAME__stamp, 5, 5},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_NESTED_TYPE,
      0,
      0,
      {builtin_interfaces__msg__Time__TYPE_NAME, 27, 27},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__PanTiltState__FIELD_NAME__control_mode, 12, 12},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_UINT8,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__PanTiltState__FIELD_NAME__horizontal_angle, 16, 16},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_FLOAT,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__PanTiltState__FIELD_NAME__vertical_angle, 14, 14},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_FLOAT,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__PanTiltState__FIELD_NAME__pan_speed, 9, 9},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_INT32,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__PanTiltState__FIELD_NAME__tilt_speed, 10, 10},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_INT32,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
};

static rosidl_runtime_c__type_description__IndividualTypeDescription combat_robot_msgs__msg__PanTiltState__REFERENCED_TYPE_DESCRIPTIONS[] = {
  {
    {builtin_interfaces__msg__Time__TYPE_NAME, 27, 27},
    {NULL, 0, 0},
  },
};

const rosidl_runtime_c__type_description__TypeDescription *
combat_robot_msgs__msg__PanTiltState__get_type_description(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static bool constructed = false;
  static const rosidl_runtime_c__type_description__TypeDescription description = {
    {
      {combat_robot_msgs__msg__PanTiltState__TYPE_NAME, 34, 34},
      {combat_robot_msgs__msg__PanTiltState__FIELDS, 6, 6},
    },
    {combat_robot_msgs__msg__PanTiltState__REFERENCED_TYPE_DESCRIPTIONS, 1, 1},
  };
  if (!constructed) {
    assert(0 == memcmp(&builtin_interfaces__msg__Time__EXPECTED_HASH, builtin_interfaces__msg__Time__get_type_hash(NULL), sizeof(rosidl_type_hash_t)));
    description.referenced_type_descriptions.data[0].fields = builtin_interfaces__msg__Time__get_type_description(NULL)->type_description.fields;
    constructed = true;
  }
  return &description;
}

static char toplevel_type_raw_source[] =
  "builtin_interfaces/Time stamp\n"
  "uint8 control_mode\n"
  "float32 horizontal_angle\n"
  "float32 vertical_angle\n"
  "int32 pan_speed\n"
  "int32 tilt_speed";

static char msg_encoding[] = "msg";

// Define all individual source functions

const rosidl_runtime_c__type_description__TypeSource *
combat_robot_msgs__msg__PanTiltState__get_individual_type_description_source(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static const rosidl_runtime_c__type_description__TypeSource source = {
    {combat_robot_msgs__msg__PanTiltState__TYPE_NAME, 34, 34},
    {msg_encoding, 3, 3},
    {toplevel_type_raw_source, 129, 129},
  };
  return &source;
}

const rosidl_runtime_c__type_description__TypeSource__Sequence *
combat_robot_msgs__msg__PanTiltState__get_type_description_sources(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static rosidl_runtime_c__type_description__TypeSource sources[2];
  static const rosidl_runtime_c__type_description__TypeSource__Sequence source_sequence = {sources, 2, 2};
  static bool constructed = false;
  if (!constructed) {
    sources[0] = *combat_robot_msgs__msg__PanTiltState__get_individual_type_description_source(NULL),
    sources[1] = *builtin_interfaces__msg__Time__get_individual_type_description_source(NULL);
    constructed = true;
  }
  return &source_sequence;
}
