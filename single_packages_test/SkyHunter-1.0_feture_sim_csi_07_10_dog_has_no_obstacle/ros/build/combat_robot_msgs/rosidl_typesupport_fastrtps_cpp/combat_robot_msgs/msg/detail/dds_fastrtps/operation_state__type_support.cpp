// generated from rosidl_typesupport_fastrtps_cpp/resource/idl__type_support.cpp.em
// with input from combat_robot_msgs:msg/OperationState.idl
// generated code does not contain a copyright notice
#include "combat_robot_msgs/msg/detail/operation_state__rosidl_typesupport_fastrtps_cpp.hpp"
#include "combat_robot_msgs/msg/detail/operation_state__functions.h"
#include "combat_robot_msgs/msg/detail/operation_state__struct.hpp"

#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>
#include "rosidl_typesupport_cpp/message_type_support.hpp"
#include "rosidl_typesupport_fastrtps_cpp/identifier.hpp"
#include "rosidl_typesupport_fastrtps_cpp/message_type_support.h"
#include "rosidl_typesupport_fastrtps_cpp/message_type_support_decl.hpp"
#include "rosidl_typesupport_fastrtps_cpp/serialization_helpers.hpp"
#include "rosidl_typesupport_fastrtps_cpp/wstring_conversion.hpp"
#include "fastcdr/Cdr.h"


// forward declaration of message dependencies and their conversion functions

