// generated from rosidl_generator_c/resource/idl__description.c.em
// with input from combat_robot_msgs:msg/Waypoint.idl
// generated code does not contain a copyright notice

#include "combat_robot_msgs/msg/detail/waypoint__functions.h"

ROSIDL_GENERATOR_C_PUBLIC_combat_robot_msgs
const rosidl_type_hash_t *
combat_robot_msgs__msg__Waypoint__get_type_hash(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static rosidl_type_hash_t hash = {1, {
      0x83, 0x81, 0x0e, 0x29, 0xe9, 0x88, 0x36, 0xb4,
      0xc5, 0xc5, 0x6d, 0x66, 0x37, 0xdc, 0x1d, 0xfa,
      0x3c, 0xba, 0x9b, 0xdc, 0xa9, 0x40, 0x81, 0xb1,
      0x2a, 0xd8, 0x90, 0x1c, 0x74, 0xc4, 0x50, 0x55,
    }};
  return &hash;
}

#include <assert.h>
#include <string.h>

// Include directives for referenced types

// Hashes for external referenced types
#ifndef NDEBUG
#endif

static char combat_robot_msgs__msg__Waypoint__TYPE_NAME[] = "combat_robot_msgs/msg/Waypoint";

// Define type names, field names, and default values
static char combat_robot_msgs__msg__Waypoint__FIELD_NAME__way_id[] = "way_id";
static char combat_robot_msgs__msg__Waypoint__FIELD_NAME__way_lon[] = "way_lon";
static char combat_robot_msgs__msg__Waypoint__FIELD_NAME__way_lat[] = "way_lat";
static char combat_robot_msgs__msg__Waypoint__FIELD_NAME__way_status[] = "way_status";

static rosidl_runtime_c__type_description__Field combat_robot_msgs__msg__Waypoint__FIELDS[] = {
  {
    {combat_robot_msgs__msg__Waypoint__FIELD_NAME__way_id, 6, 6},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_INT32,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__Waypoint__FIELD_NAME__way_lon, 7, 7},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_DOUBLE,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__Waypoint__FIELD_NAME__way_lat, 7, 7},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_DOUBLE,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__Waypoint__FIELD_NAME__way_status, 10, 10},
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
combat_robot_msgs__msg__Waypoint__get_type_description(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static bool constructed = false;
  static const rosidl_runtime_c__type_description__TypeDescription description = {
    {
      {combat_robot_msgs__msg__Waypoint__TYPE_NAME, 30, 30},
      {combat_robot_msgs__msg__Waypoint__FIELDS, 4, 4},
    },
    {NULL, 0, 0},
  };
  if (!constructed) {
    constructed = true;
  }
  return &description;
}

static char toplevel_type_raw_source[] =
  "int32 way_id\n"
  "float64 way_lon\n"
  "float64 way_lat\n"
  "int32 way_status";

static char msg_encoding[] = "msg";

// Define all individual source functions

const rosidl_runtime_c__type_description__TypeSource *
combat_robot_msgs__msg__Waypoint__get_individual_type_description_source(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static const rosidl_runtime_c__type_description__TypeSource source = {
    {combat_robot_msgs__msg__Waypoint__TYPE_NAME, 30, 30},
    {msg_encoding, 3, 3},
    {toplevel_type_raw_source, 62, 62},
  };
  return &source;
}

const rosidl_runtime_c__type_description__TypeSource__Sequence *
combat_robot_msgs__msg__Waypoint__get_type_description_sources(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static rosidl_runtime_c__type_description__TypeSource sources[1];
  static const rosidl_runtime_c__type_description__TypeSource__Sequence source_sequence = {sources, 1, 1};
  static bool constructed = false;
  if (!constructed) {
    sources[0] = *combat_robot_msgs__msg__Waypoint__get_individual_type_description_source(NULL),
    constructed = true;
  }
  return &source_sequence;
}
