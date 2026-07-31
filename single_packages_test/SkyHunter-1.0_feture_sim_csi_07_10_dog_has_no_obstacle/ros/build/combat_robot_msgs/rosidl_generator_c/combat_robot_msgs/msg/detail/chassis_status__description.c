// generated from rosidl_generator_c/resource/idl__description.c.em
// with input from combat_robot_msgs:msg/ChassisStatus.idl
// generated code does not contain a copyright notice

#include "combat_robot_msgs/msg/detail/chassis_status__functions.h"

ROSIDL_GENERATOR_C_PUBLIC_combat_robot_msgs
const rosidl_type_hash_t *
combat_robot_msgs__msg__ChassisStatus__get_type_hash(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static rosidl_type_hash_t hash = {1, {
      0xe1, 0x0f, 0xab, 0xae, 0x3a, 0xe4, 0x1a, 0x49,
      0x37, 0x82, 0xfc, 0x53, 0x68, 0xd1, 0x2e, 0x14,
      0x09, 0x22, 0x67, 0x3b, 0xa5, 0xdf, 0x40, 0xd3,
      0x10, 0xf6, 0x63, 0xa2, 0x89, 0xc7, 0x39, 0xbe,
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

static char combat_robot_msgs__msg__ChassisStatus__TYPE_NAME[] = "combat_robot_msgs/msg/ChassisStatus";
static char builtin_interfaces__msg__Time__TYPE_NAME[] = "builtin_interfaces/msg/Time";
static char std_msgs__msg__Header__TYPE_NAME[] = "std_msgs/msg/Header";

// Define type names, field names, and default values
static char combat_robot_msgs__msg__ChassisStatus__FIELD_NAME__header[] = "header";
static char combat_robot_msgs__msg__ChassisStatus__FIELD_NAME__drive_state[] = "drive_state";
static char combat_robot_msgs__msg__ChassisStatus__FIELD_NAME__battery_pct[] = "battery_pct";
static char combat_robot_msgs__msg__ChassisStatus__FIELD_NAME__battery_voltage_v[] = "battery_voltage_v";
static char combat_robot_msgs__msg__ChassisStatus__FIELD_NAME__battery_current_a[] = "battery_current_a";
static char combat_robot_msgs__msg__ChassisStatus__FIELD_NAME__linear_velocity_mps[] = "linear_velocity_mps";
static char combat_robot_msgs__msg__ChassisStatus__FIELD_NAME__angular_velocity_rps[] = "angular_velocity_rps";
static char combat_robot_msgs__msg__ChassisStatus__FIELD_NAME__fault_flags[] = "fault_flags";
static char combat_robot_msgs__msg__ChassisStatus__FIELD_NAME__motor_temp_c[] = "motor_temp_c";

static rosidl_runtime_c__type_description__Field combat_robot_msgs__msg__ChassisStatus__FIELDS[] = {
  {
    {combat_robot_msgs__msg__ChassisStatus__FIELD_NAME__header, 6, 6},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_NESTED_TYPE,
      0,
      0,
      {std_msgs__msg__Header__TYPE_NAME, 19, 19},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__ChassisStatus__FIELD_NAME__drive_state, 11, 11},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_UINT8,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__ChassisStatus__FIELD_NAME__battery_pct, 11, 11},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_UINT8,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__ChassisStatus__FIELD_NAME__battery_voltage_v, 17, 17},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_FLOAT,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__ChassisStatus__FIELD_NAME__battery_current_a, 17, 17},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_FLOAT,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__ChassisStatus__FIELD_NAME__linear_velocity_mps, 19, 19},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_FLOAT,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__ChassisStatus__FIELD_NAME__angular_velocity_rps, 20, 20},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_FLOAT,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__ChassisStatus__FIELD_NAME__fault_flags, 11, 11},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_UINT32,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__ChassisStatus__FIELD_NAME__motor_temp_c, 12, 12},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_FLOAT,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
};

