// generated from rosidl_typesupport_c/resource/idl__type_support.cpp.em
// with input from combat_robot_msgs:msg/IMUState.idl
// generated code does not contain a copyright notice

#include "cstddef"
#include "rosidl_runtime_c/message_type_support_struct.h"
#include "combat_robot_msgs/msg/detail/imu_state__struct.h"
#include "combat_robot_msgs/msg/detail/imu_state__type_support.h"
#include "combat_robot_msgs/msg/detail/imu_state__functions.h"
#include "rosidl_typesupport_c/identifier.h"
#include "rosidl_typesupport_c/message_type_support_dispatch.h"
#include "rosidl_typesupport_c/type_support_map.h"
#include "rosidl_typesupport_c/visibility_control.h"
#include "rosidl_typesupport_interface/macros.h"

namespace combat_robot_msgs
{

namespace msg
{

namespace rosidl_typesupport_c
{

typedef struct _IMUState_type_support_ids_t
{
  const char * typesupport_identifier[2];
} _IMUState_type_support_ids_t;

static const _IMUState_type_support_ids_t _IMUState_message_typesupport_ids = {
  {
    "rosidl_typesupport_fastrtps_c",  // ::rosidl_typesupport_fastrtps_c::typesupport_identifier,
    "rosidl_typesupport_introspection_c",  // ::rosidl_typesupport_introspection_c::typesupport_identifier,
  }
};

typedef struct _IMUState_type_support_symbol_names_t
{
  const char * symbol_name[2];
} _IMUState_type_support_symbol_names_t;

#define STRINGIFY_(s) #s
#define STRINGIFY(s) STRINGIFY_(s)

static const _IMUState_type_support_symbol_names_t _IMUState_message_typesupport_symbol_names = {
  {
    STRINGIFY(ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(rosidl_typesupport_fastrtps_c, combat_robot_msgs, msg, IMUState)),
    STRINGIFY(ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(rosidl_typesupport_introspection_c, combat_robot_msgs, msg, IMUState)),
  }
};

typedef struct _IMUState_type_support_data_t
{
  void * data[2];
} _IMUState_type_support_data_t;

static _IMUState_type_support_data_t _IMUState_message_typesupport_data = {
  {
    0,  // will store the shared library later
    0,  // will store the shared library later
  }
};

static const type_support_map_t _IMUState_message_typesupport_map = {
  2,
  "combat_robot_msgs",
  &_IMUState_message_typesupport_ids.typesupport_identifier[0],
  &_IMUState_message_typesupport_symbol_names.symbol_name[0],
  &_IMUState_message_typesupport_data.data[0],
};

static const rosidl_message_type_support_t IMUState_message_type_support_handle = {
  rosidl_typesupport_c__typesupport_identifier,
  reinterpret_cast<const type_support_map_t *>(&_IMUState_message_typesupport_map),
  rosidl_typesupport_c__get_message_typesupport_handle_function,
  &combat_robot_msgs__msg__IMUState__get_type_hash,
  &combat_robot_msgs__msg__IMUState__get_type_description,
  &combat_robot_msgs__msg__IMUState__get_type_description_sources,
};

}  // namespace rosidl_typesupport_c

}  // namespace msg

}  // namespace combat_robot_msgs

#ifdef __cplusplus
extern "C"
{
#endif

const rosidl_message_type_support_t *
ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(rosidl_typesupport_c, combat_robot_msgs, msg, IMUState)() {
  return &::combat_robot_msgs::msg::rosidl_typesupport_c::IMUState_message_type_support_handle;
}

#ifdef __cplusplus
}
#endif
