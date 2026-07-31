// generated from rosidl_typesupport_introspection_cpp/resource/idl__type_support.cpp.em
// with input from combat_robot_msgs:msg/BoundingBox2d.idl
// generated code does not contain a copyright notice

#include "array"
#include "cstddef"
#include "string"
#include "vector"
#include "rosidl_runtime_c/message_type_support_struct.h"
#include "rosidl_typesupport_cpp/message_type_support.hpp"
#include "rosidl_typesupport_interface/macros.h"
#include "combat_robot_msgs/msg/detail/bounding_box2d__functions.h"
#include "combat_robot_msgs/msg/detail/bounding_box2d__struct.hpp"
#include "rosidl_typesupport_introspection_cpp/field_types.hpp"
#include "rosidl_typesupport_introspection_cpp/identifier.hpp"
#include "rosidl_typesupport_introspection_cpp/message_introspection.hpp"
#include "rosidl_typesupport_introspection_cpp/message_type_support_decl.hpp"
#include "rosidl_typesupport_introspection_cpp/visibility_control.h"

namespace combat_robot_msgs
{

namespace msg
{

namespace rosidl_typesupport_introspection_cpp
{

void BoundingBox2d_init_function(
  void * message_memory, rosidl_runtime_cpp::MessageInitialization _init)
{
  new (message_memory) combat_robot_msgs::msg::BoundingBox2d(_init);
}

void BoundingBox2d_fini_function(void * message_memory)
{
  auto typed_message = static_cast<combat_robot_msgs::msg::BoundingBox2d *>(message_memory);
  typed_message->~BoundingBox2d();
}

static const ::rosidl_typesupport_introspection_cpp::MessageMember BoundingBox2d_message_member_array[4] = {
  {
    "x",  // name
    ::rosidl_typesupport_introspection_cpp::ROS_TYPE_INT32,  // type
    0,  // upper bound of string
    nullptr,  // members of sub message
    false,  // is key
    false,  // is array
    0,  // array size
    false,  // is upper bound
    offsetof(combat_robot_msgs::msg::BoundingBox2d, x),  // bytes offset in struct
    nullptr,  // default value
    nullptr,  // size() function pointer
    nullptr,  // get_const(index) function pointer
    nullptr,  // get(index) function pointer
    nullptr,  // fetch(index, &value) function pointer
    nullptr,  // assign(index, value) function pointer
    nullptr  // resize(index) function pointer
  },
  {
    "y",  // name
    ::rosidl_typesupport_introspection_cpp::ROS_TYPE_INT32,  // type
    0,  // upper bound of string
    nullptr,  // members of sub message
    false,  // is key
    false,  // is array
    0,  // array size
    false,  // is upper bound
    offsetof(combat_robot_msgs::msg::BoundingBox2d, y),  // bytes offset in struct
    nullptr,  // default value
    nullptr,  // size() function pointer
    nullptr,  // get_const(index) function pointer
    nullptr,  // get(index) function pointer
    nullptr,  // fetch(index, &value) function pointer
    nullptr,  // assign(index, value) function pointer
    nullptr  // resize(index) function pointer
  },
  {
    "width",  // name
    ::rosidl_typesupport_introspection_cpp::ROS_TYPE_INT32,  // type
    0,  // upper bound of string
    nullptr,  // members of sub message
    false,  // is key
    false,  // is array
    0,  // array size
    false,  // is upper bound
    offsetof(combat_robot_msgs::msg::BoundingBox2d, width),  // bytes offset in struct
    nullptr,  // default value
    nullptr,  // size() function pointer
    nullptr,  // get_const(index) function pointer
    nullptr,  // get(index) function pointer
    nullptr,  // fetch(index, &value) function pointer
    nullptr,  // assign(index, value) function pointer
    nullptr  // resize(index) function pointer
  },
  {
    "height",  // name
    ::rosidl_typesupport_introspection_cpp::ROS_TYPE_INT32,  // type
    0,  // upper bound of string
    nullptr,  // members of sub message
    false,  // is key
    false,  // is array
    0,  // array size
    false,  // is upper bound
    offsetof(combat_robot_msgs::msg::BoundingBox2d, height),  // bytes offset in struct
    nullptr,  // default value
    nullptr,  // size() function pointer
    nullptr,  // get_const(index) function pointer
    nullptr,  // get(index) function pointer
    nullptr,  // fetch(index, &value) function pointer
    nullptr,  // assign(index, value) function pointer
    nullptr  // resize(index) function pointer
  }
};

static const ::rosidl_typesupport_introspection_cpp::MessageMembers BoundingBox2d_message_members = {
  "combat_robot_msgs::msg",  // message namespace
  "BoundingBox2d",  // message name
  4,  // number of fields
  sizeof(combat_robot_msgs::msg::BoundingBox2d),
  false,  // has_any_key_member_
  BoundingBox2d_message_member_array,  // message members
  BoundingBox2d_init_function,  // function to initialize message memory (memory has to be allocated)
  BoundingBox2d_fini_function  // function to terminate message instance (will not free memory)
};

static const rosidl_message_type_support_t BoundingBox2d_message_type_support_handle = {
  ::rosidl_typesupport_introspection_cpp::typesupport_identifier,
  &BoundingBox2d_message_members,
  get_message_typesupport_handle_function,
  &combat_robot_msgs__msg__BoundingBox2d__get_type_hash,
  &combat_robot_msgs__msg__BoundingBox2d__get_type_description,
  &combat_robot_msgs__msg__BoundingBox2d__get_type_description_sources,
};

}  // namespace rosidl_typesupport_introspection_cpp

}  // namespace msg

}  // namespace combat_robot_msgs


namespace rosidl_typesupport_introspection_cpp
{

template<>
ROSIDL_TYPESUPPORT_INTROSPECTION_CPP_PUBLIC
const rosidl_message_type_support_t *
get_message_type_support_handle<combat_robot_msgs::msg::BoundingBox2d>()
{
  return &::combat_robot_msgs::msg::rosidl_typesupport_introspection_cpp::BoundingBox2d_message_type_support_handle;
}

}  // namespace rosidl_typesupport_introspection_cpp

#ifdef __cplusplus
extern "C"
{
#endif

ROSIDL_TYPESUPPORT_INTROSPECTION_CPP_PUBLIC
const rosidl_message_type_support_t *
ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(rosidl_typesupport_introspection_cpp, combat_robot_msgs, msg, BoundingBox2d)() {
  return &::combat_robot_msgs::msg::rosidl_typesupport_introspection_cpp::BoundingBox2d_message_type_support_handle;
}

#ifdef __cplusplus
}
#endif
