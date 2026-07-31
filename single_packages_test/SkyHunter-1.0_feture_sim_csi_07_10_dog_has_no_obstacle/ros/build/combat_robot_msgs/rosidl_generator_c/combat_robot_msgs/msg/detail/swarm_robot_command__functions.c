// generated from rosidl_generator_c/resource/idl__functions.c.em
// with input from combat_robot_msgs:msg/SwarmRobotCommand.idl
// generated code does not contain a copyright notice
#include "combat_robot_msgs/msg/detail/swarm_robot_command__functions.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "rcutils/allocator.h"


// Include directives for member types
// Member `header`
#include "std_msgs/msg/detail/header__functions.h"
// Member `path_id`
// Member `path_json`
#include "rosidl_runtime_c/string_functions.h"

bool
combat_robot_msgs__msg__SwarmRobotCommand__init(combat_robot_msgs__msg__SwarmRobotCommand * msg)
{
  if (!msg) {
    return false;
  }
  // header
  if (!std_msgs__msg__Header__init(&msg->header)) {
    combat_robot_msgs__msg__SwarmRobotCommand__fini(msg);
    return false;
  }
  // sequence
  // command_type
  // leader_robot_id
  // target_robot_id
  // operation_mode
  // estop_requested
  // path_command
  // num_waypoints
  // path_id
  if (!rosidl_runtime_c__String__init(&msg->path_id)) {
    combat_robot_msgs__msg__SwarmRobotCommand__fini(msg);
    return false;
  }
  // path_json
  if (!rosidl_runtime_c__String__init(&msg->path_json)) {
    combat_robot_msgs__msg__SwarmRobotCommand__fini(msg);
    return false;
  }
  // formation_type
  // formation_number
  // grouping_index
  // slot_index
  // selected_robot_count
  // selected_robot_ids
  return true;
}

void
combat_robot_msgs__msg__SwarmRobotCommand__fini(combat_robot_msgs__msg__SwarmRobotCommand * msg)
{
  if (!msg) {
    return;
  }
  // header
  std_msgs__msg__Header__fini(&msg->header);
  // sequence
  // command_type
  // leader_robot_id
  // target_robot_id
  // operation_mode
  // estop_requested
  // path_command
  // num_waypoints
  // path_id
  rosidl_runtime_c__String__fini(&msg->path_id);
  // path_json
  rosidl_runtime_c__String__fini(&msg->path_json);
  // formation_type
  // formation_number
  // grouping_index
  // slot_index
  // selected_robot_count
  // selected_robot_ids
}

bool
combat_robot_msgs__msg__SwarmRobotCommand__are_equal(const combat_robot_msgs__msg__SwarmRobotCommand * lhs, const combat_robot_msgs__msg__SwarmRobotCommand * rhs)
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
  // sequence
  if (lhs->sequence != rhs->sequence) {
    return false;
  }
  // command_type
  if (lhs->command_type != rhs->command_type) {
    return false;
  }
  // leader_robot_id
  if (lhs->leader_robot_id != rhs->leader_robot_id) {
    return false;
  }
  // target_robot_id
  if (lhs->target_robot_id != rhs->target_robot_id) {
    return false;
  }
  // operation_mode
  if (lhs->operation_mode != rhs->operation_mode) {
    return false;
  }
  // estop_requested
  if (lhs->estop_requested != rhs->estop_requested) {
    return false;
  }
  // path_command
  if (lhs->path_command != rhs->path_command) {
    return false;
  }
  // num_waypoints
  if (lhs->num_waypoints != rhs->num_waypoints) {
    return false;
  }
  // path_id
  if (!rosidl_runtime_c__String__are_equal(
      &(lhs->path_id), &(rhs->path_id)))
  {
    return false;
  }
  // path_json
  if (!rosidl_runtime_c__String__are_equal(
      &(lhs->path_json), &(rhs->path_json)))
  {
    return false;
  }
  // formation_type
  if (lhs->formation_type != rhs->formation_type) {
    return false;
  }
  // formation_number
  if (lhs->formation_number != rhs->formation_number) {
    return false;
  }
  // grouping_index
  if (lhs->grouping_index != rhs->grouping_index) {
    return false;
  }
  // slot_index
  if (lhs->slot_index != rhs->slot_index) {
    return false;
  }
  // selected_robot_count
  if (lhs->selected_robot_count != rhs->selected_robot_count) {
    return false;
  }
  // selected_robot_ids
  for (size_t i = 0; i < 8; ++i) {
    if (lhs->selected_robot_ids[i] != rhs->selected_robot_ids[i]) {
      return false;
    }
  }
  return true;
}

