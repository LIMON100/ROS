// generated from rosidl_typesupport_fastrtps_c/resource/idl__type_support_c.cpp.em
// with input from combat_robot_msgs:msg/BoundingBox2d.idl
// generated code does not contain a copyright notice
#include "combat_robot_msgs/msg/detail/bounding_box2d__rosidl_typesupport_fastrtps_c.h"


#include <cassert>
#include <cstddef>
#include <limits>
#include <string>
#include "rosidl_typesupport_fastrtps_c/identifier.h"
#include "rosidl_typesupport_fastrtps_c/serialization_helpers.hpp"
#include "rosidl_typesupport_fastrtps_c/wstring_conversion.hpp"
#include "rosidl_typesupport_fastrtps_cpp/message_type_support.h"
#include "combat_robot_msgs/msg/rosidl_typesupport_fastrtps_c__visibility_control.h"
#include "combat_robot_msgs/msg/detail/bounding_box2d__struct.h"
#include "combat_robot_msgs/msg/detail/bounding_box2d__functions.h"
#include "fastcdr/Cdr.h"

#ifndef _WIN32
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wunused-parameter"
# ifdef __clang__
#  pragma clang diagnostic ignored "-Wdeprecated-register"
#  pragma clang diagnostic ignored "-Wreturn-type-c-linkage"
# endif
#endif
#ifndef _WIN32
# pragma GCC diagnostic pop
#endif

// includes and forward declarations of message dependencies and their conversion functions

#if defined(__cplusplus)
extern "C"
{
#endif


// forward declare type support functions


using _BoundingBox2d__ros_msg_type = combat_robot_msgs__msg__BoundingBox2d;


ROSIDL_TYPESUPPORT_FASTRTPS_C_PUBLIC_combat_robot_msgs
bool cdr_serialize_combat_robot_msgs__msg__BoundingBox2d(
  const combat_robot_msgs__msg__BoundingBox2d * ros_message,
  eprosima::fastcdr::Cdr & cdr)
{
  // Field name: x
  {
    cdr << ros_message->x;
  }

  // Field name: y
  {
    cdr << ros_message->y;
  }

  // Field name: width
  {
    cdr << ros_message->width;
  }

  // Field name: height
  {
    cdr << ros_message->height;
  }

  return true;
}

ROSIDL_TYPESUPPORT_FASTRTPS_C_PUBLIC_combat_robot_msgs
bool cdr_deserialize_combat_robot_msgs__msg__BoundingBox2d(
  eprosima::fastcdr::Cdr & cdr,
  combat_robot_msgs__msg__BoundingBox2d * ros_message)
{
  // Field name: x
  {
    cdr >> ros_message->x;
  }

  // Field name: y
  {
    cdr >> ros_message->y;
  }

  // Field name: width
  {
    cdr >> ros_message->width;
  }

  // Field name: height
  {
    cdr >> ros_message->height;
  }

  return true;
}  // NOLINT(readability/fn_size)


ROSIDL_TYPESUPPORT_FASTRTPS_C_PUBLIC_combat_robot_msgs
size_t get_serialized_size_combat_robot_msgs__msg__BoundingBox2d(
  const void * untyped_ros_message,
  size_t current_alignment)
{
  const _BoundingBox2d__ros_msg_type * ros_message = static_cast<const _BoundingBox2d__ros_msg_type *>(untyped_ros_message);
  (void)ros_message;
  size_t initial_alignment = current_alignment;

  const size_t padding = 4;
  const size_t wchar_size = 4;
  (void)padding;
  (void)wchar_size;

  // Field name: x
  {
    size_t item_size = sizeof(ros_message->x);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: y
  {
    size_t item_size = sizeof(ros_message->y);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: width
  {
    size_t item_size = sizeof(ros_message->width);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: height
  {
    size_t item_size = sizeof(ros_message->height);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  return current_alignment - initial_alignment;
}


ROSIDL_TYPESUPPORT_FASTRTPS_C_PUBLIC_combat_robot_msgs
size_t max_serialized_size_combat_robot_msgs__msg__BoundingBox2d(
  bool & full_bounded,
  bool & is_plain,
  size_t current_alignment)
{
  size_t initial_alignment = current_alignment;

  const size_t padding = 4;
  const size_t wchar_size = 4;
  size_t last_member_size = 0;
  (void)last_member_size;
  (void)padding;
  (void)wchar_size;

  full_bounded = true;
  is_plain = true;

  // Field name: x
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint32_t);
    current_alignment += array_size * sizeof(uint32_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint32_t));
  }

  // Field name: y
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint32_t);
    current_alignment += array_size * sizeof(uint32_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint32_t));
  }

  // Field name: width
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint32_t);
    current_alignment += array_size * sizeof(uint32_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint32_t));
  }

  // Field name: height
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint32_t);
    current_alignment += array_size * sizeof(uint32_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint32_t));
  }


  size_t ret_val = current_alignment - initial_alignment;
  if (is_plain) {
    // All members are plain, and type is not empty.
    // We still need to check that the in-memory alignment
    // is the same as the CDR mandated alignment.
    using DataType = combat_robot_msgs__msg__BoundingBox2d;
    is_plain =
      (
      offsetof(DataType, height) +
      last_member_size
      ) == ret_val;
  }
  return ret_val;
}

ROSIDL_TYPESUPPORT_FASTRTPS_C_PUBLIC_combat_robot_msgs
bool cdr_serialize_key_combat_robot_msgs__msg__BoundingBox2d(
  const combat_robot_msgs__msg__BoundingBox2d * ros_message,
  eprosima::fastcdr::Cdr & cdr)
{
  // Field name: x
  {
    cdr << ros_message->x;
  }

  // Field name: y
  {
    cdr << ros_message->y;
  }

  // Field name: width
  {
    cdr << ros_message->width;
  }

  // Field name: height
  {
    cdr << ros_message->height;
  }

  return true;
}

