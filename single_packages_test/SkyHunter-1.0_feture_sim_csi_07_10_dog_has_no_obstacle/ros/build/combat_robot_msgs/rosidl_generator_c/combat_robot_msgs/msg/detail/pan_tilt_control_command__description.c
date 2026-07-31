// generated from rosidl_generator_c/resource/idl__description.c.em
// with input from combat_robot_msgs:msg/PanTiltControlCommand.idl
// generated code does not contain a copyright notice

#include "combat_robot_msgs/msg/detail/pan_tilt_control_command__functions.h"

ROSIDL_GENERATOR_C_PUBLIC_combat_robot_msgs
const rosidl_type_hash_t *
combat_robot_msgs__msg__PanTiltControlCommand__get_type_hash(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static rosidl_type_hash_t hash = {1, {
      0x7d, 0x4e, 0x62, 0xb0, 0x81, 0x1a, 0x01, 0x65,
      0xd0, 0x98, 0x15, 0xff, 0xbb, 0xc1, 0xa2, 0xfa,
      0x3c, 0x68, 0x8c, 0x49, 0xcb, 0x2b, 0xaa, 0x54,
      0x30, 0x6c, 0xfc, 0x2b, 0x83, 0x67, 0x5b, 0x3d,
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

static char combat_robot_msgs__msg__PanTiltControlCommand__TYPE_NAME[] = "combat_robot_msgs/msg/PanTiltControlCommand";
static char builtin_interfaces__msg__Time__TYPE_NAME[] = "builtin_interfaces/msg/Time";
static char std_msgs__msg__Header__TYPE_NAME[] = "std_msgs/msg/Header";

// Define type names, field names, and default values
static char combat_robot_msgs__msg__PanTiltControlCommand__FIELD_NAME__header[] = "header";
static char combat_robot_msgs__msg__PanTiltControlCommand__FIELD_NAME__control_mode[] = "control_mode";
static char combat_robot_msgs__msg__PanTiltControlCommand__FIELD_NAME__horizontal_angle[] = "horizontal_angle";
static char combat_robot_msgs__msg__PanTiltControlCommand__FIELD_NAME__vertical_angle[] = "vertical_angle";
static char combat_robot_msgs__msg__PanTiltControlCommand__FIELD_NAME__pan_speed[] = "pan_speed";
static char combat_robot_msgs__msg__PanTiltControlCommand__FIELD_NAME__tilt_speed[] = "tilt_speed";
static char combat_robot_msgs__msg__PanTiltControlCommand__FIELD_NAME__pan_dir[] = "pan_dir";
static char combat_robot_msgs__msg__PanTiltControlCommand__FIELD_NAME__tilt_dir[] = "tilt_dir";

static rosidl_runtime_c__type_description__Field combat_robot_msgs__msg__PanTiltControlCommand__FIELDS[] = {
  {
    {combat_robot_msgs__msg__PanTiltControlCommand__FIELD_NAME__header, 6, 6},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_NESTED_TYPE,
      0,
      0,
      {std_msgs__msg__Header__TYPE_NAME, 19, 19},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__PanTiltControlCommand__FIELD_NAME__control_mode, 12, 12},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_UINT8,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__PanTiltControlCommand__FIELD_NAME__horizontal_angle, 16, 16},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_FLOAT,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__PanTiltControlCommand__FIELD_NAME__vertical_angle, 14, 14},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_FLOAT,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__PanTiltControlCommand__FIELD_NAME__pan_speed, 9, 9},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_UINT8,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__PanTiltControlCommand__FIELD_NAME__tilt_speed, 10, 10},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_UINT8,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__PanTiltControlCommand__FIELD_NAME__pan_dir, 7, 7},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_UINT8,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__PanTiltControlCommand__FIELD_NAME__tilt_dir, 8, 8},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_UINT8,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
};

static rosidl_runtime_c__type_description__IndividualTypeDescription combat_robot_msgs__msg__PanTiltControlCommand__REFERENCED_TYPE_DESCRIPTIONS[] = {
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
combat_robot_msgs__msg__PanTiltControlCommand__get_type_description(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static bool constructed = false;
  static const rosidl_runtime_c__type_description__TypeDescription description = {
    {
      {combat_robot_msgs__msg__PanTiltControlCommand__TYPE_NAME, 43, 43},
      {combat_robot_msgs__msg__PanTiltControlCommand__FIELDS, 8, 8},
    },
    {combat_robot_msgs__msg__PanTiltControlCommand__REFERENCED_TYPE_DESCRIPTIONS, 2, 2},
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
  "uint8 CONTROL_BRAKE = 0\n"
  "uint8 CONTROL_HOR_POS = 1\n"
  "uint8 CONTROL_VER_POS = 2\n"
  "uint8 CONTROL_DIR = 3\n"
  "\n"
  "# Pan-Tilt Control Command message\n"
  "std_msgs/Header header\n"
  "uint8 control_mode\n"
  "float32 horizontal_angle\n"
  "float32 vertical_angle\n"
  "uint8 pan_speed\n"
  "uint8 tilt_speed\n"
  "uint8 pan_dir  # 0 - stop / 1 - right / 2 - left\n"
  "uint8 tilt_dir # 0 - stop / 1 - up / 2 - down";

static char msg_encoding[] = "msg";

// Define all individual source functions

const rosidl_runtime_c__type_description__TypeSource *
combat_robot_msgs__msg__PanTiltControlCommand__get_individual_type_description_source(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static const rosidl_runtime_c__type_description__TypeSource source = {
    {combat_robot_msgs__msg__PanTiltControlCommand__TYPE_NAME, 43, 43},
    {msg_encoding, 3, 3},
    {toplevel_type_raw_source, 351, 351},
  };
  return &source;
}

const rosidl_runtime_c__type_description__TypeSource__Sequence *
combat_robot_msgs__msg__PanTiltControlCommand__get_type_description_sources(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static rosidl_runtime_c__type_description__TypeSource sources[3];
  static const rosidl_runtime_c__type_description__TypeSource__Sequence source_sequence = {sources, 3, 3};
  static bool constructed = false;
  if (!constructed) {
    sources[0] = *combat_robot_msgs__msg__PanTiltControlCommand__get_individual_type_description_source(NULL),
    sources[1] = *builtin_interfaces__msg__Time__get_individual_type_description_source(NULL);
    sources[2] = *std_msgs__msg__Header__get_individual_type_description_source(NULL);
    constructed = true;
  }
  return &source_sequence;
}
