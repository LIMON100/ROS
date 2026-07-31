// generated from rosidl_typesupport_fastrtps_c/resource/idl__type_support_c.cpp.em
// with input from combat_robot_msgs:msg/OperationState.idl
// generated code does not contain a copyright notice
#include "combat_robot_msgs/msg/detail/operation_state__rosidl_typesupport_fastrtps_c.h"


#include <cassert>
#include <cstddef>
#include <limits>
#include <string>
#include "rosidl_typesupport_fastrtps_c/identifier.h"
#include "rosidl_typesupport_fastrtps_c/serialization_helpers.hpp"
#include "rosidl_typesupport_fastrtps_c/wstring_conversion.hpp"
#include "rosidl_typesupport_fastrtps_cpp/message_type_support.h"
#include "combat_robot_msgs/msg/rosidl_typesupport_fastrtps_c__visibility_control.h"
#include "combat_robot_msgs/msg/detail/operation_state__struct.h"
#include "combat_robot_msgs/msg/detail/operation_state__functions.h"
#include "fastcdr/Cdr.h"

#ifndef _WIN32
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wunused-parameter"
# ifdef __clang__
#  pragma clang diagnostic ignored "-Wdeprecated-register"
#  pragma clang diagnostic ignored "-Wreturn-type-c-linkage"
# endif
#endif
#ifndef _WIN32
# pragma GCC diagnostic pop
#endif

// includes and forward declarations of message dependencies and their conversion functions

