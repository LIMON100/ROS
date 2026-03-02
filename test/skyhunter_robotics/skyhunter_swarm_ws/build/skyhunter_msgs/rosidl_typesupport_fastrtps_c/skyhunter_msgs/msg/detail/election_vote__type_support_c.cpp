// generated from rosidl_typesupport_fastrtps_c/resource/idl__type_support_c.cpp.em
// with input from skyhunter_msgs:msg/ElectionVote.idl
// generated code does not contain a copyright notice
#include "skyhunter_msgs/msg/detail/election_vote__rosidl_typesupport_fastrtps_c.h"


#include <cassert>
#include <limits>
#include <string>
#include "rosidl_typesupport_fastrtps_c/identifier.h"
#include "rosidl_typesupport_fastrtps_c/wstring_conversion.hpp"
#include "rosidl_typesupport_fastrtps_cpp/message_type_support.h"
#include "skyhunter_msgs/msg/rosidl_typesupport_fastrtps_c__visibility_control.h"
#include "skyhunter_msgs/msg/detail/election_vote__struct.h"
#include "skyhunter_msgs/msg/detail/election_vote__functions.h"
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

#include "rosidl_runtime_c/string.h"  // candidate_id, voter_id
#include "rosidl_runtime_c/string_functions.h"  // candidate_id, voter_id

// forward declare type support functions


using _ElectionVote__ros_msg_type = skyhunter_msgs__msg__ElectionVote;

static bool _ElectionVote__cdr_serialize(
  const void * untyped_ros_message,
  eprosima::fastcdr::Cdr & cdr)
{
  if (!untyped_ros_message) {
    fprintf(stderr, "ros message handle is null\n");
    return false;
  }
  const _ElectionVote__ros_msg_type * ros_message = static_cast<const _ElectionVote__ros_msg_type *>(untyped_ros_message);
  // Field name: term
  {
    cdr << ros_message->term;
  }

  // Field name: candidate_id
  {
    const rosidl_runtime_c__String * str = &ros_message->candidate_id;
    if (str->capacity == 0 || str->capacity <= str->size) {
      fprintf(stderr, "string capacity not greater than size\n");
      return false;
    }
    if (str->data[str->size] != '\0') {
      fprintf(stderr, "string not null-terminated\n");
      return false;
    }
    cdr << str->data;
  }

  // Field name: voter_id
  {
    const rosidl_runtime_c__String * str = &ros_message->voter_id;
    if (str->capacity == 0 || str->capacity <= str->size) {
      fprintf(stderr, "string capacity not greater than size\n");
      return false;
    }
    if (str->data[str->size] != '\0') {
      fprintf(stderr, "string not null-terminated\n");
      return false;
    }
    cdr << str->data;
  }

  // Field name: fitness_score
  {
    cdr << ros_message->fitness_score;
  }

  return true;
}

static bool _ElectionVote__cdr_deserialize(
  eprosima::fastcdr::Cdr & cdr,
  void * untyped_ros_message)
{
  if (!untyped_ros_message) {
    fprintf(stderr, "ros message handle is null\n");
    return false;
  }
  _ElectionVote__ros_msg_type * ros_message = static_cast<_ElectionVote__ros_msg_type *>(untyped_ros_message);
  // Field name: term
  {
    cdr >> ros_message->term;
  }

  // Field name: candidate_id
  {
    std::string tmp;
    cdr >> tmp;
    if (!ros_message->candidate_id.data) {
      rosidl_runtime_c__String__init(&ros_message->candidate_id);
    }
    bool succeeded = rosidl_runtime_c__String__assign(
      &ros_message->candidate_id,
      tmp.c_str());
    if (!succeeded) {
      fprintf(stderr, "failed to assign string into field 'candidate_id'\n");
      return false;
    }
  }

  // Field name: voter_id
  {
    std::string tmp;
    cdr >> tmp;
    if (!ros_message->voter_id.data) {
      rosidl_runtime_c__String__init(&ros_message->voter_id);
    }
    bool succeeded = rosidl_runtime_c__String__assign(
      &ros_message->voter_id,
      tmp.c_str());
    if (!succeeded) {
      fprintf(stderr, "failed to assign string into field 'voter_id'\n");
      return false;
    }
  }

  // Field name: fitness_score
  {
    cdr >> ros_message->fitness_score;
  }

  return true;
}  // NOLINT(readability/fn_size)

