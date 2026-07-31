// generated from rosidl_typesupport_introspection_cpp/resource/idl__type_support.cpp.em
// with input from combat_robot_msgs:msg/SwarmControlCommand.idl
// generated code does not contain a copyright notice

#include "array"
#include "cstddef"
#include "string"
#include "vector"
#include "rosidl_runtime_c/message_type_support_struct.h"
#include "rosidl_typesupport_cpp/message_type_support.hpp"
#include "rosidl_typesupport_interface/macros.h"
#include "combat_robot_msgs/msg/detail/swarm_control_command__functions.h"
#include "combat_robot_msgs/msg/detail/swarm_control_command__struct.hpp"
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

void SwarmControlCommand_init_function(
  void * message_memory, rosidl_runtime_cpp::MessageInitialization _init)
{
  new (message_memory) combat_robot_msgs::msg::SwarmControlCommand(_init);
}

void SwarmControlCommand_fini_function(void * message_memory)
{
  auto typed_message = static_cast<combat_robot_msgs::msg::SwarmControlCommand *>(message_memory);
  typed_message->~SwarmControlCommand();
}

size_t size_function__SwarmControlCommand__selected_robot_ids(const void * untyped_member)
{
  (void)untyped_member;
  return 8;
}

const void * get_const_function__SwarmControlCommand__selected_robot_ids(const void * untyped_member, size_t index)
{
  const auto & member =
    *reinterpret_cast<const std::array<uint32_t, 8> *>(untyped_member);
  return &member[index];
}

void * get_function__SwarmControlCommand__selected_robot_ids(void * untyped_member, size_t index)
{
  auto & member =
    *reinterpret_cast<std::array<uint32_t, 8> *>(untyped_member);
  return &member[index];
}

void fetch_function__SwarmControlCommand__selected_robot_ids(
  const void * untyped_member, size_t index, void * untyped_value)
{
  const auto & item = *reinterpret_cast<const uint32_t *>(
    get_const_function__SwarmControlCommand__selected_robot_ids(untyped_member, index));
  auto & value = *reinterpret_cast<uint32_t *>(untyped_value);
  value = item;
}

void assign_function__SwarmControlCommand__selected_robot_ids(
  void * untyped_member, size_t index, const void * untyped_value)
{
  auto & item = *reinterpret_cast<uint32_t *>(
    get_function__SwarmControlCommand__selected_robot_ids(untyped_member, index));
  const auto & value = *reinterpret_cast<const uint32_t *>(untyped_value);
  item = value;
}

