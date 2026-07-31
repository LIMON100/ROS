// generated from rosidl_generator_c/resource/idl__functions.c.em
// with input from combat_robot_msgs:msg/GnssStatus.idl
// generated code does not contain a copyright notice
#include "combat_robot_msgs/msg/detail/gnss_status__functions.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "rcutils/allocator.h"


// Include directives for member types
// Member `header`
#include "std_msgs/msg/detail/header__functions.h"

bool
combat_robot_msgs__msg__GnssStatus__init(combat_robot_msgs__msg__GnssStatus * msg)
{
  if (!msg) {
    return false;
  }
  // header
  if (!std_msgs__msg__Header__init(&msg->header)) {
    combat_robot_msgs__msg__GnssStatus__fini(msg);
    return false;
  }
  // fix_status
  // num_satellites
  // latitude
  // longitude
  // altitude_m
  // heading_deg
  // ground_speed_mps
  // horizontal_accuracy_m
  // vertical_accuracy_m
  return true;
}

void
combat_robot_msgs__msg__GnssStatus__fini(combat_robot_msgs__msg__GnssStatus * msg)
{
  if (!msg) {
    return;
  }
  // header
  std_msgs__msg__Header__fini(&msg->header);
  // fix_status
  // num_satellites
  // latitude
  // longitude
  // altitude_m
  // heading_deg
  // ground_speed_mps
  // horizontal_accuracy_m
  // vertical_accuracy_m
}

bool
combat_robot_msgs__msg__GnssStatus__are_equal(const combat_robot_msgs__msg__GnssStatus * lhs, const combat_robot_msgs__msg__GnssStatus * rhs)
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
  // fix_status
  if (lhs->fix_status != rhs->fix_status) {
    return false;
  }
  // num_satellites
  if (lhs->num_satellites != rhs->num_satellites) {
    return false;
  }
  // latitude
  if (lhs->latitude != rhs->latitude) {
    return false;
  }
  // longitude
  if (lhs->longitude != rhs->longitude) {
    return false;
  }
  // altitude_m
  if (lhs->altitude_m != rhs->altitude_m) {
    return false;
  }
  // heading_deg
  if (lhs->heading_deg != rhs->heading_deg) {
    return false;
  }
  // ground_speed_mps
  if (lhs->ground_speed_mps != rhs->ground_speed_mps) {
    return false;
  }
  // horizontal_accuracy_m
  if (lhs->horizontal_accuracy_m != rhs->horizontal_accuracy_m) {
    return false;
  }
  // vertical_accuracy_m
  if (lhs->vertical_accuracy_m != rhs->vertical_accuracy_m) {
    return false;
  }
  return true;
}

bool
combat_robot_msgs__msg__GnssStatus__copy(
  const combat_robot_msgs__msg__GnssStatus * input,
  combat_robot_msgs__msg__GnssStatus * output)
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
  // fix_status
  output->fix_status = input->fix_status;
  // num_satellites
  output->num_satellites = input->num_satellites;
  // latitude
  output->latitude = input->latitude;
  // longitude
  output->longitude = input->longitude;
  // altitude_m
  output->altitude_m = input->altitude_m;
  // heading_deg
  output->heading_deg = input->heading_deg;
  // ground_speed_mps
  output->ground_speed_mps = input->ground_speed_mps;
  // horizontal_accuracy_m
  output->horizontal_accuracy_m = input->horizontal_accuracy_m;
  // vertical_accuracy_m
  output->vertical_accuracy_m = input->vertical_accuracy_m;
  return true;
}

combat_robot_msgs__msg__GnssStatus *
combat_robot_msgs__msg__GnssStatus__create(void)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  combat_robot_msgs__msg__GnssStatus * msg = (combat_robot_msgs__msg__GnssStatus *)allocator.allocate(sizeof(combat_robot_msgs__msg__GnssStatus), allocator.state);
  if (!msg) {
    return NULL;
  }
  memset(msg, 0, sizeof(combat_robot_msgs__msg__GnssStatus));
  bool success = combat_robot_msgs__msg__GnssStatus__init(msg);
  if (!success) {
    allocator.deallocate(msg, allocator.state);
    return NULL;
  }
  return msg;
}

void
combat_robot_msgs__msg__GnssStatus__destroy(combat_robot_msgs__msg__GnssStatus * msg)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  if (msg) {
    combat_robot_msgs__msg__GnssStatus__fini(msg);
  }
  allocator.deallocate(msg, allocator.state);
}


