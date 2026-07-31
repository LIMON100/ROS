// generated from rosidl_generator_c/resource/idl__description.c.em
// with input from combat_robot_msgs:msg/WaypointList.idl
// generated code does not contain a copyright notice

#include "combat_robot_msgs/msg/detail/waypoint_list__functions.h"

ROSIDL_GENERATOR_C_PUBLIC_combat_robot_msgs
const rosidl_type_hash_t *
combat_robot_msgs__msg__WaypointList__get_type_hash(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static rosidl_type_hash_t hash = {1, {
      0x21, 0x38, 0x11, 0x5d, 0xdb, 0xe1, 0xf6, 0x0e,
      0x34, 0x20, 0xb3, 0xd4, 0x68, 0xf9, 0x6f, 0xb1,
      0x3a, 0x90, 0x19, 0xfe, 0x8c, 0xeb, 0x41, 0xcf,
      0xfc, 0x45, 0x48, 0x4d, 0xd2, 0xdf, 0xf5, 0xa9,
    }};
  return &hash;
}

#include <assert.h>
#include <string.h>

// Include directives for referenced types
#include "combat_robot_msgs/msg/detail/waypoint__functions.h"

// Hashes for external referenced types
#ifndef NDEBUG
static const rosidl_type_hash_t combat_robot_msgs__msg__Waypoint__EXPECTED_HASH = {1, {
    0x83, 0x81, 0x0e, 0x29, 0xe9, 0x88, 0x36, 0xb4,
    0xc5, 0xc5, 0x6d, 0x66, 0x37, 0xdc, 0x1d, 0xfa,
    0x3c, 0xba, 0x9b, 0xdc, 0xa9, 0x40, 0x81, 0xb1,
    0x2a, 0xd8, 0x90, 0x1c, 0x74, 0xc4, 0x50, 0x55,
  }};
#endif

static char combat_robot_msgs__msg__WaypointList__TYPE_NAME[] = "combat_robot_msgs/msg/WaypointList";
static char combat_robot_msgs__msg__Waypoint__TYPE_NAME[] = "combat_robot_msgs/msg/Waypoint";

// Define type names, field names, and default values
static char combat_robot_msgs__msg__WaypointList__FIELD_NAME__mode[] = "mode";
static char combat_robot_msgs__msg__WaypointList__FIELD_NAME__formation[] = "formation";
static char combat_robot_msgs__msg__WaypointList__FIELD_NAME__mission_id[] = "mission_id";
static char combat_robot_msgs__msg__WaypointList__FIELD_NAME__mission_status[] = "mission_status";
static char combat_robot_msgs__msg__WaypointList__FIELD_NAME__waypoints[] = "waypoints";

static rosidl_runtime_c__type_description__Field combat_robot_msgs__msg__WaypointList__FIELDS[] = {
  {
    {combat_robot_msgs__msg__WaypointList__FIELD_NAME__mode, 4, 4},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_INT32,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__WaypointList__FIELD_NAME__formation, 9, 9},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_INT32,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__WaypointList__FIELD_NAME__mission_id, 10, 10},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_INT32,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__WaypointList__FIELD_NAME__mission_status, 14, 14},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_INT32,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__WaypointList__FIELD_NAME__waypoints, 9, 9},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_NESTED_TYPE_UNBOUNDED_SEQUENCE,
      0,
      0,
      {combat_robot_msgs__msg__Waypoint__TYPE_NAME, 30, 30},
    },
    {NULL, 0, 0},
  },
};

static rosidl_runtime_c__type_description__IndividualTypeDescription combat_robot_msgs__msg__WaypointList__REFERENCED_TYPE_DESCRIPTIONS[] = {
  {
    {combat_robot_msgs__msg__Waypoint__TYPE_NAME, 30, 30},
    {NULL, 0, 0},
  },
};

const rosidl_runtime_c__type_description__TypeDescription *
combat_robot_msgs__msg__WaypointList__get_type_description(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static bool constructed = false;
  static const rosidl_runtime_c__type_description__TypeDescription description = {
    {
      {combat_robot_msgs__msg__WaypointList__TYPE_NAME, 34, 34},
      {combat_robot_msgs__msg__WaypointList__FIELDS, 5, 5},
    },
    {combat_robot_msgs__msg__WaypointList__REFERENCED_TYPE_DESCRIPTIONS, 1, 1},
  };
  if (!constructed) {
    assert(0 == memcmp(&combat_robot_msgs__msg__Waypoint__EXPECTED_HASH, combat_robot_msgs__msg__Waypoint__get_type_hash(NULL), sizeof(rosidl_type_hash_t)));
    description.referenced_type_descriptions.data[0].fields = combat_robot_msgs__msg__Waypoint__get_type_description(NULL)->type_description.fields;
    constructed = true;
  }
  return &description;
}

static char toplevel_type_raw_source[] =
  "int32 mode\n"
  "int32 formation\n"
  "int32 mission_id\n"
  "int32 mission_status\n"
  "Waypoint[] waypoints";

static char msg_encoding[] = "msg";

// Define all individual source functions

const rosidl_runtime_c__type_description__TypeSource *
combat_robot_msgs__msg__WaypointList__get_individual_type_description_source(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static const rosidl_runtime_c__type_description__TypeSource source = {
    {combat_robot_msgs__msg__WaypointList__TYPE_NAME, 34, 34},
    {msg_encoding, 3, 3},
    {toplevel_type_raw_source, 86, 86},
  };
  return &source;
}

const rosidl_runtime_c__type_description__TypeSource__Sequence *
combat_robot_msgs__msg__WaypointList__get_type_description_sources(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static rosidl_runtime_c__type_description__TypeSource sources[2];
  static const rosidl_runtime_c__type_description__TypeSource__Sequence source_sequence = {sources, 2, 2};
  static bool constructed = false;
  if (!constructed) {
    sources[0] = *combat_robot_msgs__msg__WaypointList__get_individual_type_description_source(NULL),
    sources[1] = *combat_robot_msgs__msg__Waypoint__get_individual_type_description_source(NULL);
    constructed = true;
  }
  return &source_sequence;
}
