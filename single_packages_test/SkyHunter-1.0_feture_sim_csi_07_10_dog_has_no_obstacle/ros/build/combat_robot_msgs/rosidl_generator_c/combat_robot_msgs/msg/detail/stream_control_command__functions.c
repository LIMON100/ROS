// generated from rosidl_generator_c/resource/idl__functions.c.em
// with input from combat_robot_msgs:msg/StreamControlCommand.idl
// generated code does not contain a copyright notice
#include "combat_robot_msgs/msg/detail/stream_control_command__functions.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "rcutils/allocator.h"


// Include directives for member types
// Member `header`
#include "std_msgs/msg/detail/header__functions.h"

bool
combat_robot_msgs__msg__StreamControlCommand__init(combat_robot_msgs__msg__StreamControlCommand * msg)
{
  if (!msg) {
    return false;
  }
  // header
  if (!std_msgs__msg__Header__init(&msg->header)) {
    combat_robot_msgs__msg__StreamControlCommand__fini(msg);
    return false;
  }
  // stream_command
  // stream_target_robot_id
  return true;
}

void
combat_robot_msgs__msg__StreamControlCommand__fini(combat_robot_msgs__msg__StreamControlCommand * msg)
{
  if (!msg) {
    return;
  }
  // header
  std_msgs__msg__Header__fini(&msg->header);
  // stream_command
  // stream_target_robot_id
}

bool
combat_robot_msgs__msg__StreamControlCommand__are_equal(const combat_robot_msgs__msg__StreamControlCommand * lhs, const combat_robot_msgs__msg__StreamControlCommand * rhs)
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
  // stream_command
  if (lhs->stream_command != rhs->stream_command) {
    return false;
  }
  // stream_target_robot_id
  if (lhs->stream_target_robot_id != rhs->stream_target_robot_id) {
    return false;
  }
  return true;
}

bool
combat_robot_msgs__msg__StreamControlCommand__copy(
  const combat_robot_msgs__msg__StreamControlCommand * input,
  combat_robot_msgs__msg__StreamControlCommand * output)
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
  // stream_command
  output->stream_command = input->stream_command;
  // stream_target_robot_id
  output->stream_target_robot_id = input->stream_target_robot_id;
  return true;
}

combat_robot_msgs__msg__StreamControlCommand *
combat_robot_msgs__msg__StreamControlCommand__create(void)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  combat_robot_msgs__msg__StreamControlCommand * msg = (combat_robot_msgs__msg__StreamControlCommand *)allocator.allocate(sizeof(combat_robot_msgs__msg__StreamControlCommand), allocator.state);
  if (!msg) {
    return NULL;
  }
  memset(msg, 0, sizeof(combat_robot_msgs__msg__StreamControlCommand));
  bool success = combat_robot_msgs__msg__StreamControlCommand__init(msg);
  if (!success) {
    allocator.deallocate(msg, allocator.state);
    return NULL;
  }
  return msg;
}

void
combat_robot_msgs__msg__StreamControlCommand__destroy(combat_robot_msgs__msg__StreamControlCommand * msg)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  if (msg) {
    combat_robot_msgs__msg__StreamControlCommand__fini(msg);
  }
  allocator.deallocate(msg, allocator.state);
}


bool
combat_robot_msgs__msg__StreamControlCommand__Sequence__init(combat_robot_msgs__msg__StreamControlCommand__Sequence * array, size_t size)
{
  if (!array) {
    return false;
  }
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  combat_robot_msgs__msg__StreamControlCommand * data = NULL;

  if (size) {
    data = (combat_robot_msgs__msg__StreamControlCommand *)allocator.zero_allocate(size, sizeof(combat_robot_msgs__msg__StreamControlCommand), allocator.state);
    if (!data) {
      return false;
    }
    // initialize all array elements
    size_t i;
    for (i = 0; i < size; ++i) {
      bool success = combat_robot_msgs__msg__StreamControlCommand__init(&data[i]);
      if (!success) {
        break;
      }
    }
    if (i < size) {
      // if initialization failed finalize the already initialized array elements
      for (; i > 0; --i) {
        combat_robot_msgs__msg__StreamControlCommand__fini(&data[i - 1]);
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
combat_robot_msgs__msg__StreamControlCommand__Sequence__fini(combat_robot_msgs__msg__StreamControlCommand__Sequence * array)
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
      combat_robot_msgs__msg__StreamControlCommand__fini(&array->data[i]);
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

combat_robot_msgs__msg__StreamControlCommand__Sequence *
combat_robot_msgs__msg__StreamControlCommand__Sequence__create(size_t size)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  combat_robot_msgs__msg__StreamControlCommand__Sequence * array = (combat_robot_msgs__msg__StreamControlCommand__Sequence *)allocator.allocate(sizeof(combat_robot_msgs__msg__StreamControlCommand__Sequence), allocator.state);
  if (!array) {
    return NULL;
  }
  bool success = combat_robot_msgs__msg__StreamControlCommand__Sequence__init(array, size);
  if (!success) {
    allocator.deallocate(array, allocator.state);
    return NULL;
  }
  return array;
}

void
combat_robot_msgs__msg__StreamControlCommand__Sequence__destroy(combat_robot_msgs__msg__StreamControlCommand__Sequence * array)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  if (array) {
    combat_robot_msgs__msg__StreamControlCommand__Sequence__fini(array);
  }
  allocator.deallocate(array, allocator.state);
}

bool
combat_robot_msgs__msg__StreamControlCommand__Sequence__are_equal(const combat_robot_msgs__msg__StreamControlCommand__Sequence * lhs, const combat_robot_msgs__msg__StreamControlCommand__Sequence * rhs)
{
  if (!lhs || !rhs) {
    return false;
  }
  if (lhs->size != rhs->size) {
    return false;
  }
  for (size_t i = 0; i < lhs->size; ++i) {
    if (!combat_robot_msgs__msg__StreamControlCommand__are_equal(&(lhs->data[i]), &(rhs->data[i]))) {
      return false;
    }
  }
  return true;
}

bool
combat_robot_msgs__msg__StreamControlCommand__Sequence__copy(
  const combat_robot_msgs__msg__StreamControlCommand__Sequence * input,
  combat_robot_msgs__msg__StreamControlCommand__Sequence * output)
{
  if (!input || !output) {
    return false;
  }
  if (output->capacity < input->size) {
    const size_t allocation_size =
      input->size * sizeof(combat_robot_msgs__msg__StreamControlCommand);
    rcutils_allocator_t allocator = rcutils_get_default_allocator();
    combat_robot_msgs__msg__StreamControlCommand * data =
      (combat_robot_msgs__msg__StreamControlCommand *)allocator.reallocate(
      output->data, allocation_size, allocator.state);
    if (!data) {
      return false;
    }
    // If reallocation succeeded, memory may or may not have been moved
    // to fulfill the allocation request, invalidating output->data.
    output->data = data;
    for (size_t i = output->capacity; i < input->size; ++i) {
      if (!combat_robot_msgs__msg__StreamControlCommand__init(&output->data[i])) {
        // If initialization of any new item fails, roll back
        // all previously initialized items. Existing items
        // in output are to be left unmodified.
        for (; i-- > output->capacity; ) {
          combat_robot_msgs__msg__StreamControlCommand__fini(&output->data[i]);
        }
        return false;
      }
    }
    output->capacity = input->size;
  }
  output->size = input->size;
  for (size_t i = 0; i < input->size; ++i) {
    if (!combat_robot_msgs__msg__StreamControlCommand__copy(
        &(input->data[i]), &(output->data[i])))
    {
      return false;
    }
  }
  return true;
}
