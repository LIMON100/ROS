// generated from rosidl_typesupport_fastrtps_cpp/resource/idl__type_support.cpp.em
// with input from skyhunter_msgs:msg/ElectionVote.idl
// generated code does not contain a copyright notice
#include "skyhunter_msgs/msg/detail/election_vote__rosidl_typesupport_fastrtps_cpp.hpp"
#include "skyhunter_msgs/msg/detail/election_vote__struct.hpp"

#include <limits>
#include <stdexcept>
#include <string>
#include "rosidl_typesupport_cpp/message_type_support.hpp"
#include "rosidl_typesupport_fastrtps_cpp/identifier.hpp"
#include "rosidl_typesupport_fastrtps_cpp/message_type_support.h"
#include "rosidl_typesupport_fastrtps_cpp/message_type_support_decl.hpp"
#include "rosidl_typesupport_fastrtps_cpp/wstring_conversion.hpp"
#include "fastcdr/Cdr.h"


// forward declaration of message dependencies and their conversion functions

namespace skyhunter_msgs
{

namespace msg
{

namespace typesupport_fastrtps_cpp
{

bool
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_skyhunter_msgs
cdr_serialize(
  const skyhunter_msgs::msg::ElectionVote & ros_message,
  eprosima::fastcdr::Cdr & cdr)
{
  // Member: term
  cdr << ros_message.term;
  // Member: candidate_id
  cdr << ros_message.candidate_id;
  // Member: voter_id
  cdr << ros_message.voter_id;
  // Member: fitness_score
  cdr << ros_message.fitness_score;
  return true;
}

bool
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_skyhunter_msgs
cdr_deserialize(
  eprosima::fastcdr::Cdr & cdr,
  skyhunter_msgs::msg::ElectionVote & ros_message)
{
  // Member: term
  cdr >> ros_message.term;

  // Member: candidate_id
  cdr >> ros_message.candidate_id;

  // Member: voter_id
  cdr >> ros_message.voter_id;

  // Member: fitness_score
  cdr >> ros_message.fitness_score;

  return true;
}  // NOLINT(readability/fn_size)

size_t
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_skyhunter_msgs
get_serialized_size(
  const skyhunter_msgs::msg::ElectionVote & ros_message,
  size_t current_alignment)
{
  size_t initial_alignment = current_alignment;

  const size_t padding = 4;
  const size_t wchar_size = 4;
  (void)padding;
  (void)wchar_size;

  // Member: term
  {
    size_t item_size = sizeof(ros_message.term);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }
  // Member: candidate_id
  current_alignment += padding +
    eprosima::fastcdr::Cdr::alignment(current_alignment, padding) +
    (ros_message.candidate_id.size() + 1);
  // Member: voter_id
  current_alignment += padding +
    eprosima::fastcdr::Cdr::alignment(current_alignment, padding) +
    (ros_message.voter_id.size() + 1);
  // Member: fitness_score
  {
    size_t item_size = sizeof(ros_message.fitness_score);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  return current_alignment - initial_alignment;
}

size_t
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_skyhunter_msgs
max_serialized_size_ElectionVote(
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


  // Member: term
  {
    size_t array_size = 1;

    last_member_size = array_size * sizeof(uint32_t);
    current_alignment += array_size * sizeof(uint32_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint32_t));
  }

  // Member: candidate_id
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

  // Member: voter_id
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

  // Member: fitness_score
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
    using DataType = skyhunter_msgs::msg::ElectionVote;
    is_plain =
      (
      offsetof(DataType, fitness_score) +
      last_member_size
      ) == ret_val;
  }

  return ret_val;
}

static bool _ElectionVote__cdr_serialize(
  const void * untyped_ros_message,
  eprosima::fastcdr::Cdr & cdr)
{
  auto typed_message =
    static_cast<const skyhunter_msgs::msg::ElectionVote *>(
    untyped_ros_message);
  return cdr_serialize(*typed_message, cdr);
}

static bool _ElectionVote__cdr_deserialize(
  eprosima::fastcdr::Cdr & cdr,
  void * untyped_ros_message)
{
  auto typed_message =
    static_cast<skyhunter_msgs::msg::ElectionVote *>(
    untyped_ros_message);
  return cdr_deserialize(cdr, *typed_message);
}

static uint32_t _ElectionVote__get_serialized_size(
  const void * untyped_ros_message)
{
  auto typed_message =
    static_cast<const skyhunter_msgs::msg::ElectionVote *>(
    untyped_ros_message);
  return static_cast<uint32_t>(get_serialized_size(*typed_message, 0));
}

static size_t _ElectionVote__max_serialized_size(char & bounds_info)
{
  bool full_bounded;
  bool is_plain;
  size_t ret_val;

  ret_val = max_serialized_size_ElectionVote(full_bounded, is_plain, 0);

  bounds_info =
    is_plain ? ROSIDL_TYPESUPPORT_FASTRTPS_PLAIN_TYPE :
    full_bounded ? ROSIDL_TYPESUPPORT_FASTRTPS_BOUNDED_TYPE : ROSIDL_TYPESUPPORT_FASTRTPS_UNBOUNDED_TYPE;
  return ret_val;
}

static message_type_support_callbacks_t _ElectionVote__callbacks = {
  "skyhunter_msgs::msg",
  "ElectionVote",
  _ElectionVote__cdr_serialize,
  _ElectionVote__cdr_deserialize,
  _ElectionVote__get_serialized_size,
  _ElectionVote__max_serialized_size
};

static rosidl_message_type_support_t _ElectionVote__handle = {
  rosidl_typesupport_fastrtps_cpp::typesupport_identifier,
  &_ElectionVote__callbacks,
  get_message_typesupport_handle_function,
};

}  // namespace typesupport_fastrtps_cpp

}  // namespace msg

}  // namespace skyhunter_msgs

namespace rosidl_typesupport_fastrtps_cpp
{

template<>
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_EXPORT_skyhunter_msgs
const rosidl_message_type_support_t *
get_message_type_support_handle<skyhunter_msgs::msg::ElectionVote>()
{
  return &skyhunter_msgs::msg::typesupport_fastrtps_cpp::_ElectionVote__handle;
}

}  // namespace rosidl_typesupport_fastrtps_cpp

#ifdef __cplusplus
extern "C"
{
#endif

const rosidl_message_type_support_t *
ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(rosidl_typesupport_fastrtps_cpp, skyhunter_msgs, msg, ElectionVote)() {
  return &skyhunter_msgs::msg::typesupport_fastrtps_cpp::_ElectionVote__handle;
}

#ifdef __cplusplus
}
#endif
