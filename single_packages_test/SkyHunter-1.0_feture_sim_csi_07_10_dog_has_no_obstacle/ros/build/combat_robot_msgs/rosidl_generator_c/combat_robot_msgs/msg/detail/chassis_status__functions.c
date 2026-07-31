// generated from rosidl_generator_c/resource/idl__functions.c.em
// with input from combat_robot_msgs:msg/ChassisStatus.idl
// generated code does not contain a copyright notice
#include "combat_robot_msgs/msg/detail/chassis_status__functions.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "rcutils/allocator.h"


// Include directives for member types
// Member `header`
#include "std_msgs/msg/detail/header__functions.h"

bool
combat_robot_msgs__msg__ChassisStatus__init(combat_robot_msgs__msg__ChassisStatus * msg)
{
  if (!msg) {
    return false;
  }
  // header
  if (!std_msgs__msg__Header__init(&msg->header)) {
    combat_robot_msgs__msg__ChassisStatus__fini(msg);
    return false;
  }
  // drive_state
  // battery_pct
  // battery_voltage_v
  // battery_current_a
  // linear_velocity_mps
  // angular_velocity_rps
  // fault_flags
  // motor_temp_c
  return true;
}

void
combat_robot_msgs__msg__ChassisStatus__fini(combat_robot_msgs__msg__ChassisStatus * msg)
{
  if (!msg) {
    return;
  }
  // header
  std_msgs__msg__Header__fini(&msg->header);
  // drive_state
  // battery_pct
  // battery_voltage_v
  // battery_current_a
  // linear_velocity_mps
  // angular_velocity_rps
  // fault_flags
  // motor_temp_c
}

bool
combat_robot_msgs__msg__ChassisStatus__are_equal(const combat_robot_msgs__msg__ChassisStatus * lhs, const combat_robot_msgs__msg__ChassisStatus * rhs)
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
  // drive_state
  if (lhs->drive_state != rhs->drive_state) {
    return false;
  }
  // battery_pct
  if (lhs->battery_pct != rhs->battery_pct) {
    return false;
  }
  // battery_voltage_v
  if (lhs->battery_voltage_v != rhs->battery_voltage_v) {
    return false;
  }
  // battery_current_a
  if (lhs->battery_current_a != rhs->battery_current_a) {
    return false;
  }
  // linear_velocity_mps
  if (lhs->linear_velocity_mps != rhs->linear_velocity_mps) {
    return false;
  }
  // angular_velocity_rps
  if (lhs->angular_velocity_rps != rhs->angular_velocity_rps) {
    return false;
  }
  // fault_flags
  if (lhs->fault_flags != rhs->fault_flags) {
    return false;
  }
  // motor_temp_c
  if (lhs->motor_temp_c != rhs->motor_temp_c) {
    return false;
  }
  return true;
}

bool
combat_robot_msgs__msg__ChassisStatus__copy(
  const combat_robot_msgs__msg__ChassisStatus * input,
  combat_robot_msgs__msg__ChassisStatus * output)
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
  // drive_state
  output->drive_state = input->drive_state;
  // battery_pct
  output->battery_pct = input->battery_pct;
  // battery_voltage_v
  output->battery_voltage_v = input->battery_voltage_v;
  // battery_current_a
  output->battery_current_a = input->battery_current_a;
  // linear_velocity_mps
  output->linear_velocity_mps = input->linear_velocity_mps;
  // angular_velocity_rps
  output->angular_velocity_rps = input->angular_velocity_rps;
  // fault_flags
  output->fault_flags = input->fault_flags;
  // motor_temp_c
  output->motor_temp_c = input->motor_temp_c;
  return true;
}

combat_robot_msgs__msg__ChassisStatus *
combat_robot_msgs__msg__ChassisStatus__create(void)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  combat_robot_msgs__msg__ChassisStatus * msg = (combat_robot_msgs__msg__ChassisStatus *)allocator.allocate(sizeof(combat_robot_msgs__msg__ChassisStatus), allocator.state);
  if (!msg) {
    return NULL;
  }
  memset(msg, 0, sizeof(combat_robot_msgs__msg__ChassisStatus));
  bool success = combat_robot_msgs__msg__ChassisStatus__init(msg);
  if (!success) {
    allocator.deallocate(msg, allocator.state);
    return NULL;
  }
  return msg;
}

void
combat_robot_msgs__msg__ChassisStatus__destroy(combat_robot_msgs__msg__ChassisStatus * msg)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  if (msg) {
    combat_robot_msgs__msg__ChassisStatus__fini(msg);
  }
  allocator.deallocate(msg, allocator.state);
}


