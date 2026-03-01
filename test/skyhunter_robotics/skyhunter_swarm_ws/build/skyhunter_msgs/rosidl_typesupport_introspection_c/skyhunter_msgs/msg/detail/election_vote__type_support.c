// generated from rosidl_typesupport_introspection_c/resource/idl__type_support.c.em
// with input from skyhunter_msgs:msg/ElectionVote.idl
// generated code does not contain a copyright notice

#include <stddef.h>
#include "skyhunter_msgs/msg/detail/election_vote__rosidl_typesupport_introspection_c.h"
#include "skyhunter_msgs/msg/rosidl_typesupport_introspection_c__visibility_control.h"
#include "rosidl_typesupport_introspection_c/field_types.h"
#include "rosidl_typesupport_introspection_c/identifier.h"
#include "rosidl_typesupport_introspection_c/message_introspection.h"
#include "skyhunter_msgs/msg/detail/election_vote__functions.h"
#include "skyhunter_msgs/msg/detail/election_vote__struct.h"


// Include directives for member types
// Member `candidate_id`
// Member `voter_id`
#include "rosidl_runtime_c/string_functions.h"

#ifdef __cplusplus
extern "C"
{
#endif

void skyhunter_msgs__msg__ElectionVote__rosidl_typesupport_introspection_c__ElectionVote_init_function(
  void * message_memory, enum rosidl_runtime_c__message_initialization _init)
{
  // TODO(karsten1987): initializers are not yet implemented for typesupport c
  // see https://github.com/ros2/ros2/issues/397
  (void) _init;
  skyhunter_msgs__msg__ElectionVote__init(message_memory);
}

void skyhunter_msgs__msg__ElectionVote__rosidl_typesupport_introspection_c__ElectionVote_fini_function(void * message_memory)
{
  skyhunter_msgs__msg__ElectionVote__fini(message_memory);
}

static rosidl_typesupport_introspection_c__MessageMember skyhunter_msgs__msg__ElectionVote__rosidl_typesupport_introspection_c__ElectionVote_message_member_array[4] = {
  {
    "term",  // name
    rosidl_typesupport_introspection_c__ROS_TYPE_UINT32,  // type
    0,  // upper bound of string
    NULL,  // members of sub message
    false,  // is array
    0,  // array size
    false,  // is upper bound
    offsetof(skyhunter_msgs__msg__ElectionVote, term),  // bytes offset in struct
    NULL,  // default value
    NULL,  // size() function pointer
    NULL,  // get_const(index) function pointer
    NULL,  // get(index) function pointer
    NULL,  // fetch(index, &value) function pointer
    NULL,  // assign(index, value) function pointer
    NULL  // resize(index) function pointer
  },
  {
    "candidate_id",  // name
    rosidl_typesupport_introspection_c__ROS_TYPE_STRING,  // type
    0,  // upper bound of string
    NULL,  // members of sub message
    false,  // is array
    0,  // array size
    false,  // is upper bound
    offsetof(skyhunter_msgs__msg__ElectionVote, candidate_id),  // bytes offset in struct
    NULL,  // default value
    NULL,  // size() function pointer
    NULL,  // get_const(index) function pointer
    NULL,  // get(index) function pointer
    NULL,  // fetch(index, &value) function pointer
    NULL,  // assign(index, value) function pointer
    NULL  // resize(index) function pointer
  },
  {
    "voter_id",  // name
    rosidl_typesupport_introspection_c__ROS_TYPE_STRING,  // type
    0,  // upper bound of string
    NULL,  // members of sub message
    false,  // is array
    0,  // array size
    false,  // is upper bound
    offsetof(skyhunter_msgs__msg__ElectionVote, voter_id),  // bytes offset in struct
    NULL,  // default value
    NULL,  // size() function pointer
    NULL,  // get_const(index) function pointer
    NULL,  // get(index) function pointer
    NULL,  // fetch(index, &value) function pointer
    NULL,  // assign(index, value) function pointer
    NULL  // resize(index) function pointer
  },
  {
    "fitness_score",  // name
    rosidl_typesupport_introspection_c__ROS_TYPE_FLOAT,  // type
    0,  // upper bound of string
    NULL,  // members of sub message
    false,  // is array
    0,  // array size
    false,  // is upper bound
    offsetof(skyhunter_msgs__msg__ElectionVote, fitness_score),  // bytes offset in struct
    NULL,  // default value
    NULL,  // size() function pointer
    NULL,  // get_const(index) function pointer
    NULL,  // get(index) function pointer
    NULL,  // fetch(index, &value) function pointer
    NULL,  // assign(index, value) function pointer
    NULL  // resize(index) function pointer
  }
};

static const rosidl_typesupport_introspection_c__MessageMembers skyhunter_msgs__msg__ElectionVote__rosidl_typesupport_introspection_c__ElectionVote_message_members = {
  "skyhunter_msgs__msg",  // message namespace
  "ElectionVote",  // message name
  4,  // number of fields
  sizeof(skyhunter_msgs__msg__ElectionVote),
  skyhunter_msgs__msg__ElectionVote__rosidl_typesupport_introspection_c__ElectionVote_message_member_array,  // message members
  skyhunter_msgs__msg__ElectionVote__rosidl_typesupport_introspection_c__ElectionVote_init_function,  // function to initialize message memory (memory has to be allocated)
  skyhunter_msgs__msg__ElectionVote__rosidl_typesupport_introspection_c__ElectionVote_fini_function  // function to terminate message instance (will not free memory)
};

// this is not const since it must be initialized on first access
// since C does not allow non-integral compile-time constants
static rosidl_message_type_support_t skyhunter_msgs__msg__ElectionVote__rosidl_typesupport_introspection_c__ElectionVote_message_type_support_handle = {
  0,
  &skyhunter_msgs__msg__ElectionVote__rosidl_typesupport_introspection_c__ElectionVote_message_members,
  get_message_typesupport_handle_function,
};

ROSIDL_TYPESUPPORT_INTROSPECTION_C_EXPORT_skyhunter_msgs
const rosidl_message_type_support_t *
ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(rosidl_typesupport_introspection_c, skyhunter_msgs, msg, ElectionVote)() {
  if (!skyhunter_msgs__msg__ElectionVote__rosidl_typesupport_introspection_c__ElectionVote_message_type_support_handle.typesupport_identifier) {
    skyhunter_msgs__msg__ElectionVote__rosidl_typesupport_introspection_c__ElectionVote_message_type_support_handle.typesupport_identifier =
      rosidl_typesupport_introspection_c__identifier;
  }
  return &skyhunter_msgs__msg__ElectionVote__rosidl_typesupport_introspection_c__ElectionVote_message_type_support_handle;
}
#ifdef __cplusplus
}
#endif