bool
combat_robot_msgs__msg__SwarmRobotCommand__copy(
  const combat_robot_msgs__msg__SwarmRobotCommand * input,
  combat_robot_msgs__msg__SwarmRobotCommand * output)
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
  // sequence
  output->sequence = input->sequence;
  // command_type
  output->command_type = input->command_type;
  // leader_robot_id
  output->leader_robot_id = input->leader_robot_id;
  // target_robot_id
  output->target_robot_id = input->target_robot_id;
  // operation_mode
  output->operation_mode = input->operation_mode;
  // estop_requested
  output->estop_requested = input->estop_requested;
  // path_command
  output->path_command = input->path_command;
  // num_waypoints
  output->num_waypoints = input->num_waypoints;
  // path_id
  if (!rosidl_runtime_c__String__copy(
      &(input->path_id), &(output->path_id)))
  {
    return false;
  }
  // path_json
  if (!rosidl_runtime_c__String__copy(
      &(input->path_json), &(output->path_json)))
  {
    return false;
  }
  // formation_type
  output->formation_type = input->formation_type;
  // formation_number
  output->formation_number = input->formation_number;
  // grouping_index
  output->grouping_index = input->grouping_index;
  // slot_index
  output->slot_index = input->slot_index;
  // selected_robot_count
  output->selected_robot_count = input->selected_robot_count;
  // selected_robot_ids
  for (size_t i = 0; i < 8; ++i) {
    output->selected_robot_ids[i] = input->selected_robot_ids[i];
  }
  return true;
}

combat_robot_msgs__msg__SwarmRobotCommand *
combat_robot_msgs__msg__SwarmRobotCommand__create(void)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  combat_robot_msgs__msg__SwarmRobotCommand * msg = (combat_robot_msgs__msg__SwarmRobotCommand *)allocator.allocate(sizeof(combat_robot_msgs__msg__SwarmRobotCommand), allocator.state);
  if (!msg) {
    return NULL;
  }
  memset(msg, 0, sizeof(combat_robot_msgs__msg__SwarmRobotCommand));
  bool success = combat_robot_msgs__msg__SwarmRobotCommand__init(msg);
  if (!success) {
    allocator.deallocate(msg, allocator.state);
    return NULL;
  }
  return msg;
}

void
combat_robot_msgs__msg__SwarmRobotCommand__destroy(combat_robot_msgs__msg__SwarmRobotCommand * msg)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  if (msg) {
    combat_robot_msgs__msg__SwarmRobotCommand__fini(msg);
  }
  allocator.deallocate(msg, allocator.state);
}