bool
combat_robot_msgs__msg__GnssStatus__Sequence__init(combat_robot_msgs__msg__GnssStatus__Sequence * array, size_t size)
{
  if (!array) {
    return false;
  }
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  combat_robot_msgs__msg__GnssStatus * data = NULL;

  if (size) {
    data = (combat_robot_msgs__msg__GnssStatus *)allocator.zero_allocate(size, sizeof(combat_robot_msgs__msg__GnssStatus), allocator.state);
    if (!data) {
      return false;
    }
    // initialize all array elements
    size_t i;
    for (i = 0; i < size; ++i) {
      bool success = combat_robot_msgs__msg__GnssStatus__init(&data[i]);
      if (!success) {
        break;
      }
    }
    if (i < size) {
      // if initialization failed finalize the already initialized array elements
      for (; i > 0; --i) {
        combat_robot_msgs__msg__GnssStatus__fini(&data[i - 1]);
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
combat_robot_msgs__msg__GnssStatus__Sequence__fini(combat_robot_msgs__msg__GnssStatus__Sequence * array)
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
      combat_robot_msgs__msg__GnssStatus__fini(&array->data[i]);
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

combat_robot_msgs__msg__GnssStatus__Sequence *
combat_robot_msgs__msg__GnssStatus__Sequence__create(size_t size)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  combat_robot_msgs__msg__GnssStatus__Sequence * array = (combat_robot_msgs__msg__GnssStatus__Sequence *)allocator.allocate(sizeof(combat_robot_msgs__msg__GnssStatus__Sequence), allocator.state);
  if (!array) {
    return NULL;
  }
  bool success = combat_robot_msgs__msg__GnssStatus__Sequence__init(array, size);
  if (!success) {
    allocator.deallocate(array, allocator.state);
    return NULL;
  }
  return array;
}

void
combat_robot_msgs__msg__GnssStatus__Sequence__destroy(combat_robot_msgs__msg__GnssStatus__Sequence * array)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  if (array) {
    combat_robot_msgs__msg__GnssStatus__Sequence__fini(array);
  }
  allocator.deallocate(array, allocator.state);
}

bool
combat_robot_msgs__msg__GnssStatus__Sequence__are_equal(const combat_robot_msgs__msg__GnssStatus__Sequence * lhs, const combat_robot_msgs__msg__GnssStatus__Sequence * rhs)
{
  if (!lhs || !rhs) {
    return false;
  }
  if (lhs->size != rhs->size) {
    return false;
  }
  for (size_t i = 0; i < lhs->size; ++i) {
    if (!combat_robot_msgs__msg__GnssStatus__are_equal(&(lhs->data[i]), &(rhs->data[i]))) {
      return false;
    }
  }
  return true;
}

bool
combat_robot_msgs__msg__GnssStatus__Sequence__copy(
  const combat_robot_msgs__msg__GnssStatus__Sequence * input,
  combat_robot_msgs__msg__GnssStatus__Sequence * output)
{
  if (!input || !output) {
    return false;
  }
  if (output->capacity < input->size) {
    const size_t allocation_size =
      input->size * sizeof(combat_robot_msgs__msg__GnssStatus);
    rcutils_allocator_t allocator = rcutils_get_default_allocator();
    combat_robot_msgs__msg__GnssStatus * data =
      (combat_robot_msgs__msg__GnssStatus *)allocator.reallocate(
      output->data, allocation_size, allocator.state);
    if (!data) {
      return false;
    }
    // If reallocation succeeded, memory may or may not have been moved
    // to fulfill the allocation request, invalidating output->data.
    output->data = data;
    for (size_t i = output->capacity; i < input->size; ++i) {
      if (!combat_robot_msgs__msg__GnssStatus__init(&output->data[i])) {
        // If initialization of any new item fails, roll back
        // all previously initialized items. Existing items
        // in output are to be left unmodified.
        for (; i-- > output->capacity; ) {
          combat_robot_msgs__msg__GnssStatus__fini(&output->data[i]);
        }
        return false;
      }
    }
    output->capacity = input->size;
  }
  output->size = input->size;
  for (size_t i = 0; i < input->size; ++i) {
    if (!combat_robot_msgs__msg__GnssStatus__copy(
        &(input->data[i]), &(output->data[i])))
    {
      return false;
    }
  }
  return true;
}
