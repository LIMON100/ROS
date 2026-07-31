// generated from rosidl_typesupport_fastrtps_c/resource/idl__type_support_c.cpp.em
// with input from combat_robot_msgs:msg/PanTiltState.idl
// generated code does not contain a copyright notice
#include "combat_robot_msgs/msg/detail/pan_tilt_state__rosidl_typesupport_fastrtps_c.h"


#include <cassert>
#include <cstddef>
#include <limits>
#include <string>
#include "rosidl_typesupport_fastrtps_c/identifier.h"
#include "rosidl_typesupport_fastrtps_c/serialization_helpers.hpp"
#include "rosidl_typesupport_fastrtps_c/wstring_conversion.hpp"
#include "rosidl_typesupport_fastrtps_cpp/message_type_support.h"
#include "combat_robot_msgs/msg/rosidl_typesupport_fastrtps_c__visibility_control.h"
#include "combat_robot_msgs/msg/detail/pan_tilt_state__struct.h"
#include "combat_robot_msgs/msg/detail/pan_tilt_state__functions.h"
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

#include "builtin_interfaces/msg/detail/time__functions.h"  // stamp

// forward declare type support functions

ROSIDL_TYPESUPPORT_FASTRTPS_C_IMPORT_combat_robot_msgs
bool cdr_serialize_builtin_interfaces__msg__Time(
  const builtin_interfaces__msg__Time * ros_message,
  eprosima::fastcdr::Cdr & cdr);

ROSIDL_TYPESUPPORT_FASTRTPS_C_IMPORT_combat_robot_msgs
bool cdr_deserialize_builtin_interfaces__msg__Time(
  eprosima::fastcdr::Cdr & cdr,
  builtin_interfaces__msg__Time * ros_message);

ROSIDL_TYPESUPPORT_FASTRTPS_C_IMPORT_combat_robot_msgs
size_t get_serialized_size_builtin_interfaces__msg__Time(
  const void * untyped_ros_message,
  size_t current_alignment);

ROSIDL_TYPESUPPORT_FASTRTPS_C_IMPORT_combat_robot_msgs
size_t max_serialized_size_builtin_interfaces__msg__Time(
  bool & full_bounded,
  bool & is_plain,
  size_t current_alignment);

ROSIDL_TYPESUPPORT_FASTRTPS_C_IMPORT_combat_robot_msgs
bool cdr_serialize_key_builtin_interfaces__msg__Time(
  const builtin_interfaces__msg__Time * ros_message,
  eprosima::fastcdr::Cdr & cdr);

ROSIDL_TYPESUPPORT_FASTRTPS_C_IMPORT_combat_robot_msgs
size_t get_serialized_size_key_builtin_interfaces__msg__Time(
  const void * untyped_ros_message,
  size_t current_alignment);

ROSIDL_TYPESUPPORT_FASTRTPS_C_IMPORT_combat_robot_msgs
size_t max_serialized_size_key_builtin_interfaces__msg__Time(
  bool & full_bounded,
  bool & is_plain,
  size_t current_alignment);

ROSIDL_TYPESUPPORT_FASTRTPS_C_IMPORT_combat_robot_msgs
const rosidl_message_type_support_t *
  ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(rosidl_typesupport_fastrtps_c, builtin_interfaces, msg, Time)();


using _PanTiltState__ros_msg_type = combat_robot_msgs__msg__PanTiltState;


ROSIDL_TYPESUPPORT_FASTRTPS_C_PUBLIC_combat_robot_msgs
bool cdr_serialize_combat_robot_msgs__msg__PanTiltState(
  const combat_robot_msgs__msg__PanTiltState * ros_message,
  eprosima::fastcdr::Cdr & cdr)
{
  // Field name: stamp
  {
    cdr_serialize_builtin_interfaces__msg__Time(
      &ros_message->stamp, cdr);
  }

  // Field name: control_mode
  {
    cdr << ros_message->control_mode;
  }

  // Field name: horizontal_angle
  {
    cdr << ros_message->horizontal_angle;
  }

  // Field name: vertical_angle
  {
    cdr << ros_message->vertical_angle;
  }

  // Field name: pan_speed
  {
    cdr << ros_message->pan_speed;
  }

  // Field name: tilt_speed
  {
    cdr << ros_message->tilt_speed;
  }

  return true;
}

ROSIDL_TYPESUPPORT_FASTRTPS_C_PUBLIC_combat_robot_msgs
bool cdr_deserialize_combat_robot_msgs__msg__PanTiltState(
  eprosima::fastcdr::Cdr & cdr,
  combat_robot_msgs__msg__PanTiltState * ros_message)
{
  // Field name: stamp
  {
    cdr_deserialize_builtin_interfaces__msg__Time(cdr, &ros_message->stamp);
  }

  // Field name: control_mode
  {
    cdr >> ros_message->control_mode;
  }

  // Field name: horizontal_angle
  {
    cdr >> ros_message->horizontal_angle;
  }

  // Field name: vertical_angle
  {
    cdr >> ros_message->vertical_angle;
  }

  // Field name: pan_speed
  {
    cdr >> ros_message->pan_speed;
  }

  // Field name: tilt_speed
  {
    cdr >> ros_message->tilt_speed;
  }

  return true;
}  // NOLINT(readability/fn_size)


