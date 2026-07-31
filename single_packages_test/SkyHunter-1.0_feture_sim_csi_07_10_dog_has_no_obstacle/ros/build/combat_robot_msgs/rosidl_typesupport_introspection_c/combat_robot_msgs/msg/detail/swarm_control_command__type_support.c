// generated from rosidl_typesupport_introspection_c/resource/idl__type_support.c.em
// with input from combat_robot_msgs:msg/SwarmControlCommand.idl
// generated code does not contain a copyright notice

#include <stddef.h>
#include "combat_robot_msgs/msg/detail/swarm_control_command__rosidl_typesupport_introspection_c.h"
#include "combat_robot_msgs/msg/rosidl_typesupport_introspection_c__visibility_control.h"
#include "rosidl_typesupport_introspection_c/field_types.h"
#include "rosidl_typesupport_introspection_c/identifier.h"
#include "rosidl_typesupport_introspection_c/message_introspection.h"
#include "combat_robot_msgs/msg/detail/swarm_control_command__functions.h"
#include "combat_robot_msgs/msg/detail/swarm_control_command__struct.h"


// Include directives for member types
// Member `header`
#include "std_msgs/msg/header.h"
// Member `header`
#include "std_msgs/msg/detail/header__rosidl_typesupport_introspection_c.h"

