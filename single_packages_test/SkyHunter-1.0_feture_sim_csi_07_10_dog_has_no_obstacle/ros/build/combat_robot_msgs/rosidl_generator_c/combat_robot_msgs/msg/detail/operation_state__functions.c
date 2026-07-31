// generated from rosidl_generator_c/resource/idl__functions.c.em
// with input from combat_robot_msgs:msg/OperationState.idl
// generated code does not contain a copyright notice
#include "combat_robot_msgs/msg/detail/operation_state__functions.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "rcutils/allocator.h"


bool
combat_robot_msgs__msg__OperationState__init(combat_robot_msgs__msg__OperationState * msg)
{
  if (!msg) {
    return false;
  }
  // state
  // active_mode_id
  // mission_status
  // estop_active
  // permission_request_active
  // crosshair_x
  // crosshair_y
  // current_zoom_level
  // gps_lat
  // gps_lon
  // gps_heading
  // current_speed_mps
  // current_waypoint_index
  // total_waypoints
  // progress_ratio
  // distance_to_next_wp_m
  // distance_to_goal_m
  // error_code
  return true;
}

void
combat_robot_msgs__msg__OperationState__fini(combat_robot_msgs__msg__OperationState * msg)
{
  if (!msg) {
    return;
  }
  // state
  // active_mode_id
  // mission_status
  // estop_active
  // permission_request_active
  // crosshair_x
  // crosshair_y
  // current_zoom_level
  // gps_lat
  // gps_lon
  // gps_heading
  // current_speed_mps
  // current_waypoint_index
  // total_waypoints
  // progress_ratio
  // distance_to_next_wp_m
  // distance_to_goal_m
  // error_code
}

bool
combat_robot_msgs__msg__OperationState__are_equal(const combat_robot_msgs__msg__OperationState * lhs, const combat_robot_msgs__msg__OperationState * rhs)
{
  if (!lhs || !rhs) {
    return false;
  }
  // state
  if (lhs->state != rhs->state) {
    return false;
  }
  // active_mode_id
  if (lhs->active_mode_id != rhs->active_mode_id) {
    return false;
  }
  // mission_status
  if (lhs->mission_status != rhs->mission_status) {
    return false;
  }
  // estop_active
  if (lhs->estop_active != rhs->estop_active) {
    return false;
  }
  // permission_request_active
  if (lhs->permission_request_active != rhs->permission_request_active) {
    return false;
  }
  // crosshair_x
  if (lhs->crosshair_x != rhs->crosshair_x) {
    return false;
  }
  // crosshair_y
  if (lhs->crosshair_y != rhs->crosshair_y) {
    return false;
  }
  // current_zoom_level
  if (lhs->current_zoom_level != rhs->current_zoom_level) {
    return false;
  }
  // gps_lat
  if (lhs->gps_lat != rhs->gps_lat) {
    return false;
  }
  // gps_lon
  if (lhs->gps_lon != rhs->gps_lon) {
    return false;
  }
  // gps_heading
  if (lhs->gps_heading != rhs->gps_heading) {
    return false;
  }
  // current_speed_mps
  if (lhs->current_speed_mps != rhs->current_speed_mps) {
    return false;
  }
  // current_waypoint_index
  if (lhs->current_waypoint_index != rhs->current_waypoint_index) {
    return false;
  }
  // total_waypoints
  if (lhs->total_waypoints != rhs->total_waypoints) {
    return false;
  }
  // progress_ratio
  if (lhs->progress_ratio != rhs->progress_ratio) {
    return false;
  }
  // distance_to_next_wp_m
  if (lhs->distance_to_next_wp_m != rhs->distance_to_next_wp_m) {
    return false;
  }
  // distance_to_goal_m
  if (lhs->distance_to_goal_m != rhs->distance_to_goal_m) {
    return false;
  }
  // error_code
  if (lhs->error_code != rhs->error_code) {
    return false;
  }
  return true;
}

bool
combat_robot_msgs__msg__OperationState__copy(
  const combat_robot_msgs__msg__OperationState * input,
  combat_robot_msgs__msg__OperationState * output)
{
  if (!input || !output) {
    return false;
  }
  // state
  output->state = input->state;
  // active_mode_id
  output->active_mode_id = input->active_mode_id;
  // mission_status
  output->mission_status = input->mission_status;
  // estop_active
  output->estop_active = input->estop_active;
  // permission_request_active
  output->permission_request_active = input->permission_request_active;
  // crosshair_x
  output->crosshair_x = input->crosshair_x;
  // crosshair_y
  output->crosshair_y = input->crosshair_y;
  // current_zoom_level
  output->current_zoom_level = input->current_zoom_level;
  // gps_lat
  output->gps_lat = input->gps_lat;
  // gps_lon
  output->gps_lon = input->gps_lon;
  // gps_heading
  output->gps_heading = input->gps_heading;
  // current_speed_mps
  output->current_speed_mps = input->current_speed_mps;
  // current_waypoint_index
  output->current_waypoint_index = input->current_waypoint_index;
  // total_waypoints
  output->total_waypoints = input->total_waypoints;
  // progress_ratio
  output->progress_ratio = input->progress_ratio;
  // distance_to_next_wp_m
  output->distance_to_next_wp_m = input->distance_to_next_wp_m;
  // distance_to_goal_m
  output->distance_to_goal_m = input->distance_to_goal_m;
  // error_code
  output->error_code = input->error_code;
  return true;
}

