// generated from rosidl_generator_c/resource/idl__functions.c.em
// with input from skyhunter_msgs:msg/LeaderState.idl
// generated code does not contain a copyright notice
#include "skyhunter_msgs/msg/detail/leader_state__functions.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "rcutils/allocator.h"


// Include directives for member types
// Member `header`
#include "std_msgs/msg/detail/header__functions.h"
// Member `pose`
#include "geometry_msgs/msg/detail/pose__functions.h"
// Member `velocity`
#include "geometry_msgs/msg/detail/twist__functions.h"

bool
skyhunter_msgs__msg__LeaderState__init(skyhunter_msgs__msg__LeaderState * msg)
{
  if (!msg) {
    return false;
  }
  // header
  if (!std_msgs__msg__Header__init(&msg->header)) {
    skyhunter_msgs__msg__LeaderState__fini(msg);
    return false;
  }
  // pose
  if (!geometry_msgs__msg__Pose__init(&msg->pose)) {
    skyhunter_msgs__msg__LeaderState__fini(msg);
    return false;
  }
  // velocity
  if (!geometry_msgs__msg__Twist__init(&msg->velocity)) {
    skyhunter_msgs__msg__LeaderState__fini(msg);
    return false;
  }
  // formation_mode
  // formation_state
  return true;
}

void
skyhunter_msgs__msg__LeaderState__fini(skyhunter_msgs__msg__LeaderState * msg)
{
  if (!msg) {
    return;
  }
  // header
  std_msgs__msg__Header__fini(&msg->header);
  // pose
  geometry_msgs__msg__Pose__fini(&msg->pose);
  // velocity
  geometry_msgs__msg__Twist__fini(&msg->velocity);
  // formation_mode
  // formation_state
}

bool
skyhunter_msgs__msg__LeaderState__are_equal(const skyhunter_msgs__msg__LeaderState * lhs, const skyhunter_msgs__msg__LeaderState * rhs)
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
  // pose
  if (!geometry_msgs__msg__Pose__are_equal(
      &(lhs->pose), &(rhs->pose)))
  {
    return false;
  }
  // velocity
  if (!geometry_msgs__msg__Twist__are_equal(
      &(lhs->velocity), &(rhs->velocity)))
  {
    return false;
  }
  // formation_mode
  if (lhs->formation_mode != rhs->formation_mode) {
    return false;
  }
  // formation_state
  if (lhs->formation_state != rhs->formation_state) {
    return false;
  }
  return true;
}

bool
skyhunter_msgs__msg__LeaderState__copy(
  const skyhunter_msgs__msg__LeaderState * input,
  skyhunter_msgs__msg__LeaderState * output)
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
  // pose
  if (!geometry_msgs__msg__Pose__copy(
      &(input->pose), &(output->pose)))
  {
    return false;
  }
  // velocity
  if (!geometry_msgs__msg__Twist__copy(
      &(input->velocity), &(output->velocity)))
  {
    return false;
  }
  // formation_mode
  output->formation_mode = input->formation_mode;
  // formation_state
  output->formation_state = input->formation_state;
  return true;
}

skyhunter_msgs__msg__LeaderState *
skyhunter_msgs__msg__LeaderState__create()
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  skyhunter_msgs__msg__LeaderState * msg = (skyhunter_msgs__msg__LeaderState *)allocator.allocate(sizeof(skyhunter_msgs__msg__LeaderState), allocator.state);
  if (!msg) {
    return NULL;
  }
  memset(msg, 0, sizeof(skyhunter_msgs__msg__LeaderState));
  bool success = skyhunter_msgs__msg__LeaderState__init(msg);
  if (!success) {
    allocator.deallocate(msg, allocator.state);
    return NULL;
  }
  return msg;
}

void
skyhunter_msgs__msg__LeaderState__destroy(skyhunter_msgs__msg__LeaderState * msg)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  if (msg) {
    skyhunter_msgs__msg__LeaderState__fini(msg);
  }
  allocator.deallocate(msg, allocator.state);
}


