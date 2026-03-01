// generated from rosidl_generator_c/resource/idl__functions.c.em
// with input from skyhunter_msgs:msg/SwarmHeartbeat.idl
// generated code does not contain a copyright notice
#include "skyhunter_msgs/msg/detail/swarm_heartbeat__functions.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "rcutils/allocator.h"


// Include directives for member types
// Member `header`
#include "std_msgs/msg/detail/header__functions.h"
// Member `robot_id`
#include "rosidl_runtime_c/string_functions.h"

bool
skyhunter_msgs__msg__SwarmHeartbeat__init(skyhunter_msgs__msg__SwarmHeartbeat * msg)
{
  if (!msg) {
    return false;
  }
  // header
  if (!std_msgs__msg__Header__init(&msg->header)) {
    skyhunter_msgs__msg__SwarmHeartbeat__fini(msg);
    return false;
  }
  // robot_id
  if (!rosidl_runtime_c__String__init(&msg->robot_id)) {
    skyhunter_msgs__msg__SwarmHeartbeat__fini(msg);
    return false;
  }
  // term
  // is_leader
  // battery_level
  // leader_id_num
  return true;
}

void
skyhunter_msgs__msg__SwarmHeartbeat__fini(skyhunter_msgs__msg__SwarmHeartbeat * msg)
{
  if (!msg) {
    return;
  }
  // header
  std_msgs__msg__Header__fini(&msg->header);
  // robot_id
  rosidl_runtime_c__String__fini(&msg->robot_id);
  // term
  // is_leader
  // battery_level
  // leader_id_num
}

bool
skyhunter_msgs__msg__SwarmHeartbeat__are_equal(const skyhunter_msgs__msg__SwarmHeartbeat * lhs, const skyhunter_msgs__msg__SwarmHeartbeat * rhs)
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
  if (!rosidl_runtime_c__String__are_equal(
      &(lhs->robot_id), &(rhs->robot_id)))
  {
    return false;
  }
  // term
  if (lhs->term != rhs->term) {
    return false;
  }
  // is_leader
  if (lhs->is_leader != rhs->is_leader) {
    return false;
  }
  // battery_level
  if (lhs->battery_level != rhs->battery_level) {
    return false;
  }
  // leader_id_num
  if (lhs->leader_id_num != rhs->leader_id_num) {
    return false;
  }
  return true;
}

bool
skyhunter_msgs__msg__SwarmHeartbeat__copy(
  const skyhunter_msgs__msg__SwarmHeartbeat * input,
  skyhunter_msgs__msg__SwarmHeartbeat * output)
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
  if (!rosidl_runtime_c__String__copy(
      &(input->robot_id), &(output->robot_id)))
  {
    return false;
  }
  // term
  output->term = input->term;
  // is_leader
  output->is_leader = input->is_leader;
  // battery_level
  output->battery_level = input->battery_level;
  // leader_id_num
  output->leader_id_num = input->leader_id_num;
  return true;
}

skyhunter_msgs__msg__SwarmHeartbeat *
skyhunter_msgs__msg__SwarmHeartbeat__create()
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  skyhunter_msgs__msg__SwarmHeartbeat * msg = (skyhunter_msgs__msg__SwarmHeartbeat *)allocator.allocate(sizeof(skyhunter_msgs__msg__SwarmHeartbeat), allocator.state);
  if (!msg) {
    return NULL;
  }
  memset(msg, 0, sizeof(skyhunter_msgs__msg__SwarmHeartbeat));
  bool success = skyhunter_msgs__msg__SwarmHeartbeat__init(msg);
  if (!success) {
    allocator.deallocate(msg, allocator.state);
    return NULL;
  }
  return msg;
}

void
skyhunter_msgs__msg__SwarmHeartbeat__destroy(skyhunter_msgs__msg__SwarmHeartbeat * msg)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  if (msg) {
    skyhunter_msgs__msg__SwarmHeartbeat__fini(msg);
  }
  allocator.deallocate(msg, allocator.state);
}


