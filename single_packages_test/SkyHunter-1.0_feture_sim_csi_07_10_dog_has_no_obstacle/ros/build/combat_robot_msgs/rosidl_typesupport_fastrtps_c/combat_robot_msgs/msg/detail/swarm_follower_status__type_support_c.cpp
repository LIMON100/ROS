// generated from rosidl_typesupport_fastrtps_c/resource/idl__type_support_c.cpp.em
// with input from combat_robot_msgs:msg/SwarmFollowerStatus.idl
// generated code does not contain a copyright notice
#include "combat_robot_msgs/msg/detail/swarm_follower_status__rosidl_typesupport_fastrtps_c.h"


#include <cassert>
#include <cstddef>
#include <limits>
#include <string>
#include "rosidl_typesupport_fastrtps_c/identifier.h"
#include "rosidl_typesupport_fastrtps_c/serialization_helpers.hpp"
#include "rosidl_typesupport_fastrtps_c/wstring_conversion.hpp"
#include "rosidl_typesupport_fastrtps_cpp/message_type_support.h"
#include "combat_robot_msgs/msg/rosidl_typesupport_fastrtps_c__visibility_control.h"
#include "combat_robot_msgs/msg/detail/swarm_follower_status__struct.h"
#include "combat_robot_msgs/msg/detail/swarm_follower_status__functions.h"
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

#include "std_msgs/msg/detail/header__functions.h"  // header

// forward declare type support functions

ROSIDL_TYPESUPPORT_FASTRTPS_C_IMPORT_combat_robot_msgs
bool cdr_serialize_std_msgs__msg__Header(
  const std_msgs__msg__Header * ros_message,
  eprosima::fastcdr::Cdr & cdr);

ROSIDL_TYPESUPPORT_FASTRTPS_C_IMPORT_combat_robot_msgs
bool cdr_deserialize_std_msgs__msg__Header(
  eprosima::fastcdr::Cdr & cdr,
  std_msgs__msg__Header * ros_message);

ROSIDL_TYPESUPPORT_FASTRTPS_C_IMPORT_combat_robot_msgs
size_t get_serialized_size_std_msgs__msg__Header(
  const void * untyped_ros_message,
  size_t current_alignment);

ROSIDL_TYPESUPPORT_FASTRTPS_C_IMPORT_combat_robot_msgs
size_t max_serialized_size_std_msgs__msg__Header(
  bool & full_bounded,
  bool & is_plain,
  size_t current_alignment);

ROSIDL_TYPESUPPORT_FASTRTPS_C_IMPORT_combat_robot_msgs
bool cdr_serialize_key_std_msgs__msg__Header(
  const std_msgs__msg__Header * ros_message,
  eprosima::fastcdr::Cdr & cdr);

ROSIDL_TYPESUPPORT_FASTRTPS_C_IMPORT_combat_robot_msgs
size_t get_serialized_size_key_std_msgs__msg__Header(
  const void * untyped_ros_message,
  size_t current_alignment);

ROSIDL_TYPESUPPORT_FASTRTPS_C_IMPORT_combat_robot_msgs
size_t max_serialized_size_key_std_msgs__msg__Header(
  bool & full_bounded,
  bool & is_plain,
  size_t current_alignment);

ROSIDL_TYPESUPPORT_FASTRTPS_C_IMPORT_combat_robot_msgs
const rosidl_message_type_support_t *
  ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(rosidl_typesupport_fastrtps_c, std_msgs, msg, Header)();


using _SwarmFollowerStatus__ros_msg_type = combat_robot_msgs__msg__SwarmFollowerStatus;


