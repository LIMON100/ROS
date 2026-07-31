// generated from rosidl_typesupport_introspection_c/resource/idl__type_support.c.em
// with input from combat_robot_msgs:msg/WaypointList.idl
// generated code does not contain a copyright notice

#include <stddef.h>
#include "combat_robot_msgs/msg/detail/waypoint_list__rosidl_typesupport_introspection_c.h"
#include "combat_robot_msgs/msg/rosidl_typesupport_introspection_c__visibility_control.h"
#include "rosidl_typesupport_introspection_c/field_types.h"
#include "rosidl_typesupport_introspection_c/identifier.h"
#include "rosidl_typesupport_introspection_c/message_introspection.h"
#include "combat_robot_msgs/msg/detail/waypoint_list__functions.h"
#include "combat_robot_msgs/msg/detail/waypoint_list__struct.h"


// Include directives for member types
// Member `waypoints`
#include "combat_robot_msgs/msg/waypoint.h"
// Member `waypoints`
#include "combat_robot_msgs/msg/detail/waypoint__rosidl_typesupport_introspection_c.h"

#ifdef __cplusplus
extern "C"
{
#endif

void combat_robot_msgs__msg__WaypointList__rosidl_typesupport_introspection_c__WaypointList_init_function(
  void * message_memory, enum rosidl_runtime_c__message_initialization _init)
{
  // TODO(karsten1987): initializers are not yet implemented for typesupport c
  // see https://github.com/ros2/ros2/issues/397
  (void) _init;
  combat_robot_msgs__msg__WaypointList__init(message_memory);
}

void combat_robot_msgs__msg__WaypointList__rosidl_typesupport_introspection_c__WaypointList_fini_function(void * message_memory)
{
  combat_robot_msgs__msg__WaypointList__fini(message_memory);
}

size_t combat_robot_msgs__msg__WaypointList__rosidl_typesupport_introspection_c__size_function__WaypointList__waypoints(
  const void * untyped_member)
{
  const combat_robot_msgs__msg__Waypoint__Sequence * member =
    (const combat_robot_msgs__msg__Waypoint__Sequence *)(untyped_member);
  return member->size;
}

const void * combat_robot_msgs__msg__WaypointList__rosidl_typesupport_introspection_c__get_const_function__WaypointList__waypoints(
  const void * untyped_member, size_t index)
{
  const combat_robot_msgs__msg__Waypoint__Sequence * member =
    (const combat_robot_msgs__msg__Waypoint__Sequence *)(untyped_member);
  return &member->data[index];
}

void * combat_robot_msgs__msg__WaypointList__rosidl_typesupport_introspection_c__get_function__WaypointList__waypoints(
  void * untyped_member, size_t index)
{
  combat_robot_msgs__msg__Waypoint__Sequence * member =
    (combat_robot_msgs__msg__Waypoint__Sequence *)(untyped_member);
  return &member->data[index];
}

void combat_robot_msgs__msg__WaypointList__rosidl_typesupport_introspection_c__fetch_function__WaypointList__waypoints(
  const void * untyped_member, size_t index, void * untyped_value)
{
  const combat_robot_msgs__msg__Waypoint * item =
    ((const combat_robot_msgs__msg__Waypoint *)
    combat_robot_msgs__msg__WaypointList__rosidl_typesupport_introspection_c__get_const_function__WaypointList__waypoints(untyped_member, index));
  combat_robot_msgs__msg__Waypoint * value =
    (combat_robot_msgs__msg__Waypoint *)(untyped_value);
  *value = *item;
}

void combat_robot_msgs__msg__WaypointList__rosidl_typesupport_introspection_c__assign_function__WaypointList__waypoints(
  void * untyped_member, size_t index, const void * untyped_value)
{
  combat_robot_msgs__msg__Waypoint * item =
    ((combat_robot_msgs__msg__Waypoint *)
    combat_robot_msgs__msg__WaypointList__rosidl_typesupport_introspection_c__get_function__WaypointList__waypoints(untyped_member, index));
  const combat_robot_msgs__msg__Waypoint * value =
    (const combat_robot_msgs__msg__Waypoint *)(untyped_value);
  *item = *value;
}

bool combat_robot_msgs__msg__WaypointList__rosidl_typesupport_introspection_c__resize_function__WaypointList__waypoints(
  void * untyped_member, size_t size)
{
  combat_robot_msgs__msg__Waypoint__Sequence * member =
    (combat_robot_msgs__msg__Waypoint__Sequence *)(untyped_member);
  combat_robot_msgs__msg__Waypoint__Sequence__fini(member);
  return combat_robot_msgs__msg__Waypoint__Sequence__init(member, size);
}

static rosidl_typesupport_introspection_c__MessageMember combat_robot_msgs__msg__WaypointList__rosidl_typesupport_introspection_c__WaypointList_message_member_array[5] = {
  {
    "mode",  // name
    rosidl_typesupport_introspection_c__ROS_TYPE_INT32,  // type
    0,  // upper bound of string
    NULL,  // members of sub message
    false,  // is key
    false,  // is array
    0,  // array size
    false,  // is upper bound
    offsetof(combat_robot_msgs__msg__WaypointList, mode),  // bytes offset in struct
    NULL,  // default value
    NULL,  // size() function pointer
    NULL,  // get_const(index) function pointer
    NULL,  // get(index) function pointer
    NULL,  // fetch(index, &value) function pointer
    NULL,  // assign(index, value) function pointer
    NULL  // resize(index) function pointer
  },
  {
    "formation",  // name
    rosidl_typesupport_introspection_c__ROS_TYPE_INT32,  // type
    0,  // upper bound of string
    NULL,  // members of sub message
    false,  // is key
    false,  // is array
    0,  // array size
    false,  // is upper bound
    offsetof(combat_robot_msgs__msg__WaypointList, formation),  // bytes offset in struct
    NULL,  // default value
    NULL,  // size() function pointer
    NULL,  // get_const(index) function pointer
    NULL,  // get(index) function pointer
    NULL,  // fetch(index, &value) function pointer
    NULL,  // assign(index, value) function pointer
    NULL  // resize(index) function pointer
  },
  {
    "mission_id",  // name
    rosidl_typesupport_introspection_c__ROS_TYPE_INT32,  // type
    0,  // upper bound of string
    NULL,  // members of sub message
    false,  // is key
    false,  // is array
    0,  // array size
    false,  // is upper bound
    offsetof(combat_robot_msgs__msg__WaypointList, mission_id),  // bytes offset in struct
    NULL,  // default value
    NULL,  // size() function pointer
    NULL,  // get_const(index) function pointer
    NULL,  // get(index) function pointer
    NULL,  // fetch(index, &value) function pointer
    NULL,  // assign(index, value) function pointer
    NULL  // resize(index) function pointer
  },
  {
    "mission_status",  // name
    rosidl_typesupport_introspection_c__ROS_TYPE_INT32,  // type
    0,  // upper bound of string
    NULL,  // members of sub message
    false,  // is key
    false,  // is array
    0,  // array size
    false,  // is upper bound
    offsetof(combat_robot_msgs__msg__WaypointList, mission_status),  // bytes offset in struct
    NULL,  // default value
    NULL,  // size() function pointer
    NULL,  // get_const(index) function pointer
    NULL,  // get(index) function pointer
    NULL,  // fetch(index, &value) function pointer
    NULL,  // assign(index, value) function pointer
    NULL  // resize(index) function pointer
  },
  {
    "waypoints",  // name
    rosidl_typesupport_introspection_c__ROS_TYPE_MESSAGE,  // type
    0,  // upper bound of string
    NULL,  // members of sub message (initialized later)
    false,  // is key
    true,  // is array
    0,  // array size
    false,  // is upper bound
    offsetof(combat_robot_msgs__msg__WaypointList, waypoints),  // bytes offset in struct
    NULL,  // default value
    combat_robot_msgs__msg__WaypointList__rosidl_typesupport_introspection_c__size_function__WaypointList__waypoints,  // size() function pointer
    combat_robot_msgs__msg__WaypointList__rosidl_typesupport_introspection_c__get_const_function__WaypointList__waypoints,  // get_const(index) function pointer
    combat_robot_msgs__msg__WaypointList__rosidl_typesupport_introspection_c__get_function__WaypointList__waypoints,  // get(index) function pointer
    combat_robot_msgs__msg__WaypointList__rosidl_typesupport_introspection_c__fetch_function__WaypointList__waypoints,  // fetch(index, &value) function pointer
    combat_robot_msgs__msg__WaypointList__rosidl_typesupport_introspection_c__assign_function__WaypointList__waypoints,  // assign(index, value) function pointer
    combat_robot_msgs__msg__WaypointList__rosidl_typesupport_introspection_c__resize_function__WaypointList__waypoints  // resize(index) function pointer
  }
};

static const rosidl_typesupport_introspection_c__MessageMembers combat_robot_msgs__msg__WaypointList__rosidl_typesupport_introspection_c__WaypointList_message_members = {
  "combat_robot_msgs__msg",  // message namespace
  "WaypointList",  // message name
  5,  // number of fields
  sizeof(combat_robot_msgs__msg__WaypointList),
  false,  // has_any_key_member_
  combat_robot_msgs__msg__WaypointList__rosidl_typesupport_introspection_c__WaypointList_message_member_array,  // message members
  combat_robot_msgs__msg__WaypointList__rosidl_typesupport_introspection_c__WaypointList_init_function,  // function to initialize message memory (memory has to be allocated)
  combat_robot_msgs__msg__WaypointList__rosidl_typesupport_introspection_c__WaypointList_fini_function  // function to terminate message instance (will not free memory)
};

// this is not const since it must be initialized on first access
// since C does not allow non-integral compile-time constants
static rosidl_message_type_support_t combat_robot_msgs__msg__WaypointList__rosidl_typesupport_introspection_c__WaypointList_message_type_support_handle = {
  0,
  &combat_robot_msgs__msg__WaypointList__rosidl_typesupport_introspection_c__WaypointList_message_members,
  get_message_typesupport_handle_function,
  &combat_robot_msgs__msg__WaypointList__get_type_hash,
  &combat_robot_msgs__msg__WaypointList__get_type_description,
  &combat_robot_msgs__msg__WaypointList__get_type_description_sources,
};

ROSIDL_TYPESUPPORT_INTROSPECTION_C_EXPORT_combat_robot_msgs
const rosidl_message_type_support_t *
ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(rosidl_typesupport_introspection_c, combat_robot_msgs, msg, WaypointList)() {
  combat_robot_msgs__msg__WaypointList__rosidl_typesupport_introspection_c__WaypointList_message_member_array[4].members_ =
    ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(rosidl_typesupport_introspection_c, combat_robot_msgs, msg, Waypoint)();
  if (!combat_robot_msgs__msg__WaypointList__rosidl_typesupport_introspection_c__WaypointList_message_type_support_handle.typesupport_identifier) {
    combat_robot_msgs__msg__WaypointList__rosidl_typesupport_introspection_c__WaypointList_message_type_support_handle.typesupport_identifier =
      rosidl_typesupport_introspection_c__identifier;
  }
  return &combat_robot_msgs__msg__WaypointList__rosidl_typesupport_introspection_c__WaypointList_message_type_support_handle;
}
#ifdef __cplusplus
}
#endif
