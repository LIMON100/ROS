// generated from rosidl_generator_c/resource/idl__description.c.em
// with input from combat_robot_msgs:msg/BoundingBox2d.idl
// generated code does not contain a copyright notice

#include "combat_robot_msgs/msg/detail/bounding_box2d__functions.h"

ROSIDL_GENERATOR_C_PUBLIC_combat_robot_msgs
const rosidl_type_hash_t *
combat_robot_msgs__msg__BoundingBox2d__get_type_hash(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static rosidl_type_hash_t hash = {1, {
      0xae, 0x31, 0x52, 0x9e, 0xf7, 0x38, 0x03, 0xde,
      0xd0, 0x3b, 0x43, 0xb9, 0x04, 0xe2, 0xd5, 0x67,
      0x27, 0xdf, 0xa3, 0x5b, 0x27, 0x0b, 0x3b, 0xfe,
      0x92, 0x49, 0x8f, 0xb5, 0xc0, 0xe3, 0x72, 0x78,
    }};
  return &hash;
}

#include <assert.h>
#include <string.h>

// Include directives for referenced types

// Hashes for external referenced types
#ifndef NDEBUG
#endif

static char combat_robot_msgs__msg__BoundingBox2d__TYPE_NAME[] = "combat_robot_msgs/msg/BoundingBox2d";

// Define type names, field names, and default values
static char combat_robot_msgs__msg__BoundingBox2d__FIELD_NAME__x[] = "x";
static char combat_robot_msgs__msg__BoundingBox2d__FIELD_NAME__y[] = "y";
static char combat_robot_msgs__msg__BoundingBox2d__FIELD_NAME__width[] = "width";
static char combat_robot_msgs__msg__BoundingBox2d__FIELD_NAME__height[] = "height";

static rosidl_runtime_c__type_description__Field combat_robot_msgs__msg__BoundingBox2d__FIELDS[] = {
  {
    {combat_robot_msgs__msg__BoundingBox2d__FIELD_NAME__x, 1, 1},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_INT32,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__BoundingBox2d__FIELD_NAME__y, 1, 1},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_INT32,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__BoundingBox2d__FIELD_NAME__width, 5, 5},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_INT32,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__BoundingBox2d__FIELD_NAME__height, 6, 6},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_INT32,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
};

const rosidl_runtime_c__type_description__TypeDescription *
combat_robot_msgs__msg__BoundingBox2d__get_type_description(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static bool constructed = false;
  static const rosidl_runtime_c__type_description__TypeDescription description = {
    {
      {combat_robot_msgs__msg__BoundingBox2d__TYPE_NAME, 35, 35},
      {combat_robot_msgs__msg__BoundingBox2d__FIELDS, 4, 4},
    },
    {NULL, 0, 0},
  };
  if (!constructed) {
    constructed = true;
  }
  return &description;
}

static char toplevel_type_raw_source[] =
  "# BoundingBox2d.msg\n"
  "\n"
  "int32 x\n"
  "int32 y\n"
  "int32 width\n"
  "int32 height";

static char msg_encoding[] = "msg";

// Define all individual source functions

const rosidl_runtime_c__type_description__TypeSource *
combat_robot_msgs__msg__BoundingBox2d__get_individual_type_description_source(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static const rosidl_runtime_c__type_description__TypeSource source = {
    {combat_robot_msgs__msg__BoundingBox2d__TYPE_NAME, 35, 35},
    {msg_encoding, 3, 3},
    {toplevel_type_raw_source, 61, 61},
  };
  return &source;
}

const rosidl_runtime_c__type_description__TypeSource__Sequence *
combat_robot_msgs__msg__BoundingBox2d__get_type_description_sources(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static rosidl_runtime_c__type_description__TypeSource sources[1];
  static const rosidl_runtime_c__type_description__TypeSource__Sequence source_sequence = {sources, 1, 1};
  static bool constructed = false;
  if (!constructed) {
    sources[0] = *combat_robot_msgs__msg__BoundingBox2d__get_individual_type_description_source(NULL),
    constructed = true;
  }
  return &source_sequence;
}
