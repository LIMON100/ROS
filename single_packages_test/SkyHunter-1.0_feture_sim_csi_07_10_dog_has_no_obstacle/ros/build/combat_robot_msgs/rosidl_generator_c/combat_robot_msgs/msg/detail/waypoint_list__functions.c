// generated from rosidl_generator_c/resource/idl__functions.c.em
// with input from combat_robot_msgs:msg/WaypointList.idl
// generated code does not contain a copyright notice
#include "combat_robot_msgs/msg/detail/waypoint_list__functions.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "rcutils/allocator.h"


// Include directives for member types
// Member `waypoints`
#include "combat_robot_msgs/msg/detail/waypoint__functions.h"

bool
combat_robot_msgs__msg__WaypointList__init(combat_robot_msgs__msg__WaypointList * msg)
{
  if (!msg) {
    return false;
  }
  // mode
  // formation
  // mission_id
  // mission_status
  // waypoints
  if (!combat_robot_msgs__msg__Waypoint__Sequence__init(&msg->waypoints, 0)) {
    combat_robot_msgs__msg__WaypointList__fini(msg);
    return false;
  }
  return true;
}

void
combat_robot_msgs__msg__WaypointList__fini(combat_robot_msgs__msg__WaypointList * msg)
{
  if (!msg) {
    return;
  }
  // mode
  // formation
  // mission_id
  // mission_status
  // waypoints
  combat_robot_msgs__msg__Waypoint__Sequence__fini(&msg->waypoints);
}

bool
combat_robot_msgs__msg__WaypointList__are_equal(const combat_robot_msgs__msg__WaypointList * lhs, const combat_robot_msgs__msg__WaypointList * rhs)
{
  if (!lhs || !rhs) {
    return false;
  }
  // mode
  if (lhs->mode != rhs->mode) {
    return false;
  }
  // formation
  if (lhs->formation != rhs->formation) {
    return false;
  }
  // mission_id
  if (lhs->mission_id != rhs->mission_id) {
    return false;
  }
  // mission_status
  if (lhs->mission_status != rhs->mission_status) {
    return false;
  }
  // waypoints
  if (!combat_robot_msgs__msg__Waypoint__Sequence__are_equal(
      &(lhs->waypoints), &(rhs->waypoints)))
  {
    return false;
  }
  return true;
}

bool
combat_robot_msgs__msg__WaypointList__copy(
  const combat_robot_msgs__msg__WaypointList * input,
  combat_robot_msgs__msg__WaypointList * output)
{
  if (!input || !output) {
    return false;
  }
  // mode
  output->mode = input->mode;
  // formation
  output->formation = input->formation;
  // mission_id
  output->mission_id = input->mission_id;
  // mission_status
  output->mission_status = input->mission_status;
  // waypoints
  if (!combat_robot_msgs__msg__Waypoint__Sequence__copy(
      &(input->waypoints), &(output->waypoints)))
  {
    return false;
  }
  return true;
}

combat_robot_msgs__msg__WaypointList *
combat_robot_msgs__msg__WaypointList__create(void)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  combat_robot_msgs__msg__WaypointList * msg = (combat_robot_msgs__msg__WaypointList *)allocator.allocate(sizeof(combat_robot_msgs__msg__WaypointList), allocator.state);
  if (!msg) {
    return NULL;
  }
  memset(msg, 0, sizeof(combat_robot_msgs__msg__WaypointList));
  bool success = combat_robot_msgs__msg__WaypointList__init(msg);
  if (!success) {
    allocator.deallocate(msg, allocator.state);
    return NULL;
  }
  return msg;
}

void
combat_robot_msgs__msg__WaypointList__destroy(combat_robot_msgs__msg__WaypointList * msg)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  if (msg) {
    combat_robot_msgs__msg__WaypointList__fini(msg);
  }
  allocator.deallocate(msg, allocator.state);
}


