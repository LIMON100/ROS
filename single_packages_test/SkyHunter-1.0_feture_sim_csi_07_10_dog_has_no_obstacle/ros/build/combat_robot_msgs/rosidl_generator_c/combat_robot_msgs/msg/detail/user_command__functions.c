// generated from rosidl_generator_c/resource/idl__functions.c.em
// with input from combat_robot_msgs:msg/UserCommand.idl
// generated code does not contain a copyright notice
#include "combat_robot_msgs/msg/detail/user_command__functions.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "rcutils/allocator.h"


// Include directives for member types
// Member `header`
#include "std_msgs/msg/detail/header__functions.h"

bool
combat_robot_msgs__msg__UserCommand__init(combat_robot_msgs__msg__UserCommand * msg)
{
  if (!msg) {
    return false;
  }
  // header
  if (!std_msgs__msg__Header__init(&msg->header)) {
    combat_robot_msgs__msg__UserCommand__fini(msg);
    return false;
  }
  // command_from
  // command_id
  // target_x
  // target_y
  // drone_target_lat
  // drone_target_lon
  // drone_target_valid
  // gun_trigger
  // gun_trigger_permission
  // pan_speed
  // tilt_speed
  // zoom_command
  // stream_command
  return true;
}

void
combat_robot_msgs__msg__UserCommand__fini(combat_robot_msgs__msg__UserCommand * msg)
{
  if (!msg) {
    return;
  }
  // header
  std_msgs__msg__Header__fini(&msg->header);
  // command_from
  // command_id
  // target_x
  // target_y
  // drone_target_lat
  // drone_target_lon
  // drone_target_valid
  // gun_trigger
  // gun_trigger_permission
  // pan_speed
  // tilt_speed
  // zoom_command
  // stream_command
}

bool
combat_robot_msgs__msg__UserCommand__are_equal(const combat_robot_msgs__msg__UserCommand * lhs, const combat_robot_msgs__msg__UserCommand * rhs)
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
  // command_from
  if (lhs->command_from != rhs->command_from) {
    return false;
  }
  // command_id
  if (lhs->command_id != rhs->command_id) {
    return false;
  }
  // target_x
  if (lhs->target_x != rhs->target_x) {
    return false;
  }
  // target_y
  if (lhs->target_y != rhs->target_y) {
    return false;
  }
  // drone_target_lat
  if (lhs->drone_target_lat != rhs->drone_target_lat) {
    return false;
  }
  // drone_target_lon
  if (lhs->drone_target_lon != rhs->drone_target_lon) {
    return false;
  }
  // drone_target_valid
  if (lhs->drone_target_valid != rhs->drone_target_valid) {
    return false;
  }
  // gun_trigger
  if (lhs->gun_trigger != rhs->gun_trigger) {
    return false;
  }
  // gun_trigger_permission
  if (lhs->gun_trigger_permission != rhs->gun_trigger_permission) {
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
  // zoom_command
  if (lhs->zoom_command != rhs->zoom_command) {
    return false;
  }
  // stream_command
  if (lhs->stream_command != rhs->stream_command) {
    return false;
  }
  return true;
}

bool
combat_robot_msgs__msg__UserCommand__copy(
  const combat_robot_msgs__msg__UserCommand * input,
  combat_robot_msgs__msg__UserCommand * output)
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
  // command_from
  output->command_from = input->command_from;
  // command_id
  output->command_id = input->command_id;
  // target_x
  output->target_x = input->target_x;
  // target_y
  output->target_y = input->target_y;
  // drone_target_lat
  output->drone_target_lat = input->drone_target_lat;
  // drone_target_lon
  output->drone_target_lon = input->drone_target_lon;
  // drone_target_valid
  output->drone_target_valid = input->drone_target_valid;
  // gun_trigger
  output->gun_trigger = input->gun_trigger;
  // gun_trigger_permission
  output->gun_trigger_permission = input->gun_trigger_permission;
  // pan_speed
  output->pan_speed = input->pan_speed;
  // tilt_speed
  output->tilt_speed = input->tilt_speed;
  // zoom_command
  output->zoom_command = input->zoom_command;
  // stream_command
  output->stream_command = input->stream_command;
  return true;
}

combat_robot_msgs__msg__UserCommand *
combat_robot_msgs__msg__UserCommand__create(void)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  combat_robot_msgs__msg__UserCommand * msg = (combat_robot_msgs__msg__UserCommand *)allocator.allocate(sizeof(combat_robot_msgs__msg__UserCommand), allocator.state);
  if (!msg) {
    return NULL;
  }
  memset(msg, 0, sizeof(combat_robot_msgs__msg__UserCommand));
  bool success = combat_robot_msgs__msg__UserCommand__init(msg);
  if (!success) {
    allocator.deallocate(msg, allocator.state);
    return NULL;
  }
  return msg;
}