ROSIDL_TYPESUPPORT_FASTRTPS_C_PUBLIC_combat_robot_msgs
size_t get_serialized_size_combat_robot_msgs__msg__PanTiltState(
  const void * untyped_ros_message,
  size_t current_alignment)
{
  const _PanTiltState__ros_msg_type * ros_message = static_cast<const _PanTiltState__ros_msg_type *>(untyped_ros_message);
  (void)ros_message;
  size_t initial_alignment = current_alignment;

  const size_t padding = 4;
  const size_t wchar_size = 4;
  (void)padding;
  (void)wchar_size;

  // Field name: stamp
  current_alignment += get_serialized_size_builtin_interfaces__msg__Time(
    &(ros_message->stamp), current_alignment);

  // Field name: control_mode
  {
    size_t item_size = sizeof(ros_message->control_mode);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: horizontal_angle
  {
    size_t item_size = sizeof(ros_message->horizontal_angle);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: vertical_angle
  {
    size_t item_size = sizeof(ros_message->vertical_angle);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: pan_speed
  {
    size_t item_size = sizeof(ros_message->pan_speed);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: tilt_speed
  {
    size_t item_size = sizeof(ros_message->tilt_speed);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  return current_alignment - initial_alignment;
}


ROSIDL_TYPESUPPORT_FASTRTPS_C_PUBLIC_combat_robot_msgs
size_t max_serialized_size_combat_robot_msgs__msg__PanTiltState(
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

  // Field name: stamp
  {
    size_t array_size = 1;
    last_member_size = 0;
    for (size_t index = 0; index < array_size; ++index) {
      bool inner_full_bounded;
      bool inner_is_plain;
      size_t inner_size;
      inner_size =
        max_serialized_size_builtin_interfaces__msg__Time(
        inner_full_bounded, inner_is_plain, current_alignment);
      last_member_size += inner_size;
      current_alignment += inner_size;
      full_bounded &= inner_full_bounded;
      is_plain &= inner_is_plain;
    }
  }

  // Field name: control_mode
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint8_t);
    current_alignment += array_size * sizeof(uint8_t);
  }

  // Field name: horizontal_angle
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint32_t);
    current_alignment += array_size * sizeof(uint32_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint32_t));
  }

  // Field name: vertical_angle
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint32_t);
    current_alignment += array_size * sizeof(uint32_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint32_t));
  }

  // Field name: pan_speed
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint32_t);
    current_alignment += array_size * sizeof(uint32_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint32_t));
  }

  // Field name: tilt_speed
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
    using DataType = combat_robot_msgs__msg__PanTiltState;
    is_plain =
      (
      offsetof(DataType, tilt_speed) +
      last_member_size
      ) == ret_val;
  }
  return ret_val;
}

ROSIDL_TYPESUPPORT_FASTRTPS_C_PUBLIC_combat_robot_msgs
bool cdr_serialize_key_combat_robot_msgs__msg__PanTiltState(
  const combat_robot_msgs__msg__PanTiltState * ros_message,
  eprosima::fastcdr::Cdr & cdr)
{
  // Field name: stamp
  {
    cdr_serialize_key_builtin_interfaces__msg__Time(
      &ros_message->stamp, cdr);
  }

  // Field name: control_mode
  {
    cdr << ros_message->control_mode;
  }

  // Field name: horizontal_angle
  {
    cdr << ros_message->horizontal_angle;
  }

  // Field name: vertical_angle
  {
    cdr << ros_message->vertical_angle;
  }

  // Field name: pan_speed
  {
    cdr << ros_message->pan_speed;
  }

  // Field name: tilt_speed
  {
    cdr << ros_message->tilt_speed;
  }

  return true;
}

