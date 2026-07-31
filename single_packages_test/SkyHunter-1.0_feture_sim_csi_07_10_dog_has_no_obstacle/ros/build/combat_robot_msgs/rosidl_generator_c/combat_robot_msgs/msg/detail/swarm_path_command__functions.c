// generated from rosidl_generator_c/resource/idl__functions.c.em
// with input from combat_robot_msgs:msg/SwarmPathCommand.idl
// generated code does not contain a copyright notice
#include "combat_robot_msgs/msg/detail/swarm_path_command__functions.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "rcutils/allocator.h"


// Include directives for member types
// Member `header`
#include "std_msgs/msg/detail/header__functions.h"
// Member `path_json`
#include "rosidl_runtime_c/string_functions.h"

bool
combat_robot_msgs__msg__SwarmPathCommand__init(combat_robot_msgs__msg__SwarmPathCommand * msg)
{
  if (!msg) {
    return false;
  }
  // header
  if (!std_msgs__msg__Header__init(&msg->header)) {
    combat_robot_msgs__msg__SwarmPathCommand__fini(msg);
    return false;
  }
  // command
  // num_waypoints
  // path_json
  if (!rosidl_runtime_c__String__init(&msg->path_json)) {
    combat_robot_msgs__msg__SwarmPathCommand__fini(msg);
    return false;
  }
  return true;
}

void
combat_robot_msgs__msg__SwarmPathCommand__fini(combat_robot_msgs__msg__SwarmPathCommand * msg)
{
  if (!msg) {
    return;
  }
  // header
  std_msgs__msg__Header__fini(&msg->header);
  // command
  // num_waypoints
  // path_json
  rosidl_runtime_c__String__fini(&msg->path_json);
}

bool
combat_robot_msgs__msg__SwarmPathCommand__are_equal(const combat_robot_msgs__msg__SwarmPathCommand * lhs, const combat_robot_msgs__msg__SwarmPathCommand * rhs)
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
  // command
  if (lhs->command != rhs->command) {
    return false;
  }
  // num_waypoints
  if (lhs->num_waypoints != rhs->num_waypoints) {
    return false;
  }
  // path_json
  if (!rosidl_runtime_c__String__are_equal(
      &(lhs->path_json), &(rhs->path_json)))
  {
    return false;
  }
  return true;
}

bool
combat_robot_msgs__msg__SwarmPathCommand__copy(
  const combat_robot_msgs__msg__SwarmPathCommand * input,
  combat_robot_msgs__msg__SwarmPathCommand * output)
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
  // command
  output->command = input->command;
  // num_waypoints
  output->num_waypoints = input->num_waypoints;
  // path_json
  if (!rosidl_runtime_c__String__copy(
      &(input->path_json), &(output->path_json)))
  {
    return false;
  }
  return true;
}

combat_robot_msgs__msg__SwarmPathCommand *
combat_robot_msgs__msg__SwarmPathCommand__create(void)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  combat_robot_msgs__msg__SwarmPathCommand * msg = (combat_robot_msgs__msg__SwarmPathCommand *)allocator.allocate(sizeof(combat_robot_msgs__msg__SwarmPathCommand), allocator.state);
  if (!msg) {
    return NULL;
  }
  memset(msg, 0, sizeof(combat_robot_msgs__msg__SwarmPathCommand));
  bool success = combat_robot_msgs__msg__SwarmPathCommand__init(msg);
  if (!success) {
    allocator.deallocate(msg, allocator.state);
    return NULL;
  }
  return msg;
}

void
combat_robot_msgs__msg__SwarmPathCommand__destroy(combat_robot_msgs__msg__SwarmPathCommand * msg)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  if (msg) {
    combat_robot_msgs__msg__SwarmPathCommand__fini(msg);
  }
  allocator.deallocate(msg, allocator.state);
}