#if defined(__cplusplus)
extern "C"
{
#endif


// forward declare type support functions


using _OperationState__ros_msg_type = combat_robot_msgs__msg__OperationState;


ROSIDL_TYPESUPPORT_FASTRTPS_C_PUBLIC_combat_robot_msgs
bool cdr_serialize_combat_robot_msgs__msg__OperationState(
  const combat_robot_msgs__msg__OperationState * ros_message,
  eprosima::fastcdr::Cdr & cdr)
{
  // Field name: state
  {
    cdr << ros_message->state;
  }

  // Field name: active_mode_id
  {
    cdr << ros_message->active_mode_id;
  }

  // Field name: mission_status
  {
    cdr << ros_message->mission_status;
  }

  // Field name: estop_active
  {
    cdr << (ros_message->estop_active ? true : false);
  }

  // Field name: permission_request_active
  {
    cdr << (ros_message->permission_request_active ? true : false);
  }

  // Field name: crosshair_x
  {
    cdr << ros_message->crosshair_x;
  }

  // Field name: crosshair_y
  {
    cdr << ros_message->crosshair_y;
  }

  // Field name: current_zoom_level
  {
    cdr << ros_message->current_zoom_level;
  }

  // Field name: gps_lat
  {
    cdr << ros_message->gps_lat;
  }

  // Field name: gps_lon
  {
    cdr << ros_message->gps_lon;
  }

  // Field name: gps_heading
  {
    cdr << ros_message->gps_heading;
  }

  // Field name: current_speed_mps
  {
    cdr << ros_message->current_speed_mps;
  }

  // Field name: current_waypoint_index
  {
    cdr << ros_message->current_waypoint_index;
  }

  // Field name: total_waypoints
  {
    cdr << ros_message->total_waypoints;
  }

  // Field name: progress_ratio
  {
    cdr << ros_message->progress_ratio;
  }

  // Field name: distance_to_next_wp_m
  {
    cdr << ros_message->distance_to_next_wp_m;
  }

  // Field name: distance_to_goal_m
  {
    cdr << ros_message->distance_to_goal_m;
  }

  // Field name: error_code
  {
    cdr << ros_message->error_code;
  }

  return true;
}

ROSIDL_TYPESUPPORT_FASTRTPS_C_PUBLIC_combat_robot_msgs
bool cdr_deserialize_combat_robot_msgs__msg__OperationState(
  eprosima::fastcdr::Cdr & cdr,
  combat_robot_msgs__msg__OperationState * ros_message)
{
  // Field name: state
  {
    cdr >> ros_message->state;
  }

  // Field name: active_mode_id
  {
    cdr >> ros_message->active_mode_id;
  }

  // Field name: mission_status
  {
    cdr >> ros_message->mission_status;
  }

  // Field name: estop_active
  {
    uint8_t tmp;
    cdr >> tmp;
    ros_message->estop_active = tmp ? true : false;
  }

  // Field name: permission_request_active
  {
    uint8_t tmp;
    cdr >> tmp;
    ros_message->permission_request_active = tmp ? true : false;
  }

  // Field name: crosshair_x
  {
    cdr >> ros_message->crosshair_x;
  }

  // Field name: crosshair_y
  {
    cdr >> ros_message->crosshair_y;
  }

  // Field name: current_zoom_level
  {
    cdr >> ros_message->current_zoom_level;
  }

  // Field name: gps_lat
  {
    cdr >> ros_message->gps_lat;
  }

  // Field name: gps_lon
  {
    cdr >> ros_message->gps_lon;
  }

  // Field name: gps_heading
  {
    cdr >> ros_message->gps_heading;
  }

  // Field name: current_speed_mps
  {
    cdr >> ros_message->current_speed_mps;
  }

  // Field name: current_waypoint_index
  {
    cdr >> ros_message->current_waypoint_index;
  }

  // Field name: total_waypoints
  {
    cdr >> ros_message->total_waypoints;
  }

  // Field name: progress_ratio
  {
    cdr >> ros_message->progress_ratio;
  }

  // Field name: distance_to_next_wp_m
  {
    cdr >> ros_message->distance_to_next_wp_m;
  }

  // Field name: distance_to_goal_m
  {
    cdr >> ros_message->distance_to_goal_m;
  }

  // Field name: error_code
  {
    cdr >> ros_message->error_code;
  }

  return true;
}  // NOLINT(readability/fn_size)


ROSIDL_TYPESUPPORT_FASTRTPS_C_PUBLIC_combat_robot_msgs
size_t get_serialized_size_combat_robot_msgs__msg__OperationState(
  const void * untyped_ros_message,
  size_t current_alignment)
{
  const _OperationState__ros_msg_type * ros_message = static_cast<const _OperationState__ros_msg_type *>(untyped_ros_message);
  (void)ros_message;
  size_t initial_alignment = current_alignment;

  const size_t padding = 4;
  const size_t wchar_size = 4;
  (void)padding;
  (void)wchar_size;

  // Field name: state
  {
    size_t item_size = sizeof(ros_message->state);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: active_mode_id
  {
    size_t item_size = sizeof(ros_message->active_mode_id);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: mission_status
  {
    size_t item_size = sizeof(ros_message->mission_status);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: estop_active
  {
    size_t item_size = sizeof(ros_message->estop_active);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: permission_request_active
  {
    size_t item_size = sizeof(ros_message->permission_request_active);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: crosshair_x
  {
    size_t item_size = sizeof(ros_message->crosshair_x);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: crosshair_y
  {
    size_t item_size = sizeof(ros_message->crosshair_y);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: current_zoom_level
  {
    size_t item_size = sizeof(ros_message->current_zoom_level);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: gps_lat
  {
    size_t item_size = sizeof(ros_message->gps_lat);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: gps_lon
  {
    size_t item_size = sizeof(ros_message->gps_lon);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: gps_heading
  {
    size_t item_size = sizeof(ros_message->gps_heading);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: current_speed_mps
  {
    size_t item_size = sizeof(ros_message->current_speed_mps);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: current_waypoint_index
  {
    size_t item_size = sizeof(ros_message->current_waypoint_index);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: total_waypoints
  {
    size_t item_size = sizeof(ros_message->total_waypoints);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: progress_ratio
  {
    size_t item_size = sizeof(ros_message->progress_ratio);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: distance_to_next_wp_m
  {
    size_t item_size = sizeof(ros_message->distance_to_next_wp_m);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: distance_to_goal_m
  {
    size_t item_size = sizeof(ros_message->distance_to_goal_m);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: error_code
  {
    size_t item_size = sizeof(ros_message->error_code);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  return current_alignment - initial_alignment;
}


ROSIDL_TYPESUPPORT_FASTRTPS_C_PUBLIC_combat_robot_msgs
size_t max_serialized_size_combat_robot_msgs__msg__OperationState(
  bool & full_bounded,
  bool & is_plain,
  size_t current_alignment)
{
  size_t initial_alignment = current_alignment;

  const size_t padding = 4;
  const size_t wchar_size = 4;
  size_t last_member_size = 0;
  (void)last_member_size;
  (void)padding;
  (void)wchar_size;

  full_bounded = true;
  is_plain = true;

  // Field name: state
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint8_t);
    current_alignment += array_size * sizeof(uint8_t);
  }

  // Field name: active_mode_id
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint8_t);
    current_alignment += array_size * sizeof(uint8_t);
  }

  // Field name: mission_status
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint8_t);
    current_alignment += array_size * sizeof(uint8_t);
  }

  // Field name: estop_active
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint8_t);
    current_alignment += array_size * sizeof(uint8_t);
  }

  // Field name: permission_request_active
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint8_t);
    current_alignment += array_size * sizeof(uint8_t);
  }

  // Field name: crosshair_x
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint32_t);
    current_alignment += array_size * sizeof(uint32_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint32_t));
  }

  // Field name: crosshair_y
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint32_t);
    current_alignment += array_size * sizeof(uint32_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint32_t));
  }

  // Field name: current_zoom_level
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint32_t);
    current_alignment += array_size * sizeof(uint32_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint32_t));
  }

  // Field name: gps_lat
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint64_t);
    current_alignment += array_size * sizeof(uint64_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint64_t));
  }

  // Field name: gps_lon
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint64_t);
    current_alignment += array_size * sizeof(uint64_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint64_t));
  }

  // Field name: gps_heading
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint32_t);
    current_alignment += array_size * sizeof(uint32_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint32_t));
  }

  // Field name: current_speed_mps
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint32_t);
    current_alignment += array_size * sizeof(uint32_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint32_t));
  }

  // Field name: current_waypoint_index
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint16_t);
    current_alignment += array_size * sizeof(uint16_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint16_t));
  }

  // Field name: total_waypoints
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint16_t);
    current_alignment += array_size * sizeof(uint16_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint16_t));
  }

  // Field name: progress_ratio
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint32_t);
    current_alignment += array_size * sizeof(uint32_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint32_t));
  }

  // Field name: distance_to_next_wp_m
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint32_t);
    current_alignment += array_size * sizeof(uint32_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint32_t));
  }

  // Field name: distance_to_goal_m
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint32_t);
    current_alignment += array_size * sizeof(uint32_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint32_t));
  }

  // Field name: error_code
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint8_t);
    current_alignment += array_size * sizeof(uint8_t);
  }


  size_t ret_val = current_alignment - initial_alignment;
  if (is_plain) {
    // All members are plain, and type is not empty.
    // We still need to check that the in-memory alignment
    // is the same as the CDR mandated alignment.
    using DataType = combat_robot_msgs__msg__OperationState;
    is_plain =
      (
      offsetof(DataType, error_code) +
      last_member_size
      ) == ret_val;
  }
  return ret_val;
}