combat_robot_msgs__msg__OperationState *
combat_robot_msgs__msg__OperationState__create(void)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  combat_robot_msgs__msg__OperationState * msg = (combat_robot_msgs__msg__OperationState *)allocator.allocate(sizeof(combat_robot_msgs__msg__OperationState), allocator.state);
  if (!msg) {
    return NULL;
  }
  memset(msg, 0, sizeof(combat_robot_msgs__msg__OperationState));
  bool success = combat_robot_msgs__msg__OperationState__init(msg);
  if (!success) {
    allocator.deallocate(msg, allocator.state);
    return NULL;
  }
  return msg;
}

void
combat_robot_msgs__msg__OperationState__destroy(combat_robot_msgs__msg__OperationState * msg)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  if (msg) {
    combat_robot_msgs__msg__OperationState__fini(msg);
  }
  allocator.deallocate(msg, allocator.state);
}


bool
combat_robot_msgs__msg__OperationState__Sequence__init(combat_robot_msgs__msg__OperationState__Sequence * array, size_t size)
{
  if (!array) {
    return false;
  }
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  combat_robot_msgs__msg__OperationState * data = NULL;

  if (size) {
    data = (combat_robot_msgs__msg__OperationState *)allocator.zero_allocate(size, sizeof(combat_robot_msgs__msg__OperationState), allocator.state);
    if (!data) {
      return false;
    }
    // initialize all array elements
    size_t i;
    for (i = 0; i < size; ++i) {
      bool success = combat_robot_msgs__msg__OperationState__init(&data[i]);
      if (!success) {
        break;
      }
    }
    if (i < size) {
      // if initialization failed finalize the already initialized array elements
      for (; i > 0; --i) {
        combat_robot_msgs__msg__OperationState__fini(&data[i - 1]);
      }
      allocator.deallocate(data, allocator.state);
      return false;
    }
  }
  array->data = data;
  array->size = size;
  array->capacity = size;
  return true;
}

void
combat_robot_msgs__msg__OperationState__Sequence__fini(combat_robot_msgs__msg__OperationState__Sequence * array)
{
  if (!array) {
    return;
  }
  rcutils_allocator_t allocator = rcutils_get_default_allocator();

  if (array->data) {
    // ensure that data and capacity values are consistent
    assert(array->capacity > 0);
    // finalize all array elements
    for (size_t i = 0; i < array->capacity; ++i) {
      combat_robot_msgs__msg__OperationState__fini(&array->data[i]);
    }
    allocator.deallocate(array->data, allocator.state);
    array->data = NULL;
    array->size = 0;
    array->capacity = 0;
  } else {
    // ensure that data, size, and capacity values are consistent
    assert(0 == array->size);
    assert(0 == array->capacity);
  }
}

combat_robot_msgs__msg__OperationState__Sequence *
combat_robot_msgs__msg__OperationState__Sequence__create(size_t size)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  combat_robot_msgs__msg__OperationState__Sequence * array = (combat_robot_msgs__msg__OperationState__Sequence *)allocator.allocate(sizeof(combat_robot_msgs__msg__OperationState__Sequence), allocator.state);
  if (!array) {
    return NULL;
  }
  bool success = combat_robot_msgs__msg__OperationState__Sequence__init(array, size);
  if (!success) {
    allocator.deallocate(array, allocator.state);
    return NULL;
  }
  return array;
}

void
combat_robot_msgs__msg__OperationState__Sequence__destroy(combat_robot_msgs__msg__OperationState__Sequence * array)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  if (array) {
    combat_robot_msgs__msg__OperationState__Sequence__fini(array);
  }
  allocator.deallocate(array, allocator.state);
}

bool
combat_robot_msgs__msg__OperationState__Sequence__are_equal(const combat_robot_msgs__msg__OperationState__Sequence * lhs, const combat_robot_msgs__msg__OperationState__Sequence * rhs)
{
  if (!lhs || !rhs) {
    return false;
  }
  if (lhs->size != rhs->size) {
    return false;
  }
  for (size_t i = 0; i < lhs->size; ++i) {
    if (!combat_robot_msgs__msg__OperationState__are_equal(&(lhs->data[i]), &(rhs->data[i]))) {
      return false;
    }
  }
  return true;
}

bool
combat_robot_msgs__msg__OperationState__Sequence__copy(
  const combat_robot_msgs__msg__OperationState__Sequence * input,
  combat_robot_msgs__msg__OperationState__Sequence * output)
{
  if (!input || !output) {
    return false;
  }
  if (output->capacity < input->size) {
    const size_t allocation_size =
      input->size * sizeof(combat_robot_msgs__msg__OperationState);
    rcutils_allocator_t allocator = rcutils_get_default_allocator();
    combat_robot_msgs__msg__OperationState * data =
      (combat_robot_msgs__msg__OperationState *)allocator.reallocate(
      output->data, allocation_size, allocator.state);
    if (!data) {
      return false;
    }
    // If reallocation succeeded, memory may or may not have been moved
    // to fulfill the allocation request, invalidating output->data.
    output->data = data;
    for (size_t i = output->capacity; i < input->size; ++i) {
      if (!combat_robot_msgs__msg__OperationState__init(&output->data[i])) {
        // If initialization of any new item fails, roll back
        // all previously initialized items. Existing items
        // in output are to be left unmodified.
        for (; i-- > output->capacity; ) {
          combat_robot_msgs__msg__OperationState__fini(&output->data[i]);
        }
        return false;
      }
    }
    output->capacity = input->size;
  }
  output->size = input->size;
  for (size_t i = 0; i < input->size; ++i) {
    if (!combat_robot_msgs__msg__OperationState__copy(
        &(input->data[i]), &(output->data[i])))
    {
      return false;
    }
  }
  return true;
}