bool
combat_robot_msgs__msg__ChassisStatus__Sequence__init(combat_robot_msgs__msg__ChassisStatus__Sequence * array, size_t size)
{
  if (!array) {
    return false;
  }
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  combat_robot_msgs__msg__ChassisStatus * data = NULL;

  if (size) {
    data = (combat_robot_msgs__msg__ChassisStatus *)allocator.zero_allocate(size, sizeof(combat_robot_msgs__msg__ChassisStatus), allocator.state);
    if (!data) {
      return false;
    }
    // initialize all array elements
    size_t i;
    for (i = 0; i < size; ++i) {
      bool success = combat_robot_msgs__msg__ChassisStatus__init(&data[i]);
      if (!success) {
        break;
      }
    }
    if (i < size) {
      // if initialization failed finalize the already initialized array elements
      for (; i > 0; --i) {
        combat_robot_msgs__msg__ChassisStatus__fini(&data[i - 1]);
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
combat_robot_msgs__msg__ChassisStatus__Sequence__fini(combat_robot_msgs__msg__ChassisStatus__Sequence * array)
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
      combat_robot_msgs__msg__ChassisStatus__fini(&array->data[i]);
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

combat_robot_msgs__msg__ChassisStatus__Sequence *
combat_robot_msgs__msg__ChassisStatus__Sequence__create(size_t size)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  combat_robot_msgs__msg__ChassisStatus__Sequence * array = (combat_robot_msgs__msg__ChassisStatus__Sequence *)allocator.allocate(sizeof(combat_robot_msgs__msg__ChassisStatus__Sequence), allocator.state);
  if (!array) {
    return NULL;
  }
  bool success = combat_robot_msgs__msg__ChassisStatus__Sequence__init(array, size);
  if (!success) {
    allocator.deallocate(array, allocator.state);
    return NULL;
  }
  return array;
}

void
combat_robot_msgs__msg__ChassisStatus__Sequence__destroy(combat_robot_msgs__msg__ChassisStatus__Sequence * array)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  if (array) {
    combat_robot_msgs__msg__ChassisStatus__Sequence__fini(array);
  }
  allocator.deallocate(array, allocator.state);
}

bool
combat_robot_msgs__msg__ChassisStatus__Sequence__are_equal(const combat_robot_msgs__msg__ChassisStatus__Sequence * lhs, const combat_robot_msgs__msg__ChassisStatus__Sequence * rhs)
{
  if (!lhs || !rhs) {
    return false;
  }
  if (lhs->size != rhs->size) {
    return false;
  }
  for (size_t i = 0; i < lhs->size; ++i) {
    if (!combat_robot_msgs__msg__ChassisStatus__are_equal(&(lhs->data[i]), &(rhs->data[i]))) {
      return false;
    }
  }
  return true;
}

bool
combat_robot_msgs__msg__ChassisStatus__Sequence__copy(
  const combat_robot_msgs__msg__ChassisStatus__Sequence * input,
  combat_robot_msgs__msg__ChassisStatus__Sequence * output)
{
  if (!input || !output) {
    return false;
  }
  if (output->capacity < input->size) {
    const size_t allocation_size =
      input->size * sizeof(combat_robot_msgs__msg__ChassisStatus);
    rcutils_allocator_t allocator = rcutils_get_default_allocator();
    combat_robot_msgs__msg__ChassisStatus * data =
      (combat_robot_msgs__msg__ChassisStatus *)allocator.reallocate(
      output->data, allocation_size, allocator.state);
    if (!data) {
      return false;
    }
    // If reallocation succeeded, memory may or may not have been moved
    // to fulfill the allocation request, invalidating output->data.
    output->data = data;
    for (size_t i = output->capacity; i < input->size; ++i) {
      if (!combat_robot_msgs__msg__ChassisStatus__init(&output->data[i])) {
        // If initialization of any new item fails, roll back
        // all previously initialized items. Existing items
        // in output are to be left unmodified.
        for (; i-- > output->capacity; ) {
          combat_robot_msgs__msg__ChassisStatus__fini(&output->data[i]);
        }
        return false;
      }
    }
    output->capacity = input->size;
  }
  output->size = input->size;
  for (size_t i = 0; i < input->size; ++i) {
    if (!combat_robot_msgs__msg__ChassisStatus__copy(
        &(input->data[i]), &(output->data[i])))
    {
      return false;
    }
  }
  return true;
}
