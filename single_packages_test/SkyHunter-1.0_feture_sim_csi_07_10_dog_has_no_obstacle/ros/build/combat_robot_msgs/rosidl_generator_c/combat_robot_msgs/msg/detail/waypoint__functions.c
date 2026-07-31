// generated from rosidl_generator_c/resource/idl__functions.c.em
// with input from combat_robot_msgs:msg/Waypoint.idl
// generated code does not contain a copyright notice
#include "combat_robot_msgs/msg/detail/waypoint__functions.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "rcutils/allocator.h"


bool
combat_robot_msgs__msg__Waypoint__init(combat_robot_msgs__msg__Waypoint * msg)
{
  if (!msg) {
    return false;
  }
  // way_id
  // way_lon
  // way_lat
  // way_status
  return true;
}

void
combat_robot_msgs__msg__Waypoint__fini(combat_robot_msgs__msg__Waypoint * msg)
{
  if (!msg) {
    return;
  }
  // way_id
  // way_lon
  // way_lat
  // way_status
}

bool
combat_robot_msgs__msg__Waypoint__are_equal(const combat_robot_msgs__msg__Waypoint * lhs, const combat_robot_msgs__msg__Waypoint * rhs)
{
  if (!lhs || !rhs) {
    return false;
  }
  // way_id
  if (lhs->way_id != rhs->way_id) {
    return false;
  }
  // way_lon
  if (lhs->way_lon != rhs->way_lon) {
    return false;
  }
  // way_lat
  if (lhs->way_lat != rhs->way_lat) {
    return false;
  }
  // way_status
  if (lhs->way_status != rhs->way_status) {
    return false;
  }
  return true;
}

bool
combat_robot_msgs__msg__Waypoint__copy(
  const combat_robot_msgs__msg__Waypoint * input,
  combat_robot_msgs__msg__Waypoint * output)
{
  if (!input || !output) {
    return false;
  }
  // way_id
  output->way_id = input->way_id;
  // way_lon
  output->way_lon = input->way_lon;
  // way_lat
  output->way_lat = input->way_lat;
  // way_status
  output->way_status = input->way_status;
  return true;
}

combat_robot_msgs__msg__Waypoint *
combat_robot_msgs__msg__Waypoint__create(void)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  combat_robot_msgs__msg__Waypoint * msg = (combat_robot_msgs__msg__Waypoint *)allocator.allocate(sizeof(combat_robot_msgs__msg__Waypoint), allocator.state);
  if (!msg) {
    return NULL;
  }
  memset(msg, 0, sizeof(combat_robot_msgs__msg__Waypoint));
  bool success = combat_robot_msgs__msg__Waypoint__init(msg);
  if (!success) {
    allocator.deallocate(msg, allocator.state);
    return NULL;
  }
  return msg;
}

void
combat_robot_msgs__msg__Waypoint__destroy(combat_robot_msgs__msg__Waypoint * msg)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  if (msg) {
    combat_robot_msgs__msg__Waypoint__fini(msg);
  }
  allocator.deallocate(msg, allocator.state);
}


bool
combat_robot_msgs__msg__Waypoint__Sequence__init(combat_robot_msgs__msg__Waypoint__Sequence * array, size_t size)
{
  if (!array) {
    return false;
  }
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  combat_robot_msgs__msg__Waypoint * data = NULL;

  if (size) {
    data = (combat_robot_msgs__msg__Waypoint *)allocator.zero_allocate(size, sizeof(combat_robot_msgs__msg__Waypoint), allocator.state);
    if (!data) {
      return false;
    }
    // initialize all array elements
    size_t i;
    for (i = 0; i < size; ++i) {
      bool success = combat_robot_msgs__msg__Waypoint__init(&data[i]);
      if (!success) {
        break;
      }
    }
    if (i < size) {
      // if initialization failed finalize the already initialized array elements
      for (; i > 0; --i) {
        combat_robot_msgs__msg__Waypoint__fini(&data[i - 1]);
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
combat_robot_msgs__msg__Waypoint__Sequence__fini(combat_robot_msgs__msg__Waypoint__Sequence * array)
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
      combat_robot_msgs__msg__Waypoint__fini(&array->data[i]);
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

combat_robot_msgs__msg__Waypoint__Sequence *
combat_robot_msgs__msg__Waypoint__Sequence__create(size_t size)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  combat_robot_msgs__msg__Waypoint__Sequence * array = (combat_robot_msgs__msg__Waypoint__Sequence *)allocator.allocate(sizeof(combat_robot_msgs__msg__Waypoint__Sequence), allocator.state);
  if (!array) {
    return NULL;
  }
  bool success = combat_robot_msgs__msg__Waypoint__Sequence__init(array, size);
  if (!success) {
    allocator.deallocate(array, allocator.state);
    return NULL;
  }
  return array;
}

void
combat_robot_msgs__msg__Waypoint__Sequence__destroy(combat_robot_msgs__msg__Waypoint__Sequence * array)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  if (array) {
    combat_robot_msgs__msg__Waypoint__Sequence__fini(array);
  }
  allocator.deallocate(array, allocator.state);
}

bool
combat_robot_msgs__msg__Waypoint__Sequence__are_equal(const combat_robot_msgs__msg__Waypoint__Sequence * lhs, const combat_robot_msgs__msg__Waypoint__Sequence * rhs)
{
  if (!lhs || !rhs) {
    return false;
  }
  if (lhs->size != rhs->size) {
    return false;
  }
  for (size_t i = 0; i < lhs->size; ++i) {
    if (!combat_robot_msgs__msg__Waypoint__are_equal(&(lhs->data[i]), &(rhs->data[i]))) {
      return false;
    }
  }
  return true;
}

bool
combat_robot_msgs__msg__Waypoint__Sequence__copy(
  const combat_robot_msgs__msg__Waypoint__Sequence * input,
  combat_robot_msgs__msg__Waypoint__Sequence * output)
{
  if (!input || !output) {
    return false;
  }
  if (output->capacity < input->size) {
    const size_t allocation_size =
      input->size * sizeof(combat_robot_msgs__msg__Waypoint);
    rcutils_allocator_t allocator = rcutils_get_default_allocator();
    combat_robot_msgs__msg__Waypoint * data =
      (combat_robot_msgs__msg__Waypoint *)allocator.reallocate(
      output->data, allocation_size, allocator.state);
    if (!data) {
      return false;
    }
    // If reallocation succeeded, memory may or may not have been moved
    // to fulfill the allocation request, invalidating output->data.
    output->data = data;
    for (size_t i = output->capacity; i < input->size; ++i) {
      if (!combat_robot_msgs__msg__Waypoint__init(&output->data[i])) {
        // If initialization of any new item fails, roll back
        // all previously initialized items. Existing items
        // in output are to be left unmodified.
        for (; i-- > output->capacity; ) {
          combat_robot_msgs__msg__Waypoint__fini(&output->data[i]);
        }
        return false;
      }
    }
    output->capacity = input->size;
  }
  output->size = input->size;
  for (size_t i = 0; i < input->size; ++i) {
    if (!combat_robot_msgs__msg__Waypoint__copy(
        &(input->data[i]), &(output->data[i])))
    {
      return false;
    }
  }
  return true;
}
