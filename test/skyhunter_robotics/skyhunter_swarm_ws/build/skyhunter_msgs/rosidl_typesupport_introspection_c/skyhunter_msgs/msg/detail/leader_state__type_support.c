// generated from rosidl_typesupport_introspection_c/resource/idl__type_support.c.em
// with input from skyhunter_msgs:msg/LeaderState.idl
// generated code does not contain a copyright notice

#include <stddef.h>
#include "skyhunter_msgs/msg/detail/leader_state__rosidl_typesupport_introspection_c.h"
#include "skyhunter_msgs/msg/rosidl_typesupport_introspection_c__visibility_control.h"
#include "rosidl_typesupport_introspection_c/field_types.h"
#include "rosidl_typesupport_introspection_c/identifier.h"
#include "rosidl_typesupport_introspection_c/message_introspection.h"
#include "skyhunter_msgs/msg/detail/leader_state__functions.h"
#include "skyhunter_msgs/msg/detail/leader_state__struct.h"


// Include directives for member types
// Member `header`
#include "std_msgs/msg/header.h"
// Member `header`
#include "std_msgs/msg/detail/header__rosidl_typesupport_introspection_c.h"
// Member `pose`
// Member `next_waypoints`
#include "geometry_msgs/msg/pose.h"
// Member `pose`
// Member `next_waypoints`
#include "geometry_msgs/msg/detail/pose__rosidl_typesupport_introspection_c.h"
// Member `velocity`
#include "geometry_msgs/msg/twist.h"
// Member `velocity`
#include "geometry_msgs/msg/detail/twist__rosidl_typesupport_introspection_c.h"

