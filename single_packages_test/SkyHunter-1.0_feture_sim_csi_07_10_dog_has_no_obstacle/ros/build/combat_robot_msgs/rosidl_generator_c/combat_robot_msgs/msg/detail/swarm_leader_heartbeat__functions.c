// generated from rosidl_generator_c/resource/idl__functions.c.em
// with input from combat_robot_msgs:msg/SwarmLeaderHeartbeat.idl
// generated code does not contain a copyright notice
#include "combat_robot_msgs/msg/detail/swarm_leader_heartbeat__functions.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "rcutils/allocator.h"


// Include directives for member types
// Member `header`
#include "std_msgs/msg/detail/header__functions.h"

bool
combat_robot_msgs__msg__SwarmLeaderHeartbeat__init(combat_robot_msgs__msg__SwarmLeaderHeartbeat * msg)
{
  if (!msg) {
    return false;
  }
  // header
  if (!std_msgs__msg__Header__init(&msg->header)) {
    combat_robot_msgs__msg__SwarmLeaderHeartbeat__fini(msg);
    return false;
  }
  // sequence
  // leader_robot_id
  // operation_mode
  // estop_active
  // formation_type
  // formation_number
  // grouping_index
  // selected_robot_count
  // selected_robot_ids
  return true;
}

void
combat_robot_msgs__msg__SwarmLeaderHeartbeat__fini(combat_robot_msgs__msg__SwarmLeaderHeartbeat * msg)
{
  if (!msg) {
    return;
  }
  // header
  std_msgs__msg__Header__fini(&msg->header);
  // sequence
  // leader_robot_id
  // operation_mode
  // estop_active
  // formation_type
  // formation_number
  // grouping_index
  // selected_robot_count
  // selected_robot_ids
}

bool
combat_robot_msgs__msg__SwarmLeaderHeartbeat__are_equal(const combat_robot_msgs__msg__SwarmLeaderHeartbeat * lhs, const combat_robot_msgs__msg__SwarmLeaderHeartbeat * rhs)
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
  // sequence
  if (lhs->sequence != rhs->sequence) {
    return false;
  }
  // leader_robot_id
  if (lhs->leader_robot_id != rhs->leader_robot_id) {
    return false;
  }
  // operation_mode
  if (lhs->operation_mode != rhs->operation_mode) {
    return false;
  }
  // estop_active
  if (lhs->estop_active != rhs->estop_active) {
    return false;
  }
  // formation_type
  if (lhs->formation_type != rhs->formation_type) {
    return false;
  }
  // formation_number
  if (lhs->formation_number != rhs->formation_number) {
    return false;
  }
  // grouping_index
  if (lhs->grouping_index != rhs->grouping_index) {
    return false;
  }
  // selected_robot_count
  if (lhs->selected_robot_count != rhs->selected_robot_count) {
    return false;
  }
  // selected_robot_ids
  for (size_t i = 0; i < 8; ++i) {
    if (lhs->selected_robot_ids[i] != rhs->selected_robot_ids[i]) {
      return false;
    }
  }
  return true;
}

bool
combat_robot_msgs__msg__SwarmLeaderHeartbeat__copy(
  const combat_robot_msgs__msg__SwarmLeaderHeartbeat * input,
  combat_robot_msgs__msg__SwarmLeaderHeartbeat * output)
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
  // sequence
  output->sequence = input->sequence;
  // leader_robot_id
  output->leader_robot_id = input->leader_robot_id;
  // operation_mode
  output->operation_mode = input->operation_mode;
  // estop_active
  output->estop_active = input->estop_active;
  // formation_type
  output->formation_type = input->formation_type;
  // formation_number
  output->formation_number = input->formation_number;
  // grouping_index
  output->grouping_index = input->grouping_index;
  // selected_robot_count
  output->selected_robot_count = input->selected_robot_count;
  // selected_robot_ids
  for (size_t i = 0; i < 8; ++i) {
    output->selected_robot_ids[i] = input->selected_robot_ids[i];
  }
  return true;
}

combat_robot_msgs__msg__SwarmLeaderHeartbeat *
combat_robot_msgs__msg__SwarmLeaderHeartbeat__create(void)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  combat_robot_msgs__msg__SwarmLeaderHeartbeat * msg = (combat_robot_msgs__msg__SwarmLeaderHeartbeat *)allocator.allocate(sizeof(combat_robot_msgs__msg__SwarmLeaderHeartbeat), allocator.state);
  if (!msg) {
    return NULL;
  }
  memset(msg, 0, sizeof(combat_robot_msgs__msg__SwarmLeaderHeartbeat));
  bool success = combat_robot_msgs__msg__SwarmLeaderHeartbeat__init(msg);
  if (!success) {
    allocator.deallocate(msg, allocator.state);
    return NULL;
  }
  return msg;
}

void
combat_robot_msgs__msg__SwarmLeaderHeartbeat__destroy(combat_robot_msgs__msg__SwarmLeaderHeartbeat * msg)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  if (msg) {
    combat_robot_msgs__msg__SwarmLeaderHeartbeat__fini(msg);
  }
  allocator.deallocate(msg, allocator.state);
}


