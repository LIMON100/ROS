// generated from rosidl_typesupport_introspection_c/resource/idl__type_support.c.em
// with input from combat_robot_msgs:msg/SwarmPathCommand.idl
// generated code does not contain a copyright notice

#include <stddef.h>
#include "combat_robot_msgs/msg/detail/swarm_path_command__rosidl_typesupport_introspection_c.h"
#include "combat_robot_msgs/msg/rosidl_typesupport_introspection_c__visibility_control.h"
#include "rosidl_typesupport_introspection_c/field_types.h"
#include "rosidl_typesupport_introspection_c/identifier.h"
#include "rosidl_typesupport_introspection_c/message_introspection.h"
#include "combat_robot_msgs/msg/detail/swarm_path_command__functions.h"
#include "combat_robot_msgs/msg/detail/swarm_path_command__struct.h"


// Include directives for member types
// Member `header`
#include "std_msgs/msg/header.h"
// Member `header`
#include "std_msgs/msg/detail/header__rosidl_typesupport_introspection_c.h"
// Member `path_json`
#include "rosidl_runtime_c/string_functions.h"

#ifdef __cplusplus
extern "C"
{
#endif

void combat_robot_msgs__msg__SwarmPathCommand__rosidl_typesupport_introspection_c__SwarmPathCommand_init_function(
  void * message_memory, enum rosidl_runtime_c__message_initialization _init)
{
  // TODO(karsten1987): initializers are not yet implemented for typesupport c
  // see https://github.com/ros2/ros2/issues/397
  (void) _init;
  combat_robot_msgs__msg__SwarmPathCommand__init(message_memory);
}

void combat_robot_msgs__msg__SwarmPathCommand__rosidl_typesupport_introspection_c__SwarmPathCommand_fini_function(void * message_memory)
{
  combat_robot_msgs__msg__SwarmPathCommand__fini(message_memory);
}

static rosidl_typesupport_introspection_c__MessageMember combat_robot_msgs__msg__SwarmPathCommand__rosidl_typesupport_introspection_c__SwarmPathCommand_message_member_array[4] = {
  {
    "header",  // name
    rosidl_typesupport_introspection_c__ROS_TYPE_MESSAGE,  // type
    0,  // upper bound of string
    NULL,  // members of sub message (initialized later)
    false,  // is key
    false,  // is array
    0,  // array size
    false,  // is upper bound
    offsetof(combat_robot_msgs__msg__SwarmPathCommand, header),  // bytes offset in struct
    NULL,  // default value
    NULL,  // size() function pointer
    NULL,  // get_const(index) function pointer
    NULL,  // get(index) function pointer
    NULL,  // fetch(index, &value) function pointer
    NULL,  // assign(index, value) function pointer
    NULL  // resize(index) function pointer
  },
  {
    "command",  // name
    rosidl_typesupport_introspection_c__ROS_TYPE_UINT8,  // type
    0,  // upper bound of string
    NULL,  // members of sub message
    false,  // is key
    false,  // is array
    0,  // array size
    false,  // is upper bound
    offsetof(combat_robot_msgs__msg__SwarmPathCommand, command),  // bytes offset in struct
    NULL,  // default value
    NULL,  // size() function pointer
    NULL,  // get_const(index) function pointer
    NULL,  // get(index) function pointer
    NULL,  // fetch(index, &value) function pointer
    NULL,  // assign(index, value) function pointer
    NULL  // resize(index) function pointer
  },
  {
    "num_waypoints",  // name
    rosidl_typesupport_introspection_c__ROS_TYPE_UINT16,  // type
    0,  // upper bound of string
    NULL,  // members of sub message
    false,  // is key
    false,  // is array
    0,  // array size
    false,  // is upper bound
    offsetof(combat_robot_msgs__msg__SwarmPathCommand, num_waypoints),  // bytes offset in struct
    NULL,  // default value
    NULL,  // size() function pointer
    NULL,  // get_const(index) function pointer
    NULL,  // get(index) function pointer
    NULL,  // fetch(index, &value) function pointer
    NULL,  // assign(index, value) function pointer
    NULL  // resize(index) function pointer
  },
  {
    "path_json",  // name
    rosidl_typesupport_introspection_c__ROS_TYPE_STRING,  // type
    0,  // upper bound of string
    NULL,  // members of sub message
    false,  // is key
    false,  // is array
    0,  // array size
    false,  // is upper bound
    offsetof(combat_robot_msgs__msg__SwarmPathCommand, path_json),  // bytes offset in struct
    NULL,  // default value
    NULL,  // size() function pointer
    NULL,  // get_const(index) function pointer
    NULL,  // get(index) function pointer
    NULL,  // fetch(index, &value) function pointer
    NULL,  // assign(index, value) function pointer
    NULL  // resize(index) function pointer
  }
};

static const rosidl_typesupport_introspection_c__MessageMembers combat_robot_msgs__msg__SwarmPathCommand__rosidl_typesupport_introspection_c__SwarmPathCommand_message_members = {
  "combat_robot_msgs__msg",  // message namespace
  "SwarmPathCommand",  // message name
  4,  // number of fields
  sizeof(combat_robot_msgs__msg__SwarmPathCommand),
  false,  // has_any_key_member_
  combat_robot_msgs__msg__SwarmPathCommand__rosidl_typesupport_introspection_c__SwarmPathCommand_message_member_array,  // message members
  combat_robot_msgs__msg__SwarmPathCommand__rosidl_typesupport_introspection_c__SwarmPathCommand_init_function,  // function to initialize message memory (memory has to be allocated)
  combat_robot_msgs__msg__SwarmPathCommand__rosidl_typesupport_introspection_c__SwarmPathCommand_fini_function  // function to terminate message instance (will not free memory)
};

// this is not const since it must be initialized on first access
// since C does not allow non-integral compile-time constants
static rosidl_message_type_support_t combat_robot_msgs__msg__SwarmPathCommand__rosidl_typesupport_introspection_c__SwarmPathCommand_message_type_support_handle = {
  0,
  &combat_robot_msgs__msg__SwarmPathCommand__rosidl_typesupport_introspection_c__SwarmPathCommand_message_members,
  get_message_typesupport_handle_function,
  &combat_robot_msgs__msg__SwarmPathCommand__get_type_hash,
  &combat_robot_msgs__msg__SwarmPathCommand__get_type_description,
  &combat_robot_msgs__msg__SwarmPathCommand__get_type_description_sources,
};

ROSIDL_TYPESUPPORT_INTROSPECTION_C_EXPORT_combat_robot_msgs
const rosidl_message_type_support_t *
ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(rosidl_typesupport_introspection_c, combat_robot_msgs, msg, SwarmPathCommand)() {
  combat_robot_msgs__msg__SwarmPathCommand__rosidl_typesupport_introspection_c__SwarmPathCommand_message_member_array[0].members_ =
    ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(rosidl_typesupport_introspection_c, std_msgs, msg, Header)();
  if (!combat_robot_msgs__msg__SwarmPathCommand__rosidl_typesupport_introspection_c__SwarmPathCommand_message_type_support_handle.typesupport_identifier) {
    combat_robot_msgs__msg__SwarmPathCommand__rosidl_typesupport_introspection_c__SwarmPathCommand_message_type_support_handle.typesupport_identifier =
      rosidl_typesupport_introspection_c__identifier;
  }
  return &combat_robot_msgs__msg__SwarmPathCommand__rosidl_typesupport_introspection_c__SwarmPathCommand_message_type_support_handle;
}
#ifdef __cplusplus
}
#endif