static const ::rosidl_typesupport_introspection_cpp::MessageMember SwarmControlCommand_message_member_array[6] = {
  {
    "header",  // name
    ::rosidl_typesupport_introspection_cpp::ROS_TYPE_MESSAGE,  // type
    0,  // upper bound of string
    ::rosidl_typesupport_introspection_cpp::get_message_type_support_handle<std_msgs::msg::Header>(),  // members of sub message
    false,  // is key
    false,  // is array
    0,  // array size
    false,  // is upper bound
    offsetof(combat_robot_msgs::msg::SwarmControlCommand, header),  // bytes offset in struct
    nullptr,  // default value
    nullptr,  // size() function pointer
    nullptr,  // get_const(index) function pointer
    nullptr,  // get(index) function pointer
    nullptr,  // fetch(index, &value) function pointer
    nullptr,  // assign(index, value) function pointer
    nullptr  // resize(index) function pointer
  },
  {
    "formation_type",  // name
    ::rosidl_typesupport_introspection_cpp::ROS_TYPE_UINT8,  // type
    0,  // upper bound of string
    nullptr,  // members of sub message
    false,  // is key
    false,  // is array
    0,  // array size
    false,  // is upper bound
    offsetof(combat_robot_msgs::msg::SwarmControlCommand, formation_type),  // bytes offset in struct
    nullptr,  // default value
    nullptr,  // size() function pointer
    nullptr,  // get_const(index) function pointer
    nullptr,  // get(index) function pointer
    nullptr,  // fetch(index, &value) function pointer
    nullptr,  // assign(index, value) function pointer
    nullptr  // resize(index) function pointer
  },
  {
    "formation_number",  // name
    ::rosidl_typesupport_introspection_cpp::ROS_TYPE_UINT8,  // type
    0,  // upper bound of string
    nullptr,  // members of sub message
    false,  // is key
    false,  // is array
    0,  // array size
    false,  // is upper bound
    offsetof(combat_robot_msgs::msg::SwarmControlCommand, formation_number),  // bytes offset in struct
    nullptr,  // default value
    nullptr,  // size() function pointer
    nullptr,  // get_const(index) function pointer
    nullptr,  // get(index) function pointer
    nullptr,  // fetch(index, &value) function pointer
    nullptr,  // assign(index, value) function pointer
    nullptr  // resize(index) function pointer
  },
  {
    "grouping_index",  // name
    ::rosidl_typesupport_introspection_cpp::ROS_TYPE_UINT8,  // type
    0,  // upper bound of string
    nullptr,  // members of sub message
    false,  // is key
    false,  // is array
    0,  // array size
    false,  // is upper bound
    offsetof(combat_robot_msgs::msg::SwarmControlCommand, grouping_index),  // bytes offset in struct
    nullptr,  // default value
    nullptr,  // size() function pointer
    nullptr,  // get_const(index) function pointer
    nullptr,  // get(index) function pointer
    nullptr,  // fetch(index, &value) function pointer
    nullptr,  // assign(index, value) function pointer
    nullptr  // resize(index) function pointer
  },
  {
    "selected_robot_count",  // name
    ::rosidl_typesupport_introspection_cpp::ROS_TYPE_UINT8,  // type
    0,  // upper bound of string
    nullptr,  // members of sub message
    false,  // is key
    false,  // is array
    0,  // array size
    false,  // is upper bound
    offsetof(combat_robot_msgs::msg::SwarmControlCommand, selected_robot_count),  // bytes offset in struct
    nullptr,  // default value
    nullptr,  // size() function pointer
    nullptr,  // get_const(index) function pointer
    nullptr,  // get(index) function pointer
    nullptr,  // fetch(index, &value) function pointer
    nullptr,  // assign(index, value) function pointer
    nullptr  // resize(index) function pointer
  },
  {
    "selected_robot_ids",  // name
    ::rosidl_typesupport_introspection_cpp::ROS_TYPE_UINT32,  // type
    0,  // upper bound of string
    nullptr,  // members of sub message
    false,  // is key
    true,  // is array
    8,  // array size
    false,  // is upper bound
    offsetof(combat_robot_msgs::msg::SwarmControlCommand, selected_robot_ids),  // bytes offset in struct
    nullptr,  // default value
    size_function__SwarmControlCommand__selected_robot_ids,  // size() function pointer
    get_const_function__SwarmControlCommand__selected_robot_ids,  // get_const(index) function pointer
    get_function__SwarmControlCommand__selected_robot_ids,  // get(index) function pointer
    fetch_function__SwarmControlCommand__selected_robot_ids,  // fetch(index, &value) function pointer
    assign_function__SwarmControlCommand__selected_robot_ids,  // assign(index, value) function pointer
    nullptr  // resize(index) function pointer
  }
};

static const ::rosidl_typesupport_introspection_cpp::MessageMembers SwarmControlCommand_message_members = {
  "combat_robot_msgs::msg",  // message namespace
  "SwarmControlCommand",  // message name
  6,  // number of fields
  sizeof(combat_robot_msgs::msg::SwarmControlCommand),
  false,  // has_any_key_member_
  SwarmControlCommand_message_member_array,  // message members
  SwarmControlCommand_init_function,  // function to initialize message memory (memory has to be allocated)
  SwarmControlCommand_fini_function  // function to terminate message instance (will not free memory)
};

static const rosidl_message_type_support_t SwarmControlCommand_message_type_support_handle = {
  ::rosidl_typesupport_introspection_cpp::typesupport_identifier,
  &SwarmControlCommand_message_members,
  get_message_typesupport_handle_function,
  &combat_robot_msgs__msg__SwarmControlCommand__get_type_hash,
  &combat_robot_msgs__msg__SwarmControlCommand__get_type_description,
  &combat_robot_msgs__msg__SwarmControlCommand__get_type_description_sources,
};

}  // namespace rosidl_typesupport_introspection_cpp

}  // namespace msg

}  // namespace combat_robot_msgs


namespace rosidl_typesupport_introspection_cpp
{

template<>
ROSIDL_TYPESUPPORT_INTROSPECTION_CPP_PUBLIC
const rosidl_message_type_support_t *
get_message_type_support_handle<combat_robot_msgs::msg::SwarmControlCommand>()
{
  return &::combat_robot_msgs::msg::rosidl_typesupport_introspection_cpp::SwarmControlCommand_message_type_support_handle;
}

}  // namespace rosidl_typesupport_introspection_cpp

#ifdef __cplusplus
extern "C"
{
#endif

ROSIDL_TYPESUPPORT_INTROSPECTION_CPP_PUBLIC
const rosidl_message_type_support_t *
ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(rosidl_typesupport_introspection_cpp, combat_robot_msgs, msg, SwarmControlCommand)() {
  return &::combat_robot_msgs::msg::rosidl_typesupport_introspection_cpp::SwarmControlCommand_message_type_support_handle;
}

#ifdef __cplusplus
}
#endif