void
combat_robot_msgs__msg__UserCommand__destroy(combat_robot_msgs__msg__UserCommand * msg)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  if (msg) {
    combat_robot_msgs__msg__UserCommand__fini(msg);
  }
  allocator.deallocate(msg, allocator.state);
}


bool
combat_robot_msgs__msg__UserCommand__Sequence__init(combat_robot_msgs__msg__UserCommand__Sequence * array, size_t size)
{
  if (!array) {
    return false;
  }
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  combat_robot_msgs__msg__UserCommand * data = NULL;

  if (size) {
    data = (combat_robot_msgs__msg__UserCommand *)allocator.zero_allocate(size, sizeof(combat_robot_msgs__msg__UserCommand), allocator.state);
    if (!data) {
      return false;
    }
    // initialize all array elements
    size_t i;
    for (i = 0; i < size; ++i) {
      bool success = combat_robot_msgs__msg__UserCommand__init(&data[i]);
      if (!success) {
        break;
      }
    }
    if (i < size) {
      // if initialization failed finalize the already initialized array elements
      for (; i > 0; --i) {
        combat_robot_msgs__msg__UserCommand__fini(&data[i - 1]);
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
combat_robot_msgs__msg__UserCommand__Sequence__fini(combat_robot_msgs__msg__UserCommand__Sequence * array)
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
      combat_robot_msgs__msg__UserCommand__fini(&array->data[i]);
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

combat_robot_msgs__msg__UserCommand__Sequence *
combat_robot_msgs__msg__UserCommand__Sequence__create(size_t size)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  combat_robot_msgs__msg__UserCommand__Sequence * array = (combat_robot_msgs__msg__UserCommand__Sequence *)allocator.allocate(sizeof(combat_robot_msgs__msg__UserCommand__Sequence), allocator.state);
  if (!array) {
    return NULL;
  }
  bool success = combat_robot_msgs__msg__UserCommand__Sequence__init(array, size);
  if (!success) {
    allocator.deallocate(array, allocator.state);
    return NULL;
  }
  return array;
}

void
combat_robot_msgs__msg__UserCommand__Sequence__destroy(combat_robot_msgs__msg__UserCommand__Sequence * array)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  if (array) {
    combat_robot_msgs__msg__UserCommand__Sequence__fini(array);
  }
  allocator.deallocate(array, allocator.state);
}

bool
combat_robot_msgs__msg__UserCommand__Sequence__are_equal(const combat_robot_msgs__msg__UserCommand__Sequence * lhs, const combat_robot_msgs__msg__UserCommand__Sequence * rhs)
{
  if (!lhs || !rhs) {
    return false;
  }
  if (lhs->size != rhs->size) {
    return false;
  }
  for (size_t i = 0; i < lhs->size; ++i) {
    if (!combat_robot_msgs__msg__UserCommand__are_equal(&(lhs->data[i]), &(rhs->data[i]))) {
      return false;
    }
  }
  return true;
}

bool
combat_robot_msgs__msg__UserCommand__Sequence__copy(
  const combat_robot_msgs__msg__UserCommand__Sequence * input,
  combat_robot_msgs__msg__UserCommand__Sequence * output)
{
  if (!input || !output) {
    return false;
  }
  if (output->capacity < input->size) {
    const size_t allocation_size =
      input->size * sizeof(combat_robot_msgs__msg__UserCommand);
    rcutils_allocator_t allocator = rcutils_get_default_allocator();
    combat_robot_msgs__msg__UserCommand * data =
      (combat_robot_msgs__msg__UserCommand *)allocator.reallocate(
      output->data, allocation_size, allocator.state);
    if (!data) {
      return false;
    }
    // If reallocation succeeded, memory may or may not have been moved
    // to fulfill the allocation request, invalidating output->data.
    output->data = data;
    for (size_t i = output->capacity; i < input->size; ++i) {
      if (!combat_robot_msgs__msg__UserCommand__init(&output->data[i])) {
        // If initialization of any new item fails, roll back
        // all previously initialized items. Existing items
        // in output are to be left unmodified.
        for (; i-- > output->capacity; ) {
          combat_robot_msgs__msg__UserCommand__fini(&output->data[i]);
        }
        return false;
      }
    }
    output->capacity = input->size;
  }
  output->size = input->size;
  for (size_t i = 0; i < input->size; ++i) {
    if (!combat_robot_msgs__msg__UserCommand__copy(
        &(input->data[i]), &(output->data[i])))
    {
      return false;
    }
  }
  return true;
}
