// generated from rosidl_generator_c/resource/idl__description.c.em
// with input from combat_robot_msgs:msg/GnssStatus.idl
// generated code does not contain a copyright notice

#include "combat_robot_msgs/msg/detail/gnss_status__functions.h"

ROSIDL_GENERATOR_C_PUBLIC_combat_robot_msgs
const rosidl_type_hash_t *
combat_robot_msgs__msg__GnssStatus__get_type_hash(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static rosidl_type_hash_t hash = {1, {
      0x68, 0xdf, 0x35, 0xd0, 0x0e, 0xac, 0x78, 0x5b,
      0x4a, 0x9d, 0xcf, 0xb5, 0x74, 0xdd, 0x47, 0x27,
      0x36, 0x04, 0xb0, 0x33, 0x5f, 0xd0, 0xa6, 0xaa,
      0xc0, 0xa2, 0x42, 0xb1, 0xcc, 0x47, 0xde, 0x68,
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

static char combat_robot_msgs__msg__GnssStatus__TYPE_NAME[] = "combat_robot_msgs/msg/GnssStatus";
static char builtin_interfaces__msg__Time__TYPE_NAME[] = "builtin_interfaces/msg/Time";
static char std_msgs__msg__Header__TYPE_NAME[] = "std_msgs/msg/Header";

// Define type names, field names, and default values
static char combat_robot_msgs__msg__GnssStatus__FIELD_NAME__header[] = "header";
static char combat_robot_msgs__msg__GnssStatus__FIELD_NAME__fix_status[] = "fix_status";
static char combat_robot_msgs__msg__GnssStatus__FIELD_NAME__num_satellites[] = "num_satellites";
static char combat_robot_msgs__msg__GnssStatus__FIELD_NAME__latitude[] = "latitude";
static char combat_robot_msgs__msg__GnssStatus__FIELD_NAME__longitude[] = "longitude";
static char combat_robot_msgs__msg__GnssStatus__FIELD_NAME__altitude_m[] = "altitude_m";
static char combat_robot_msgs__msg__GnssStatus__FIELD_NAME__heading_deg[] = "heading_deg";
static char combat_robot_msgs__msg__GnssStatus__FIELD_NAME__ground_speed_mps[] = "ground_speed_mps";
static char combat_robot_msgs__msg__GnssStatus__FIELD_NAME__horizontal_accuracy_m[] = "horizontal_accuracy_m";
static char combat_robot_msgs__msg__GnssStatus__FIELD_NAME__vertical_accuracy_m[] = "vertical_accuracy_m";

static rosidl_runtime_c__type_description__Field combat_robot_msgs__msg__GnssStatus__FIELDS[] = {
  {
    {combat_robot_msgs__msg__GnssStatus__FIELD_NAME__header, 6, 6},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_NESTED_TYPE,
      0,
      0,
      {std_msgs__msg__Header__TYPE_NAME, 19, 19},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__GnssStatus__FIELD_NAME__fix_status, 10, 10},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_UINT8,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__GnssStatus__FIELD_NAME__num_satellites, 14, 14},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_UINT8,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__GnssStatus__FIELD_NAME__latitude, 8, 8},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_DOUBLE,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__GnssStatus__FIELD_NAME__longitude, 9, 9},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_DOUBLE,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__GnssStatus__FIELD_NAME__altitude_m, 10, 10},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_DOUBLE,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__GnssStatus__FIELD_NAME__heading_deg, 11, 11},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_FLOAT,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__GnssStatus__FIELD_NAME__ground_speed_mps, 16, 16},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_FLOAT,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__GnssStatus__FIELD_NAME__horizontal_accuracy_m, 21, 21},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_FLOAT,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__GnssStatus__FIELD_NAME__vertical_accuracy_m, 19, 19},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_FLOAT,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
};

