// generated from rosidl_typesupport_introspection_c/resource/idl__type_support.c.em
// with input from combat_robot_msgs:msg/PanTiltControlCommand.idl
// generated code does not contain a copyright notice

#include <stddef.h>
#include "combat_robot_msgs/msg/detail/pan_tilt_control_command__rosidl_typesupport_introspection_c.h"
#include "combat_robot_msgs/msg/rosidl_typesupport_introspection_c__visibility_control.h"
#include "rosidl_typesupport_introspection_c/field_types.h"
#include "rosidl_typesupport_introspection_c/identifier.h"
#include "rosidl_typesupport_introspection_c/message_introspection.h"
#include "combat_robot_msgs/msg/detail/pan_tilt_control_command__functions.h"
#include "combat_robot_msgs/msg/detail/pan_tilt_control_command__struct.h"


// Include directives for member types
// Member `header`
#include "std_msgs/msg/header.h"
// Member `header`
#include "std_msgs/msg/detail/header__rosidl_typesupport_introspection_c.h"

#ifdef __cplusplus
extern "C"
{
#endif

void combat_robot_msgs__msg__PanTiltControlCommand__rosidl_typesupport_introspection_c__PanTiltControlCommand_init_function(
  void * message_memory, enum rosidl_runtime_c__message_initialization _init)
{
  // TODO(karsten1987): initializers are not yet implemented for typesupport c
  // see https://github.com/ros2/ros2/issues/397
  (void) _init;
  combat_robot_msgs__msg__PanTiltControlCommand__init(message_memory);
}

void combat_robot_msgs__msg__PanTiltControlCommand__rosidl_typesupport_introspection_c__PanTiltControlCommand_fini_function(void * message_memory)
{
  combat_robot_msgs__msg__PanTiltControlCommand__fini(message_memory);
}

static rosidl_typesupport_introspection_c__MessageMember combat_robot_msgs__msg__PanTiltControlCommand__rosidl_typesupport_introspection_c__PanTiltControlCommand_message_member_array[8] = {
  {
    "header",  // name
    rosidl_typesupport_introspection_c__ROS_TYPE_MESSAGE,  // type
    0,  // upper bound of string
    NULL,  // members of sub message (initialized later)
    false,  // is key
    false,  // is array
    0,  // array size
    false,  // is upper bound
    offsetof(combat_robot_msgs__msg__PanTiltControlCommand, header),  // bytes offset in struct
    NULL,  // default value
    NULL,  // size() function pointer
    NULL,  // get_const(index) function pointer
    NULL,  // get(index) function pointer
    NULL,  // fetch(index, &value) function pointer
    NULL,  // assign(index, value) function pointer
    NULL  // resize(index) function pointer
  },
  {
    "control_mode",  // name
    rosidl_typesupport_introspection_c__ROS_TYPE_UINT8,  // type
    0,  // upper bound of string
    NULL,  // members of sub message
    false,  // is key
    false,  // is array
    0,  // array size
    false,  // is upper bound
    offsetof(combat_robot_msgs__msg__PanTiltControlCommand, control_mode),  // bytes offset in struct
    NULL,  // default value
    NULL,  // size() function pointer
    NULL,  // get_const(index) function pointer
    NULL,  // get(index) function pointer
    NULL,  // fetch(index, &value) function pointer
    NULL,  // assign(index, value) function pointer
    NULL  // resize(index) function pointer
  },
  {
    "horizontal_angle",  // name
    rosidl_typesupport_introspection_c__ROS_TYPE_FLOAT,  // type
    0,  // upper bound of string
    NULL,  // members of sub message
    false,  // is key
    false,  // is array
    0,  // array size
    false,  // is upper bound
    offsetof(combat_robot_msgs__msg__PanTiltControlCommand, horizontal_angle),  // bytes offset in struct
    NULL,  // default value
    NULL,  // size() function pointer
    NULL,  // get_const(index) function pointer
    NULL,  // get(index) function pointer
    NULL,  // fetch(index, &value) function pointer
    NULL,  // assign(index, value) function pointer
    NULL  // resize(index) function pointer
  },
  {
    "vertical_angle",  // name
    rosidl_typesupport_introspection_c__ROS_TYPE_FLOAT,  // type
    0,  // upper bound of string
    NULL,  // members of sub message
    false,  // is key
    false,  // is array
    0,  // array size
    false,  // is upper bound
    offsetof(combat_robot_msgs__msg__PanTiltControlCommand, vertical_angle),  // bytes offset in struct
    NULL,  // default value
    NULL,  // size() function pointer
    NULL,  // get_const(index) function pointer
    NULL,  // get(index) function pointer
    NULL,  // fetch(index, &value) function pointer
    NULL,  // assign(index, value) function pointer
    NULL  // resize(index) function pointer
  },
  {
    "pan_speed",  // name
    rosidl_typesupport_introspection_c__ROS_TYPE_UINT8,  // type
    0,  // upper bound of string
    NULL,  // members of sub message
    false,  // is key
    false,  // is array
    0,  // array size
    false,  // is upper bound
    offsetof(combat_robot_msgs__msg__PanTiltControlCommand, pan_speed),  // bytes offset in struct
    NULL,  // default value
    NULL,  // size() function pointer
    NULL,  // get_const(index) function pointer
    NULL,  // get(index) function pointer
    NULL,  // fetch(index, &value) function pointer
    NULL,  // assign(index, value) function pointer
    NULL  // resize(index) function pointer
  },
  {
    "tilt_speed",  // name
    rosidl_typesupport_introspection_c__ROS_TYPE_UINT8,  // type
    0,  // upper bound of string
    NULL,  // members of sub message
    false,  // is key
    false,  // is array
    0,  // array size
    false,  // is upper bound
    offsetof(combat_robot_msgs__msg__PanTiltControlCommand, tilt_speed),  // bytes offset in struct
    NULL,  // default value
    NULL,  // size() function pointer
    NULL,  // get_const(index) function pointer
    NULL,  // get(index) function pointer
    NULL,  // fetch(index, &value) function pointer
    NULL,  // assign(index, value) function pointer
    NULL  // resize(index) function pointer
  },
  {
    "pan_dir",  // name
    rosidl_typesupport_introspection_c__ROS_TYPE_UINT8,  // type
    0,  // upper bound of string
    NULL,  // members of sub message
    false,  // is key
    false,  // is array
    0,  // array size
    false,  // is upper bound
    offsetof(combat_robot_msgs__msg__PanTiltControlCommand, pan_dir),  // bytes offset in struct
    NULL,  // default value
    NULL,  // size() function pointer
    NULL,  // get_const(index) function pointer
    NULL,  // get(index) function pointer
    NULL,  // fetch(index, &value) function pointer
    NULL,  // assign(index, value) function pointer
    NULL  // resize(index) function pointer
  },
  {
    "tilt_dir",  // name
    rosidl_typesupport_introspection_c__ROS_TYPE_UINT8,  // type
    0,  // upper bound of string
    NULL,  // members of sub message
    false,  // is key
    false,  // is array
    0,  // array size
    false,  // is upper bound
    offsetof(combat_robot_msgs__msg__PanTiltControlCommand, tilt_dir),  // bytes offset in struct
    NULL,  // default value
    NULL,  // size() function pointer
    NULL,  // get_const(index) function pointer
    NULL,  // get(index) function pointer
    NULL,  // fetch(index, &value) function pointer
    NULL,  // assign(index, value) function pointer
    NULL  // resize(index) function pointer
  }
};

static const rosidl_typesupport_introspection_c__MessageMembers combat_robot_msgs__msg__PanTiltControlCommand__rosidl_typesupport_introspection_c__PanTiltControlCommand_message_members = {
  "combat_robot_msgs__msg",  // message namespace
  "PanTiltControlCommand",  // message name
  8,  // number of fields
  sizeof(combat_robot_msgs__msg__PanTiltControlCommand),
  false,  // has_any_key_member_
  combat_robot_msgs__msg__PanTiltControlCommand__rosidl_typesupport_introspection_c__PanTiltControlCommand_message_member_array,  // message members
  combat_robot_msgs__msg__PanTiltControlCommand__rosidl_typesupport_introspection_c__PanTiltControlCommand_init_function,  // function to initialize message memory (memory has to be allocated)
  combat_robot_msgs__msg__PanTiltControlCommand__rosidl_typesupport_introspection_c__PanTiltControlCommand_fini_function  // function to terminate message instance (will not free memory)
};

// this is not const since it must be initialized on first access
// since C does not allow non-integral compile-time constants
static rosidl_message_type_support_t combat_robot_msgs__msg__PanTiltControlCommand__rosidl_typesupport_introspection_c__PanTiltControlCommand_message_type_support_handle = {
  0,
  &combat_robot_msgs__msg__PanTiltControlCommand__rosidl_typesupport_introspection_c__PanTiltControlCommand_message_members,
  get_message_typesupport_handle_function,
  &combat_robot_msgs__msg__PanTiltControlCommand__get_type_hash,
  &combat_robot_msgs__msg__PanTiltControlCommand__get_type_description,
  &combat_robot_msgs__msg__PanTiltControlCommand__get_type_description_sources,
};

ROSIDL_TYPESUPPORT_INTROSPECTION_C_EXPORT_combat_robot_msgs
const rosidl_message_type_support_t *
ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(rosidl_typesupport_introspection_c, combat_robot_msgs, msg, PanTiltControlCommand)() {
  combat_robot_msgs__msg__PanTiltControlCommand__rosidl_typesupport_introspection_c__PanTiltControlCommand_message_member_array[0].members_ =
    ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(rosidl_typesupport_introspection_c, std_msgs, msg, Header)();
  if (!combat_robot_msgs__msg__PanTiltControlCommand__rosidl_typesupport_introspection_c__PanTiltControlCommand_message_type_support_handle.typesupport_identifier) {
    combat_robot_msgs__msg__PanTiltControlCommand__rosidl_typesupport_introspection_c__PanTiltControlCommand_message_type_support_handle.typesupport_identifier =
      rosidl_typesupport_introspection_c__identifier;
  }
  return &combat_robot_msgs__msg__PanTiltControlCommand__rosidl_typesupport_introspection_c__PanTiltControlCommand_message_type_support_handle;
}
#ifdef __cplusplus
}
#endif