ROSIDL_TYPESUPPORT_FASTRTPS_C_PUBLIC_combat_robot_msgs
bool cdr_serialize_key_combat_robot_msgs__msg__OperationState(
  const combat_robot_msgs__msg__OperationState * ros_message,
  eprosima::fastcdr::Cdr & cdr)
{
  // Field name: state
  {
    cdr << ros_message->state;
  }

  // Field name: active_mode_id
  {
    cdr << ros_message->active_mode_id;
  }

  // Field name: mission_status
  {
    cdr << ros_message->mission_status;
  }

  // Field name: estop_active
  {
    cdr << (ros_message->estop_active ? true : false);
  }

  // Field name: permission_request_active
  {
    cdr << (ros_message->permission_request_active ? true : false);
  }

  // Field name: crosshair_x
  {
    cdr << ros_message->crosshair_x;
  }

  // Field name: crosshair_y
  {
    cdr << ros_message->crosshair_y;
  }

  // Field name: current_zoom_level
  {
    cdr << ros_message->current_zoom_level;
  }

  // Field name: gps_lat
  {
    cdr << ros_message->gps_lat;
  }

  // Field name: gps_lon
  {
    cdr << ros_message->gps_lon;
  }

  // Field name: gps_heading
  {
    cdr << ros_message->gps_heading;
  }

  // Field name: current_speed_mps
  {
    cdr << ros_message->current_speed_mps;
  }

  // Field name: current_waypoint_index
  {
    cdr << ros_message->current_waypoint_index;
  }

  // Field name: total_waypoints
  {
    cdr << ros_message->total_waypoints;
  }

  // Field name: progress_ratio
  {
    cdr << ros_message->progress_ratio;
  }

  // Field name: distance_to_next_wp_m
  {
    cdr << ros_message->distance_to_next_wp_m;
  }

  // Field name: distance_to_goal_m
  {
    cdr << ros_message->distance_to_goal_m;
  }

  // Field name: error_code
  {
    cdr << ros_message->error_code;
  }

  return true;
}