bool
skyhunter_msgs__msg__LeaderState__Sequence__init(skyhunter_msgs__msg__LeaderState__Sequence * array, size_t size)
{
  if (!array) {
    return false;
  }
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  skyhunter_msgs__msg__LeaderState * data = NULL;

  if (size) {
    data = (skyhunter_msgs__msg__LeaderState *)allocator.zero_allocate(size, sizeof(skyhunter_msgs__msg__LeaderState), allocator.state);
    if (!data) {
      return false;
    }
    // initialize all array elements
    size_t i;
    for (i = 0; i < size; ++i) {
      bool success = skyhunter_msgs__msg__LeaderState__init(&data[i]);
      if (!success) {
        break;
      }
    }
    if (i < size) {
      // if initialization failed finalize the already initialized array elements
      for (; i > 0; --i) {
        skyhunter_msgs__msg__LeaderState__fini(&data[i - 1]);
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
skyhunter_msgs__msg__LeaderState__Sequence__fini(skyhunter_msgs__msg__LeaderState__Sequence * array)
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
      skyhunter_msgs__msg__LeaderState__fini(&array->data[i]);
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

skyhunter_msgs__msg__LeaderState__Sequence *
skyhunter_msgs__msg__LeaderState__Sequence__create(size_t size)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  skyhunter_msgs__msg__LeaderState__Sequence * array = (skyhunter_msgs__msg__LeaderState__Sequence *)allocator.allocate(sizeof(skyhunter_msgs__msg__LeaderState__Sequence), allocator.state);
  if (!array) {
    return NULL;
  }
  bool success = skyhunter_msgs__msg__LeaderState__Sequence__init(array, size);
  if (!success) {
    allocator.deallocate(array, allocator.state);
    return NULL;
  }
  return array;
}

void
skyhunter_msgs__msg__LeaderState__Sequence__destroy(skyhunter_msgs__msg__LeaderState__Sequence * array)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  if (array) {
    skyhunter_msgs__msg__LeaderState__Sequence__fini(array);
  }
  allocator.deallocate(array, allocator.state);
}

bool
skyhunter_msgs__msg__LeaderState__Sequence__are_equal(const skyhunter_msgs__msg__LeaderState__Sequence * lhs, const skyhunter_msgs__msg__LeaderState__Sequence * rhs)
{
  if (!lhs || !rhs) {
    return false;
  }
  if (lhs->size != rhs->size) {
    return false;
  }
  for (size_t i = 0; i < lhs->size; ++i) {
    if (!skyhunter_msgs__msg__LeaderState__are_equal(&(lhs->data[i]), &(rhs->data[i]))) {
      return false;
    }
  }
  return true;
}

bool
skyhunter_msgs__msg__LeaderState__Sequence__copy(
  const skyhunter_msgs__msg__LeaderState__Sequence * input,
  skyhunter_msgs__msg__LeaderState__Sequence * output)
{
  if (!input || !output) {
    return false;
  }
  if (output->capacity < input->size) {
    const size_t allocation_size =
      input->size * sizeof(skyhunter_msgs__msg__LeaderState);
    rcutils_allocator_t allocator = rcutils_get_default_allocator();
    skyhunter_msgs__msg__LeaderState * data =
      (skyhunter_msgs__msg__LeaderState *)allocator.reallocate(
      output->data, allocation_size, allocator.state);
    if (!data) {
      return false;
    }
    // If reallocation succeeded, memory may or may not have been moved
    // to fulfill the allocation request, invalidating output->data.
    output->data = data;
    for (size_t i = output->capacity; i < input->size; ++i) {
      if (!skyhunter_msgs__msg__LeaderState__init(&output->data[i])) {
        // If initialization of any new item fails, roll back
        // all previously initialized items. Existing items
        // in output are to be left unmodified.
        for (; i-- > output->capacity; ) {
          skyhunter_msgs__msg__LeaderState__fini(&output->data[i]);
        }
        return false;
      }
    }
    output->capacity = input->size;
  }
  output->size = input->size;
  for (size_t i = 0; i < input->size; ++i) {
    if (!skyhunter_msgs__msg__LeaderState__copy(
        &(input->data[i]), &(output->data[i])))
    {
      return false;
    }
  }
  return true;
}
