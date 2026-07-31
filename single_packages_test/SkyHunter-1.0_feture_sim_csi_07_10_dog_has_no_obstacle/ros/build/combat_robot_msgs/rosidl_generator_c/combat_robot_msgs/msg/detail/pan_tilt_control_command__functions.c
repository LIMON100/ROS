// generated from rosidl_generator_c/resource/idl__functions.c.em
// with input from combat_robot_msgs:msg/PanTiltControlCommand.idl
// generated code does not contain a copyright notice
#include "combat_robot_msgs/msg/detail/pan_tilt_control_command__functions.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "rcutils/allocator.h"


// Include directives for member types
// Member `header`
#include "std_msgs/msg/detail/header__functions.h"

bool
combat_robot_msgs__msg__PanTiltControlCommand__init(combat_robot_msgs__msg__PanTiltControlCommand * msg)
{
  if (!msg) {
    return false;
  }
  // header
  if (!std_msgs__msg__Header__init(&msg->header)) {
    combat_robot_msgs__msg__PanTiltControlCommand__fini(msg);
    return false;
  }
  // control_mode
  // horizontal_angle
  // vertical_angle
  // pan_speed
  // tilt_speed
  // pan_dir
  // tilt_dir
  return true;
}

void
combat_robot_msgs__msg__PanTiltControlCommand__fini(combat_robot_msgs__msg__PanTiltControlCommand * msg)
{
  if (!msg) {
    return;
  }
  // header
  std_msgs__msg__Header__fini(&msg->header);
  // control_mode
  // horizontal_angle
  // vertical_angle
  // pan_speed
  // tilt_speed
  // pan_dir
  // tilt_dir
}

bool
combat_robot_msgs__msg__PanTiltControlCommand__are_equal(const combat_robot_msgs__msg__PanTiltControlCommand * lhs, const combat_robot_msgs__msg__PanTiltControlCommand * rhs)
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
  // control_mode
  if (lhs->control_mode != rhs->control_mode) {
    return false;
  }
  // horizontal_angle
  if (lhs->horizontal_angle != rhs->horizontal_angle) {
    return false;
  }
  // vertical_angle
  if (lhs->vertical_angle != rhs->vertical_angle) {
    return false;
  }
  // pan_speed
  if (lhs->pan_speed != rhs->pan_speed) {
    return false;
  }
  // tilt_speed
  if (lhs->tilt_speed != rhs->tilt_speed) {
    return false;
  }
  // pan_dir
  if (lhs->pan_dir != rhs->pan_dir) {
    return false;
  }
  // tilt_dir
  if (lhs->tilt_dir != rhs->tilt_dir) {
    return false;
  }
  return true;
}

bool
combat_robot_msgs__msg__PanTiltControlCommand__copy(
  const combat_robot_msgs__msg__PanTiltControlCommand * input,
  combat_robot_msgs__msg__PanTiltControlCommand * output)
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
  // control_mode
  output->control_mode = input->control_mode;
  // horizontal_angle
  output->horizontal_angle = input->horizontal_angle;
  // vertical_angle
  output->vertical_angle = input->vertical_angle;
  // pan_speed
  output->pan_speed = input->pan_speed;
  // tilt_speed
  output->tilt_speed = input->tilt_speed;
  // pan_dir
  output->pan_dir = input->pan_dir;
  // tilt_dir
  output->tilt_dir = input->tilt_dir;
  return true;
}

combat_robot_msgs__msg__PanTiltControlCommand *
combat_robot_msgs__msg__PanTiltControlCommand__create(void)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  combat_robot_msgs__msg__PanTiltControlCommand * msg = (combat_robot_msgs__msg__PanTiltControlCommand *)allocator.allocate(sizeof(combat_robot_msgs__msg__PanTiltControlCommand), allocator.state);
  if (!msg) {
    return NULL;
  }
  memset(msg, 0, sizeof(combat_robot_msgs__msg__PanTiltControlCommand));
  bool success = combat_robot_msgs__msg__PanTiltControlCommand__init(msg);
  if (!success) {
    allocator.deallocate(msg, allocator.state);
    return NULL;
  }
  return msg;
}

void
combat_robot_msgs__msg__PanTiltControlCommand__destroy(combat_robot_msgs__msg__PanTiltControlCommand * msg)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  if (msg) {
    combat_robot_msgs__msg__PanTiltControlCommand__fini(msg);
  }
  allocator.deallocate(msg, allocator.state);
}


