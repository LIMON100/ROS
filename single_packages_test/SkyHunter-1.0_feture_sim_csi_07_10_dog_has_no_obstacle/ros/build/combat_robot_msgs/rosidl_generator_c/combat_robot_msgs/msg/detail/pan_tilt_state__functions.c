// generated from rosidl_generator_c/resource/idl__functions.c.em
// with input from combat_robot_msgs:msg/PanTiltState.idl
// generated code does not contain a copyright notice
#include "combat_robot_msgs/msg/detail/pan_tilt_state__functions.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "rcutils/allocator.h"


// Include directives for member types
// Member `stamp`
#include "builtin_interfaces/msg/detail/time__functions.h"

bool
combat_robot_msgs__msg__PanTiltState__init(combat_robot_msgs__msg__PanTiltState * msg)
{
  if (!msg) {
    return false;
  }
  // stamp
  if (!builtin_interfaces__msg__Time__init(&msg->stamp)) {
    combat_robot_msgs__msg__PanTiltState__fini(msg);
    return false;
  }
  // control_mode
  // horizontal_angle
  // vertical_angle
  // pan_speed
  // tilt_speed
  return true;
}

void
combat_robot_msgs__msg__PanTiltState__fini(combat_robot_msgs__msg__PanTiltState * msg)
{
  if (!msg) {
    return;
  }
  // stamp
  builtin_interfaces__msg__Time__fini(&msg->stamp);
  // control_mode
  // horizontal_angle
  // vertical_angle
  // pan_speed
  // tilt_speed
}

bool
combat_robot_msgs__msg__PanTiltState__are_equal(const combat_robot_msgs__msg__PanTiltState * lhs, const combat_robot_msgs__msg__PanTiltState * rhs)
{
  if (!lhs || !rhs) {
    return false;
  }
  // stamp
  if (!builtin_interfaces__msg__Time__are_equal(
      &(lhs->stamp), &(rhs->stamp)))
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
  return true;
}

bool
combat_robot_msgs__msg__PanTiltState__copy(
  const combat_robot_msgs__msg__PanTiltState * input,
  combat_robot_msgs__msg__PanTiltState * output)
{
  if (!input || !output) {
    return false;
  }
  // stamp
  if (!builtin_interfaces__msg__Time__copy(
      &(input->stamp), &(output->stamp)))
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
  return true;
}

combat_robot_msgs__msg__PanTiltState *
combat_robot_msgs__msg__PanTiltState__create(void)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  combat_robot_msgs__msg__PanTiltState * msg = (combat_robot_msgs__msg__PanTiltState *)allocator.allocate(sizeof(combat_robot_msgs__msg__PanTiltState), allocator.state);
  if (!msg) {
    return NULL;
  }
  memset(msg, 0, sizeof(combat_robot_msgs__msg__PanTiltState));
  bool success = combat_robot_msgs__msg__PanTiltState__init(msg);
  if (!success) {
    allocator.deallocate(msg, allocator.state);
    return NULL;
  }
  return msg;
}

void
combat_robot_msgs__msg__PanTiltState__destroy(combat_robot_msgs__msg__PanTiltState * msg)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  if (msg) {
    combat_robot_msgs__msg__PanTiltState__fini(msg);
  }
  allocator.deallocate(msg, allocator.state);
}


bool
combat_robot_msgs__msg__PanTiltState__Sequence__init(combat_robot_msgs__msg__PanTiltState__Sequence * array, size_t size)
{
  if (!array) {
    return false;
  }
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  combat_robot_msgs__msg__PanTiltState * data = NULL;

  if (size) {
    data = (combat_robot_msgs__msg__PanTiltState *)allocator.zero_allocate(size, sizeof(combat_robot_msgs__msg__PanTiltState), allocator.state);
    if (!data) {
      return false;
    }
    // initialize all array elements
    size_t i;
    for (i = 0; i < size; ++i) {
      bool success = combat_robot_msgs__msg__PanTiltState__init(&data[i]);
      if (!success) {
        break;
      }
    }
    if (i < size) {
      // if initialization failed finalize the already initialized array elements
      for (; i > 0; --i) {
        combat_robot_msgs__msg__PanTiltState__fini(&data[i - 1]);
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
combat_robot_msgs__msg__PanTiltState__Sequence__fini(combat_robot_msgs__msg__PanTiltState__Sequence * array)
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
      combat_robot_msgs__msg__PanTiltState__fini(&array->data[i]);
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

combat_robot_msgs__msg__PanTiltState__Sequence *
combat_robot_msgs__msg__PanTiltState__Sequence__create(size_t size)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  combat_robot_msgs__msg__PanTiltState__Sequence * array = (combat_robot_msgs__msg__PanTiltState__Sequence *)allocator.allocate(sizeof(combat_robot_msgs__msg__PanTiltState__Sequence), allocator.state);
  if (!array) {
    return NULL;
  }
  bool success = combat_robot_msgs__msg__PanTiltState__Sequence__init(array, size);
  if (!success) {
    allocator.deallocate(array, allocator.state);
    return NULL;
  }
  return array;
}

void
combat_robot_msgs__msg__PanTiltState__Sequence__destroy(combat_robot_msgs__msg__PanTiltState__Sequence * array)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  if (array) {
    combat_robot_msgs__msg__PanTiltState__Sequence__fini(array);
  }
  allocator.deallocate(array, allocator.state);
}

bool
combat_robot_msgs__msg__PanTiltState__Sequence__are_equal(const combat_robot_msgs__msg__PanTiltState__Sequence * lhs, const combat_robot_msgs__msg__PanTiltState__Sequence * rhs)
{
  if (!lhs || !rhs) {
    return false;
  }
  if (lhs->size != rhs->size) {
    return false;
  }
  for (size_t i = 0; i < lhs->size; ++i) {
    if (!combat_robot_msgs__msg__PanTiltState__are_equal(&(lhs->data[i]), &(rhs->data[i]))) {
      return false;
    }
  }
  return true;
}

bool
combat_robot_msgs__msg__PanTiltState__Sequence__copy(
  const combat_robot_msgs__msg__PanTiltState__Sequence * input,
  combat_robot_msgs__msg__PanTiltState__Sequence * output)
{
  if (!input || !output) {
    return false;
  }
  if (output->capacity < input->size) {
    const size_t allocation_size =
      input->size * sizeof(combat_robot_msgs__msg__PanTiltState);
    rcutils_allocator_t allocator = rcutils_get_default_allocator();
    combat_robot_msgs__msg__PanTiltState * data =
      (combat_robot_msgs__msg__PanTiltState *)allocator.reallocate(
      output->data, allocation_size, allocator.state);
    if (!data) {
      return false;
    }
    // If reallocation succeeded, memory may or may not have been moved
    // to fulfill the allocation request, invalidating output->data.
    output->data = data;
    for (size_t i = output->capacity; i < input->size; ++i) {
      if (!combat_robot_msgs__msg__PanTiltState__init(&output->data[i])) {
        // If initialization of any new item fails, roll back
        // all previously initialized items. Existing items
        // in output are to be left unmodified.
        for (; i-- > output->capacity; ) {
          combat_robot_msgs__msg__PanTiltState__fini(&output->data[i]);
        }
        return false;
      }
    }
    output->capacity = input->size;
  }
  output->size = input->size;
  for (size_t i = 0; i < input->size; ++i) {
    if (!combat_robot_msgs__msg__PanTiltState__copy(
        &(input->data[i]), &(output->data[i])))
    {
      return false;
    }
  }
  return true;
}
