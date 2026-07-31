// generated from rosidl_generator_c/resource/idl__functions.c.em
// with input from combat_robot_msgs:msg/SwarmFollowerStatus.idl
// generated code does not contain a copyright notice
#include "combat_robot_msgs/msg/detail/swarm_follower_status__functions.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "rcutils/allocator.h"


// Include directives for member types
// Member `header`
#include "std_msgs/msg/detail/header__functions.h"

bool
combat_robot_msgs__msg__SwarmFollowerStatus__init(combat_robot_msgs__msg__SwarmFollowerStatus * msg)
{
  if (!msg) {
    return false;
  }
  // header
  if (!std_msgs__msg__Header__init(&msg->header)) {
    combat_robot_msgs__msg__SwarmFollowerStatus__fini(msg);
    return false;
  }
  // robot_id
  // leader_robot_id
  // link_status
  // last_heartbeat_sequence
  // heartbeat_age_sec
  // last_operation_mode
  // last_formation_type
  // last_formation_number
  // last_grouping_index
  // latitude
  // longitude
  // heading_deg
  // ground_speed_mps
  return true;
}

void
combat_robot_msgs__msg__SwarmFollowerStatus__fini(combat_robot_msgs__msg__SwarmFollowerStatus * msg)
{
  if (!msg) {
    return;
  }
  // header
  std_msgs__msg__Header__fini(&msg->header);
  // robot_id
  // leader_robot_id
  // link_status
  // last_heartbeat_sequence
  // heartbeat_age_sec
  // last_operation_mode
  // last_formation_type
  // last_formation_number
  // last_grouping_index
  // latitude
  // longitude
  // heading_deg
  // ground_speed_mps
}

bool
combat_robot_msgs__msg__SwarmFollowerStatus__are_equal(const combat_robot_msgs__msg__SwarmFollowerStatus * lhs, const combat_robot_msgs__msg__SwarmFollowerStatus * rhs)
{
  if (!lhs || !rhs) {
    return false;
  }
  // header
  if (!std_msgs__msg__Header__are_equal(
      &(lhs->header), &(rhs->header)))
  {
    return false;
  }
  // robot_id
  if (lhs->robot_id != rhs->robot_id) {
    return false;
  }
  // leader_robot_id
  if (lhs->leader_robot_id != rhs->leader_robot_id) {
    return false;
  }
  // link_status
  if (lhs->link_status != rhs->link_status) {
    return false;
  }
  // last_heartbeat_sequence
  if (lhs->last_heartbeat_sequence != rhs->last_heartbeat_sequence) {
    return false;
  }
  // heartbeat_age_sec
  if (lhs->heartbeat_age_sec != rhs->heartbeat_age_sec) {
    return false;
  }
  // last_operation_mode
  if (lhs->last_operation_mode != rhs->last_operation_mode) {
    return false;
  }
  // last_formation_type
  if (lhs->last_formation_type != rhs->last_formation_type) {
    return false;
  }
  // last_formation_number
  if (lhs->last_formation_number != rhs->last_formation_number) {
    return false;
  }
  // last_grouping_index
  if (lhs->last_grouping_index != rhs->last_grouping_index) {
    return false;
  }
  // latitude
  if (lhs->latitude != rhs->latitude) {
    return false;
  }
  // longitude
  if (lhs->longitude != rhs->longitude) {
    return false;
  }
  // heading_deg
  if (lhs->heading_deg != rhs->heading_deg) {
    return false;
  }
  // ground_speed_mps
  if (lhs->ground_speed_mps != rhs->ground_speed_mps) {
    return false;
  }
  return true;
}

bool
combat_robot_msgs__msg__SwarmFollowerStatus__copy(
  const combat_robot_msgs__msg__SwarmFollowerStatus * input,
  combat_robot_msgs__msg__SwarmFollowerStatus * output)
{
  if (!input || !output) {
    return false;
  }
  // header
  if (!std_msgs__msg__Header__copy(
      &(input->header), &(output->header)))
  {
    return false;
  }
  // robot_id
  output->robot_id = input->robot_id;
  // leader_robot_id
  output->leader_robot_id = input->leader_robot_id;
  // link_status
  output->link_status = input->link_status;
  // last_heartbeat_sequence
  output->last_heartbeat_sequence = input->last_heartbeat_sequence;
  // heartbeat_age_sec
  output->heartbeat_age_sec = input->heartbeat_age_sec;
  // last_operation_mode
  output->last_operation_mode = input->last_operation_mode;
  // last_formation_type
  output->last_formation_type = input->last_formation_type;
  // last_formation_number
  output->last_formation_number = input->last_formation_number;
  // last_grouping_index
  output->last_grouping_index = input->last_grouping_index;
  // latitude
  output->latitude = input->latitude;
  // longitude
  output->longitude = input->longitude;
  // heading_deg
  output->heading_deg = input->heading_deg;
  // ground_speed_mps
  output->ground_speed_mps = input->ground_speed_mps;
  return true;
}

combat_robot_msgs__msg__SwarmFollowerStatus *
combat_robot_msgs__msg__SwarmFollowerStatus__create(void)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  combat_robot_msgs__msg__SwarmFollowerStatus * msg = (combat_robot_msgs__msg__SwarmFollowerStatus *)allocator.allocate(sizeof(combat_robot_msgs__msg__SwarmFollowerStatus), allocator.state);
  if (!msg) {
    return NULL;
  }
  memset(msg, 0, sizeof(combat_robot_msgs__msg__SwarmFollowerStatus));
  bool success = combat_robot_msgs__msg__SwarmFollowerStatus__init(msg);
  if (!success) {
    allocator.deallocate(msg, allocator.state);
    return NULL;
  }
  return msg;
}