namespace combat_robot_msgs
{

namespace msg
{

namespace typesupport_fastrtps_cpp
{


bool
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_combat_robot_msgs
cdr_serialize(
  const combat_robot_msgs::msg::OperationState & ros_message,
  eprosima::fastcdr::Cdr & cdr)
{
  // Member: state
  cdr << ros_message.state;

  // Member: active_mode_id
  cdr << ros_message.active_mode_id;

  // Member: mission_status
  cdr << ros_message.mission_status;

  // Member: estop_active
  cdr << (ros_message.estop_active ? true : false);

  // Member: permission_request_active
  cdr << (ros_message.permission_request_active ? true : false);

  // Member: crosshair_x
  cdr << ros_message.crosshair_x;

  // Member: crosshair_y
  cdr << ros_message.crosshair_y;

  // Member: current_zoom_level
  cdr << ros_message.current_zoom_level;

  // Member: gps_lat
  cdr << ros_message.gps_lat;

  // Member: gps_lon
  cdr << ros_message.gps_lon;

  // Member: gps_heading
  cdr << ros_message.gps_heading;

  // Member: current_speed_mps
  cdr << ros_message.current_speed_mps;

  // Member: current_waypoint_index
  cdr << ros_message.current_waypoint_index;

  // Member: total_waypoints
  cdr << ros_message.total_waypoints;

  // Member: progress_ratio
  cdr << ros_message.progress_ratio;

  // Member: distance_to_next_wp_m
  cdr << ros_message.distance_to_next_wp_m;

  // Member: distance_to_goal_m
  cdr << ros_message.distance_to_goal_m;

  // Member: error_code
  cdr << ros_message.error_code;

  return true;
}

bool
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_combat_robot_msgs
cdr_deserialize(
  eprosima::fastcdr::Cdr & cdr,
  combat_robot_msgs::msg::OperationState & ros_message)
{
  // Member: state
  cdr >> ros_message.state;

  // Member: active_mode_id
  cdr >> ros_message.active_mode_id;

  // Member: mission_status
  cdr >> ros_message.mission_status;

  // Member: estop_active
  {
    uint8_t tmp;
    cdr >> tmp;
    ros_message.estop_active = tmp ? true : false;
  }

  // Member: permission_request_active
  {
    uint8_t tmp;
    cdr >> tmp;
    ros_message.permission_request_active = tmp ? true : false;
  }

  // Member: crosshair_x
  cdr >> ros_message.crosshair_x;

  // Member: crosshair_y
  cdr >> ros_message.crosshair_y;

  // Member: current_zoom_level
  cdr >> ros_message.current_zoom_level;

  // Member: gps_lat
  cdr >> ros_message.gps_lat;

  // Member: gps_lon
  cdr >> ros_message.gps_lon;

  // Member: gps_heading
  cdr >> ros_message.gps_heading;

  // Member: current_speed_mps
  cdr >> ros_message.current_speed_mps;

  // Member: current_waypoint_index
  cdr >> ros_message.current_waypoint_index;

  // Member: total_waypoints
  cdr >> ros_message.total_waypoints;

  // Member: progress_ratio
  cdr >> ros_message.progress_ratio;

  // Member: distance_to_next_wp_m
  cdr >> ros_message.distance_to_next_wp_m;

  // Member: distance_to_goal_m
  cdr >> ros_message.distance_to_goal_m;

  // Member: error_code
  cdr >> ros_message.error_code;

  return true;
}  // NOLINT(readability/fn_size)


size_t
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_combat_robot_msgs
get_serialized_size(
  const combat_robot_msgs::msg::OperationState & ros_message,
  size_t current_alignment)
{
  size_t initial_alignment = current_alignment;

  const size_t padding = 4;
  const size_t wchar_size = 4;
  (void)padding;
  (void)wchar_size;

  // Member: state
  {
    size_t item_size = sizeof(ros_message.state);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Member: active_mode_id
  {
    size_t item_size = sizeof(ros_message.active_mode_id);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Member: mission_status
  {
    size_t item_size = sizeof(ros_message.mission_status);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Member: estop_active
  {
    size_t item_size = sizeof(ros_message.estop_active);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Member: permission_request_active
  {
    size_t item_size = sizeof(ros_message.permission_request_active);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Member: crosshair_x
  {
    size_t item_size = sizeof(ros_message.crosshair_x);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Member: crosshair_y
  {
    size_t item_size = sizeof(ros_message.crosshair_y);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Member: current_zoom_level
  {
    size_t item_size = sizeof(ros_message.current_zoom_level);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Member: gps_lat
  {
    size_t item_size = sizeof(ros_message.gps_lat);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Member: gps_lon
  {
    size_t item_size = sizeof(ros_message.gps_lon);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Member: gps_heading
  {
    size_t item_size = sizeof(ros_message.gps_heading);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Member: current_speed_mps
  {
    size_t item_size = sizeof(ros_message.current_speed_mps);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Member: current_waypoint_index
  {
    size_t item_size = sizeof(ros_message.current_waypoint_index);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Member: total_waypoints
  {
    size_t item_size = sizeof(ros_message.total_waypoints);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Member: progress_ratio
  {
    size_t item_size = sizeof(ros_message.progress_ratio);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Member: distance_to_next_wp_m
  {
    size_t item_size = sizeof(ros_message.distance_to_next_wp_m);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Member: distance_to_goal_m
  {
    size_t item_size = sizeof(ros_message.distance_to_goal_m);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Member: error_code
  {
    size_t item_size = sizeof(ros_message.error_code);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  return current_alignment - initial_alignment;
}


size_t
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_combat_robot_msgs
max_serialized_size_OperationState(
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

  // Member: state
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint8_t);
    current_alignment += array_size * sizeof(uint8_t);
  }
  // Member: active_mode_id
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint8_t);
    current_alignment += array_size * sizeof(uint8_t);
  }
  // Member: mission_status
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint8_t);
    current_alignment += array_size * sizeof(uint8_t);
  }
  // Member: estop_active
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint8_t);
    current_alignment += array_size * sizeof(uint8_t);
  }
  // Member: permission_request_active
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint8_t);
    current_alignment += array_size * sizeof(uint8_t);
  }
  // Member: crosshair_x
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint32_t);
    current_alignment += array_size * sizeof(uint32_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint32_t));
  }
  // Member: crosshair_y
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint32_t);
    current_alignment += array_size * sizeof(uint32_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint32_t));
  }
  // Member: current_zoom_level
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint32_t);
    current_alignment += array_size * sizeof(uint32_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint32_t));
  }
  // Member: gps_lat
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint64_t);
    current_alignment += array_size * sizeof(uint64_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint64_t));
  }
  // Member: gps_lon
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint64_t);
    current_alignment += array_size * sizeof(uint64_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint64_t));
  }
  // Member: gps_heading
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint32_t);
    current_alignment += array_size * sizeof(uint32_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint32_t));
  }
  // Member: current_speed_mps
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint32_t);
    current_alignment += array_size * sizeof(uint32_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint32_t));
  }
  // Member: current_waypoint_index
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint16_t);
    current_alignment += array_size * sizeof(uint16_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint16_t));
  }
  // Member: total_waypoints
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint16_t);
    current_alignment += array_size * sizeof(uint16_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint16_t));
  }
  // Member: progress_ratio
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint32_t);
    current_alignment += array_size * sizeof(uint32_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint32_t));
  }
  // Member: distance_to_next_wp_m
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint32_t);
    current_alignment += array_size * sizeof(uint32_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint32_t));
  }
  // Member: distance_to_goal_m
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint32_t);
    current_alignment += array_size * sizeof(uint32_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint32_t));
  }
  // Member: error_code
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
    using DataType = combat_robot_msgs::msg::OperationState;
    is_plain =
      (
      offsetof(DataType, error_code) +
      last_member_size
      ) == ret_val;
  }

  return ret_val;
}

