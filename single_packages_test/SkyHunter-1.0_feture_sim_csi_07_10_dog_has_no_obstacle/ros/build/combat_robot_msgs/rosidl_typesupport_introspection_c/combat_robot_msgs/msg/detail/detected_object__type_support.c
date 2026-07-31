// generated from rosidl_typesupport_introspection_c/resource/idl__type_support.c.em
// with input from combat_robot_msgs:msg/DetectedObject.idl
// generated code does not contain a copyright notice

#include <stddef.h>
#include "combat_robot_msgs/msg/detail/detected_object__rosidl_typesupport_introspection_c.h"
#include "combat_robot_msgs/msg/rosidl_typesupport_introspection_c__visibility_control.h"
#include "rosidl_typesupport_introspection_c/field_types.h"
#include "rosidl_typesupport_introspection_c/identifier.h"
#include "rosidl_typesupport_introspection_c/message_introspection.h"
#include "combat_robot_msgs/msg/detail/detected_object__functions.h"
#include "combat_robot_msgs/msg/detail/detected_object__struct.h"


// Include directives for member types
// Member `box`
#include "combat_robot_msgs/msg/bounding_box2d.h"
// Member `box`
#include "combat_robot_msgs/msg/detail/bounding_box2d__rosidl_typesupport_introspection_c.h"

#ifdef __cplusplus
extern "C"
{
#endif

void combat_robot_msgs__msg__DetectedObject__rosidl_typesupport_introspection_c__DetectedObject_init_function(
  void * message_memory, enum rosidl_runtime_c__message_initialization _init)
{
  // TODO(karsten1987): initializers are not yet implemented for typesupport c
  // see https://github.com/ros2/ros2/issues/397
  (void) _init;
  combat_robot_msgs__msg__DetectedObject__init(message_memory);
}

void combat_robot_msgs__msg__DetectedObject__rosidl_typesupport_introspection_c__DetectedObject_fini_function(void * message_memory)
{
  combat_robot_msgs__msg__DetectedObject__fini(message_memory);
}

static rosidl_typesupport_introspection_c__MessageMember combat_robot_msgs__msg__DetectedObject__rosidl_typesupport_introspection_c__DetectedObject_message_member_array[3] = {
  {
    "id",  // name
    rosidl_typesupport_introspection_c__ROS_TYPE_INT32,  // type
    0,  // upper bound of string
    NULL,  // members of sub message
    false,  // is key
    false,  // is array
    0,  // array size
    false,  // is upper bound
    offsetof(combat_robot_msgs__msg__DetectedObject, id),  // bytes offset in struct
    NULL,  // default value
    NULL,  // size() function pointer
    NULL,  // get_const(index) function pointer
    NULL,  // get(index) function pointer
    NULL,  // fetch(index, &value) function pointer
    NULL,  // assign(index, value) function pointer
    NULL  // resize(index) function pointer
  },
  {
    "prob",  // name
    rosidl_typesupport_introspection_c__ROS_TYPE_FLOAT,  // type
    0,  // upper bound of string
    NULL,  // members of sub message
    false,  // is key
    false,  // is array
    0,  // array size
    false,  // is upper bound
    offsetof(combat_robot_msgs__msg__DetectedObject, prob),  // bytes offset in struct
    NULL,  // default value
    NULL,  // size() function pointer
    NULL,  // get_const(index) function pointer
    NULL,  // get(index) function pointer
    NULL,  // fetch(index, &value) function pointer
    NULL,  // assign(index, value) function pointer
    NULL  // resize(index) function pointer
  },
  {
    "box",  // name
    rosidl_typesupport_introspection_c__ROS_TYPE_MESSAGE,  // type
    0,  // upper bound of string
    NULL,  // members of sub message (initialized later)
    false,  // is key
    false,  // is array
    0,  // array size
    false,  // is upper bound
    offsetof(combat_robot_msgs__msg__DetectedObject, box),  // bytes offset in struct
    NULL,  // default value
    NULL,  // size() function pointer
    NULL,  // get_const(index) function pointer
    NULL,  // get(index) function pointer
    NULL,  // fetch(index, &value) function pointer
    NULL,  // assign(index, value) function pointer
    NULL  // resize(index) function pointer
  }
};

static const rosidl_typesupport_introspection_c__MessageMembers combat_robot_msgs__msg__DetectedObject__rosidl_typesupport_introspection_c__DetectedObject_message_members = {
  "combat_robot_msgs__msg",  // message namespace
  "DetectedObject",  // message name
  3,  // number of fields
  sizeof(combat_robot_msgs__msg__DetectedObject),
  false,  // has_any_key_member_
  combat_robot_msgs__msg__DetectedObject__rosidl_typesupport_introspection_c__DetectedObject_message_member_array,  // message members
  combat_robot_msgs__msg__DetectedObject__rosidl_typesupport_introspection_c__DetectedObject_init_function,  // function to initialize message memory (memory has to be allocated)
  combat_robot_msgs__msg__DetectedObject__rosidl_typesupport_introspection_c__DetectedObject_fini_function  // function to terminate message instance (will not free memory)
};

// this is not const since it must be initialized on first access
// since C does not allow non-integral compile-time constants
static rosidl_message_type_support_t combat_robot_msgs__msg__DetectedObject__rosidl_typesupport_introspection_c__DetectedObject_message_type_support_handle = {
  0,
  &combat_robot_msgs__msg__DetectedObject__rosidl_typesupport_introspection_c__DetectedObject_message_members,
  get_message_typesupport_handle_function,
  &combat_robot_msgs__msg__DetectedObject__get_type_hash,
  &combat_robot_msgs__msg__DetectedObject__get_type_description,
  &combat_robot_msgs__msg__DetectedObject__get_type_description_sources,
};

ROSIDL_TYPESUPPORT_INTROSPECTION_C_EXPORT_combat_robot_msgs
const rosidl_message_type_support_t *
ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(rosidl_typesupport_introspection_c, combat_robot_msgs, msg, DetectedObject)() {
  combat_robot_msgs__msg__DetectedObject__rosidl_typesupport_introspection_c__DetectedObject_message_member_array[2].members_ =
    ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(rosidl_typesupport_introspection_c, combat_robot_msgs, msg, BoundingBox2d)();
  if (!combat_robot_msgs__msg__DetectedObject__rosidl_typesupport_introspection_c__DetectedObject_message_type_support_handle.typesupport_identifier) {
    combat_robot_msgs__msg__DetectedObject__rosidl_typesupport_introspection_c__DetectedObject_message_type_support_handle.typesupport_identifier =
      rosidl_typesupport_introspection_c__identifier;
  }
  return &combat_robot_msgs__msg__DetectedObject__rosidl_typesupport_introspection_c__DetectedObject_message_type_support_handle;
}
#ifdef __cplusplus
}
#endif
