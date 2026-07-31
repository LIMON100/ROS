// generated from rosidl_generator_c/resource/idl__functions.c.em
// with input from combat_robot_msgs:msg/TargetPoint.idl
// generated code does not contain a copyright notice
#include "combat_robot_msgs/msg/detail/target_point__functions.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "rcutils/allocator.h"


// Include directives for member types
// Member `header`
#include "std_msgs/msg/detail/header__functions.h"
// Member `box`
#include "combat_robot_msgs/msg/detail/bounding_box2d__functions.h"

bool
combat_robot_msgs__msg__TargetPoint__init(combat_robot_msgs__msg__TargetPoint * msg)
{
  if (!msg) {
    return false;
  }
  // header
  if (!std_msgs__msg__Header__init(&msg->header)) {
    combat_robot_msgs__msg__TargetPoint__fini(msg);
    return false;
  }
  // is_locked
  // x
  // y
  // height
  // class_id
  // box
  if (!combat_robot_msgs__msg__BoundingBox2d__init(&msg->box)) {
    combat_robot_msgs__msg__TargetPoint__fini(msg);
    return false;
  }
  // track_id
  return true;
}

void
combat_robot_msgs__msg__TargetPoint__fini(combat_robot_msgs__msg__TargetPoint * msg)
{
  if (!msg) {
    return;
  }
  // header
  std_msgs__msg__Header__fini(&msg->header);
  // is_locked
  // x
  // y
  // height
  // class_id
  // box
  combat_robot_msgs__msg__BoundingBox2d__fini(&msg->box);
  // track_id
}

bool
combat_robot_msgs__msg__TargetPoint__are_equal(const combat_robot_msgs__msg__TargetPoint * lhs, const combat_robot_msgs__msg__TargetPoint * rhs)
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
  // is_locked
  if (lhs->is_locked != rhs->is_locked) {
    return false;
  }
  // x
  if (lhs->x != rhs->x) {
    return false;
  }
  // y
  if (lhs->y != rhs->y) {
    return false;
  }
  // height
  if (lhs->height != rhs->height) {
    return false;
  }
  // class_id
  if (lhs->class_id != rhs->class_id) {
    return false;
  }
  // box
  if (!combat_robot_msgs__msg__BoundingBox2d__are_equal(
      &(lhs->box), &(rhs->box)))
  {
    return false;
  }
  // track_id
  if (lhs->track_id != rhs->track_id) {
    return false;
  }
  return true;
}

bool
combat_robot_msgs__msg__TargetPoint__copy(
  const combat_robot_msgs__msg__TargetPoint * input,
  combat_robot_msgs__msg__TargetPoint * output)
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
  // is_locked
  output->is_locked = input->is_locked;
  // x
  output->x = input->x;
  // y
  output->y = input->y;
  // height
  output->height = input->height;
  // class_id
  output->class_id = input->class_id;
  // box
  if (!combat_robot_msgs__msg__BoundingBox2d__copy(
      &(input->box), &(output->box)))
  {
    return false;
  }
  // track_id
  output->track_id = input->track_id;
  return true;
}

combat_robot_msgs__msg__TargetPoint *
combat_robot_msgs__msg__TargetPoint__create(void)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  combat_robot_msgs__msg__TargetPoint * msg = (combat_robot_msgs__msg__TargetPoint *)allocator.allocate(sizeof(combat_robot_msgs__msg__TargetPoint), allocator.state);
  if (!msg) {
    return NULL;
  }
  memset(msg, 0, sizeof(combat_robot_msgs__msg__TargetPoint));
  bool success = combat_robot_msgs__msg__TargetPoint__init(msg);
  if (!success) {
    allocator.deallocate(msg, allocator.state);
    return NULL;
  }
  return msg;
}

void
combat_robot_msgs__msg__TargetPoint__destroy(combat_robot_msgs__msg__TargetPoint * msg)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  if (msg) {
    combat_robot_msgs__msg__TargetPoint__fini(msg);
  }
  allocator.deallocate(msg, allocator.state);
}