static rosidl_runtime_c__type_description__IndividualTypeDescription combat_robot_msgs__msg__ChassisStatus__REFERENCED_TYPE_DESCRIPTIONS[] = {
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
combat_robot_msgs__msg__ChassisStatus__get_type_description(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static bool constructed = false;
  static const rosidl_runtime_c__type_description__TypeDescription description = {
    {
      {combat_robot_msgs__msg__ChassisStatus__TYPE_NAME, 35, 35},
      {combat_robot_msgs__msg__ChassisStatus__FIELDS, 9, 9},
    },
    {combat_robot_msgs__msg__ChassisStatus__REFERENCED_TYPE_DESCRIPTIONS, 2, 2},
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
  "# \\xec\\xb0\\xa8\\xeb\\x9f\\x89(chassis) \\xec\\xbb\\xa8\\xed\\x8a\\xb8\\xeb\\xa1\\xa4\\xeb\\x9f\\xac\\xea\\xb0\\x80 publish \\xed\\x95\\x98\\xeb\\x8a\\x94 status.\n"
  "# robot_server \\xea\\xb0\\x80 /chassis/status \\xed\\x86\\xa0\\xed\\x94\\xbd\\xec\\x9c\\xbc\\xeb\\xa1\\x9c subscribe \\xed\\x95\\x98\\xec\\x97\\xac BMS / \\xea\\xb5\\xac\\xeb\\x8f\\x99 \\xec\\x83\\x81\\xed\\x83\\x9c\\xeb\\xa5\\xbc\n"
  "# \\xec\\x95\\xb1 \\xec\\x83\\x81\\xed\\x83\\x9c \\xed\\x8c\\xa8\\xed\\x82\\xb7\\xec\\x9d\\x98 battery_pct, velocity \\xeb\\x93\\xb1\\xec\\x97\\x90 \\xeb\\xb0\\x98\\xec\\x98\\x81\\xed\\x95\\xa8.\n"
  "#\n"
  "# \\xea\\xb0\\x80\\xeb\\x8a\\xa5\\xed\\x95\\x9c \\xed\\x95\\x9c \\xeb\\xaa\\xa8\\xeb\\x93\\xa0 \\xed\\x95\\x84\\xeb\\x93\\x9c\\xeb\\xa5\\xbc \\xeb\\xa7\\xa4 \\xec\\xa3\\xbc\\xea\\xb8\\xb0 \\xec\\xb1\\x84\\xec\\x9b\\x8c \\xeb\\xb3\\xb4\\xeb\\x82\\xb4\\xec\\xa3\\xbc\\xec\\x84\\xb8\\xec\\x9a\\x94. \\xea\\xb0\\x92\\xec\\x9d\\x84 \\xeb\\xaa\\xa8\\xeb\\xa5\\xb4\\xeb\\xa9\\xb4 0 \\xeb\\x98\\x90\\xeb\\x8a\\x94 NaN \\xec\\x82\\xac\\xec\\x9a\\xa9 \\xea\\xb0\\x80\\xeb\\x8a\\xa5.\n"
  "\n"
  "std_msgs/Header header\n"
  "\n"
  "# Drive state\n"
  "uint8 DRIVE_OK = 0          # \\xec\\xa0\\x95\\xec\\x83\\x81 \\xec\\x9a\\xb4\\xed\\x96\\x89 \\xea\\xb0\\x80\\xeb\\x8a\\xa5\n"
  "uint8 DRIVE_FAULT = 1       # \\xeb\\xaa\\xa8\\xed\\x84\\xb0/\\xed\\x86\\xb5\\xec\\x8b\\xa0 \\xec\\x9e\\xa5\\xec\\x95\\xa0 \\xe2\\x80\\x94 \\xec\\x9a\\xb4\\xed\\x96\\x89 \\xeb\\xb6\\x88\\xea\\xb0\\x80\n"
  "uint8 DRIVE_ESTOP = 2       # E-Stop \\xec\\x9d\\xb8\\xea\\xb0\\x80\\xeb\\x90\\xa8\n"
  "uint8 drive_state\n"
  "\n"
  "# \\xeb\\xb0\\xb0\\xed\\x84\\xb0\\xeb\\xa6\\xac \\xe2\\x80\\x94 BMS \\xea\\xb0\\x92\n"
  "uint8 battery_pct           # 0~100 (255 = \\xec\\x95\\x8c \\xec\\x88\\x98 \\xec\\x97\\x86\\xec\\x9d\\x8c)\n"
  "float32 battery_voltage_v   # V\n"
  "float32 battery_current_a   # A  ( + = \\xeb\\xb0\\xa9\\xec\\xa0\\x84 / - = \\xec\\xb6\\xa9\\xec\\xa0\\x84 )\n"
  "\n"
  "# Velocity feedback \\xe2\\x80\\x94 \\xec\\xb0\\xa8\\xeb\\x9f\\x89\\xec\\x9d\\xb4 \\xec\\x8b\\xa4\\xec\\xa0\\x9c\\xeb\\xa1\\x9c \\xec\\x9b\\x80\\xec\\xa7\\x81\\xec\\x9d\\xb4\\xeb\\x8a\\x94 \\xec\\x86\\x8d\\xeb\\x8f\\x84 (encoder/odometry \\xea\\xb8\\xb0\\xeb\\xb0\\x98)\n"
  "# \\xec\\x95\\x8c \\xec\\x88\\x98 \\xec\\x97\\x86\\xec\\x9c\\xbc\\xeb\\xa9\\xb4 NaN.\n"
  "float32 linear_velocity_mps     # \\xec\\xa0\\x84\\xec\\xa7\\x84 \\xec\\x96\\x91 = +, \\xed\\x9b\\x84\\xec\\xa7\\x84 = -\n"
  "float32 angular_velocity_rps    # rad/s, \\xeb\\xb0\\x98\\xec\\x8b\\x9c\\xea\\xb3\\x84 \\xec\\x96\\x91 = +\n"
  "\n"
  "# Fault flags \\xe2\\x80\\x94 bitmask. \\xec\\x97\\xac\\xeb\\x9f\\xac \\xea\\xb2\\xb0\\xed\\x95\\xa8 \\xeb\\x8f\\x99\\xec\\x8b\\x9c \\xed\\x91\\x9c\\xed\\x98\\x84 \\xea\\xb0\\x80\\xeb\\x8a\\xa5.\n"
  "uint32 FAULT_NONE = 0\n"
  "uint32 FAULT_LEFT_WHEEL = 1\n"
  "uint32 FAULT_RIGHT_WHEEL = 2\n"
  "uint32 FAULT_LOW_BATTERY = 4\n"
  "uint32 FAULT_OVERTEMP = 8\n"
  "uint32 FAULT_COMM = 16          # chassis \\xe2\\x86\\x94 robot_server \\xed\\x86\\xb5\\xec\\x8b\\xa0 (Modbus \\xeb\\x93\\xb1) \\xec\\x9e\\xa5\\xec\\x95\\xa0\n"
  "uint32 fault_flags\n"
  "\n"
  "# \\xeb\\xaa\\xa8\\xed\\x84\\xb0 \\xec\\xb5\\x9c\\xea\\xb3\\xa0 \\xec\\x98\\xa8\\xeb\\x8f\\x84 (\\xec\\xa2\\x8c/\\xec\\x9a\\xb0 \\xed\\x9c\\xa0 \\xec\\xa4\\x91 \\xed\\x81\\xb0 \\xea\\xb0\\x92). \\xec\\x95\\x8c \\xec\\x88\\x98 \\xec\\x97\\x86\\xec\\x9c\\xbc\\xeb\\xa9\\xb4 NaN.\n"
  "float32 motor_temp_c";

static char msg_encoding[] = "msg";

// Define all individual source functions

const rosidl_runtime_c__type_description__TypeSource *
combat_robot_msgs__msg__ChassisStatus__get_individual_type_description_source(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static const rosidl_runtime_c__type_description__TypeSource source = {
    {combat_robot_msgs__msg__ChassisStatus__TYPE_NAME, 35, 35},
    {msg_encoding, 3, 3},
    {toplevel_type_raw_source, 1042, 1042},
  };
  return &source;
}

const rosidl_runtime_c__type_description__TypeSource__Sequence *
combat_robot_msgs__msg__ChassisStatus__get_type_description_sources(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static rosidl_runtime_c__type_description__TypeSource sources[3];
  static const rosidl_runtime_c__type_description__TypeSource__Sequence source_sequence = {sources, 3, 3};
  static bool constructed = false;
  if (!constructed) {
    sources[0] = *combat_robot_msgs__msg__ChassisStatus__get_individual_type_description_source(NULL),
    sources[1] = *builtin_interfaces__msg__Time__get_individual_type_description_source(NULL);
    sources[2] = *std_msgs__msg__Header__get_individual_type_description_source(NULL);
    constructed = true;
  }
  return &source_sequence;
}