bool
combat_robot_msgs__msg__SwarmRobotCommand__Sequence__init(combat_robot_msgs__msg__SwarmRobotCommand__Sequence * array, size_t size)
{
  if (!array) {
    return false;
  }
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  combat_robot_msgs__msg__SwarmRobotCommand * data = NULL;

  if (size) {
    data = (combat_robot_msgs__msg__SwarmRobotCommand *)allocator.zero_allocate(size, sizeof(combat_robot_msgs__msg__SwarmRobotCommand), allocator.state);
    if (!data) {
      return false;
    }
    // initialize all array elements
    size_t i;
    for (i = 0; i < size; ++i) {
      bool success = combat_robot_msgs__msg__SwarmRobotCommand__init(&data[i]);
      if (!success) {
        break;
      }
    }
    if (i < size) {
      // if initialization failed finalize the already initialized array elements
      for (; i > 0; --i) {
        combat_robot_msgs__msg__SwarmRobotCommand__fini(&data[i - 1]);
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
combat_robot_msgs__msg__SwarmRobotCommand__Sequence__fini(combat_robot_msgs__msg__SwarmRobotCommand__Sequence * array)
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
      combat_robot_msgs__msg__SwarmRobotCommand__fini(&array->data[i]);
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

combat_robot_msgs__msg__SwarmRobotCommand__Sequence *
combat_robot_msgs__msg__SwarmRobotCommand__Sequence__create(size_t size)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  combat_robot_msgs__msg__SwarmRobotCommand__Sequence * array = (combat_robot_msgs__msg__SwarmRobotCommand__Sequence *)allocator.allocate(sizeof(combat_robot_msgs__msg__SwarmRobotCommand__Sequence), allocator.state);
  if (!array) {
    return NULL;
  }
  bool success = combat_robot_msgs__msg__SwarmRobotCommand__Sequence__init(array, size);
  if (!success) {
    allocator.deallocate(array, allocator.state);
    return NULL;
  }
  return array;
}

void
combat_robot_msgs__msg__SwarmRobotCommand__Sequence__destroy(combat_robot_msgs__msg__SwarmRobotCommand__Sequence * array)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  if (array) {
    combat_robot_msgs__msg__SwarmRobotCommand__Sequence__fini(array);
  }
  allocator.deallocate(array, allocator.state);
}

bool
combat_robot_msgs__msg__SwarmRobotCommand__Sequence__are_equal(const combat_robot_msgs__msg__SwarmRobotCommand__Sequence * lhs, const combat_robot_msgs__msg__SwarmRobotCommand__Sequence * rhs)
{
  if (!lhs || !rhs) {
    return false;
  }
  if (lhs->size != rhs->size) {
    return false;
  }
  for (size_t i = 0; i < lhs->size; ++i) {
    if (!combat_robot_msgs__msg__SwarmRobotCommand__are_equal(&(lhs->data[i]), &(rhs->data[i]))) {
      return false;
    }
  }
  return true;
}

bool
combat_robot_msgs__msg__SwarmRobotCommand__Sequence__copy(
  const combat_robot_msgs__msg__SwarmRobotCommand__Sequence * input,
  combat_robot_msgs__msg__SwarmRobotCommand__Sequence * output)
{
  if (!input || !output) {
    return false;
  }
  if (output->capacity < input->size) {
    const size_t allocation_size =
      input->size * sizeof(combat_robot_msgs__msg__SwarmRobotCommand);
    rcutils_allocator_t allocator = rcutils_get_default_allocator();
    combat_robot_msgs__msg__SwarmRobotCommand * data =
      (combat_robot_msgs__msg__SwarmRobotCommand *)allocator.reallocate(
      output->data, allocation_size, allocator.state);
    if (!data) {
      return false;
    }
    // If reallocation succeeded, memory may or may not have been moved
    // to fulfill the allocation request, invalidating output->data.
    output->data = data;
    for (size_t i = output->capacity; i < input->size; ++i) {
      if (!combat_robot_msgs__msg__SwarmRobotCommand__init(&output->data[i])) {
        // If initialization of any new item fails, roll back
        // all previously initialized items. Existing items
        // in output are to be left unmodified.
        for (; i-- > output->capacity; ) {
          combat_robot_msgs__msg__SwarmRobotCommand__fini(&output->data[i]);
        }
        return false;
      }
    }
    output->capacity = input->size;
  }
  output->size = input->size;
  for (size_t i = 0; i < input->size; ++i) {
    if (!combat_robot_msgs__msg__SwarmRobotCommand__copy(
        &(input->data[i]), &(output->data[i])))
    {
      return false;
    }
  }
  return true;
}