#ifdef __cplusplus
extern "C"
{
#endif

void skyhunter_msgs__msg__LeaderState__rosidl_typesupport_introspection_c__LeaderState_init_function(
  void * message_memory, enum rosidl_runtime_c__message_initialization _init)
{
  // TODO(karsten1987): initializers are not yet implemented for typesupport c
  // see https://github.com/ros2/ros2/issues/397
  (void) _init;
  skyhunter_msgs__msg__LeaderState__init(message_memory);
}

void skyhunter_msgs__msg__LeaderState__rosidl_typesupport_introspection_c__LeaderState_fini_function(void * message_memory)
{
  skyhunter_msgs__msg__LeaderState__fini(message_memory);
}

size_t skyhunter_msgs__msg__LeaderState__rosidl_typesupport_introspection_c__size_function__LeaderState__next_waypoints(
  const void * untyped_member)
{
  const geometry_msgs__msg__Pose__Sequence * member =
    (const geometry_msgs__msg__Pose__Sequence *)(untyped_member);
  return member->size;
}

const void * skyhunter_msgs__msg__LeaderState__rosidl_typesupport_introspection_c__get_const_function__LeaderState__next_waypoints(
  const void * untyped_member, size_t index)
{
  const geometry_msgs__msg__Pose__Sequence * member =
    (const geometry_msgs__msg__Pose__Sequence *)(untyped_member);
  return &member->data[index];
}

void * skyhunter_msgs__msg__LeaderState__rosidl_typesupport_introspection_c__get_function__LeaderState__next_waypoints(
  void * untyped_member, size_t index)
{
  geometry_msgs__msg__Pose__Sequence * member =
    (geometry_msgs__msg__Pose__Sequence *)(untyped_member);
  return &member->data[index];
}

void skyhunter_msgs__msg__LeaderState__rosidl_typesupport_introspection_c__fetch_function__LeaderState__next_waypoints(
  const void * untyped_member, size_t index, void * untyped_value)
{
  const geometry_msgs__msg__Pose * item =
    ((const geometry_msgs__msg__Pose *)
    skyhunter_msgs__msg__LeaderState__rosidl_typesupport_introspection_c__get_const_function__LeaderState__next_waypoints(untyped_member, index));
  geometry_msgs__msg__Pose * value =
    (geometry_msgs__msg__Pose *)(untyped_value);
  *value = *item;
}

void skyhunter_msgs__msg__LeaderState__rosidl_typesupport_introspection_c__assign_function__LeaderState__next_waypoints(
  void * untyped_member, size_t index, const void * untyped_value)
{
  geometry_msgs__msg__Pose * item =
    ((geometry_msgs__msg__Pose *)
    skyhunter_msgs__msg__LeaderState__rosidl_typesupport_introspection_c__get_function__LeaderState__next_waypoints(untyped_member, index));
  const geometry_msgs__msg__Pose * value =
    (const geometry_msgs__msg__Pose *)(untyped_value);
  *item = *value;
}

bool skyhunter_msgs__msg__LeaderState__rosidl_typesupport_introspection_c__resize_function__LeaderState__next_waypoints(
  void * untyped_member, size_t size)
{
  geometry_msgs__msg__Pose__Sequence * member =
    (geometry_msgs__msg__Pose__Sequence *)(untyped_member);
  geometry_msgs__msg__Pose__Sequence__fini(member);
  return geometry_msgs__msg__Pose__Sequence__init(member, size);
}

static rosidl_typesupport_introspection_c__MessageMember skyhunter_msgs__msg__LeaderState__rosidl_typesupport_introspection_c__LeaderState_message_member_array[9] = {
  {
    "header",  // name
    rosidl_typesupport_introspection_c__ROS_TYPE_MESSAGE,  // type
    0,  // upper bound of string
    NULL,  // members of sub message (initialized later)
    false,  // is array
    0,  // array size
    false,  // is upper bound
    offsetof(skyhunter_msgs__msg__LeaderState, header),  // bytes offset in struct
    NULL,  // default value
    NULL,  // size() function pointer
    NULL,  // get_const(index) function pointer
    NULL,  // get(index) function pointer
    NULL,  // fetch(index, &value) function pointer
    NULL,  // assign(index, value) function pointer
    NULL  // resize(index) function pointer
  },
  {
    "pose",  // name
    rosidl_typesupport_introspection_c__ROS_TYPE_MESSAGE,  // type
    0,  // upper bound of string
    NULL,  // members of sub message (initialized later)
    false,  // is array
    0,  // array size
    false,  // is upper bound
    offsetof(skyhunter_msgs__msg__LeaderState, pose),  // bytes offset in struct
    NULL,  // default value
    NULL,  // size() function pointer
    NULL,  // get_const(index) function pointer
    NULL,  // get(index) function pointer
    NULL,  // fetch(index, &value) function pointer
    NULL,  // assign(index, value) function pointer
    NULL  // resize(index) function pointer
  },
  {
    "velocity",  // name
    rosidl_typesupport_introspection_c__ROS_TYPE_MESSAGE,  // type
    0,  // upper bound of string
    NULL,  // members of sub message (initialized later)
    false,  // is array
    0,  // array size
    false,  // is upper bound
    offsetof(skyhunter_msgs__msg__LeaderState, velocity),  // bytes offset in struct
    NULL,  // default value
    NULL,  // size() function pointer
    NULL,  // get_const(index) function pointer
    NULL,  // get(index) function pointer
    NULL,  // fetch(index, &value) function pointer
    NULL,  // assign(index, value) function pointer
    NULL  // resize(index) function pointer
  },
  {
    "next_waypoints",  // name
    rosidl_typesupport_introspection_c__ROS_TYPE_MESSAGE,  // type
    0,  // upper bound of string
    NULL,  // members of sub message (initialized later)
    true,  // is array
    0,  // array size
    false,  // is upper bound
    offsetof(skyhunter_msgs__msg__LeaderState, next_waypoints),  // bytes offset in struct
    NULL,  // default value
    skyhunter_msgs__msg__LeaderState__rosidl_typesupport_introspection_c__size_function__LeaderState__next_waypoints,  // size() function pointer
    skyhunter_msgs__msg__LeaderState__rosidl_typesupport_introspection_c__get_const_function__LeaderState__next_waypoints,  // get_const(index) function pointer
    skyhunter_msgs__msg__LeaderState__rosidl_typesupport_introspection_c__get_function__LeaderState__next_waypoints,  // get(index) function pointer
    skyhunter_msgs__msg__LeaderState__rosidl_typesupport_introspection_c__fetch_function__LeaderState__next_waypoints,  // fetch(index, &value) function pointer
    skyhunter_msgs__msg__LeaderState__rosidl_typesupport_introspection_c__assign_function__LeaderState__next_waypoints,  // assign(index, value) function pointer
    skyhunter_msgs__msg__LeaderState__rosidl_typesupport_introspection_c__resize_function__LeaderState__next_waypoints  // resize(index) function pointer
  },
  {
    "formation_mode",  // name
    rosidl_typesupport_introspection_c__ROS_TYPE_UINT8,  // type
    0,  // upper bound of string
    NULL,  // members of sub message
    false,  // is array
    0,  // array size
    false,  // is upper bound
    offsetof(skyhunter_msgs__msg__LeaderState, formation_mode),  // bytes offset in struct
    NULL,  // default value
    NULL,  // size() function pointer
    NULL,  // get_const(index) function pointer
    NULL,  // get(index) function pointer
    NULL,  // fetch(index, &value) function pointer
    NULL,  // assign(index, value) function pointer
    NULL  // resize(index) function pointer
  },
  {
    "formation_state",  // name
    rosidl_typesupport_introspection_c__ROS_TYPE_UINT8,  // type
    0,  // upper bound of string
    NULL,  // members of sub message
    false,  // is array
    0,  // array size
    false,  // is upper bound
    offsetof(skyhunter_msgs__msg__LeaderState, formation_state),  // bytes offset in struct
    NULL,  // default value
    NULL,  // size() function pointer
    NULL,  // get_const(index) function pointer
    NULL,  // get(index) function pointer
    NULL,  // fetch(index, &value) function pointer
    NULL,  // assign(index, value) function pointer
    NULL  // resize(index) function pointer
  },
  {
    "swarm_state",  // name
    rosidl_typesupport_introspection_c__ROS_TYPE_INT8,  // type
    0,  // upper bound of string
    NULL,  // members of sub message
    false,  // is array
    0,  // array size
    false,  // is upper bound
    offsetof(skyhunter_msgs__msg__LeaderState, swarm_state),  // bytes offset in struct
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
    rosidl_typesupport_introspection_c__ROS_TYPE_INT8,  // type
    0,  // upper bound of string
    NULL,  // members of sub message
    false,  // is array
    0,  // array size
    false,  // is upper bound
    offsetof(skyhunter_msgs__msg__LeaderState, formation_type),  // bytes offset in struct
    NULL,  // default value
    NULL,  // size() function pointer
    NULL,  // get_const(index) function pointer
    NULL,  // get(index) function pointer
    NULL,  // fetch(index, &value) function pointer
    NULL,  // assign(index, value) function pointer
    NULL  // resize(index) function pointer
  },
  {
    "current_waypoint_index",  // name
    rosidl_typesupport_introspection_c__ROS_TYPE_INT32,  // type
    0,  // upper bound of string
    NULL,  // members of sub message
    false,  // is array
    0,  // array size
    false,  // is upper bound
    offsetof(skyhunter_msgs__msg__LeaderState, current_waypoint_index),  // bytes offset in struct
    NULL,  // default value
    NULL,  // size() function pointer
    NULL,  // get_const(index) function pointer
    NULL,  // get(index) function pointer
    NULL,  // fetch(index, &value) function pointer
    NULL,  // assign(index, value) function pointer
    NULL  // resize(index) function pointer
  }
};

static const rosidl_typesupport_introspection_c__MessageMembers skyhunter_msgs__msg__LeaderState__rosidl_typesupport_introspection_c__LeaderState_message_members = {
  "skyhunter_msgs__msg",  // message namespace
  "LeaderState",  // message name
  9,  // number of fields
  sizeof(skyhunter_msgs__msg__LeaderState),
  skyhunter_msgs__msg__LeaderState__rosidl_typesupport_introspection_c__LeaderState_message_member_array,  // message members
  skyhunter_msgs__msg__LeaderState__rosidl_typesupport_introspection_c__LeaderState_init_function,  // function to initialize message memory (memory has to be allocated)
  skyhunter_msgs__msg__LeaderState__rosidl_typesupport_introspection_c__LeaderState_fini_function  // function to terminate message instance (will not free memory)
};

// this is not const since it must be initialized on first access
// since C does not allow non-integral compile-time constants
static rosidl_message_type_support_t skyhunter_msgs__msg__LeaderState__rosidl_typesupport_introspection_c__LeaderState_message_type_support_handle = {
  0,
  &skyhunter_msgs__msg__LeaderState__rosidl_typesupport_introspection_c__LeaderState_message_members,
  get_message_typesupport_handle_function,
};

ROSIDL_TYPESUPPORT_INTROSPECTION_C_EXPORT_skyhunter_msgs
const rosidl_message_type_support_t *
ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(rosidl_typesupport_introspection_c, skyhunter_msgs, msg, LeaderState)() {
  skyhunter_msgs__msg__LeaderState__rosidl_typesupport_introspection_c__LeaderState_message_member_array[0].members_ =
    ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(rosidl_typesupport_introspection_c, std_msgs, msg, Header)();
  skyhunter_msgs__msg__LeaderState__rosidl_typesupport_introspection_c__LeaderState_message_member_array[1].members_ =
    ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(rosidl_typesupport_introspection_c, geometry_msgs, msg, Pose)();
  skyhunter_msgs__msg__LeaderState__rosidl_typesupport_introspection_c__LeaderState_message_member_array[2].members_ =
    ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(rosidl_typesupport_introspection_c, geometry_msgs, msg, Twist)();
  skyhunter_msgs__msg__LeaderState__rosidl_typesupport_introspection_c__LeaderState_message_member_array[3].members_ =
    ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(rosidl_typesupport_introspection_c, geometry_msgs, msg, Pose)();
  if (!skyhunter_msgs__msg__LeaderState__rosidl_typesupport_introspection_c__LeaderState_message_type_support_handle.typesupport_identifier) {
    skyhunter_msgs__msg__LeaderState__rosidl_typesupport_introspection_c__LeaderState_message_type_support_handle.typesupport_identifier =
      rosidl_typesupport_introspection_c__identifier;
  }
  return &skyhunter_msgs__msg__LeaderState__rosidl_typesupport_introspection_c__LeaderState_message_type_support_handle;
}
#ifdef __cplusplus
}
#endif