static rosidl_runtime_c__type_description__IndividualTypeDescription combat_robot_msgs__msg__GnssStatus__REFERENCED_TYPE_DESCRIPTIONS[] = {
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
combat_robot_msgs__msg__GnssStatus__get_type_description(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static bool constructed = false;
  static const rosidl_runtime_c__type_description__TypeDescription description = {
    {
      {combat_robot_msgs__msg__GnssStatus__TYPE_NAME, 32, 32},
      {combat_robot_msgs__msg__GnssStatus__FIELDS, 10, 10},
    },
    {combat_robot_msgs__msg__GnssStatus__REFERENCED_TYPE_DESCRIPTIONS, 2, 2},
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
  "# GNSS / RTK \\xeb\\x93\\x9c\\xeb\\x9d\\xbc\\xec\\x9d\\xb4\\xeb\\xb2\\x84\\xea\\xb0\\x80 publish \\xed\\x95\\x98\\xeb\\x8a\\x94 status.\n"
  "# robot_server \\xea\\xb0\\x80 /gnss/status \\xed\\x86\\xa0\\xed\\x94\\xbd\\xec\\x9c\\xbc\\xeb\\xa1\\x9c subscribe \\xed\\x95\\x98\\xec\\x97\\xac leader \\xec\\x9c\\x84\\xec\\xb9\\x98/heading/speed \\xec\\x99\\x80\n"
  "# \\xec\\x95\\xb1 \\xec\\x83\\x81\\xed\\x83\\x9c \\xed\\x8c\\xa8\\xed\\x82\\xb7\\xec\\x9d\\x98 fix \\xed\\x92\\x88\\xec\\xa7\\x88 \\xed\\x91\\x9c\\xec\\x8b\\x9c\\xec\\x97\\x90 \\xec\\x82\\xac\\xec\\x9a\\xa9\\xed\\x95\\xa8.\n"
  "#\n"
  "# \\xeb\\xaa\\xa8\\xeb\\x93\\xa0 \\xed\\x95\\x84\\xeb\\x93\\x9c\\xeb\\xa5\\xbc \\xeb\\xa7\\xa4 \\xec\\xa3\\xbc\\xea\\xb8\\xb0 \\xec\\xb1\\x84\\xec\\x9b\\x8c \\xeb\\xb3\\xb4\\xeb\\x82\\xb4\\xec\\xa3\\xbc\\xec\\x84\\xb8\\xec\\x9a\\x94. \\xea\\xb0\\x92\\xec\\x9d\\x84 \\xec\\x95\\x8c \\xec\\x88\\x98 \\xec\\x97\\x86\\xec\\x9c\\xbc\\xeb\\xa9\\xb4 \\xec\\x95\\x84\\xeb\\x9e\\x98 \\xec\\xa0\\x95\\xec\\x9d\\x98\\xeb\\x90\\x9c INVALID \\xea\\xb0\\x92\\xec\\x9d\\x84 \\xec\\x82\\xac\\xec\\x9a\\xa9.\n"
  "\n"
  "std_msgs/Header header\n"
  "\n"
  "# Fix \\xed\\x92\\x88\\xec\\xa7\\x88 (NMEA / RTK \\xea\\xb8\\xb0\\xec\\xa4\\x80)\n"
  "uint8 FIX_NONE = 0          # No fix / \\xec\\x88\\x98\\xec\\x8b\\xa0\\xea\\xb8\\xb0 \\xeb\\x81\\x8a\\xea\\xb9\\x80\n"
  "uint8 FIX_2D = 1            # 2D \\xec\\x9c\\x84\\xec\\x84\\xb1 fix\n"
  "uint8 FIX_3D = 2            # 3D \\xec\\x9c\\x84\\xec\\x84\\xb1 fix\n"
  "uint8 FIX_DGPS = 3          # Differential GPS\n"
  "uint8 FIX_RTK_FLOAT = 4     # RTK Float (cm~m \\xec\\x88\\x98\\xec\\xa4\\x80)\n"
  "uint8 FIX_RTK_FIXED = 5     # RTK Fixed (cm \\xec\\x88\\x98\\xec\\xa4\\x80)\n"
  "uint8 fix_status\n"
  "\n"
  "# \\xec\\xb6\\x94\\xec\\xa0\\x81 \\xec\\x9c\\x84\\xec\\x84\\xb1 \\xec\\x88\\x98 (0~32 typical)\n"
  "uint8 num_satellites\n"
  "\n"
  "# \\xec\\x9c\\x84\\xec\\xb9\\x98 \\xe2\\x80\\x94 WGS-84\n"
  "# \\xec\\x9c\\xa0\\xed\\x9a\\xa8\\xed\\x95\\x98\\xec\\xa7\\x80 \\xec\\x95\\x8a\\xec\\x9c\\xbc\\xeb\\xa9\\xb4 latitude=longitude=NaN\n"
  "float64 latitude            # \\xeb\\x8f\\x84\n"
  "float64 longitude           # \\xeb\\x8f\\x84\n"
  "float64 altitude_m          # MSL m\n"
  "\n"
  "# \\xeb\\xb0\\xa9\\xec\\x9c\\x84\\xea\\xb0\\x81 / \\xec\\x86\\x8d\\xeb\\x8f\\x84\n"
  "# \\xec\\x9c\\xa0\\xed\\x9a\\xa8\\xed\\x95\\x98\\xec\\xa7\\x80 \\xec\\x95\\x8a\\xec\\x9c\\xbc\\xeb\\xa9\\xb4 -1.0\n"
  "float32 heading_deg         # 0~360, true north \\xea\\xb8\\xb0\\xec\\xa4\\x80\n"
  "float32 ground_speed_mps    # >= 0\n"
  "\n"
  "# \\xec\\xa0\\x95\\xed\\x99\\x95\\xeb\\x8f\\x84 (1-sigma CEP)\n"
  "# \\xec\\x95\\x8c \\xec\\x88\\x98 \\xec\\x97\\x86\\xec\\x9c\\xbc\\xeb\\xa9\\xb4 -1.0\n"
  "float32 horizontal_accuracy_m\n"
  "float32 vertical_accuracy_m";

static char msg_encoding[] = "msg";

// Define all individual source functions

const rosidl_runtime_c__type_description__TypeSource *
combat_robot_msgs__msg__GnssStatus__get_individual_type_description_source(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static const rosidl_runtime_c__type_description__TypeSource source = {
    {combat_robot_msgs__msg__GnssStatus__TYPE_NAME, 32, 32},
    {msg_encoding, 3, 3},
    {toplevel_type_raw_source, 940, 940},
  };
  return &source;
}

const rosidl_runtime_c__type_description__TypeSource__Sequence *
combat_robot_msgs__msg__GnssStatus__get_type_description_sources(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static rosidl_runtime_c__type_description__TypeSource sources[3];
  static const rosidl_runtime_c__type_description__TypeSource__Sequence source_sequence = {sources, 3, 3};
  static bool constructed = false;
  if (!constructed) {
    sources[0] = *combat_robot_msgs__msg__GnssStatus__get_individual_type_description_source(NULL),
    sources[1] = *builtin_interfaces__msg__Time__get_individual_type_description_source(NULL);
    sources[2] = *std_msgs__msg__Header__get_individual_type_description_source(NULL);
    constructed = true;
  }
  return &source_sequence;
}