bool
combat_robot_msgs__msg__PanTiltControlCommand__Sequence__init(combat_robot_msgs__msg__PanTiltControlCommand__Sequence * array, size_t size)
{
  if (!array) {
    return false;
  }
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  combat_robot_msgs__msg__PanTiltControlCommand * data = NULL;

  if (size) {
    data = (combat_robot_msgs__msg__PanTiltControlCommand *)allocator.zero_allocate(size, sizeof(combat_robot_msgs__msg__PanTiltControlCommand), allocator.state);
    if (!data) {
      return false;
    }
    // initialize all array elements
    size_t i;
    for (i = 0; i < size; ++i) {
      bool success = combat_robot_msgs__msg__PanTiltControlCommand__init(&data[i]);
      if (!success) {
        break;
      }
    }
    if (i < size) {
      // if initialization failed finalize the already initialized array elements
      for (; i > 0; --i) {
        combat_robot_msgs__msg__PanTiltControlCommand__fini(&data[i - 1]);
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
combat_robot_msgs__msg__PanTiltControlCommand__Sequence__fini(combat_robot_msgs__msg__PanTiltControlCommand__Sequence * array)
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
      combat_robot_msgs__msg__PanTiltControlCommand__fini(&array->data[i]);
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

combat_robot_msgs__msg__PanTiltControlCommand__Sequence *
combat_robot_msgs__msg__PanTiltControlCommand__Sequence__create(size_t size)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  combat_robot_msgs__msg__PanTiltControlCommand__Sequence * array = (combat_robot_msgs__msg__PanTiltControlCommand__Sequence *)allocator.allocate(sizeof(combat_robot_msgs__msg__PanTiltControlCommand__Sequence), allocator.state);
  if (!array) {
    return NULL;
  }
  bool success = combat_robot_msgs__msg__PanTiltControlCommand__Sequence__init(array, size);
  if (!success) {
    allocator.deallocate(array, allocator.state);
    return NULL;
  }
  return array;
}

void
combat_robot_msgs__msg__PanTiltControlCommand__Sequence__destroy(combat_robot_msgs__msg__PanTiltControlCommand__Sequence * array)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  if (array) {
    combat_robot_msgs__msg__PanTiltControlCommand__Sequence__fini(array);
  }
  allocator.deallocate(array, allocator.state);
}

bool
combat_robot_msgs__msg__PanTiltControlCommand__Sequence__are_equal(const combat_robot_msgs__msg__PanTiltControlCommand__Sequence * lhs, const combat_robot_msgs__msg__PanTiltControlCommand__Sequence * rhs)
{
  if (!lhs || !rhs) {
    return false;
  }
  if (lhs->size != rhs->size) {
    return false;
  }
  for (size_t i = 0; i < lhs->size; ++i) {
    if (!combat_robot_msgs__msg__PanTiltControlCommand__are_equal(&(lhs->data[i]), &(rhs->data[i]))) {
      return false;
    }
  }
  return true;
}

bool
combat_robot_msgs__msg__PanTiltControlCommand__Sequence__copy(
  const combat_robot_msgs__msg__PanTiltControlCommand__Sequence * input,
  combat_robot_msgs__msg__PanTiltControlCommand__Sequence * output)
{
  if (!input || !output) {
    return false;
  }
  if (output->capacity < input->size) {
    const size_t allocation_size =
      input->size * sizeof(combat_robot_msgs__msg__PanTiltControlCommand);
    rcutils_allocator_t allocator = rcutils_get_default_allocator();
    combat_robot_msgs__msg__PanTiltControlCommand * data =
      (combat_robot_msgs__msg__PanTiltControlCommand *)allocator.reallocate(
      output->data, allocation_size, allocator.state);
    if (!data) {
      return false;
    }
    // If reallocation succeeded, memory may or may not have been moved
    // to fulfill the allocation request, invalidating output->data.
    output->data = data;
    for (size_t i = output->capacity; i < input->size; ++i) {
      if (!combat_robot_msgs__msg__PanTiltControlCommand__init(&output->data[i])) {
        // If initialization of any new item fails, roll back
        // all previously initialized items. Existing items
        // in output are to be left unmodified.
        for (; i-- > output->capacity; ) {
          combat_robot_msgs__msg__PanTiltControlCommand__fini(&output->data[i]);
        }
        return false;
      }
    }
    output->capacity = input->size;
  }
  output->size = input->size;
  for (size_t i = 0; i < input->size; ++i) {
    if (!combat_robot_msgs__msg__PanTiltControlCommand__copy(
        &(input->data[i]), &(output->data[i])))
    {
      return false;
    }
  }
  return true;
}
