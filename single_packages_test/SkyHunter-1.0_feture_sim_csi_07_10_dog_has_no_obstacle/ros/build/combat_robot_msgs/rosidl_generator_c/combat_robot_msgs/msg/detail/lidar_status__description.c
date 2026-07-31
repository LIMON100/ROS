// generated from rosidl_generator_c/resource/idl__description.c.em
// with input from combat_robot_msgs:msg/LidarStatus.idl
// generated code does not contain a copyright notice

#include "combat_robot_msgs/msg/detail/lidar_status__functions.h"

ROSIDL_GENERATOR_C_PUBLIC_combat_robot_msgs
const rosidl_type_hash_t *
combat_robot_msgs__msg__LidarStatus__get_type_hash(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static rosidl_type_hash_t hash = {1, {
      0x01, 0x3e, 0x68, 0xd4, 0xe1, 0x8d, 0x72, 0xb9,
      0x1c, 0xf1, 0x9e, 0x6d, 0x18, 0x25, 0xe6, 0xbb,
      0x0d, 0xfd, 0x54, 0x65, 0xf2, 0xcf, 0xb2, 0x7e,
      0xf9, 0x46, 0xfb, 0xb9, 0x31, 0x8d, 0x0a, 0x98,
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

static char combat_robot_msgs__msg__LidarStatus__TYPE_NAME[] = "combat_robot_msgs/msg/LidarStatus";
static char builtin_interfaces__msg__Time__TYPE_NAME[] = "builtin_interfaces/msg/Time";
static char std_msgs__msg__Header__TYPE_NAME[] = "std_msgs/msg/Header";

// Define type names, field names, and default values
static char combat_robot_msgs__msg__LidarStatus__FIELD_NAME__header[] = "header";
static char combat_robot_msgs__msg__LidarStatus__FIELD_NAME__status[] = "status";
static char combat_robot_msgs__msg__LidarStatus__FIELD_NAME__last_scan_point_count[] = "last_scan_point_count";
static char combat_robot_msgs__msg__LidarStatus__FIELD_NAME__scan_rate_hz[] = "scan_rate_hz";
static char combat_robot_msgs__msg__LidarStatus__FIELD_NAME__obstacle_detected[] = "obstacle_detected";
static char combat_robot_msgs__msg__LidarStatus__FIELD_NAME__min_obstacle_distance_m[] = "min_obstacle_distance_m";

static rosidl_runtime_c__type_description__Field combat_robot_msgs__msg__LidarStatus__FIELDS[] = {
  {
    {combat_robot_msgs__msg__LidarStatus__FIELD_NAME__header, 6, 6},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_NESTED_TYPE,
      0,
      0,
      {std_msgs__msg__Header__TYPE_NAME, 19, 19},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__LidarStatus__FIELD_NAME__status, 6, 6},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_UINT8,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__LidarStatus__FIELD_NAME__last_scan_point_count, 21, 21},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_UINT32,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__LidarStatus__FIELD_NAME__scan_rate_hz, 12, 12},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_FLOAT,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__LidarStatus__FIELD_NAME__obstacle_detected, 17, 17},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_BOOLEAN,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__LidarStatus__FIELD_NAME__min_obstacle_distance_m, 23, 23},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_FLOAT,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
};