#ifdef __cplusplus
extern "C"
{
#endif

void combat_robot_msgs__msg__SwarmControlCommand__rosidl_typesupport_introspection_c__SwarmControlCommand_init_function(
  void * message_memory, enum rosidl_runtime_c__message_initialization _init)
{
  // TODO(karsten1987): initializers are not yet implemented for typesupport c
  // see https://github.com/ros2/ros2/issues/397
  (void) _init;
  combat_robot_msgs__msg__SwarmControlCommand__init(message_memory);
}

void combat_robot_msgs__msg__SwarmControlCommand__rosidl_typesupport_introspection_c__SwarmControlCommand_fini_function(void * message_memory)
{
  combat_robot_msgs__msg__SwarmControlCommand__fini(message_memory);
}

size_t combat_robot_msgs__msg__SwarmControlCommand__rosidl_typesupport_introspection_c__size_function__SwarmControlCommand__selected_robot_ids(
  const void * untyped_member)
{
  (void)untyped_member;
  return 8;
}

const void * combat_robot_msgs__msg__SwarmControlCommand__rosidl_typesupport_introspection_c__get_const_function__SwarmControlCommand__selected_robot_ids(
  const void * untyped_member, size_t index)
{
  const uint32_t * member =
    (const uint32_t *)(untyped_member);
  return &member[index];
}

void * combat_robot_msgs__msg__SwarmControlCommand__rosidl_typesupport_introspection_c__get_function__SwarmControlCommand__selected_robot_ids(
  void * untyped_member, size_t index)
{
  uint32_t * member =
    (uint32_t *)(untyped_member);
  return &member[index];
}

void combat_robot_msgs__msg__SwarmControlCommand__rosidl_typesupport_introspection_c__fetch_function__SwarmControlCommand__selected_robot_ids(
  const void * untyped_member, size_t index, void * untyped_value)
{
  const uint32_t * item =
    ((const uint32_t *)
    combat_robot_msgs__msg__SwarmControlCommand__rosidl_typesupport_introspection_c__get_const_function__SwarmControlCommand__selected_robot_ids(untyped_member, index));
  uint32_t * value =
    (uint32_t *)(untyped_value);
  *value = *item;
}

void combat_robot_msgs__msg__SwarmControlCommand__rosidl_typesupport_introspection_c__assign_function__SwarmControlCommand__selected_robot_ids(
  void * untyped_member, size_t index, const void * untyped_value)
{
  uint32_t * item =
    ((uint32_t *)
    combat_robot_msgs__msg__SwarmControlCommand__rosidl_typesupport_introspection_c__get_function__SwarmControlCommand__selected_robot_ids(untyped_member, index));
  const uint32_t * value =
    (const uint32_t *)(untyped_value);
  *item = *value;
}

static rosidl_typesupport_introspection_c__MessageMember combat_robot_msgs__msg__SwarmControlCommand__rosidl_typesupport_introspection_c__SwarmControlCommand_message_member_array[6] = {
  {
    "header",  // name
    rosidl_typesupport_introspection_c__ROS_TYPE_MESSAGE,  // type
    0,  // upper bound of string
    NULL,  // members of sub message (initialized later)
    false,  // is key
    false,  // is array
    0,  // array size
    false,  // is upper bound
    offsetof(combat_robot_msgs__msg__SwarmControlCommand, header),  // bytes offset in struct
    NULL,  // default value
    NULL,  // size() function pointer
    NULL,  // get_const(index) function pointer
    NULL,  // get(index) function pointer
    NULL,  // fetch(index, &value) function pointer
    NULL,  // assign(index, value) function pointer
    NULL  // resize(index) function pointer
  },
  {
    "formation_type",  // name
    rosidl_typesupport_introspection_c__ROS_TYPE_UINT8,  // type
    0,  // upper bound of string
    NULL,  // members of sub message
    false,  // is key
    false,  // is array
    0,  // array size
    false,  // is upper bound
    offsetof(combat_robot_msgs__msg__SwarmControlCommand, formation_type),  // bytes offset in struct
    NULL,  // default value
    NULL,  // size() function pointer
    NULL,  // get_const(index) function pointer
    NULL,  // get(index) function pointer
    NULL,  // fetch(index, &value) function pointer
    NULL,  // assign(index, value) function pointer
    NULL  // resize(index) function pointer
  },
  {
    "formation_number",  // name
    rosidl_typesupport_introspection_c__ROS_TYPE_UINT8,  // type
    0,  // upper bound of string
    NULL,  // members of sub message
    false,  // is key
    false,  // is array
    0,  // array size
    false,  // is upper bound
    offsetof(combat_robot_msgs__msg__SwarmControlCommand, formation_number),  // bytes offset in struct
    NULL,  // default value
    NULL,  // size() function pointer
    NULL,  // get_const(index) function pointer
    NULL,  // get(index) function pointer
    NULL,  // fetch(index, &value) function pointer
    NULL,  // assign(index, value) function pointer
    NULL  // resize(index) function pointer
  },
  {
    "grouping_index",  // name
    rosidl_typesupport_introspection_c__ROS_TYPE_UINT8,  // type
    0,  // upper bound of string
    NULL,  // members of sub message
    false,  // is key
    false,  // is array
    0,  // array size
    false,  // is upper bound
    offsetof(combat_robot_msgs__msg__SwarmControlCommand, grouping_index),  // bytes offset in struct
    NULL,  // default value
    NULL,  // size() function pointer
    NULL,  // get_const(index) function pointer
    NULL,  // get(index) function pointer
    NULL,  // fetch(index, &value) function pointer
    NULL,  // assign(index, value) function pointer
    NULL  // resize(index) function pointer
  },
  {
    "selected_robot_count",  // name
    rosidl_typesupport_introspection_c__ROS_TYPE_UINT8,  // type
    0,  // upper bound of string
    NULL,  // members of sub message
    false,  // is key
    false,  // is array
    0,  // array size
    false,  // is upper bound
    offsetof(combat_robot_msgs__msg__SwarmControlCommand, selected_robot_count),  // bytes offset in struct
    NULL,  // default value
    NULL,  // size() function pointer
    NULL,  // get_const(index) function pointer
    NULL,  // get(index) function pointer
    NULL,  // fetch(index, &value) function pointer
    NULL,  // assign(index, value) function pointer
    NULL  // resize(index) function pointer
  },
  {
    "selected_robot_ids",  // name
    rosidl_typesupport_introspection_c__ROS_TYPE_UINT32,  // type
    0,  // upper bound of string
    NULL,  // members of sub message
    false,  // is key
    true,  // is array
    8,  // array size
    false,  // is upper bound
    offsetof(combat_robot_msgs__msg__SwarmControlCommand, selected_robot_ids),  // bytes offset in struct
    NULL,  // default value
    combat_robot_msgs__msg__SwarmControlCommand__rosidl_typesupport_introspection_c__size_function__SwarmControlCommand__selected_robot_ids,  // size() function pointer
    combat_robot_msgs__msg__SwarmControlCommand__rosidl_typesupport_introspection_c__get_const_function__SwarmControlCommand__selected_robot_ids,  // get_const(index) function pointer
    combat_robot_msgs__msg__SwarmControlCommand__rosidl_typesupport_introspection_c__get_function__SwarmControlCommand__selected_robot_ids,  // get(index) function pointer
    combat_robot_msgs__msg__SwarmControlCommand__rosidl_typesupport_introspection_c__fetch_function__SwarmControlCommand__selected_robot_ids,  // fetch(index, &value) function pointer
    combat_robot_msgs__msg__SwarmControlCommand__rosidl_typesupport_introspection_c__assign_function__SwarmControlCommand__selected_robot_ids,  // assign(index, value) function pointer
    NULL  // resize(index) function pointer
  }
};

static const rosidl_typesupport_introspection_c__MessageMembers combat_robot_msgs__msg__SwarmControlCommand__rosidl_typesupport_introspection_c__SwarmControlCommand_message_members = {
  "combat_robot_msgs__msg",  // message namespace
  "SwarmControlCommand",  // message name
  6,  // number of fields
  sizeof(combat_robot_msgs__msg__SwarmControlCommand),
  false,  // has_any_key_member_
  combat_robot_msgs__msg__SwarmControlCommand__rosidl_typesupport_introspection_c__SwarmControlCommand_message_member_array,  // message members
  combat_robot_msgs__msg__SwarmControlCommand__rosidl_typesupport_introspection_c__SwarmControlCommand_init_function,  // function to initialize message memory (memory has to be allocated)
  combat_robot_msgs__msg__SwarmControlCommand__rosidl_typesupport_introspection_c__SwarmControlCommand_fini_function  // function to terminate message instance (will not free memory)
};

// this is not const since it must be initialized on first access
// since C does not allow non-integral compile-time constants
static rosidl_message_type_support_t combat_robot_msgs__msg__SwarmControlCommand__rosidl_typesupport_introspection_c__SwarmControlCommand_message_type_support_handle = {
  0,
  &combat_robot_msgs__msg__SwarmControlCommand__rosidl_typesupport_introspection_c__SwarmControlCommand_message_members,
  get_message_typesupport_handle_function,
  &combat_robot_msgs__msg__SwarmControlCommand__get_type_hash,
  &combat_robot_msgs__msg__SwarmControlCommand__get_type_description,
  &combat_robot_msgs__msg__SwarmControlCommand__get_type_description_sources,
};

ROSIDL_TYPESUPPORT_INTROSPECTION_C_EXPORT_combat_robot_msgs
const rosidl_message_type_support_t *
ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(rosidl_typesupport_introspection_c, combat_robot_msgs, msg, SwarmControlCommand)() {
  combat_robot_msgs__msg__SwarmControlCommand__rosidl_typesupport_introspection_c__SwarmControlCommand_message_member_array[0].members_ =
    ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(rosidl_typesupport_introspection_c, std_msgs, msg, Header)();
  if (!combat_robot_msgs__msg__SwarmControlCommand__rosidl_typesupport_introspection_c__SwarmControlCommand_message_type_support_handle.typesupport_identifier) {
    combat_robot_msgs__msg__SwarmControlCommand__rosidl_typesupport_introspection_c__SwarmControlCommand_message_type_support_handle.typesupport_identifier =
      rosidl_typesupport_introspection_c__identifier;
  }
  return &combat_robot_msgs__msg__SwarmControlCommand__rosidl_typesupport_introspection_c__SwarmControlCommand_message_type_support_handle;
}
#ifdef __cplusplus
}
#endif