ROSIDL_TYPESUPPORT_FASTRTPS_C_PUBLIC_combat_robot_msgs
bool cdr_serialize_combat_robot_msgs__msg__SwarmFollowerStatus(
  const combat_robot_msgs__msg__SwarmFollowerStatus * ros_message,
  eprosima::fastcdr::Cdr & cdr)
{
  // Field name: header
  {
    cdr_serialize_std_msgs__msg__Header(
      &ros_message->header, cdr);
  }

  // Field name: robot_id
  {
    cdr << ros_message->robot_id;
  }

  // Field name: leader_robot_id
  {
    cdr << ros_message->leader_robot_id;
  }

  // Field name: link_status
  {
    cdr << ros_message->link_status;
  }

  // Field name: last_heartbeat_sequence
  {
    cdr << ros_message->last_heartbeat_sequence;
  }

  // Field name: heartbeat_age_sec
  {
    cdr << ros_message->heartbeat_age_sec;
  }

  // Field name: last_operation_mode
  {
    cdr << ros_message->last_operation_mode;
  }

  // Field name: last_formation_type
  {
    cdr << ros_message->last_formation_type;
  }

  // Field name: last_formation_number
  {
    cdr << ros_message->last_formation_number;
  }

  // Field name: last_grouping_index
  {
    cdr << ros_message->last_grouping_index;
  }

  // Field name: latitude
  {
    cdr << ros_message->latitude;
  }

  // Field name: longitude
  {
    cdr << ros_message->longitude;
  }

  // Field name: heading_deg
  {
    cdr << ros_message->heading_deg;
  }

  // Field name: ground_speed_mps
  {
    cdr << ros_message->ground_speed_mps;
  }

  return true;
}

ROSIDL_TYPESUPPORT_FASTRTPS_C_PUBLIC_combat_robot_msgs
bool cdr_deserialize_combat_robot_msgs__msg__SwarmFollowerStatus(
  eprosima::fastcdr::Cdr & cdr,
  combat_robot_msgs__msg__SwarmFollowerStatus * ros_message)
{
  // Field name: header
  {
    cdr_deserialize_std_msgs__msg__Header(cdr, &ros_message->header);
  }

  // Field name: robot_id
  {
    cdr >> ros_message->robot_id;
  }

  // Field name: leader_robot_id
  {
    cdr >> ros_message->leader_robot_id;
  }

  // Field name: link_status
  {
    cdr >> ros_message->link_status;
  }

  // Field name: last_heartbeat_sequence
  {
    cdr >> ros_message->last_heartbeat_sequence;
  }

  // Field name: heartbeat_age_sec
  {
    cdr >> ros_message->heartbeat_age_sec;
  }

  // Field name: last_operation_mode
  {
    cdr >> ros_message->last_operation_mode;
  }

  // Field name: last_formation_type
  {
    cdr >> ros_message->last_formation_type;
  }

  // Field name: last_formation_number
  {
    cdr >> ros_message->last_formation_number;
  }

  // Field name: last_grouping_index
  {
    cdr >> ros_message->last_grouping_index;
  }

  // Field name: latitude
  {
    cdr >> ros_message->latitude;
  }

  // Field name: longitude
  {
    cdr >> ros_message->longitude;
  }

  // Field name: heading_deg
  {
    cdr >> ros_message->heading_deg;
  }

  // Field name: ground_speed_mps
  {
    cdr >> ros_message->ground_speed_mps;
  }

  return true;
}  // NOLINT(readability/fn_size)