static rosidl_runtime_c__type_description__IndividualTypeDescription combat_robot_msgs__msg__LidarStatus__REFERENCED_TYPE_DESCRIPTIONS[] = {
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
combat_robot_msgs__msg__LidarStatus__get_type_description(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static bool constructed = false;
  static const rosidl_runtime_c__type_description__TypeDescription description = {
    {
      {combat_robot_msgs__msg__LidarStatus__TYPE_NAME, 33, 33},
      {combat_robot_msgs__msg__LidarStatus__FIELDS, 6, 6},
    },
    {combat_robot_msgs__msg__LidarStatus__REFERENCED_TYPE_DESCRIPTIONS, 2, 2},
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
  "# LiDAR \\xeb\\x93\\x9c\\xeb\\x9d\\xbc\\xec\\x9d\\xb4\\xeb\\xb2\\x84\\xea\\xb0\\x80 publish \\xed\\x95\\x98\\xeb\\x8a\\x94 status.\n"
  "# robot_server \\xea\\xb0\\x80 /lidar/status \\xed\\x86\\xa0\\xed\\x94\\xbd\\xec\\x9c\\xbc\\xeb\\xa1\\x9c subscribe \\xed\\x95\\x98\\xec\\x97\\xac LiDAR \\xed\\x97\\xac\\xec\\x8a\\xa4 / \\xec\\x9e\\xa5\\xec\\x95\\xa0\\xeb\\xac\\xbc \\xec\\xa0\\x95\\xeb\\xb3\\xb4\\xeb\\xa5\\xbc\n"
  "# \\xec\\xb6\\x94\\xed\\x9b\\x84 status \\xed\\x8c\\xa8\\xed\\x82\\xb7\\xec\\x97\\x90 \\xeb\\xb0\\x98\\xec\\x98\\x81\\xed\\x95\\x98\\xea\\xb1\\xb0\\xeb\\x82\\x98 \\xec\\x95\\x88\\xec\\xa0\\x84 \\xeb\\xb6\\x84\\xea\\xb8\\xb0 \\xec\\x9e\\x85\\xeb\\xa0\\xa5\\xec\\x9c\\xbc\\xeb\\xa1\\x9c \\xed\\x99\\x9c\\xec\\x9a\\xa9\\xed\\x95\\xa8.\n"
  "#\n"
  "# \\xea\\xb0\\x80\\xeb\\x8a\\xa5\\xed\\x95\\x9c \\xed\\x95\\x9c \\xeb\\xaa\\xa8\\xeb\\x93\\xa0 \\xed\\x95\\x84\\xeb\\x93\\x9c\\xeb\\xa5\\xbc \\xeb\\xa7\\xa4 \\xec\\xa3\\xbc\\xea\\xb8\\xb0 \\xec\\xb1\\x84\\xec\\x9b\\x8c \\xeb\\xb3\\xb4\\xeb\\x82\\xb4\\xec\\xa3\\xbc\\xec\\x84\\xb8\\xec\\x9a\\x94.\n"
  "\n"
  "std_msgs/Header header\n"
  "\n"
  "# Driver / hardware \\xec\\x83\\x81\\xed\\x83\\x9c\n"
  "uint8 LIDAR_OK = 0          # \\xec\\xa0\\x95\\xec\\x83\\x81 \\xec\\x8a\\xa4\\xec\\xba\\x94 \\xec\\xa4\\x91\n"
  "uint8 LIDAR_DEGRADED = 1    # \\xec\\x8a\\xa4\\xec\\xba\\x94\\xeb\\x90\\x98\\xec\\xa7\\x80\\xeb\\xa7\\x8c \\xed\\x92\\x88\\xec\\xa7\\x88 \\xec\\xa0\\x80\\xed\\x95\\x98 (rate\\xe2\\x86\\x93, noise\\xe2\\x86\\x91)\n"
  "uint8 LIDAR_FAULT = 2       # \\xed\\x86\\xb5\\xec\\x8b\\xa0 / hardware \\xec\\x9e\\xa5\\xec\\x95\\xa0\n"
  "uint8 status\n"
  "\n"
  "# \\xeb\\xa7\\x88\\xec\\xa7\\x80\\xeb\\xa7\\x89 \\xec\\x8a\\xa4\\xec\\xba\\x94 \\xed\\x86\\xb5\\xea\\xb3\\x84\n"
  "uint32 last_scan_point_count\n"
  "float32 scan_rate_hz        # \\xec\\x8b\\xa4\\xec\\xb8\\xa1 Hz (\\xeb\\xaa\\xa9\\xed\\x91\\x9c\\xea\\xb0\\x92\\xea\\xb3\\xbc \\xec\\xb0\\xa8\\xec\\x9d\\xb4 \\xeb\\x82\\x98\\xeb\\xa9\\xb4 LIDAR_DEGRADED \\xea\\xb6\\x8c\\xec\\x9e\\xa5)\n"
  "\n"
  "# \\xec\\x9e\\xa5\\xec\\x95\\xa0\\xeb\\xac\\xbc \\xea\\xb0\\x90\\xec\\xa7\\x80 \\xe2\\x80\\x94 \\xec\\x95\\x88\\xec\\xa0\\x84 \\xec\\xa0\\x95\\xec\\xa7\\x80 \\xec\\x9e\\x85\\xeb\\xa0\\xa5\\xec\\x9c\\xbc\\xeb\\xa1\\x9c \\xed\\x99\\x9c\\xec\\x9a\\xa9 \\xea\\xb0\\x80\\xeb\\x8a\\xa5.\n"
  "# \\xea\\xb0\\x80\\xea\\xb9\\x8c\\xec\\x9d\\xb4 \\xec\\x9e\\x88\\xeb\\x8a\\x94 \\xec\\x9e\\xa5\\xec\\x95\\xa0\\xeb\\xac\\xbc \\xec\\x97\\x86\\xec\\x9c\\xbc\\xeb\\xa9\\xb4 obstacle_detected=false, min_obstacle_distance_m=NaN.\n"
  "bool obstacle_detected\n"
  "float32 min_obstacle_distance_m";

static char msg_encoding[] = "msg";

// Define all individual source functions

const rosidl_runtime_c__type_description__TypeSource *
combat_robot_msgs__msg__LidarStatus__get_individual_type_description_source(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static const rosidl_runtime_c__type_description__TypeSource source = {
    {combat_robot_msgs__msg__LidarStatus__TYPE_NAME, 33, 33},
    {msg_encoding, 3, 3},
    {toplevel_type_raw_source, 640, 640},
  };
  return &source;
}

const rosidl_runtime_c__type_description__TypeSource__Sequence *
combat_robot_msgs__msg__LidarStatus__get_type_description_sources(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static rosidl_runtime_c__type_description__TypeSource sources[3];
  static const rosidl_runtime_c__type_description__TypeSource__Sequence source_sequence = {sources, 3, 3};
  static bool constructed = false;
  if (!constructed) {
    sources[0] = *combat_robot_msgs__msg__LidarStatus__get_individual_type_description_source(NULL),
    sources[1] = *builtin_interfaces__msg__Time__get_individual_type_description_source(NULL);
    sources[2] = *std_msgs__msg__Header__get_individual_type_description_source(NULL);
    constructed = true;
  }
  return &source_sequence;
}