ROSIDL_TYPESUPPORT_FASTRTPS_C_PUBLIC_skyhunter_msgs
size_t get_serialized_size_skyhunter_msgs__msg__ElectionVote(
  const void * untyped_ros_message,
  size_t current_alignment)
{
  const _ElectionVote__ros_msg_type * ros_message = static_cast<const _ElectionVote__ros_msg_type *>(untyped_ros_message);
  (void)ros_message;
  size_t initial_alignment = current_alignment;

  const size_t padding = 4;
  const size_t wchar_size = 4;
  (void)padding;
  (void)wchar_size;

  // field.name term
  {
    size_t item_size = sizeof(ros_message->term);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }
  // field.name candidate_id
  current_alignment += padding +
    eprosima::fastcdr::Cdr::alignment(current_alignment, padding) +
    (ros_message->candidate_id.size + 1);
  // field.name voter_id
  current_alignment += padding +
    eprosima::fastcdr::Cdr::alignment(current_alignment, padding) +
    (ros_message->voter_id.size + 1);
  // field.name fitness_score
  {
    size_t item_size = sizeof(ros_message->fitness_score);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  return current_alignment - initial_alignment;
}

static uint32_t _ElectionVote__get_serialized_size(const void * untyped_ros_message)
{
  return static_cast<uint32_t>(
    get_serialized_size_skyhunter_msgs__msg__ElectionVote(
      untyped_ros_message, 0));
}

ROSIDL_TYPESUPPORT_FASTRTPS_C_PUBLIC_skyhunter_msgs
size_t max_serialized_size_skyhunter_msgs__msg__ElectionVote(
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

  // member: term
  {
    size_t array_size = 1;

    last_member_size = array_size * sizeof(uint32_t);
    current_alignment += array_size * sizeof(uint32_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint32_t));
  }
  // member: candidate_id
  {
    size_t array_size = 1;

    full_bounded = false;
    is_plain = false;
    for (size_t index = 0; index < array_size; ++index) {
      current_alignment += padding +
        eprosima::fastcdr::Cdr::alignment(current_alignment, padding) +
        1;
    }
  }
  // member: voter_id
  {
    size_t array_size = 1;

    full_bounded = false;
    is_plain = false;
    for (size_t index = 0; index < array_size; ++index) {
      current_alignment += padding +
        eprosima::fastcdr::Cdr::alignment(current_alignment, padding) +
        1;
    }
  }
  // member: fitness_score
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
    using DataType = skyhunter_msgs__msg__ElectionVote;
    is_plain =
      (
      offsetof(DataType, fitness_score) +
      last_member_size
      ) == ret_val;
  }

  return ret_val;
}

static size_t _ElectionVote__max_serialized_size(char & bounds_info)
{
  bool full_bounded;
  bool is_plain;
  size_t ret_val;

  ret_val = max_serialized_size_skyhunter_msgs__msg__ElectionVote(
    full_bounded, is_plain, 0);

  bounds_info =
    is_plain ? ROSIDL_TYPESUPPORT_FASTRTPS_PLAIN_TYPE :
    full_bounded ? ROSIDL_TYPESUPPORT_FASTRTPS_BOUNDED_TYPE : ROSIDL_TYPESUPPORT_FASTRTPS_UNBOUNDED_TYPE;
  return ret_val;
}


static message_type_support_callbacks_t __callbacks_ElectionVote = {
  "skyhunter_msgs::msg",
  "ElectionVote",
  _ElectionVote__cdr_serialize,
  _ElectionVote__cdr_deserialize,
  _ElectionVote__get_serialized_size,
  _ElectionVote__max_serialized_size
};

static rosidl_message_type_support_t _ElectionVote__type_support = {
  rosidl_typesupport_fastrtps_c__identifier,
  &__callbacks_ElectionVote,
  get_message_typesupport_handle_function,
};

const rosidl_message_type_support_t *
ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(rosidl_typesupport_fastrtps_c, skyhunter_msgs, msg, ElectionVote)() {
  return &_ElectionVote__type_support;
}

#if defined(__cplusplus)
}
#endif