bool
combat_robot_msgs__msg__SwarmLeaderHeartbeat__Sequence__init(combat_robot_msgs__msg__SwarmLeaderHeartbeat__Sequence * array, size_t size)
{
  if (!array) {
    return false;
  }
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  combat_robot_msgs__msg__SwarmLeaderHeartbeat * data = NULL;

  if (size) {
    data = (combat_robot_msgs__msg__SwarmLeaderHeartbeat *)allocator.zero_allocate(size, sizeof(combat_robot_msgs__msg__SwarmLeaderHeartbeat), allocator.state);
    if (!data) {
      return false;
    }
    // initialize all array elements
    size_t i;
    for (i = 0; i < size; ++i) {
      bool success = combat_robot_msgs__msg__SwarmLeaderHeartbeat__init(&data[i]);
      if (!success) {
        break;
      }
    }
    if (i < size) {
      // if initialization failed finalize the already initialized array elements
      for (; i > 0; --i) {
        combat_robot_msgs__msg__SwarmLeaderHeartbeat__fini(&data[i - 1]);
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
combat_robot_msgs__msg__SwarmLeaderHeartbeat__Sequence__fini(combat_robot_msgs__msg__SwarmLeaderHeartbeat__Sequence * array)
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
      combat_robot_msgs__msg__SwarmLeaderHeartbeat__fini(&array->data[i]);
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

combat_robot_msgs__msg__SwarmLeaderHeartbeat__Sequence *
combat_robot_msgs__msg__SwarmLeaderHeartbeat__Sequence__create(size_t size)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  combat_robot_msgs__msg__SwarmLeaderHeartbeat__Sequence * array = (combat_robot_msgs__msg__SwarmLeaderHeartbeat__Sequence *)allocator.allocate(sizeof(combat_robot_msgs__msg__SwarmLeaderHeartbeat__Sequence), allocator.state);
  if (!array) {
    return NULL;
  }
  bool success = combat_robot_msgs__msg__SwarmLeaderHeartbeat__Sequence__init(array, size);
  if (!success) {
    allocator.deallocate(array, allocator.state);
    return NULL;
  }
  return array;
}

void
combat_robot_msgs__msg__SwarmLeaderHeartbeat__Sequence__destroy(combat_robot_msgs__msg__SwarmLeaderHeartbeat__Sequence * array)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  if (array) {
    combat_robot_msgs__msg__SwarmLeaderHeartbeat__Sequence__fini(array);
  }
  allocator.deallocate(array, allocator.state);
}

bool
combat_robot_msgs__msg__SwarmLeaderHeartbeat__Sequence__are_equal(const combat_robot_msgs__msg__SwarmLeaderHeartbeat__Sequence * lhs, const combat_robot_msgs__msg__SwarmLeaderHeartbeat__Sequence * rhs)
{
  if (!lhs || !rhs) {
    return false;
  }
  if (lhs->size != rhs->size) {
    return false;
  }
  for (size_t i = 0; i < lhs->size; ++i) {
    if (!combat_robot_msgs__msg__SwarmLeaderHeartbeat__are_equal(&(lhs->data[i]), &(rhs->data[i]))) {
      return false;
    }
  }
  return true;
}

bool
combat_robot_msgs__msg__SwarmLeaderHeartbeat__Sequence__copy(
  const combat_robot_msgs__msg__SwarmLeaderHeartbeat__Sequence * input,
  combat_robot_msgs__msg__SwarmLeaderHeartbeat__Sequence * output)
{
  if (!input || !output) {
    return false;
  }
  if (output->capacity < input->size) {
    const size_t allocation_size =
      input->size * sizeof(combat_robot_msgs__msg__SwarmLeaderHeartbeat);
    rcutils_allocator_t allocator = rcutils_get_default_allocator();
    combat_robot_msgs__msg__SwarmLeaderHeartbeat * data =
      (combat_robot_msgs__msg__SwarmLeaderHeartbeat *)allocator.reallocate(
      output->data, allocation_size, allocator.state);
    if (!data) {
      return false;
    }
    // If reallocation succeeded, memory may or may not have been moved
    // to fulfill the allocation request, invalidating output->data.
    output->data = data;
    for (size_t i = output->capacity; i < input->size; ++i) {
      if (!combat_robot_msgs__msg__SwarmLeaderHeartbeat__init(&output->data[i])) {
        // If initialization of any new item fails, roll back
        // all previously initialized items. Existing items
        // in output are to be left unmodified.
        for (; i-- > output->capacity; ) {
          combat_robot_msgs__msg__SwarmLeaderHeartbeat__fini(&output->data[i]);
        }
        return false;
      }
    }
    output->capacity = input->size;
  }
  output->size = input->size;
  for (size_t i = 0; i < input->size; ++i) {
    if (!combat_robot_msgs__msg__SwarmLeaderHeartbeat__copy(
        &(input->data[i]), &(output->data[i])))
    {
      return false;
    }
  }
  return true;
}
