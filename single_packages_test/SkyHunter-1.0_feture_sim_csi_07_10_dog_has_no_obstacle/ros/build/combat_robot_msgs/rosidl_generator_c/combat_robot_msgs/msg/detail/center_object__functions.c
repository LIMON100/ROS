// generated from rosidl_generator_c/resource/idl__functions.c.em
// with input from combat_robot_msgs:msg/CenterObject.idl
// generated code does not contain a copyright notice
#include "combat_robot_msgs/msg/detail/center_object__functions.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "rcutils/allocator.h"


// Include directives for member types
// Member `header`
#include "std_msgs/msg/detail/header__functions.h"
// Member `bounding_box`
#include "combat_robot_msgs/msg/detail/bounding_box2d__functions.h"

bool
combat_robot_msgs__msg__CenterObject__init(combat_robot_msgs__msg__CenterObject * msg)
{
  if (!msg) {
    return false;
  }
  // header
  if (!std_msgs__msg__Header__init(&msg->header)) {
    combat_robot_msgs__msg__CenterObject__fini(msg);
    return false;
  }
  // class_id
  // bounding_box
  if (!combat_robot_msgs__msg__BoundingBox2d__init(&msg->bounding_box)) {
    combat_robot_msgs__msg__CenterObject__fini(msg);
    return false;
  }
  // target_x
  // target_y
  // laser_distance
  // zoom_level
  return true;
}

void
combat_robot_msgs__msg__CenterObject__fini(combat_robot_msgs__msg__CenterObject * msg)
{
  if (!msg) {
    return;
  }
  // header
  std_msgs__msg__Header__fini(&msg->header);
  // class_id
  // bounding_box
  combat_robot_msgs__msg__BoundingBox2d__fini(&msg->bounding_box);
  // target_x
  // target_y
  // laser_distance
  // zoom_level
}

bool
combat_robot_msgs__msg__CenterObject__are_equal(const combat_robot_msgs__msg__CenterObject * lhs, const combat_robot_msgs__msg__CenterObject * rhs)
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
  // class_id
  if (lhs->class_id != rhs->class_id) {
    return false;
  }
  // bounding_box
  if (!combat_robot_msgs__msg__BoundingBox2d__are_equal(
      &(lhs->bounding_box), &(rhs->bounding_box)))
  {
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
  // laser_distance
  if (lhs->laser_distance != rhs->laser_distance) {
    return false;
  }
  // zoom_level
  if (lhs->zoom_level != rhs->zoom_level) {
    return false;
  }
  return true;
}

bool
combat_robot_msgs__msg__CenterObject__copy(
  const combat_robot_msgs__msg__CenterObject * input,
  combat_robot_msgs__msg__CenterObject * output)
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
  // class_id
  output->class_id = input->class_id;
  // bounding_box
  if (!combat_robot_msgs__msg__BoundingBox2d__copy(
      &(input->bounding_box), &(output->bounding_box)))
  {
    return false;
  }
  // target_x
  output->target_x = input->target_x;
  // target_y
  output->target_y = input->target_y;
  // laser_distance
  output->laser_distance = input->laser_distance;
  // zoom_level
  output->zoom_level = input->zoom_level;
  return true;
}

combat_robot_msgs__msg__CenterObject *
combat_robot_msgs__msg__CenterObject__create(void)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  combat_robot_msgs__msg__CenterObject * msg = (combat_robot_msgs__msg__CenterObject *)allocator.allocate(sizeof(combat_robot_msgs__msg__CenterObject), allocator.state);
  if (!msg) {
    return NULL;
  }
  memset(msg, 0, sizeof(combat_robot_msgs__msg__CenterObject));
  bool success = combat_robot_msgs__msg__CenterObject__init(msg);
  if (!success) {
    allocator.deallocate(msg, allocator.state);
    return NULL;
  }
  return msg;
}

void
combat_robot_msgs__msg__CenterObject__destroy(combat_robot_msgs__msg__CenterObject * msg)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  if (msg) {
    combat_robot_msgs__msg__CenterObject__fini(msg);
  }
  allocator.deallocate(msg, allocator.state);
}