ROSIDL_TYPESUPPORT_FASTRTPS_C_PUBLIC_combat_robot_msgs
size_t get_serialized_size_combat_robot_msgs__msg__SwarmFollowerStatus(
  const void * untyped_ros_message,
  size_t current_alignment)
{
  const _SwarmFollowerStatus__ros_msg_type * ros_message = static_cast<const _SwarmFollowerStatus__ros_msg_type *>(untyped_ros_message);
  (void)ros_message;
  size_t initial_alignment = current_alignment;

  const size_t padding = 4;
  const size_t wchar_size = 4;
  (void)padding;
  (void)wchar_size;

  // Field name: header
  current_alignment += get_serialized_size_std_msgs__msg__Header(
    &(ros_message->header), current_alignment);

  // Field name: robot_id
  {
    size_t item_size = sizeof(ros_message->robot_id);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: leader_robot_id
  {
    size_t item_size = sizeof(ros_message->leader_robot_id);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: link_status
  {
    size_t item_size = sizeof(ros_message->link_status);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: last_heartbeat_sequence
  {
    size_t item_size = sizeof(ros_message->last_heartbeat_sequence);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: heartbeat_age_sec
  {
    size_t item_size = sizeof(ros_message->heartbeat_age_sec);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: last_operation_mode
  {
    size_t item_size = sizeof(ros_message->last_operation_mode);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: last_formation_type
  {
    size_t item_size = sizeof(ros_message->last_formation_type);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: last_formation_number
  {
    size_t item_size = sizeof(ros_message->last_formation_number);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: last_grouping_index
  {
    size_t item_size = sizeof(ros_message->last_grouping_index);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: latitude
  {
    size_t item_size = sizeof(ros_message->latitude);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: longitude
  {
    size_t item_size = sizeof(ros_message->longitude);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: heading_deg
  {
    size_t item_size = sizeof(ros_message->heading_deg);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: ground_speed_mps
  {
    size_t item_size = sizeof(ros_message->ground_speed_mps);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  return current_alignment - initial_alignment;
}


ROSIDL_TYPESUPPORT_FASTRTPS_C_PUBLIC_combat_robot_msgs
size_t max_serialized_size_combat_robot_msgs__msg__SwarmFollowerStatus(
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

  // Field name: header
  {
    size_t array_size = 1;
    last_member_size = 0;
    for (size_t index = 0; index < array_size; ++index) {
      bool inner_full_bounded;
      bool inner_is_plain;
      size_t inner_size;
      inner_size =
        max_serialized_size_std_msgs__msg__Header(
        inner_full_bounded, inner_is_plain, current_alignment);
      last_member_size += inner_size;
      current_alignment += inner_size;
      full_bounded &= inner_full_bounded;
      is_plain &= inner_is_plain;
    }
  }

  // Field name: robot_id
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint32_t);
    current_alignment += array_size * sizeof(uint32_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint32_t));
  }

  // Field name: leader_robot_id
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint32_t);
    current_alignment += array_size * sizeof(uint32_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint32_t));
  }

  // Field name: link_status
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint8_t);
    current_alignment += array_size * sizeof(uint8_t);
  }

  // Field name: last_heartbeat_sequence
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint32_t);
    current_alignment += array_size * sizeof(uint32_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint32_t));
  }

  // Field name: heartbeat_age_sec
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint32_t);
    current_alignment += array_size * sizeof(uint32_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint32_t));
  }

  // Field name: last_operation_mode
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint8_t);
    current_alignment += array_size * sizeof(uint8_t);
  }

  // Field name: last_formation_type
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint8_t);
    current_alignment += array_size * sizeof(uint8_t);
  }

  // Field name: last_formation_number
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint8_t);
    current_alignment += array_size * sizeof(uint8_t);
  }

  // Field name: last_grouping_index
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint8_t);
    current_alignment += array_size * sizeof(uint8_t);
  }

  // Field name: latitude
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint64_t);
    current_alignment += array_size * sizeof(uint64_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint64_t));
  }

  // Field name: longitude
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint64_t);
    current_alignment += array_size * sizeof(uint64_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint64_t));
  }

  // Field name: heading_deg
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint32_t);
    current_alignment += array_size * sizeof(uint32_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint32_t));
  }

  // Field name: ground_speed_mps
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
    using DataType = combat_robot_msgs__msg__SwarmFollowerStatus;
    is_plain =
      (
      offsetof(DataType, ground_speed_mps) +
      last_member_size
      ) == ret_val;
  }
  return ret_val;
}

ROSIDL_TYPESUPPORT_FASTRTPS_C_PUBLIC_combat_robot_msgs
bool cdr_serialize_key_combat_robot_msgs__msg__SwarmFollowerStatus(
  const combat_robot_msgs__msg__SwarmFollowerStatus * ros_message,
  eprosima::fastcdr::Cdr & cdr)
{
  // Field name: header
  {
    cdr_serialize_key_std_msgs__msg__Header(
      &ros_message->header, cdr);
  }

  // Field name: robot_id
  {
    cdr << ros_message->robot_id;
  }

  // Field name: leader_robot_id
  {
    cdr << ros_message->leader_robot_id;
  }

  // Field name: link_status
  {
    cdr << ros_message->link_status;
  }

  // Field name: last_heartbeat_sequence
  {
    cdr << ros_message->last_heartbeat_sequence;
  }

  // Field name: heartbeat_age_sec
  {
    cdr << ros_message->heartbeat_age_sec;
  }

  // Field name: last_operation_mode
  {
    cdr << ros_message->last_operation_mode;
  }

  // Field name: last_formation_type
  {
    cdr << ros_message->last_formation_type;
  }

  // Field name: last_formation_number
  {
    cdr << ros_message->last_formation_number;
  }

  // Field name: last_grouping_index
  {
    cdr << ros_message->last_grouping_index;
  }

  // Field name: latitude
  {
    cdr << ros_message->latitude;
  }

  // Field name: longitude
  {
    cdr << ros_message->longitude;
  }

  // Field name: heading_deg
  {
    cdr << ros_message->heading_deg;
  }

  // Field name: ground_speed_mps
  {
    cdr << ros_message->ground_speed_mps;
  }

  return true;
}