bool
combat_robot_msgs__msg__SwarmPathCommand__Sequence__init(combat_robot_msgs__msg__SwarmPathCommand__Sequence * array, size_t size)
{
  if (!array) {
    return false;
  }
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  combat_robot_msgs__msg__SwarmPathCommand * data = NULL;

  if (size) {
    data = (combat_robot_msgs__msg__SwarmPathCommand *)allocator.zero_allocate(size, sizeof(combat_robot_msgs__msg__SwarmPathCommand), allocator.state);
    if (!data) {
      return false;
    }
    // initialize all array elements
    size_t i;
    for (i = 0; i < size; ++i) {
      bool success = combat_robot_msgs__msg__SwarmPathCommand__init(&data[i]);
      if (!success) {
        break;
      }
    }
    if (i < size) {
      // if initialization failed finalize the already initialized array elements
      for (; i > 0; --i) {
        combat_robot_msgs__msg__SwarmPathCommand__fini(&data[i - 1]);
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
combat_robot_msgs__msg__SwarmPathCommand__Sequence__fini(combat_robot_msgs__msg__SwarmPathCommand__Sequence * array)
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
      combat_robot_msgs__msg__SwarmPathCommand__fini(&array->data[i]);
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

combat_robot_msgs__msg__SwarmPathCommand__Sequence *
combat_robot_msgs__msg__SwarmPathCommand__Sequence__create(size_t size)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  combat_robot_msgs__msg__SwarmPathCommand__Sequence * array = (combat_robot_msgs__msg__SwarmPathCommand__Sequence *)allocator.allocate(sizeof(combat_robot_msgs__msg__SwarmPathCommand__Sequence), allocator.state);
  if (!array) {
    return NULL;
  }
  bool success = combat_robot_msgs__msg__SwarmPathCommand__Sequence__init(array, size);
  if (!success) {
    allocator.deallocate(array, allocator.state);
    return NULL;
  }
  return array;
}

void
combat_robot_msgs__msg__SwarmPathCommand__Sequence__destroy(combat_robot_msgs__msg__SwarmPathCommand__Sequence * array)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  if (array) {
    combat_robot_msgs__msg__SwarmPathCommand__Sequence__fini(array);
  }
  allocator.deallocate(array, allocator.state);
}

bool
combat_robot_msgs__msg__SwarmPathCommand__Sequence__are_equal(const combat_robot_msgs__msg__SwarmPathCommand__Sequence * lhs, const combat_robot_msgs__msg__SwarmPathCommand__Sequence * rhs)
{
  if (!lhs || !rhs) {
    return false;
  }
  if (lhs->size != rhs->size) {
    return false;
  }
  for (size_t i = 0; i < lhs->size; ++i) {
    if (!combat_robot_msgs__msg__SwarmPathCommand__are_equal(&(lhs->data[i]), &(rhs->data[i]))) {
      return false;
    }
  }
  return true;
}

bool
combat_robot_msgs__msg__SwarmPathCommand__Sequence__copy(
  const combat_robot_msgs__msg__SwarmPathCommand__Sequence * input,
  combat_robot_msgs__msg__SwarmPathCommand__Sequence * output)
{
  if (!input || !output) {
    return false;
  }
  if (output->capacity < input->size) {
    const size_t allocation_size =
      input->size * sizeof(combat_robot_msgs__msg__SwarmPathCommand);
    rcutils_allocator_t allocator = rcutils_get_default_allocator();
    combat_robot_msgs__msg__SwarmPathCommand * data =
      (combat_robot_msgs__msg__SwarmPathCommand *)allocator.reallocate(
      output->data, allocation_size, allocator.state);
    if (!data) {
      return false;
    }
    // If reallocation succeeded, memory may or may not have been moved
    // to fulfill the allocation request, invalidating output->data.
    output->data = data;
    for (size_t i = output->capacity; i < input->size; ++i) {
      if (!combat_robot_msgs__msg__SwarmPathCommand__init(&output->data[i])) {
        // If initialization of any new item fails, roll back
        // all previously initialized items. Existing items
        // in output are to be left unmodified.
        for (; i-- > output->capacity; ) {
          combat_robot_msgs__msg__SwarmPathCommand__fini(&output->data[i]);
        }
        return false;
      }
    }
    output->capacity = input->size;
  }
  output->size = input->size;
  for (size_t i = 0; i < input->size; ++i) {
    if (!combat_robot_msgs__msg__SwarmPathCommand__copy(
        &(input->data[i]), &(output->data[i])))
    {
      return false;
    }
  }
  return true;
}