bool
skyhunter_msgs__msg__SwarmHeartbeat__Sequence__init(skyhunter_msgs__msg__SwarmHeartbeat__Sequence * array, size_t size)
{
  if (!array) {
    return false;
  }
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  skyhunter_msgs__msg__SwarmHeartbeat * data = NULL;

  if (size) {
    data = (skyhunter_msgs__msg__SwarmHeartbeat *)allocator.zero_allocate(size, sizeof(skyhunter_msgs__msg__SwarmHeartbeat), allocator.state);
    if (!data) {
      return false;
    }
    // initialize all array elements
    size_t i;
    for (i = 0; i < size; ++i) {
      bool success = skyhunter_msgs__msg__SwarmHeartbeat__init(&data[i]);
      if (!success) {
        break;
      }
    }
    if (i < size) {
      // if initialization failed finalize the already initialized array elements
      for (; i > 0; --i) {
        skyhunter_msgs__msg__SwarmHeartbeat__fini(&data[i - 1]);
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
skyhunter_msgs__msg__SwarmHeartbeat__Sequence__fini(skyhunter_msgs__msg__SwarmHeartbeat__Sequence * array)
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
      skyhunter_msgs__msg__SwarmHeartbeat__fini(&array->data[i]);
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

skyhunter_msgs__msg__SwarmHeartbeat__Sequence *
skyhunter_msgs__msg__SwarmHeartbeat__Sequence__create(size_t size)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  skyhunter_msgs__msg__SwarmHeartbeat__Sequence * array = (skyhunter_msgs__msg__SwarmHeartbeat__Sequence *)allocator.allocate(sizeof(skyhunter_msgs__msg__SwarmHeartbeat__Sequence), allocator.state);
  if (!array) {
    return NULL;
  }
  bool success = skyhunter_msgs__msg__SwarmHeartbeat__Sequence__init(array, size);
  if (!success) {
    allocator.deallocate(array, allocator.state);
    return NULL;
  }
  return array;
}

void
skyhunter_msgs__msg__SwarmHeartbeat__Sequence__destroy(skyhunter_msgs__msg__SwarmHeartbeat__Sequence * array)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  if (array) {
    skyhunter_msgs__msg__SwarmHeartbeat__Sequence__fini(array);
  }
  allocator.deallocate(array, allocator.state);
}

bool
skyhunter_msgs__msg__SwarmHeartbeat__Sequence__are_equal(const skyhunter_msgs__msg__SwarmHeartbeat__Sequence * lhs, const skyhunter_msgs__msg__SwarmHeartbeat__Sequence * rhs)
{
  if (!lhs || !rhs) {
    return false;
  }
  if (lhs->size != rhs->size) {
    return false;
  }
  for (size_t i = 0; i < lhs->size; ++i) {
    if (!skyhunter_msgs__msg__SwarmHeartbeat__are_equal(&(lhs->data[i]), &(rhs->data[i]))) {
      return false;
    }
  }
  return true;
}

bool
skyhunter_msgs__msg__SwarmHeartbeat__Sequence__copy(
  const skyhunter_msgs__msg__SwarmHeartbeat__Sequence * input,
  skyhunter_msgs__msg__SwarmHeartbeat__Sequence * output)
{
  if (!input || !output) {
    return false;
  }
  if (output->capacity < input->size) {
    const size_t allocation_size =
      input->size * sizeof(skyhunter_msgs__msg__SwarmHeartbeat);
    rcutils_allocator_t allocator = rcutils_get_default_allocator();
    skyhunter_msgs__msg__SwarmHeartbeat * data =
      (skyhunter_msgs__msg__SwarmHeartbeat *)allocator.reallocate(
      output->data, allocation_size, allocator.state);
    if (!data) {
      return false;
    }
    // If reallocation succeeded, memory may or may not have been moved
    // to fulfill the allocation request, invalidating output->data.
    output->data = data;
    for (size_t i = output->capacity; i < input->size; ++i) {
      if (!skyhunter_msgs__msg__SwarmHeartbeat__init(&output->data[i])) {
        // If initialization of any new item fails, roll back
        // all previously initialized items. Existing items
        // in output are to be left unmodified.
        for (; i-- > output->capacity; ) {
          skyhunter_msgs__msg__SwarmHeartbeat__fini(&output->data[i]);
        }
        return false;
      }
    }
    output->capacity = input->size;
  }
  output->size = input->size;
  for (size_t i = 0; i < input->size; ++i) {
    if (!skyhunter_msgs__msg__SwarmHeartbeat__copy(
        &(input->data[i]), &(output->data[i])))
    {
      return false;
    }
  }
  return true;
}