ROSIDL_TYPESUPPORT_FASTRTPS_C_PUBLIC_combat_robot_msgs
size_t get_serialized_size_key_combat_robot_msgs__msg__SwarmFollowerStatus(
  const void * untyped_ros_message,
  size_t current_alignment)
{
  const _SwarmFollowerStatus__ros_msg_type * ros_message = static_cast<const _SwarmFollowerStatus__ros_msg_type *>(untyped_ros_message);
  (void)ros_message;

  size_t initial_alignment = current_alignment;

  const size_t padding = 4;
  const size_t wchar_size = 4;
  (void)padding;
  (void)wchar_size;

  // Field name: header
  current_alignment += get_serialized_size_key_std_msgs__msg__Header(
    &(ros_message->header), current_alignment);

  // Field name: robot_id
  {
    size_t item_size = sizeof(ros_message->robot_id);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: leader_robot_id
  {
    size_t item_size = sizeof(ros_message->leader_robot_id);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: link_status
  {
    size_t item_size = sizeof(ros_message->link_status);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: last_heartbeat_sequence
  {
    size_t item_size = sizeof(ros_message->last_heartbeat_sequence);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: heartbeat_age_sec
  {
    size_t item_size = sizeof(ros_message->heartbeat_age_sec);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: last_operation_mode
  {
    size_t item_size = sizeof(ros_message->last_operation_mode);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: last_formation_type
  {
    size_t item_size = sizeof(ros_message->last_formation_type);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: last_formation_number
  {
    size_t item_size = sizeof(ros_message->last_formation_number);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: last_grouping_index
  {
    size_t item_size = sizeof(ros_message->last_grouping_index);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: latitude
  {
    size_t item_size = sizeof(ros_message->latitude);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: longitude
  {
    size_t item_size = sizeof(ros_message->longitude);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: heading_deg
  {
    size_t item_size = sizeof(ros_message->heading_deg);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: ground_speed_mps
  {
    size_t item_size = sizeof(ros_message->ground_speed_mps);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  return current_alignment - initial_alignment;
}

ROSIDL_TYPESUPPORT_FASTRTPS_C_PUBLIC_combat_robot_msgs
size_t max_serialized_size_key_combat_robot_msgs__msg__SwarmFollowerStatus(
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
  // Field name: header
  {
    size_t array_size = 1;
    last_member_size = 0;
    for (size_t index = 0; index < array_size; ++index) {
      bool inner_full_bounded;
      bool inner_is_plain;
      size_t inner_size;
      inner_size =
        max_serialized_size_key_std_msgs__msg__Header(
        inner_full_bounded, inner_is_plain, current_alignment);
      last_member_size += inner_size;
      current_alignment += inner_size;
      full_bounded &= inner_full_bounded;
      is_plain &= inner_is_plain;
    }
  }

  // Field name: robot_id
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint32_t);
    current_alignment += array_size * sizeof(uint32_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint32_t));
  }

  // Field name: leader_robot_id
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint32_t);
    current_alignment += array_size * sizeof(uint32_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint32_t));
  }

  // Field name: link_status
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint8_t);
    current_alignment += array_size * sizeof(uint8_t);
  }

  // Field name: last_heartbeat_sequence
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint32_t);
    current_alignment += array_size * sizeof(uint32_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint32_t));
  }

  // Field name: heartbeat_age_sec
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint32_t);
    current_alignment += array_size * sizeof(uint32_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint32_t));
  }

  // Field name: last_operation_mode
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint8_t);
    current_alignment += array_size * sizeof(uint8_t);
  }

  // Field name: last_formation_type
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint8_t);
    current_alignment += array_size * sizeof(uint8_t);
  }

  // Field name: last_formation_number
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint8_t);
    current_alignment += array_size * sizeof(uint8_t);
  }

  // Field name: last_grouping_index
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint8_t);
    current_alignment += array_size * sizeof(uint8_t);
  }

  // Field name: latitude
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint64_t);
    current_alignment += array_size * sizeof(uint64_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint64_t));
  }

  // Field name: longitude
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint64_t);
    current_alignment += array_size * sizeof(uint64_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint64_t));
  }

  // Field name: heading_deg
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint32_t);
    current_alignment += array_size * sizeof(uint32_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint32_t));
  }

  // Field name: ground_speed_mps
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
    using DataType = combat_robot_msgs__msg__SwarmFollowerStatus;
    is_plain =
      (
      offsetof(DataType, ground_speed_mps) +
      last_member_size
      ) == ret_val;
  }
  return ret_val;
}


