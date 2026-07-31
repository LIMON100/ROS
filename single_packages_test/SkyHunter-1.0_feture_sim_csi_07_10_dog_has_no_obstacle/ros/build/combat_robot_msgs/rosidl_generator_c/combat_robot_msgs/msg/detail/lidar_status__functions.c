// generated from rosidl_generator_c/resource/idl__functions.c.em
// with input from combat_robot_msgs:msg/LidarStatus.idl
// generated code does not contain a copyright notice
#include "combat_robot_msgs/msg/detail/lidar_status__functions.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "rcutils/allocator.h"


// Include directives for member types
// Member `header`
#include "std_msgs/msg/detail/header__functions.h"

bool
combat_robot_msgs__msg__LidarStatus__init(combat_robot_msgs__msg__LidarStatus * msg)
{
  if (!msg) {
    return false;
  }
  // header
  if (!std_msgs__msg__Header__init(&msg->header)) {
    combat_robot_msgs__msg__LidarStatus__fini(msg);
    return false;
  }
  // status
  // last_scan_point_count
  // scan_rate_hz
  // obstacle_detected
  // min_obstacle_distance_m
  return true;
}

void
combat_robot_msgs__msg__LidarStatus__fini(combat_robot_msgs__msg__LidarStatus * msg)
{
  if (!msg) {
    return;
  }
  // header
  std_msgs__msg__Header__fini(&msg->header);
  // status
  // last_scan_point_count
  // scan_rate_hz
  // obstacle_detected
  // min_obstacle_distance_m
}

bool
combat_robot_msgs__msg__LidarStatus__are_equal(const combat_robot_msgs__msg__LidarStatus * lhs, const combat_robot_msgs__msg__LidarStatus * rhs)
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
  // status
  if (lhs->status != rhs->status) {
    return false;
  }
  // last_scan_point_count
  if (lhs->last_scan_point_count != rhs->last_scan_point_count) {
    return false;
  }
  // scan_rate_hz
  if (lhs->scan_rate_hz != rhs->scan_rate_hz) {
    return false;
  }
  // obstacle_detected
  if (lhs->obstacle_detected != rhs->obstacle_detected) {
    return false;
  }
  // min_obstacle_distance_m
  if (lhs->min_obstacle_distance_m != rhs->min_obstacle_distance_m) {
    return false;
  }
  return true;
}

bool
combat_robot_msgs__msg__LidarStatus__copy(
  const combat_robot_msgs__msg__LidarStatus * input,
  combat_robot_msgs__msg__LidarStatus * output)
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
  // status
  output->status = input->status;
  // last_scan_point_count
  output->last_scan_point_count = input->last_scan_point_count;
  // scan_rate_hz
  output->scan_rate_hz = input->scan_rate_hz;
  // obstacle_detected
  output->obstacle_detected = input->obstacle_detected;
  // min_obstacle_distance_m
  output->min_obstacle_distance_m = input->min_obstacle_distance_m;
  return true;
}

combat_robot_msgs__msg__LidarStatus *
combat_robot_msgs__msg__LidarStatus__create(void)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  combat_robot_msgs__msg__LidarStatus * msg = (combat_robot_msgs__msg__LidarStatus *)allocator.allocate(sizeof(combat_robot_msgs__msg__LidarStatus), allocator.state);
  if (!msg) {
    return NULL;
  }
  memset(msg, 0, sizeof(combat_robot_msgs__msg__LidarStatus));
  bool success = combat_robot_msgs__msg__LidarStatus__init(msg);
  if (!success) {
    allocator.deallocate(msg, allocator.state);
    return NULL;
  }
  return msg;
}

void
combat_robot_msgs__msg__LidarStatus__destroy(combat_robot_msgs__msg__LidarStatus * msg)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  if (msg) {
    combat_robot_msgs__msg__LidarStatus__fini(msg);
  }
  allocator.deallocate(msg, allocator.state);
}


