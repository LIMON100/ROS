// generated from rosidl_generator_c/resource/idl__description.c.em
// with input from combat_robot_msgs:msg/DetectedObject.idl
// generated code does not contain a copyright notice

#include "combat_robot_msgs/msg/detail/detected_object__functions.h"

ROSIDL_GENERATOR_C_PUBLIC_combat_robot_msgs
const rosidl_type_hash_t *
combat_robot_msgs__msg__DetectedObject__get_type_hash(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static rosidl_type_hash_t hash = {1, {
      0x2f, 0xe6, 0xd6, 0xb0, 0xf8, 0x47, 0xa4, 0x9f,
      0x2e, 0xaa, 0xac, 0x73, 0x65, 0x42, 0xc9, 0x9a,
      0x5f, 0x70, 0xb2, 0x48, 0x9c, 0x66, 0xd5, 0x12,
      0x89, 0x3d, 0xac, 0xa5, 0x31, 0xd0, 0x4a, 0x7f,
    }};
  return &hash;
}

#include <assert.h>
#include <string.h>

// Include directives for referenced types
#include "combat_robot_msgs/msg/detail/bounding_box2d__functions.h"

// Hashes for external referenced types
#ifndef NDEBUG
static const rosidl_type_hash_t combat_robot_msgs__msg__BoundingBox2d__EXPECTED_HASH = {1, {
    0xae, 0x31, 0x52, 0x9e, 0xf7, 0x38, 0x03, 0xde,
    0xd0, 0x3b, 0x43, 0xb9, 0x04, 0xe2, 0xd5, 0x67,
    0x27, 0xdf, 0xa3, 0x5b, 0x27, 0x0b, 0x3b, 0xfe,
    0x92, 0x49, 0x8f, 0xb5, 0xc0, 0xe3, 0x72, 0x78,
  }};
#endif

static char combat_robot_msgs__msg__DetectedObject__TYPE_NAME[] = "combat_robot_msgs/msg/DetectedObject";
static char combat_robot_msgs__msg__BoundingBox2d__TYPE_NAME[] = "combat_robot_msgs/msg/BoundingBox2d";

// Define type names, field names, and default values
static char combat_robot_msgs__msg__DetectedObject__FIELD_NAME__id[] = "id";
static char combat_robot_msgs__msg__DetectedObject__FIELD_NAME__prob[] = "prob";
static char combat_robot_msgs__msg__DetectedObject__FIELD_NAME__box[] = "box";

static rosidl_runtime_c__type_description__Field combat_robot_msgs__msg__DetectedObject__FIELDS[] = {
  {
    {combat_robot_msgs__msg__DetectedObject__FIELD_NAME__id, 2, 2},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_INT32,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__DetectedObject__FIELD_NAME__prob, 4, 4},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_FLOAT,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {combat_robot_msgs__msg__DetectedObject__FIELD_NAME__box, 3, 3},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_NESTED_TYPE,
      0,
      0,
      {combat_robot_msgs__msg__BoundingBox2d__TYPE_NAME, 35, 35},
    },
    {NULL, 0, 0},
  },
};

static rosidl_runtime_c__type_description__IndividualTypeDescription combat_robot_msgs__msg__DetectedObject__REFERENCED_TYPE_DESCRIPTIONS[] = {
  {
    {combat_robot_msgs__msg__BoundingBox2d__TYPE_NAME, 35, 35},
    {NULL, 0, 0},
  },
};

const rosidl_runtime_c__type_description__TypeDescription *
combat_robot_msgs__msg__DetectedObject__get_type_description(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static bool constructed = false;
  static const rosidl_runtime_c__type_description__TypeDescription description = {
    {
      {combat_robot_msgs__msg__DetectedObject__TYPE_NAME, 36, 36},
      {combat_robot_msgs__msg__DetectedObject__FIELDS, 3, 3},
    },
    {combat_robot_msgs__msg__DetectedObject__REFERENCED_TYPE_DESCRIPTIONS, 1, 1},
  };
  if (!constructed) {
    assert(0 == memcmp(&combat_robot_msgs__msg__BoundingBox2d__EXPECTED_HASH, combat_robot_msgs__msg__BoundingBox2d__get_type_hash(NULL), sizeof(rosidl_type_hash_t)));
    description.referenced_type_descriptions.data[0].fields = combat_robot_msgs__msg__BoundingBox2d__get_type_description(NULL)->type_description.fields;
    constructed = true;
  }
  return &description;
}

static char toplevel_type_raw_source[] =
  "# DetectedObject.msg\n"
  "\n"
  "int32 id\n"
  "float32 prob\n"
  "BoundingBox2d box";

static char msg_encoding[] = "msg";

// Define all individual source functions

const rosidl_runtime_c__type_description__TypeSource *
combat_robot_msgs__msg__DetectedObject__get_individual_type_description_source(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static const rosidl_runtime_c__type_description__TypeSource source = {
    {combat_robot_msgs__msg__DetectedObject__TYPE_NAME, 36, 36},
    {msg_encoding, 3, 3},
    {toplevel_type_raw_source, 61, 61},
  };
  return &source;
}

const rosidl_runtime_c__type_description__TypeSource__Sequence *
combat_robot_msgs__msg__DetectedObject__get_type_description_sources(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static rosidl_runtime_c__type_description__TypeSource sources[2];
  static const rosidl_runtime_c__type_description__TypeSource__Sequence source_sequence = {sources, 2, 2};
  static bool constructed = false;
  if (!constructed) {
    sources[0] = *combat_robot_msgs__msg__DetectedObject__get_individual_type_description_source(NULL),
    sources[1] = *combat_robot_msgs__msg__BoundingBox2d__get_individual_type_description_source(NULL);
    constructed = true;
  }
  return &source_sequence;
}