bool
combat_robot_msgs__msg__CenterObject__Sequence__init(combat_robot_msgs__msg__CenterObject__Sequence * array, size_t size)
{
  if (!array) {
    return false;
  }
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  combat_robot_msgs__msg__CenterObject * data = NULL;

  if (size) {
    data = (combat_robot_msgs__msg__CenterObject *)allocator.zero_allocate(size, sizeof(combat_robot_msgs__msg__CenterObject), allocator.state);
    if (!data) {
      return false;
    }
    // initialize all array elements
    size_t i;
    for (i = 0; i < size; ++i) {
      bool success = combat_robot_msgs__msg__CenterObject__init(&data[i]);
      if (!success) {
        break;
      }
    }
    if (i < size) {
      // if initialization failed finalize the already initialized array elements
      for (; i > 0; --i) {
        combat_robot_msgs__msg__CenterObject__fini(&data[i - 1]);
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
combat_robot_msgs__msg__CenterObject__Sequence__fini(combat_robot_msgs__msg__CenterObject__Sequence * array)
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
      combat_robot_msgs__msg__CenterObject__fini(&array->data[i]);
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

combat_robot_msgs__msg__CenterObject__Sequence *
combat_robot_msgs__msg__CenterObject__Sequence__create(size_t size)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  combat_robot_msgs__msg__CenterObject__Sequence * array = (combat_robot_msgs__msg__CenterObject__Sequence *)allocator.allocate(sizeof(combat_robot_msgs__msg__CenterObject__Sequence), allocator.state);
  if (!array) {
    return NULL;
  }
  bool success = combat_robot_msgs__msg__CenterObject__Sequence__init(array, size);
  if (!success) {
    allocator.deallocate(array, allocator.state);
    return NULL;
  }
  return array;
}

void
combat_robot_msgs__msg__CenterObject__Sequence__destroy(combat_robot_msgs__msg__CenterObject__Sequence * array)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  if (array) {
    combat_robot_msgs__msg__CenterObject__Sequence__fini(array);
  }
  allocator.deallocate(array, allocator.state);
}

bool
combat_robot_msgs__msg__CenterObject__Sequence__are_equal(const combat_robot_msgs__msg__CenterObject__Sequence * lhs, const combat_robot_msgs__msg__CenterObject__Sequence * rhs)
{
  if (!lhs || !rhs) {
    return false;
  }
  if (lhs->size != rhs->size) {
    return false;
  }
  for (size_t i = 0; i < lhs->size; ++i) {
    if (!combat_robot_msgs__msg__CenterObject__are_equal(&(lhs->data[i]), &(rhs->data[i]))) {
      return false;
    }
  }
  return true;
}

bool
combat_robot_msgs__msg__CenterObject__Sequence__copy(
  const combat_robot_msgs__msg__CenterObject__Sequence * input,
  combat_robot_msgs__msg__CenterObject__Sequence * output)
{
  if (!input || !output) {
    return false;
  }
  if (output->capacity < input->size) {
    const size_t allocation_size =
      input->size * sizeof(combat_robot_msgs__msg__CenterObject);
    rcutils_allocator_t allocator = rcutils_get_default_allocator();
    combat_robot_msgs__msg__CenterObject * data =
      (combat_robot_msgs__msg__CenterObject *)allocator.reallocate(
      output->data, allocation_size, allocator.state);
    if (!data) {
      return false;
    }
    // If reallocation succeeded, memory may or may not have been moved
    // to fulfill the allocation request, invalidating output->data.
    output->data = data;
    for (size_t i = output->capacity; i < input->size; ++i) {
      if (!combat_robot_msgs__msg__CenterObject__init(&output->data[i])) {
        // If initialization of any new item fails, roll back
        // all previously initialized items. Existing items
        // in output are to be left unmodified.
        for (; i-- > output->capacity; ) {
          combat_robot_msgs__msg__CenterObject__fini(&output->data[i]);
        }
        return false;
      }
    }
    output->capacity = input->size;
  }
  output->size = input->size;
  for (size_t i = 0; i < input->size; ++i) {
    if (!combat_robot_msgs__msg__CenterObject__copy(
        &(input->data[i]), &(output->data[i])))
    {
      return false;
    }
  }
  return true;
}