static bool _SwarmFollowerStatus__cdr_serialize(
  const void * untyped_ros_message,
  eprosima::fastcdr::Cdr & cdr)
{
  if (!untyped_ros_message) {
    fprintf(stderr, "ros message handle is null\n");
    return false;
  }
  const combat_robot_msgs__msg__SwarmFollowerStatus * ros_message = static_cast<const combat_robot_msgs__msg__SwarmFollowerStatus *>(untyped_ros_message);
  (void)ros_message;
  return cdr_serialize_combat_robot_msgs__msg__SwarmFollowerStatus(ros_message, cdr);
}

static bool _SwarmFollowerStatus__cdr_deserialize(
  eprosima::fastcdr::Cdr & cdr,
  void * untyped_ros_message)
{
  if (!untyped_ros_message) {
    fprintf(stderr, "ros message handle is null\n");
    return false;
  }
  combat_robot_msgs__msg__SwarmFollowerStatus * ros_message = static_cast<combat_robot_msgs__msg__SwarmFollowerStatus *>(untyped_ros_message);
  (void)ros_message;
  return cdr_deserialize_combat_robot_msgs__msg__SwarmFollowerStatus(cdr, ros_message);
}

static uint32_t _SwarmFollowerStatus__get_serialized_size(const void * untyped_ros_message)
{
  return static_cast<uint32_t>(
    get_serialized_size_combat_robot_msgs__msg__SwarmFollowerStatus(
      untyped_ros_message, 0));
}

static size_t _SwarmFollowerStatus__max_serialized_size(char & bounds_info)
{
  bool full_bounded;
  bool is_plain;
  size_t ret_val;

  ret_val = max_serialized_size_combat_robot_msgs__msg__SwarmFollowerStatus(
    full_bounded, is_plain, 0);

  bounds_info =
    is_plain ? ROSIDL_TYPESUPPORT_FASTRTPS_PLAIN_TYPE :
    full_bounded ? ROSIDL_TYPESUPPORT_FASTRTPS_BOUNDED_TYPE : ROSIDL_TYPESUPPORT_FASTRTPS_UNBOUNDED_TYPE;
  return ret_val;
}


static message_type_support_callbacks_t __callbacks_SwarmFollowerStatus = {
  "combat_robot_msgs::msg",
  "SwarmFollowerStatus",
  _SwarmFollowerStatus__cdr_serialize,
  _SwarmFollowerStatus__cdr_deserialize,
  _SwarmFollowerStatus__get_serialized_size,
  _SwarmFollowerStatus__max_serialized_size,
  nullptr
};

static rosidl_message_type_support_t _SwarmFollowerStatus__type_support = {
  rosidl_typesupport_fastrtps_c__identifier,
  &__callbacks_SwarmFollowerStatus,
  get_message_typesupport_handle_function,
  &combat_robot_msgs__msg__SwarmFollowerStatus__get_type_hash,
  &combat_robot_msgs__msg__SwarmFollowerStatus__get_type_description,
  &combat_robot_msgs__msg__SwarmFollowerStatus__get_type_description_sources,
};

const rosidl_message_type_support_t *
ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(rosidl_typesupport_fastrtps_c, combat_robot_msgs, msg, SwarmFollowerStatus)() {
  return &_SwarmFollowerStatus__type_support;
}

#if defined(__cplusplus)
}
#endif