bool
combat_robot_msgs__msg__TargetPoint__Sequence__init(combat_robot_msgs__msg__TargetPoint__Sequence * array, size_t size)
{
  if (!array) {
    return false;
  }
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  combat_robot_msgs__msg__TargetPoint * data = NULL;

  if (size) {
    data = (combat_robot_msgs__msg__TargetPoint *)allocator.zero_allocate(size, sizeof(combat_robot_msgs__msg__TargetPoint), allocator.state);
    if (!data) {
      return false;
    }
    // initialize all array elements
    size_t i;
    for (i = 0; i < size; ++i) {
      bool success = combat_robot_msgs__msg__TargetPoint__init(&data[i]);
      if (!success) {
        break;
      }
    }
    if (i < size) {
      // if initialization failed finalize the already initialized array elements
      for (; i > 0; --i) {
        combat_robot_msgs__msg__TargetPoint__fini(&data[i - 1]);
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
combat_robot_msgs__msg__TargetPoint__Sequence__fini(combat_robot_msgs__msg__TargetPoint__Sequence * array)
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
      combat_robot_msgs__msg__TargetPoint__fini(&array->data[i]);
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

combat_robot_msgs__msg__TargetPoint__Sequence *
combat_robot_msgs__msg__TargetPoint__Sequence__create(size_t size)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  combat_robot_msgs__msg__TargetPoint__Sequence * array = (combat_robot_msgs__msg__TargetPoint__Sequence *)allocator.allocate(sizeof(combat_robot_msgs__msg__TargetPoint__Sequence), allocator.state);
  if (!array) {
    return NULL;
  }
  bool success = combat_robot_msgs__msg__TargetPoint__Sequence__init(array, size);
  if (!success) {
    allocator.deallocate(array, allocator.state);
    return NULL;
  }
  return array;
}

void
combat_robot_msgs__msg__TargetPoint__Sequence__destroy(combat_robot_msgs__msg__TargetPoint__Sequence * array)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  if (array) {
    combat_robot_msgs__msg__TargetPoint__Sequence__fini(array);
  }
  allocator.deallocate(array, allocator.state);
}

bool
combat_robot_msgs__msg__TargetPoint__Sequence__are_equal(const combat_robot_msgs__msg__TargetPoint__Sequence * lhs, const combat_robot_msgs__msg__TargetPoint__Sequence * rhs)
{
  if (!lhs || !rhs) {
    return false;
  }
  if (lhs->size != rhs->size) {
    return false;
  }
  for (size_t i = 0; i < lhs->size; ++i) {
    if (!combat_robot_msgs__msg__TargetPoint__are_equal(&(lhs->data[i]), &(rhs->data[i]))) {
      return false;
    }
  }
  return true;
}

bool
combat_robot_msgs__msg__TargetPoint__Sequence__copy(
  const combat_robot_msgs__msg__TargetPoint__Sequence * input,
  combat_robot_msgs__msg__TargetPoint__Sequence * output)
{
  if (!input || !output) {
    return false;
  }
  if (output->capacity < input->size) {
    const size_t allocation_size =
      input->size * sizeof(combat_robot_msgs__msg__TargetPoint);
    rcutils_allocator_t allocator = rcutils_get_default_allocator();
    combat_robot_msgs__msg__TargetPoint * data =
      (combat_robot_msgs__msg__TargetPoint *)allocator.reallocate(
      output->data, allocation_size, allocator.state);
    if (!data) {
      return false;
    }
    // If reallocation succeeded, memory may or may not have been moved
    // to fulfill the allocation request, invalidating output->data.
    output->data = data;
    for (size_t i = output->capacity; i < input->size; ++i) {
      if (!combat_robot_msgs__msg__TargetPoint__init(&output->data[i])) {
        // If initialization of any new item fails, roll back
        // all previously initialized items. Existing items
        // in output are to be left unmodified.
        for (; i-- > output->capacity; ) {
          combat_robot_msgs__msg__TargetPoint__fini(&output->data[i]);
        }
        return false;
      }
    }
    output->capacity = input->size;
  }
  output->size = input->size;
  for (size_t i = 0; i < input->size; ++i) {
    if (!combat_robot_msgs__msg__TargetPoint__copy(
        &(input->data[i]), &(output->data[i])))
    {
      return false;
    }
  }
  return true;
}