ROSIDL_TYPESUPPORT_FASTRTPS_C_PUBLIC_combat_robot_msgs
size_t get_serialized_size_key_combat_robot_msgs__msg__OperationState(
  const void * untyped_ros_message,
  size_t current_alignment)
{
  const _OperationState__ros_msg_type * ros_message = static_cast<const _OperationState__ros_msg_type *>(untyped_ros_message);
  (void)ros_message;

  size_t initial_alignment = current_alignment;

  const size_t padding = 4;
  const size_t wchar_size = 4;
  (void)padding;
  (void)wchar_size;

  // Field name: state
  {
    size_t item_size = sizeof(ros_message->state);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: active_mode_id
  {
    size_t item_size = sizeof(ros_message->active_mode_id);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: mission_status
  {
    size_t item_size = sizeof(ros_message->mission_status);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: estop_active
  {
    size_t item_size = sizeof(ros_message->estop_active);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: permission_request_active
  {
    size_t item_size = sizeof(ros_message->permission_request_active);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: crosshair_x
  {
    size_t item_size = sizeof(ros_message->crosshair_x);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: crosshair_y
  {
    size_t item_size = sizeof(ros_message->crosshair_y);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: current_zoom_level
  {
    size_t item_size = sizeof(ros_message->current_zoom_level);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: gps_lat
  {
    size_t item_size = sizeof(ros_message->gps_lat);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: gps_lon
  {
    size_t item_size = sizeof(ros_message->gps_lon);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: gps_heading
  {
    size_t item_size = sizeof(ros_message->gps_heading);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: current_speed_mps
  {
    size_t item_size = sizeof(ros_message->current_speed_mps);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: current_waypoint_index
  {
    size_t item_size = sizeof(ros_message->current_waypoint_index);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: total_waypoints
  {
    size_t item_size = sizeof(ros_message->total_waypoints);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: progress_ratio
  {
    size_t item_size = sizeof(ros_message->progress_ratio);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: distance_to_next_wp_m
  {
    size_t item_size = sizeof(ros_message->distance_to_next_wp_m);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: distance_to_goal_m
  {
    size_t item_size = sizeof(ros_message->distance_to_goal_m);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Field name: error_code
  {
    size_t item_size = sizeof(ros_message->error_code);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  return current_alignment - initial_alignment;
}

ROSIDL_TYPESUPPORT_FASTRTPS_C_PUBLIC_combat_robot_msgs
size_t max_serialized_size_key_combat_robot_msgs__msg__OperationState(
  bool & full_bounded,
  bool & is_plain,
  size_t current_alignment)
{
  size_t initial_alignment = current_alignment;

  const size_t padding = 4;
  const size_t wchar_size = 4;
  size_t last_member_size = 0;
  (void)last_member_size;
  (void)padding;
  (void)wchar_size;

  full_bounded = true;
  is_plain = true;
  // Field name: state
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint8_t);
    current_alignment += array_size * sizeof(uint8_t);
  }

  // Field name: active_mode_id
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint8_t);
    current_alignment += array_size * sizeof(uint8_t);
  }

  // Field name: mission_status
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint8_t);
    current_alignment += array_size * sizeof(uint8_t);
  }

  // Field name: estop_active
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint8_t);
    current_alignment += array_size * sizeof(uint8_t);
  }

  // Field name: permission_request_active
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint8_t);
    current_alignment += array_size * sizeof(uint8_t);
  }

  // Field name: crosshair_x
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint32_t);
    current_alignment += array_size * sizeof(uint32_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint32_t));
  }

  // Field name: crosshair_y
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint32_t);
    current_alignment += array_size * sizeof(uint32_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint32_t));
  }

  // Field name: current_zoom_level
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint32_t);
    current_alignment += array_size * sizeof(uint32_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint32_t));
  }

  // Field name: gps_lat
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint64_t);
    current_alignment += array_size * sizeof(uint64_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint64_t));
  }

  // Field name: gps_lon
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint64_t);
    current_alignment += array_size * sizeof(uint64_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint64_t));
  }

  // Field name: gps_heading
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint32_t);
    current_alignment += array_size * sizeof(uint32_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint32_t));
  }

  // Field name: current_speed_mps
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint32_t);
    current_alignment += array_size * sizeof(uint32_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint32_t));
  }

  // Field name: current_waypoint_index
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint16_t);
    current_alignment += array_size * sizeof(uint16_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint16_t));
  }

  // Field name: total_waypoints
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint16_t);
    current_alignment += array_size * sizeof(uint16_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint16_t));
  }

  // Field name: progress_ratio
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint32_t);
    current_alignment += array_size * sizeof(uint32_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint32_t));
  }

  // Field name: distance_to_next_wp_m
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint32_t);
    current_alignment += array_size * sizeof(uint32_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint32_t));
  }

  // Field name: distance_to_goal_m
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint32_t);
    current_alignment += array_size * sizeof(uint32_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint32_t));
  }

  // Field name: error_code
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint8_t);
    current_alignment += array_size * sizeof(uint8_t);
  }

  size_t ret_val = current_alignment - initial_alignment;
  if (is_plain) {
    // All members are plain, and type is not empty.
    // We still need to check that the in-memory alignment
    // is the same as the CDR mandated alignment.
    using DataType = combat_robot_msgs__msg__OperationState;
    is_plain =
      (
      offsetof(DataType, error_code) +
      last_member_size
      ) == ret_val;
  }
  return ret_val;
}