ROSIDL_TYPESUPPORT_FASTRTPS_C_PUBLIC_combat_robot_msgs
size_t get_serialized_size_key_combat_robot_msgs__msg__BoundingBox2d(
  const void * untyped_ros_message,
  size_t current_alignment)
{
  const _BoundingBox2d__ros_msg_type * ros_message = static_cast<const _BoundingBox2d__ros_msg_type *>(untyped_ros_message);
  (void)ros_message;

  size_t initial_alignment = current_alignment;

  const size_t padding = 4;
  const size_t wchar_size = 4;
  (void)padding;
  (void)wchar_size;

  // Field name: x
  {
    size_t item_size = sizeof(ros_message->x);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: y
  {
    size_t item_size = sizeof(ros_message->y);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: width
  {
    size_t item_size = sizeof(ros_message->width);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: height
  {
    size_t item_size = sizeof(ros_message->height);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  return current_alignment - initial_alignment;
}

ROSIDL_TYPESUPPORT_FASTRTPS_C_PUBLIC_combat_robot_msgs
size_t max_serialized_size_key_combat_robot_msgs__msg__BoundingBox2d(
  bool & full_bounded,
  bool & is_plain,
  size_t current_alignment)
{
  size_t initial_alignment = current_alignment;

  const size_t padding = 4;
  const size_t wchar_size = 4;
  size_t last_member_size = 0;
  (void)last_member_size;
  (void)padding;
  (void)wchar_size;

  full_bounded = true;
  is_plain = true;
  // Field name: x
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint32_t);
    current_alignment += array_size * sizeof(uint32_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint32_t));
  }

  // Field name: y
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint32_t);
    current_alignment += array_size * sizeof(uint32_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint32_t));
  }

  // Field name: width
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint32_t);
    current_alignment += array_size * sizeof(uint32_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint32_t));
  }

  // Field name: height
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint32_t);
    current_alignment += array_size * sizeof(uint32_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint32_t));
  }

  size_t ret_val = current_alignment - initial_alignment;
  if (is_plain) {
    // All members are plain, and type is not empty.
    // We still need to check that the in-memory alignment
    // is the same as the CDR mandated alignment.
    using DataType = combat_robot_msgs__msg__BoundingBox2d;
    is_plain =
      (
      offsetof(DataType, height) +
      last_member_size
      ) == ret_val;
  }
  return ret_val;
}


static bool _BoundingBox2d__cdr_serialize(
  const void * untyped_ros_message,
  eprosima::fastcdr::Cdr & cdr)
{
  if (!untyped_ros_message) {
    fprintf(stderr, "ros message handle is null\n");
    return false;
  }
  const combat_robot_msgs__msg__BoundingBox2d * ros_message = static_cast<const combat_robot_msgs__msg__BoundingBox2d *>(untyped_ros_message);
  (void)ros_message;
  return cdr_serialize_combat_robot_msgs__msg__BoundingBox2d(ros_message, cdr);
}

static bool _BoundingBox2d__cdr_deserialize(
  eprosima::fastcdr::Cdr & cdr,
  void * untyped_ros_message)
{
  if (!untyped_ros_message) {
    fprintf(stderr, "ros message handle is null\n");
    return false;
  }
  combat_robot_msgs__msg__BoundingBox2d * ros_message = static_cast<combat_robot_msgs__msg__BoundingBox2d *>(untyped_ros_message);
  (void)ros_message;
  return cdr_deserialize_combat_robot_msgs__msg__BoundingBox2d(cdr, ros_message);
}

static uint32_t _BoundingBox2d__get_serialized_size(const void * untyped_ros_message)
{
  return static_cast<uint32_t>(
    get_serialized_size_combat_robot_msgs__msg__BoundingBox2d(
      untyped_ros_message, 0));
}

static size_t _BoundingBox2d__max_serialized_size(char & bounds_info)
{
  bool full_bounded;
  bool is_plain;
  size_t ret_val;

  ret_val = max_serialized_size_combat_robot_msgs__msg__BoundingBox2d(
    full_bounded, is_plain, 0);

  bounds_info =
    is_plain ? ROSIDL_TYPESUPPORT_FASTRTPS_PLAIN_TYPE :
    full_bounded ? ROSIDL_TYPESUPPORT_FASTRTPS_BOUNDED_TYPE : ROSIDL_TYPESUPPORT_FASTRTPS_UNBOUNDED_TYPE;
  return ret_val;
}


static message_type_support_callbacks_t __callbacks_BoundingBox2d = {
  "combat_robot_msgs::msg",
  "BoundingBox2d",
  _BoundingBox2d__cdr_serialize,
  _BoundingBox2d__cdr_deserialize,
  _BoundingBox2d__get_serialized_size,
  _BoundingBox2d__max_serialized_size,
  nullptr
};

static rosidl_message_type_support_t _BoundingBox2d__type_support = {
  rosidl_typesupport_fastrtps_c__identifier,
  &__callbacks_BoundingBox2d,
  get_message_typesupport_handle_function,
  &combat_robot_msgs__msg__BoundingBox2d__get_type_hash,
  &combat_robot_msgs__msg__BoundingBox2d__get_type_description,
  &combat_robot_msgs__msg__BoundingBox2d__get_type_description_sources,
};

const rosidl_message_type_support_t *
ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(rosidl_typesupport_fastrtps_c, combat_robot_msgs, msg, BoundingBox2d)() {
  return &_BoundingBox2d__type_support;
}

#if defined(__cplusplus)
}
#endif