bool
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_combat_robot_msgs
cdr_serialize_key(
  const combat_robot_msgs::msg::OperationState & ros_message,
  eprosima::fastcdr::Cdr & cdr)
{
  // Member: state
  cdr << ros_message.state;

  // Member: active_mode_id
  cdr << ros_message.active_mode_id;

  // Member: mission_status
  cdr << ros_message.mission_status;

  // Member: estop_active
  cdr << (ros_message.estop_active ? true : false);

  // Member: permission_request_active
  cdr << (ros_message.permission_request_active ? true : false);

  // Member: crosshair_x
  cdr << ros_message.crosshair_x;

  // Member: crosshair_y
  cdr << ros_message.crosshair_y;

  // Member: current_zoom_level
  cdr << ros_message.current_zoom_level;

  // Member: gps_lat
  cdr << ros_message.gps_lat;

  // Member: gps_lon
  cdr << ros_message.gps_lon;

  // Member: gps_heading
  cdr << ros_message.gps_heading;

  // Member: current_speed_mps
  cdr << ros_message.current_speed_mps;

  // Member: current_waypoint_index
  cdr << ros_message.current_waypoint_index;

  // Member: total_waypoints
  cdr << ros_message.total_waypoints;

  // Member: progress_ratio
  cdr << ros_message.progress_ratio;

  // Member: distance_to_next_wp_m
  cdr << ros_message.distance_to_next_wp_m;

  // Member: distance_to_goal_m
  cdr << ros_message.distance_to_goal_m;

  // Member: error_code
  cdr << ros_message.error_code;

  return true;
}

size_t
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_combat_robot_msgs
get_serialized_size_key(
  const combat_robot_msgs::msg::OperationState & ros_message,
  size_t current_alignment)
{
  size_t initial_alignment = current_alignment;

  const size_t padding = 4;
  const size_t wchar_size = 4;
  (void)padding;
  (void)wchar_size;

  // Member: state
  {
    size_t item_size = sizeof(ros_message.state);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Member: active_mode_id
  {
    size_t item_size = sizeof(ros_message.active_mode_id);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Member: mission_status
  {
    size_t item_size = sizeof(ros_message.mission_status);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Member: estop_active
  {
    size_t item_size = sizeof(ros_message.estop_active);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Member: permission_request_active
  {
    size_t item_size = sizeof(ros_message.permission_request_active);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Member: crosshair_x
  {
    size_t item_size = sizeof(ros_message.crosshair_x);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Member: crosshair_y
  {
    size_t item_size = sizeof(ros_message.crosshair_y);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Member: current_zoom_level
  {
    size_t item_size = sizeof(ros_message.current_zoom_level);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Member: gps_lat
  {
    size_t item_size = sizeof(ros_message.gps_lat);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Member: gps_lon
  {
    size_t item_size = sizeof(ros_message.gps_lon);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Member: gps_heading
  {
    size_t item_size = sizeof(ros_message.gps_heading);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Member: current_speed_mps
  {
    size_t item_size = sizeof(ros_message.current_speed_mps);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Member: current_waypoint_index
  {
    size_t item_size = sizeof(ros_message.current_waypoint_index);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Member: total_waypoints
  {
    size_t item_size = sizeof(ros_message.total_waypoints);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Member: progress_ratio
  {
    size_t item_size = sizeof(ros_message.progress_ratio);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Member: distance_to_next_wp_m
  {
    size_t item_size = sizeof(ros_message.distance_to_next_wp_m);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Member: distance_to_goal_m
  {
    size_t item_size = sizeof(ros_message.distance_to_goal_m);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  // Member: error_code
  {
    size_t item_size = sizeof(ros_message.error_code);
    current_alignment += item_size +
      eprosima::fastcdr::Cdr::alignment(current_alignment, item_size);
  }

  return current_alignment - initial_alignment;
}

size_t
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_combat_robot_msgs
max_serialized_size_key_OperationState(
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

  // Member: state
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint8_t);
    current_alignment += array_size * sizeof(uint8_t);
  }

  // Member: active_mode_id
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint8_t);
    current_alignment += array_size * sizeof(uint8_t);
  }

  // Member: mission_status
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint8_t);
    current_alignment += array_size * sizeof(uint8_t);
  }

  // Member: estop_active
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint8_t);
    current_alignment += array_size * sizeof(uint8_t);
  }

  // Member: permission_request_active
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint8_t);
    current_alignment += array_size * sizeof(uint8_t);
  }

  // Member: crosshair_x
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint32_t);
    current_alignment += array_size * sizeof(uint32_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint32_t));
  }

  // Member: crosshair_y
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint32_t);
    current_alignment += array_size * sizeof(uint32_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint32_t));
  }

  // Member: current_zoom_level
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint32_t);
    current_alignment += array_size * sizeof(uint32_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint32_t));
  }

  // Member: gps_lat
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint64_t);
    current_alignment += array_size * sizeof(uint64_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint64_t));
  }

  // Member: gps_lon
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint64_t);
    current_alignment += array_size * sizeof(uint64_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint64_t));
  }

  // Member: gps_heading
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint32_t);
    current_alignment += array_size * sizeof(uint32_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint32_t));
  }

  // Member: current_speed_mps
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint32_t);
    current_alignment += array_size * sizeof(uint32_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint32_t));
  }

  // Member: current_waypoint_index
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint16_t);
    current_alignment += array_size * sizeof(uint16_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint16_t));
  }

  // Member: total_waypoints
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint16_t);
    current_alignment += array_size * sizeof(uint16_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint16_t));
  }

  // Member: progress_ratio
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint32_t);
    current_alignment += array_size * sizeof(uint32_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint32_t));
  }

  // Member: distance_to_next_wp_m
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint32_t);
    current_alignment += array_size * sizeof(uint32_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint32_t));
  }

  // Member: distance_to_goal_m
  {
    size_t array_size = 1;
    last_member_size = array_size * sizeof(uint32_t);
    current_alignment += array_size * sizeof(uint32_t) +
      eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(uint32_t));
  }

  // Member: error_code
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
    using DataType = combat_robot_msgs::msg::OperationState;
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
  auto typed_message =
    static_cast<const combat_robot_msgs::msg::OperationState *>(
    untyped_ros_message);
  return cdr_serialize(*typed_message, cdr);
}