ROSIDL_TYPESUPPORT_FASTRTPS_C_PUBLIC_combat_robot_msgs
size_t get_serialized_size_key_combat_robot_msgs__msg__PanTiltState(
  const void * untyped_ros_message,
  size_t current_alignment)
{
  const _PanTiltState__ros_msg_type * ros_message = static_cast<const _PanTiltState__ros_msg_type *>(untyped_ros_message);
  (void)ros_message;

  size_t initial_alignment = current_alignment;

  const size_t padding = 4;
  const size_t wchar_size = 4;
  (void)padding;
  (void)wchar_size;

  // Field name: stamp
  current_alignment += get_serialized_size_key_builtin_interfaces__msg__Time(
    &(ros_message->stamp), current_alignment);

  // Field name: control_mode
  {
    size_t item_size = sizeof(ros_message->control_mode);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: horizontal_angle
  {
    size_t item_size = sizeof(ros_message->horizontal_angle);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: vertical_angle
  {
    size_t item_size = sizeof(ros_message->vertical_angle);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: pan_speed
  {
    size_t item_size = sizeof(ros_message->pan_speed);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: tilt_speed
  {
    size_t item_size = sizeof(ros_message->tilt_speed);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  return current_alignment - initial_alignment;
}

ROSIDL_TYPESUPPORT_FASTRTPS_C_PUBLIC_combat_robot_msgs
size_t max_serialized_size_key_combat_robot_msgs__msg__PanTiltState(
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
  // Field name: stamp
  {
    size_t array_size = 1;
    last_member_size = 0;
    for (size_t index = 0; index < array_size; ++index) {
      bool inner_full_bounded;
      bool inner_is_plain;
      size_t inner_size;
      inner_size =
        max_serialized_size_key_builtin_interfaces__msg__Time(
        inner_full_bounded, inner_is_plain, current_alignment);
      last_member_size += inner_size;
      current_alignment += inner_size;
      full_bounded &= inner_full_bounded;
      is_plain &= inner_is_plain;
    }
  }

  // Field name: control_mode
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint8_t);
    current_alignment += array_size * sizeof(uint8_t);
  }

  // Field name: horizontal_angle
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint32_t);
    current_alignment += array_size * sizeof(uint32_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint32_t));
  }

  // Field name: vertical_angle
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint32_t);
    current_alignment += array_size * sizeof(uint32_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint32_t));
  }

  // Field name: pan_speed
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint32_t);
    current_alignment += array_size * sizeof(uint32_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint32_t));
  }

  // Field name: tilt_speed
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
    using DataType = combat_robot_msgs__msg__PanTiltState;
    is_plain =
      (
      offsetof(DataType, tilt_speed) +
      last_member_size
      ) == ret_val;
  }
  return ret_val;
}


static bool _PanTiltState__cdr_serialize(
  const void * untyped_ros_message,
  eprosima::fastcdr::Cdr & cdr)
{
  if (!untyped_ros_message) {
    fprintf(stderr, "ros message handle is null\n");
    return false;
  }
  const combat_robot_msgs__msg__PanTiltState * ros_message = static_cast<const combat_robot_msgs__msg__PanTiltState *>(untyped_ros_message);
  (void)ros_message;
  return cdr_serialize_combat_robot_msgs__msg__PanTiltState(ros_message, cdr);
}

static bool _PanTiltState__cdr_deserialize(
  eprosima::fastcdr::Cdr & cdr,
  void * untyped_ros_message)
{
  if (!untyped_ros_message) {
    fprintf(stderr, "ros message handle is null\n");
    return false;
  }
  combat_robot_msgs__msg__PanTiltState * ros_message = static_cast<combat_robot_msgs__msg__PanTiltState *>(untyped_ros_message);
  (void)ros_message;
  return cdr_deserialize_combat_robot_msgs__msg__PanTiltState(cdr, ros_message);
}

static uint32_t _PanTiltState__get_serialized_size(const void * untyped_ros_message)
{
  return static_cast<uint32_t>(
    get_serialized_size_combat_robot_msgs__msg__PanTiltState(
      untyped_ros_message, 0));
}

static size_t _PanTiltState__max_serialized_size(char & bounds_info)
{
  bool full_bounded;
  bool is_plain;
  size_t ret_val;

  ret_val = max_serialized_size_combat_robot_msgs__msg__PanTiltState(
    full_bounded, is_plain, 0);

  bounds_info =
    is_plain ? ROSIDL_TYPESUPPORT_FASTRTPS_PLAIN_TYPE :
    full_bounded ? ROSIDL_TYPESUPPORT_FASTRTPS_BOUNDED_TYPE : ROSIDL_TYPESUPPORT_FASTRTPS_UNBOUNDED_TYPE;
  return ret_val;
}


static message_type_support_callbacks_t __callbacks_PanTiltState = {
  "combat_robot_msgs::msg",
  "PanTiltState",
  _PanTiltState__cdr_serialize,
  _PanTiltState__cdr_deserialize,
  _PanTiltState__get_serialized_size,
  _PanTiltState__max_serialized_size,
  nullptr
};

static rosidl_message_type_support_t _PanTiltState__type_support = {
  rosidl_typesupport_fastrtps_c__identifier,
  &__callbacks_PanTiltState,
  get_message_typesupport_handle_function,
  &combat_robot_msgs__msg__PanTiltState__get_type_hash,
  &combat_robot_msgs__msg__PanTiltState__get_type_description,
  &combat_robot_msgs__msg__PanTiltState__get_type_description_sources,
};

const rosidl_message_type_support_t *
ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(rosidl_typesupport_fastrtps_c, combat_robot_msgs, msg, PanTiltState)() {
  return &_PanTiltState__type_support;
}

#if defined(__cplusplus)
}
#endif
