// generated from rosidl_generator_c/resource/idl__functions.c.em
// with input from skyhunter_msgs:msg/ElectionVote.idl
// generated code does not contain a copyright notice
#include "skyhunter_msgs/msg/detail/election_vote__functions.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "rcutils/allocator.h"


// Include directives for member types
// Member `candidate_id`
// Member `voter_id`
#include "rosidl_runtime_c/string_functions.h"

bool
skyhunter_msgs__msg__ElectionVote__init(skyhunter_msgs__msg__ElectionVote * msg)
{
  if (!msg) {
    return false;
  }
  // term
  // candidate_id
  if (!rosidl_runtime_c__String__init(&msg->candidate_id)) {
    skyhunter_msgs__msg__ElectionVote__fini(msg);
    return false;
  }
  // voter_id
  if (!rosidl_runtime_c__String__init(&msg->voter_id)) {
    skyhunter_msgs__msg__ElectionVote__fini(msg);
    return false;
  }
  // fitness_score
  return true;
}

void
skyhunter_msgs__msg__ElectionVote__fini(skyhunter_msgs__msg__ElectionVote * msg)
{
  if (!msg) {
    return;
  }
  // term
  // candidate_id
  rosidl_runtime_c__String__fini(&msg->candidate_id);
  // voter_id
  rosidl_runtime_c__String__fini(&msg->voter_id);
  // fitness_score
}

bool
skyhunter_msgs__msg__ElectionVote__are_equal(const skyhunter_msgs__msg__ElectionVote * lhs, const skyhunter_msgs__msg__ElectionVote * rhs)
{
  if (!lhs || !rhs) {
    return false;
  }
  // term
  if (lhs->term != rhs->term) {
    return false;
  }
  // candidate_id
  if (!rosidl_runtime_c__String__are_equal(
      &(lhs->candidate_id), &(rhs->candidate_id)))
  {
    return false;
  }
  // voter_id
  if (!rosidl_runtime_c__String__are_equal(
      &(lhs->voter_id), &(rhs->voter_id)))
  {
    return false;
  }
  // fitness_score
  if (lhs->fitness_score != rhs->fitness_score) {
    return false;
  }
  return true;
}

bool
skyhunter_msgs__msg__ElectionVote__copy(
  const skyhunter_msgs__msg__ElectionVote * input,
  skyhunter_msgs__msg__ElectionVote * output)
{
  if (!input || !output) {
    return false;
  }
  // term
  output->term = input->term;
  // candidate_id
  if (!rosidl_runtime_c__String__copy(
      &(input->candidate_id), &(output->candidate_id)))
  {
    return false;
  }
  // voter_id
  if (!rosidl_runtime_c__String__copy(
      &(input->voter_id), &(output->voter_id)))
  {
    return false;
  }
  // fitness_score
  output->fitness_score = input->fitness_score;
  return true;
}

skyhunter_msgs__msg__ElectionVote *
skyhunter_msgs__msg__ElectionVote__create()
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  skyhunter_msgs__msg__ElectionVote * msg = (skyhunter_msgs__msg__ElectionVote *)allocator.allocate(sizeof(skyhunter_msgs__msg__ElectionVote), allocator.state);
  if (!msg) {
    return NULL;
  }
  memset(msg, 0, sizeof(skyhunter_msgs__msg__ElectionVote));
  bool success = skyhunter_msgs__msg__ElectionVote__init(msg);
  if (!success) {
    allocator.deallocate(msg, allocator.state);
    return NULL;
  }
  return msg;
}

void
skyhunter_msgs__msg__ElectionVote__destroy(skyhunter_msgs__msg__ElectionVote * msg)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  if (msg) {
    skyhunter_msgs__msg__ElectionVote__fini(msg);
  }
  allocator.deallocate(msg, allocator.state);
}