static bool _OperationState__cdr_serialize(
  const void * untyped_ros_message,
  eprosima::fastcdr::Cdr & cdr)
{
  if (!untyped_ros_message) {
    fprintf(stderr, "ros message handle is null\n");
    return false;
  }
  const combat_robot_msgs__msg__OperationState * ros_message = static_cast<const combat_robot_msgs__msg__OperationState *>(untyped_ros_message);
  (void)ros_message;
  return cdr_serialize_combat_robot_msgs__msg__OperationState(ros_message, cdr);
}

static bool _OperationState__cdr_deserialize(
  eprosima::fastcdr::Cdr & cdr,
  void * untyped_ros_message)
{
  if (!untyped_ros_message) {
    fprintf(stderr, "ros message handle is null\n");
    return false;
  }
  combat_robot_msgs__msg__OperationState * ros_message = static_cast<combat_robot_msgs__msg__OperationState *>(untyped_ros_message);
  (void)ros_message;
  return cdr_deserialize_combat_robot_msgs__msg__OperationState(cdr, ros_message);
}

static uint32_t _OperationState__get_serialized_size(const void * untyped_ros_message)
{
  return static_cast<uint32_t>(
    get_serialized_size_combat_robot_msgs__msg__OperationState(
      untyped_ros_message, 0));
}

static size_t _OperationState__max_serialized_size(char & bounds_info)
{
  bool full_bounded;
  bool is_plain;
  size_t ret_val;

  ret_val = max_serialized_size_combat_robot_msgs__msg__OperationState(
    full_bounded, is_plain, 0);

  bounds_info =
    is_plain ? ROSIDL_TYPESUPPORT_FASTRTPS_PLAIN_TYPE :
    full_bounded ? ROSIDL_TYPESUPPORT_FASTRTPS_BOUNDED_TYPE : ROSIDL_TYPESUPPORT_FASTRTPS_UNBOUNDED_TYPE;
  return ret_val;
}


static message_type_support_callbacks_t __callbacks_OperationState = {
  "combat_robot_msgs::msg",
  "OperationState",
  _OperationState__cdr_serialize,
  _OperationState__cdr_deserialize,
  _OperationState__get_serialized_size,
  _OperationState__max_serialized_size,
  nullptr
};

static rosidl_message_type_support_t _OperationState__type_support = {
  rosidl_typesupport_fastrtps_c__identifier,
  &__callbacks_OperationState,
  get_message_typesupport_handle_function,
  &combat_robot_msgs__msg__OperationState__get_type_hash,
  &combat_robot_msgs__msg__OperationState__get_type_description,
  &combat_robot_msgs__msg__OperationState__get_type_description_sources,
};

const rosidl_message_type_support_t *
ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(rosidl_typesupport_fastrtps_c, combat_robot_msgs, msg, OperationState)() {
  return &_OperationState__type_support;
}

#if defined(__cplusplus)
}
#endif
