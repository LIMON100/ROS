// generated from rosidl_generator_c/resource/idl__functions.c.em
// with input from combat_robot_msgs:msg/DetectedObject.idl
// generated code does not contain a copyright notice
#include "combat_robot_msgs/msg/detail/detected_object__functions.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "rcutils/allocator.h"


// Include directives for member types
// Member `box`
#include "combat_robot_msgs/msg/detail/bounding_box2d__functions.h"

bool
combat_robot_msgs__msg__DetectedObject__init(combat_robot_msgs__msg__DetectedObject * msg)
{
  if (!msg) {
    return false;
  }
  // id
  // prob
  // box
  if (!combat_robot_msgs__msg__BoundingBox2d__init(&msg->box)) {
    combat_robot_msgs__msg__DetectedObject__fini(msg);
    return false;
  }
  return true;
}

void
combat_robot_msgs__msg__DetectedObject__fini(combat_robot_msgs__msg__DetectedObject * msg)
{
  if (!msg) {
    return;
  }
  // id
  // prob
  // box
  combat_robot_msgs__msg__BoundingBox2d__fini(&msg->box);
}

bool
combat_robot_msgs__msg__DetectedObject__are_equal(const combat_robot_msgs__msg__DetectedObject * lhs, const combat_robot_msgs__msg__DetectedObject * rhs)
{
  if (!lhs || !rhs) {
    return false;
  }
  // id
  if (lhs->id != rhs->id) {
    return false;
  }
  // prob
  if (lhs->prob != rhs->prob) {
    return false;
  }
  // box
  if (!combat_robot_msgs__msg__BoundingBox2d__are_equal(
      &(lhs->box), &(rhs->box)))
  {
    return false;
  }
  return true;
}

bool
combat_robot_msgs__msg__DetectedObject__copy(
  const combat_robot_msgs__msg__DetectedObject * input,
  combat_robot_msgs__msg__DetectedObject * output)
{
  if (!input || !output) {
    return false;
  }
  // id
  output->id = input->id;
  // prob
  output->prob = input->prob;
  // box
  if (!combat_robot_msgs__msg__BoundingBox2d__copy(
      &(input->box), &(output->box)))
  {
    return false;
  }
  return true;
}

combat_robot_msgs__msg__DetectedObject *
combat_robot_msgs__msg__DetectedObject__create(void)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  combat_robot_msgs__msg__DetectedObject * msg = (combat_robot_msgs__msg__DetectedObject *)allocator.allocate(sizeof(combat_robot_msgs__msg__DetectedObject), allocator.state);
  if (!msg) {
    return NULL;
  }
  memset(msg, 0, sizeof(combat_robot_msgs__msg__DetectedObject));
  bool success = combat_robot_msgs__msg__DetectedObject__init(msg);
  if (!success) {
    allocator.deallocate(msg, allocator.state);
    return NULL;
  }
  return msg;
}

void
combat_robot_msgs__msg__DetectedObject__destroy(combat_robot_msgs__msg__DetectedObject * msg)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  if (msg) {
    combat_robot_msgs__msg__DetectedObject__fini(msg);
  }
  allocator.deallocate(msg, allocator.state);
}


bool
combat_robot_msgs__msg__DetectedObject__Sequence__init(combat_robot_msgs__msg__DetectedObject__Sequence * array, size_t size)
{
  if (!array) {
    return false;
  }
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  combat_robot_msgs__msg__DetectedObject * data = NULL;

  if (size) {
    data = (combat_robot_msgs__msg__DetectedObject *)allocator.zero_allocate(size, sizeof(combat_robot_msgs__msg__DetectedObject), allocator.state);
    if (!data) {
      return false;
    }
    // initialize all array elements
    size_t i;
    for (i = 0; i < size; ++i) {
      bool success = combat_robot_msgs__msg__DetectedObject__init(&data[i]);
      if (!success) {
        break;
      }
    }
    if (i < size) {
      // if initialization failed finalize the already initialized array elements
      for (; i > 0; --i) {
        combat_robot_msgs__msg__DetectedObject__fini(&data[i - 1]);
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
combat_robot_msgs__msg__DetectedObject__Sequence__fini(combat_robot_msgs__msg__DetectedObject__Sequence * array)
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
      combat_robot_msgs__msg__DetectedObject__fini(&array->data[i]);
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

combat_robot_msgs__msg__DetectedObject__Sequence *
combat_robot_msgs__msg__DetectedObject__Sequence__create(size_t size)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  combat_robot_msgs__msg__DetectedObject__Sequence * array = (combat_robot_msgs__msg__DetectedObject__Sequence *)allocator.allocate(sizeof(combat_robot_msgs__msg__DetectedObject__Sequence), allocator.state);
  if (!array) {
    return NULL;
  }
  bool success = combat_robot_msgs__msg__DetectedObject__Sequence__init(array, size);
  if (!success) {
    allocator.deallocate(array, allocator.state);
    return NULL;
  }
  return array;
}

void
combat_robot_msgs__msg__DetectedObject__Sequence__destroy(combat_robot_msgs__msg__DetectedObject__Sequence * array)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  if (array) {
    combat_robot_msgs__msg__DetectedObject__Sequence__fini(array);
  }
  allocator.deallocate(array, allocator.state);
}

bool
combat_robot_msgs__msg__DetectedObject__Sequence__are_equal(const combat_robot_msgs__msg__DetectedObject__Sequence * lhs, const combat_robot_msgs__msg__DetectedObject__Sequence * rhs)
{
  if (!lhs || !rhs) {
    return false;
  }
  if (lhs->size != rhs->size) {
    return false;
  }
  for (size_t i = 0; i < lhs->size; ++i) {
    if (!combat_robot_msgs__msg__DetectedObject__are_equal(&(lhs->data[i]), &(rhs->data[i]))) {
      return false;
    }
  }
  return true;
}

bool
combat_robot_msgs__msg__DetectedObject__Sequence__copy(
  const combat_robot_msgs__msg__DetectedObject__Sequence * input,
  combat_robot_msgs__msg__DetectedObject__Sequence * output)
{
  if (!input || !output) {
    return false;
  }
  if (output->capacity < input->size) {
    const size_t allocation_size =
      input->size * sizeof(combat_robot_msgs__msg__DetectedObject);
    rcutils_allocator_t allocator = rcutils_get_default_allocator();
    combat_robot_msgs__msg__DetectedObject * data =
      (combat_robot_msgs__msg__DetectedObject *)allocator.reallocate(
      output->data, allocation_size, allocator.state);
    if (!data) {
      return false;
    }
    // If reallocation succeeded, memory may or may not have been moved
    // to fulfill the allocation request, invalidating output->data.
    output->data = data;
    for (size_t i = output->capacity; i < input->size; ++i) {
      if (!combat_robot_msgs__msg__DetectedObject__init(&output->data[i])) {
        // If initialization of any new item fails, roll back
        // all previously initialized items. Existing items
        // in output are to be left unmodified.
        for (; i-- > output->capacity; ) {
          combat_robot_msgs__msg__DetectedObject__fini(&output->data[i]);
        }
        return false;
      }
    }
    output->capacity = input->size;
  }
  output->size = input->size;
  for (size_t i = 0; i < input->size; ++i) {
    if (!combat_robot_msgs__msg__DetectedObject__copy(
        &(input->data[i]), &(output->data[i])))
    {
      return false;
    }
  }
  return true;
}