bool
skyhunter_msgs__msg__ElectionVote__Sequence__init(skyhunter_msgs__msg__ElectionVote__Sequence * array, size_t size)
{
  if (!array) {
    return false;
  }
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  skyhunter_msgs__msg__ElectionVote * data = NULL;

  if (size) {
    data = (skyhunter_msgs__msg__ElectionVote *)allocator.zero_allocate(size, sizeof(skyhunter_msgs__msg__ElectionVote), allocator.state);
    if (!data) {
      return false;
    }
    // initialize all array elements
    size_t i;
    for (i = 0; i < size; ++i) {
      bool success = skyhunter_msgs__msg__ElectionVote__init(&data[i]);
      if (!success) {
        break;
      }
    }
    if (i < size) {
      // if initialization failed finalize the already initialized array elements
      for (; i > 0; --i) {
        skyhunter_msgs__msg__ElectionVote__fini(&data[i - 1]);
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
skyhunter_msgs__msg__ElectionVote__Sequence__fini(skyhunter_msgs__msg__ElectionVote__Sequence * array)
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
      skyhunter_msgs__msg__ElectionVote__fini(&array->data[i]);
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

skyhunter_msgs__msg__ElectionVote__Sequence *
skyhunter_msgs__msg__ElectionVote__Sequence__create(size_t size)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  skyhunter_msgs__msg__ElectionVote__Sequence * array = (skyhunter_msgs__msg__ElectionVote__Sequence *)allocator.allocate(sizeof(skyhunter_msgs__msg__ElectionVote__Sequence), allocator.state);
  if (!array) {
    return NULL;
  }
  bool success = skyhunter_msgs__msg__ElectionVote__Sequence__init(array, size);
  if (!success) {
    allocator.deallocate(array, allocator.state);
    return NULL;
  }
  return array;
}

void
skyhunter_msgs__msg__ElectionVote__Sequence__destroy(skyhunter_msgs__msg__ElectionVote__Sequence * array)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  if (array) {
    skyhunter_msgs__msg__ElectionVote__Sequence__fini(array);
  }
  allocator.deallocate(array, allocator.state);
}

bool
skyhunter_msgs__msg__ElectionVote__Sequence__are_equal(const skyhunter_msgs__msg__ElectionVote__Sequence * lhs, const skyhunter_msgs__msg__ElectionVote__Sequence * rhs)
{
  if (!lhs || !rhs) {
    return false;
  }
  if (lhs->size != rhs->size) {
    return false;
  }
  for (size_t i = 0; i < lhs->size; ++i) {
    if (!skyhunter_msgs__msg__ElectionVote__are_equal(&(lhs->data[i]), &(rhs->data[i]))) {
      return false;
    }
  }
  return true;
}

bool
skyhunter_msgs__msg__ElectionVote__Sequence__copy(
  const skyhunter_msgs__msg__ElectionVote__Sequence * input,
  skyhunter_msgs__msg__ElectionVote__Sequence * output)
{
  if (!input || !output) {
    return false;
  }
  if (output->capacity < input->size) {
    const size_t allocation_size =
      input->size * sizeof(skyhunter_msgs__msg__ElectionVote);
    rcutils_allocator_t allocator = rcutils_get_default_allocator();
    skyhunter_msgs__msg__ElectionVote * data =
      (skyhunter_msgs__msg__ElectionVote *)allocator.reallocate(
      output->data, allocation_size, allocator.state);
    if (!data) {
      return false;
    }
    // If reallocation succeeded, memory may or may not have been moved
    // to fulfill the allocation request, invalidating output->data.
    output->data = data;
    for (size_t i = output->capacity; i < input->size; ++i) {
      if (!skyhunter_msgs__msg__ElectionVote__init(&output->data[i])) {
        // If initialization of any new item fails, roll back
        // all previously initialized items. Existing items
        // in output are to be left unmodified.
        for (; i-- > output->capacity; ) {
          skyhunter_msgs__msg__ElectionVote__fini(&output->data[i]);
        }
        return false;
      }
    }
    output->capacity = input->size;
  }
  output->size = input->size;
  for (size_t i = 0; i < input->size; ++i) {
    if (!skyhunter_msgs__msg__ElectionVote__copy(
        &(input->data[i]), &(output->data[i])))
    {
      return false;
    }
  }
  return true;
}