void
combat_robot_msgs__msg__SwarmFollowerStatus__destroy(combat_robot_msgs__msg__SwarmFollowerStatus * msg)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  if (msg) {
    combat_robot_msgs__msg__SwarmFollowerStatus__fini(msg);
  }
  allocator.deallocate(msg, allocator.state);
}


bool
combat_robot_msgs__msg__SwarmFollowerStatus__Sequence__init(combat_robot_msgs__msg__SwarmFollowerStatus__Sequence * array, size_t size)
{
  if (!array) {
    return false;
  }
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  combat_robot_msgs__msg__SwarmFollowerStatus * data = NULL;

  if (size) {
    data = (combat_robot_msgs__msg__SwarmFollowerStatus *)allocator.zero_allocate(size, sizeof(combat_robot_msgs__msg__SwarmFollowerStatus), allocator.state);
    if (!data) {
      return false;
    }
    // initialize all array elements
    size_t i;
    for (i = 0; i < size; ++i) {
      bool success = combat_robot_msgs__msg__SwarmFollowerStatus__init(&data[i]);
      if (!success) {
        break;
      }
    }
    if (i < size) {
      // if initialization failed finalize the already initialized array elements
      for (; i > 0; --i) {
        combat_robot_msgs__msg__SwarmFollowerStatus__fini(&data[i - 1]);
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
combat_robot_msgs__msg__SwarmFollowerStatus__Sequence__fini(combat_robot_msgs__msg__SwarmFollowerStatus__Sequence * array)
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
      combat_robot_msgs__msg__SwarmFollowerStatus__fini(&array->data[i]);
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

combat_robot_msgs__msg__SwarmFollowerStatus__Sequence *
combat_robot_msgs__msg__SwarmFollowerStatus__Sequence__create(size_t size)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  combat_robot_msgs__msg__SwarmFollowerStatus__Sequence * array = (combat_robot_msgs__msg__SwarmFollowerStatus__Sequence *)allocator.allocate(sizeof(combat_robot_msgs__msg__SwarmFollowerStatus__Sequence), allocator.state);
  if (!array) {
    return NULL;
  }
  bool success = combat_robot_msgs__msg__SwarmFollowerStatus__Sequence__init(array, size);
  if (!success) {
    allocator.deallocate(array, allocator.state);
    return NULL;
  }
  return array;
}

void
combat_robot_msgs__msg__SwarmFollowerStatus__Sequence__destroy(combat_robot_msgs__msg__SwarmFollowerStatus__Sequence * array)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  if (array) {
    combat_robot_msgs__msg__SwarmFollowerStatus__Sequence__fini(array);
  }
  allocator.deallocate(array, allocator.state);
}

bool
combat_robot_msgs__msg__SwarmFollowerStatus__Sequence__are_equal(const combat_robot_msgs__msg__SwarmFollowerStatus__Sequence * lhs, const combat_robot_msgs__msg__SwarmFollowerStatus__Sequence * rhs)
{
  if (!lhs || !rhs) {
    return false;
  }
  if (lhs->size != rhs->size) {
    return false;
  }
  for (size_t i = 0; i < lhs->size; ++i) {
    if (!combat_robot_msgs__msg__SwarmFollowerStatus__are_equal(&(lhs->data[i]), &(rhs->data[i]))) {
      return false;
    }
  }
  return true;
}

bool
combat_robot_msgs__msg__SwarmFollowerStatus__Sequence__copy(
  const combat_robot_msgs__msg__SwarmFollowerStatus__Sequence * input,
  combat_robot_msgs__msg__SwarmFollowerStatus__Sequence * output)
{
  if (!input || !output) {
    return false;
  }
  if (output->capacity < input->size) {
    const size_t allocation_size =
      input->size * sizeof(combat_robot_msgs__msg__SwarmFollowerStatus);
    rcutils_allocator_t allocator = rcutils_get_default_allocator();
    combat_robot_msgs__msg__SwarmFollowerStatus * data =
      (combat_robot_msgs__msg__SwarmFollowerStatus *)allocator.reallocate(
      output->data, allocation_size, allocator.state);
    if (!data) {
      return false;
    }
    // If reallocation succeeded, memory may or may not have been moved
    // to fulfill the allocation request, invalidating output->data.
    output->data = data;
    for (size_t i = output->capacity; i < input->size; ++i) {
      if (!combat_robot_msgs__msg__SwarmFollowerStatus__init(&output->data[i])) {
        // If initialization of any new item fails, roll back
        // all previously initialized items. Existing items
        // in output are to be left unmodified.
        for (; i-- > output->capacity; ) {
          combat_robot_msgs__msg__SwarmFollowerStatus__fini(&output->data[i]);
        }
        return false;
      }
    }
    output->capacity = input->size;
  }
  output->size = input->size;
  for (size_t i = 0; i < input->size; ++i) {
    if (!combat_robot_msgs__msg__SwarmFollowerStatus__copy(
        &(input->data[i]), &(output->data[i])))
    {
      return false;
    }
  }
  return true;
}