bool
combat_robot_msgs__msg__LidarStatus__Sequence__init(combat_robot_msgs__msg__LidarStatus__Sequence * array, size_t size)
{
  if (!array) {
    return false;
  }
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  combat_robot_msgs__msg__LidarStatus * data = NULL;

  if (size) {
    data = (combat_robot_msgs__msg__LidarStatus *)allocator.zero_allocate(size, sizeof(combat_robot_msgs__msg__LidarStatus), allocator.state);
    if (!data) {
      return false;
    }
    // initialize all array elements
    size_t i;
    for (i = 0; i < size; ++i) {
      bool success = combat_robot_msgs__msg__LidarStatus__init(&data[i]);
      if (!success) {
        break;
      }
    }
    if (i < size) {
      // if initialization failed finalize the already initialized array elements
      for (; i > 0; --i) {
        combat_robot_msgs__msg__LidarStatus__fini(&data[i - 1]);
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
combat_robot_msgs__msg__LidarStatus__Sequence__fini(combat_robot_msgs__msg__LidarStatus__Sequence * array)
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
      combat_robot_msgs__msg__LidarStatus__fini(&array->data[i]);
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

combat_robot_msgs__msg__LidarStatus__Sequence *
combat_robot_msgs__msg__LidarStatus__Sequence__create(size_t size)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  combat_robot_msgs__msg__LidarStatus__Sequence * array = (combat_robot_msgs__msg__LidarStatus__Sequence *)allocator.allocate(sizeof(combat_robot_msgs__msg__LidarStatus__Sequence), allocator.state);
  if (!array) {
    return NULL;
  }
  bool success = combat_robot_msgs__msg__LidarStatus__Sequence__init(array, size);
  if (!success) {
    allocator.deallocate(array, allocator.state);
    return NULL;
  }
  return array;
}

void
combat_robot_msgs__msg__LidarStatus__Sequence__destroy(combat_robot_msgs__msg__LidarStatus__Sequence * array)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  if (array) {
    combat_robot_msgs__msg__LidarStatus__Sequence__fini(array);
  }
  allocator.deallocate(array, allocator.state);
}

bool
combat_robot_msgs__msg__LidarStatus__Sequence__are_equal(const combat_robot_msgs__msg__LidarStatus__Sequence * lhs, const combat_robot_msgs__msg__LidarStatus__Sequence * rhs)
{
  if (!lhs || !rhs) {
    return false;
  }
  if (lhs->size != rhs->size) {
    return false;
  }
  for (size_t i = 0; i < lhs->size; ++i) {
    if (!combat_robot_msgs__msg__LidarStatus__are_equal(&(lhs->data[i]), &(rhs->data[i]))) {
      return false;
    }
  }
  return true;
}

bool
combat_robot_msgs__msg__LidarStatus__Sequence__copy(
  const combat_robot_msgs__msg__LidarStatus__Sequence * input,
  combat_robot_msgs__msg__LidarStatus__Sequence * output)
{
  if (!input || !output) {
    return false;
  }
  if (output->capacity < input->size) {
    const size_t allocation_size =
      input->size * sizeof(combat_robot_msgs__msg__LidarStatus);
    rcutils_allocator_t allocator = rcutils_get_default_allocator();
    combat_robot_msgs__msg__LidarStatus * data =
      (combat_robot_msgs__msg__LidarStatus *)allocator.reallocate(
      output->data, allocation_size, allocator.state);
    if (!data) {
      return false;
    }
    // If reallocation succeeded, memory may or may not have been moved
    // to fulfill the allocation request, invalidating output->data.
    output->data = data;
    for (size_t i = output->capacity; i < input->size; ++i) {
      if (!combat_robot_msgs__msg__LidarStatus__init(&output->data[i])) {
        // If initialization of any new item fails, roll back
        // all previously initialized items. Existing items
        // in output are to be left unmodified.
        for (; i-- > output->capacity; ) {
          combat_robot_msgs__msg__LidarStatus__fini(&output->data[i]);
        }
        return false;
      }
    }
    output->capacity = input->size;
  }
  output->size = input->size;
  for (size_t i = 0; i < input->size; ++i) {
    if (!combat_robot_msgs__msg__LidarStatus__copy(
        &(input->data[i]), &(output->data[i])))
    {
      return false;
    }
  }
  return true;
}
