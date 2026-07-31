// generated from rosidl_generator_c/resource/idl__description.c.em
// with input from combat_robot_msgs:msg/TouchTargetPoint.idl
// generated code does not contain a copyright notice

#include "combat_robot_msgs/msg/detail/touch_target_point__functions.h"

ROSIDL_GENERATOR_C_PUBLIC_combat_robot_msgs
const rosidl_type_hash_t *
combat_robot_msgs__msg__TouchTargetPoint__get_type_hash(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static rosidl_type_hash_t hash = {1, {
      0x52, 0xe3, 0xae, 0xed, 0xb3, 0x31, 0xb7, 0xa1,
      0x64, 0xc7, 0x21, 0x16, 0x79, 0x6d, 0x88, 0x89,
      0x63, 0x17, 0x61, 0xac, 0x9c, 0x2a, 0x74, 0x15,
      0xe7, 0x14, 0xf1, 0xa2, 0x72, 0x8b, 0xe4, 0x73,
    }};
  return &hash;
}

#include <assert.h>
#include <string.h>

// Include directives for referenced types

// Hashes for external referenced types
#ifndef NDEBUG
#endif

static char combat_robot_msgs__msg__TouchTargetPoint__TYPE_NAME[] = "combat_robot_msgs/msg/TouchTargetPoint";

// Define type names, field names, and default values
static char combat_robot_msgs__msg__TouchTargetPoint__FIELD_NAME__touch_x[] = "touch_x";
static char combat_robot_msgs__msg__TouchTargetPoint__FIELD_NAME__touch_y[] = "touch_y";

static rosidl_runtime_c__type_description__Field combat_robot_msgs__msg__TouchTargetPoint__FIELDS[] = {
  {
    {combat_robot_msgs__msg__TouchTargetPoint__FIELD_NAME__touch_x, 7, 7},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_FLOAT,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__TouchTargetPoint__FIELD_NAME__touch_y, 7, 7},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_FLOAT,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
};

const rosidl_runtime_c__type_description__TypeDescription *
combat_robot_msgs__msg__TouchTargetPoint__get_type_description(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static bool constructed = false;
  static const rosidl_runtime_c__type_description__TypeDescription description = {
    {
      {combat_robot_msgs__msg__TouchTargetPoint__TYPE_NAME, 38, 38},
      {combat_robot_msgs__msg__TouchTargetPoint__FIELDS, 2, 2},
    },
    {NULL, 0, 0},
  };
  if (!constructed) {
    constructed = true;
  }
  return &description;
}

static char toplevel_type_raw_source[] =
  "float32 touch_x\n"
  "float32 touch_y";

static char msg_encoding[] = "msg";

// Define all individual source functions

const rosidl_runtime_c__type_description__TypeSource *
combat_robot_msgs__msg__TouchTargetPoint__get_individual_type_description_source(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static const rosidl_runtime_c__type_description__TypeSource source = {
    {combat_robot_msgs__msg__TouchTargetPoint__TYPE_NAME, 38, 38},
    {msg_encoding, 3, 3},
    {toplevel_type_raw_source, 31, 31},
  };
  return &source;
}

const rosidl_runtime_c__type_description__TypeSource__Sequence *
combat_robot_msgs__msg__TouchTargetPoint__get_type_description_sources(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static rosidl_runtime_c__type_description__TypeSource sources[1];
  static const rosidl_runtime_c__type_description__TypeSource__Sequence source_sequence = {sources, 1, 1};
  static bool constructed = false;
  if (!constructed) {
    sources[0] = *combat_robot_msgs__msg__TouchTargetPoint__get_individual_type_description_source(NULL),
    constructed = true;
  }
  return &source_sequence;
}