bool
combat_robot_msgs__msg__WaypointList__Sequence__init(combat_robot_msgs__msg__WaypointList__Sequence * array, size_t size)
{
  if (!array) {
    return false;
  }
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  combat_robot_msgs__msg__WaypointList * data = NULL;

  if (size) {
    data = (combat_robot_msgs__msg__WaypointList *)allocator.zero_allocate(size, sizeof(combat_robot_msgs__msg__WaypointList), allocator.state);
    if (!data) {
      return false;
    }
    // initialize all array elements
    size_t i;
    for (i = 0; i < size; ++i) {
      bool success = combat_robot_msgs__msg__WaypointList__init(&data[i]);
      if (!success) {
        break;
      }
    }
    if (i < size) {
      // if initialization failed finalize the already initialized array elements
      for (; i > 0; --i) {
        combat_robot_msgs__msg__WaypointList__fini(&data[i - 1]);
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
combat_robot_msgs__msg__WaypointList__Sequence__fini(combat_robot_msgs__msg__WaypointList__Sequence * array)
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
      combat_robot_msgs__msg__WaypointList__fini(&array->data[i]);
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

combat_robot_msgs__msg__WaypointList__Sequence *
combat_robot_msgs__msg__WaypointList__Sequence__create(size_t size)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  combat_robot_msgs__msg__WaypointList__Sequence * array = (combat_robot_msgs__msg__WaypointList__Sequence *)allocator.allocate(sizeof(combat_robot_msgs__msg__WaypointList__Sequence), allocator.state);
  if (!array) {
    return NULL;
  }
  bool success = combat_robot_msgs__msg__WaypointList__Sequence__init(array, size);
  if (!success) {
    allocator.deallocate(array, allocator.state);
    return NULL;
  }
  return array;
}

void
combat_robot_msgs__msg__WaypointList__Sequence__destroy(combat_robot_msgs__msg__WaypointList__Sequence * array)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  if (array) {
    combat_robot_msgs__msg__WaypointList__Sequence__fini(array);
  }
  allocator.deallocate(array, allocator.state);
}

bool
combat_robot_msgs__msg__WaypointList__Sequence__are_equal(const combat_robot_msgs__msg__WaypointList__Sequence * lhs, const combat_robot_msgs__msg__WaypointList__Sequence * rhs)
{
  if (!lhs || !rhs) {
    return false;
  }
  if (lhs->size != rhs->size) {
    return false;
  }
  for (size_t i = 0; i < lhs->size; ++i) {
    if (!combat_robot_msgs__msg__WaypointList__are_equal(&(lhs->data[i]), &(rhs->data[i]))) {
      return false;
    }
  }
  return true;
}

bool
combat_robot_msgs__msg__WaypointList__Sequence__copy(
  const combat_robot_msgs__msg__WaypointList__Sequence * input,
  combat_robot_msgs__msg__WaypointList__Sequence * output)
{
  if (!input || !output) {
    return false;
  }
  if (output->capacity < input->size) {
    const size_t allocation_size =
      input->size * sizeof(combat_robot_msgs__msg__WaypointList);
    rcutils_allocator_t allocator = rcutils_get_default_allocator();
    combat_robot_msgs__msg__WaypointList * data =
      (combat_robot_msgs__msg__WaypointList *)allocator.reallocate(
      output->data, allocation_size, allocator.state);
    if (!data) {
      return false;
    }
    // If reallocation succeeded, memory may or may not have been moved
    // to fulfill the allocation request, invalidating output->data.
    output->data = data;
    for (size_t i = output->capacity; i < input->size; ++i) {
      if (!combat_robot_msgs__msg__WaypointList__init(&output->data[i])) {
        // If initialization of any new item fails, roll back
        // all previously initialized items. Existing items
        // in output are to be left unmodified.
        for (; i-- > output->capacity; ) {
          combat_robot_msgs__msg__WaypointList__fini(&output->data[i]);
        }
        return false;
      }
    }
    output->capacity = input->size;
  }
  output->size = input->size;
  for (size_t i = 0; i < input->size; ++i) {
    if (!combat_robot_msgs__msg__WaypointList__copy(
        &(input->data[i]), &(output->data[i])))
    {
      return false;
    }
  }
  return true;
}