static bool _OperationState__cdr_deserialize(
  eprosima::fastcdr::Cdr & cdr,
  void * untyped_ros_message)
{
  auto typed_message =
    static_cast<combat_robot_msgs::msg::OperationState *>(
    untyped_ros_message);
  return cdr_deserialize(cdr, *typed_message);
}

static uint32_t _OperationState__get_serialized_size(
  const void * untyped_ros_message)
{
  auto typed_message =
    static_cast<const combat_robot_msgs::msg::OperationState *>(
    untyped_ros_message);
  return static_cast<uint32_t>(get_serialized_size(*typed_message, 0));
}

static size_t _OperationState__max_serialized_size(char & bounds_info)
{
  bool full_bounded;
  bool is_plain;
  size_t ret_val;

  ret_val = max_serialized_size_OperationState(full_bounded, is_plain, 0);

  bounds_info =
    is_plain ? ROSIDL_TYPESUPPORT_FASTRTPS_PLAIN_TYPE :
    full_bounded ? ROSIDL_TYPESUPPORT_FASTRTPS_BOUNDED_TYPE : ROSIDL_TYPESUPPORT_FASTRTPS_UNBOUNDED_TYPE;
  return ret_val;
}

static message_type_support_callbacks_t _OperationState__callbacks = {
  "combat_robot_msgs::msg",
  "OperationState",
  _OperationState__cdr_serialize,
  _OperationState__cdr_deserialize,
  _OperationState__get_serialized_size,
  _OperationState__max_serialized_size,
  nullptr
};

static rosidl_message_type_support_t _OperationState__handle = {
  rosidl_typesupport_fastrtps_cpp::typesupport_identifier,
  &_OperationState__callbacks,
  get_message_typesupport_handle_function,
  &combat_robot_msgs__msg__OperationState__get_type_hash,
  &combat_robot_msgs__msg__OperationState__get_type_description,
  &combat_robot_msgs__msg__OperationState__get_type_description_sources,
};

}  // namespace typesupport_fastrtps_cpp

}  // namespace msg

}  // namespace combat_robot_msgs

namespace rosidl_typesupport_fastrtps_cpp
{

template<>
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_EXPORT_combat_robot_msgs
const rosidl_message_type_support_t *
get_message_type_support_handle<combat_robot_msgs::msg::OperationState>()
{
  return &combat_robot_msgs::msg::typesupport_fastrtps_cpp::_OperationState__handle;
}

}  // namespace rosidl_typesupport_fastrtps_cpp

#ifdef __cplusplus
extern "C"
{
#endif

const rosidl_message_type_support_t *
ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(rosidl_typesupport_fastrtps_cpp, combat_robot_msgs, msg, OperationState)() {
  return &combat_robot_msgs::msg::typesupport_fastrtps_cpp::_OperationState__handle;
}

#ifdef __cplusplus
}
#endif
