// generated from rosidl_generator_c/resource/idl__functions.h.em
// with input from skyhunter_msgs:msg/SwarmHeartbeat.idl
// generated code does not contain a copyright notice

#ifndef SKYHUNTER_MSGS__MSG__DETAIL__SWARM_HEARTBEAT__FUNCTIONS_H_
#define SKYHUNTER_MSGS__MSG__DETAIL__SWARM_HEARTBEAT__FUNCTIONS_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stdlib.h>

#include "rosidl_runtime_c/visibility_control.h"
#include "skyhunter_msgs/msg/rosidl_generator_c__visibility_control.h"

#include "skyhunter_msgs/msg/detail/swarm_heartbeat__struct.h"

/// Initialize msg/SwarmHeartbeat message.
/**
 * If the init function is called twice for the same message without
 * calling fini inbetween previously allocated memory will be leaked.
 * \param[in,out] msg The previously allocated message pointer.
 * Fields without a default value will not be initialized by this function.
 * You might want to call memset(msg, 0, sizeof(
 * skyhunter_msgs__msg__SwarmHeartbeat
 * )) before or use
 * skyhunter_msgs__msg__SwarmHeartbeat__create()
 * to allocate and initialize the message.
 * \return true if initialization was successful, otherwise false
 */
ROSIDL_GENERATOR_C_PUBLIC_skyhunter_msgs
bool
skyhunter_msgs__msg__SwarmHeartbeat__init(skyhunter_msgs__msg__SwarmHeartbeat * msg);

/// Finalize msg/SwarmHeartbeat message.
/**
 * \param[in,out] msg The allocated message pointer.
 */
ROSIDL_GENERATOR_C_PUBLIC_skyhunter_msgs
void
skyhunter_msgs__msg__SwarmHeartbeat__fini(skyhunter_msgs__msg__SwarmHeartbeat * msg);

/// Create msg/SwarmHeartbeat message.
/**
 * It allocates the memory for the message, sets the memory to zero, and
 * calls
 * skyhunter_msgs__msg__SwarmHeartbeat__init().
 * \return The pointer to the initialized message if successful,
 * otherwise NULL
 */
ROSIDL_GENERATOR_C_PUBLIC_skyhunter_msgs
skyhunter_msgs__msg__SwarmHeartbeat *
skyhunter_msgs__msg__SwarmHeartbeat__create();

/// Destroy msg/SwarmHeartbeat message.
/**
 * It calls
 * skyhunter_msgs__msg__SwarmHeartbeat__fini()
 * and frees the memory of the message.
 * \param[in,out] msg The allocated message pointer.
 */
ROSIDL_GENERATOR_C_PUBLIC_skyhunter_msgs
void
skyhunter_msgs__msg__SwarmHeartbeat__destroy(skyhunter_msgs__msg__SwarmHeartbeat * msg);

/// Check for msg/SwarmHeartbeat message equality.
/**
 * \param[in] lhs The message on the left hand size of the equality operator.
 * \param[in] rhs The message on the right hand size of the equality operator.
 * \return true if messages are equal, otherwise false.
 */
ROSIDL_GENERATOR_C_PUBLIC_skyhunter_msgs
bool
skyhunter_msgs__msg__SwarmHeartbeat__are_equal(const skyhunter_msgs__msg__SwarmHeartbeat * lhs, const skyhunter_msgs__msg__SwarmHeartbeat * rhs);

/// Copy a msg/SwarmHeartbeat message.
/**
 * This functions performs a deep copy, as opposed to the shallow copy that
 * plain assignment yields.
 *
 * \param[in] input The source message pointer.
 * \param[out] output The target message pointer, which must
 *   have been initialized before calling this function.
 * \return true if successful, or false if either pointer is null
 *   or memory allocation fails.
 */
ROSIDL_GENERATOR_C_PUBLIC_skyhunter_msgs
bool
skyhunter_msgs__msg__SwarmHeartbeat__copy(
  const skyhunter_msgs__msg__SwarmHeartbeat * input,
  skyhunter_msgs__msg__SwarmHeartbeat * output);

/// Initialize array of msg/SwarmHeartbeat messages.
/**
 * It allocates the memory for the number of elements and calls
 * skyhunter_msgs__msg__SwarmHeartbeat__init()
 * for each element of the array.
 * \param[in,out] array The allocated array pointer.
 * \param[in] size The size / capacity of the array.
 * \return true if initialization was successful, otherwise false
 * If the array pointer is valid and the size is zero it is guaranteed
 # to return true.
 */
ROSIDL_GENERATOR_C_PUBLIC_skyhunter_msgs
bool
skyhunter_msgs__msg__SwarmHeartbeat__Sequence__init(skyhunter_msgs__msg__SwarmHeartbeat__Sequence * array, size_t size);

/// Finalize array of msg/SwarmHeartbeat messages.
/**
 * It calls
 * skyhunter_msgs__msg__SwarmHeartbeat__fini()
 * for each element of the array and frees the memory for the number of
 * elements.
 * \param[in,out] array The initialized array pointer.
 */
ROSIDL_GENERATOR_C_PUBLIC_skyhunter_msgs
void
skyhunter_msgs__msg__SwarmHeartbeat__Sequence__fini(skyhunter_msgs__msg__SwarmHeartbeat__Sequence * array);

/// Create array of msg/SwarmHeartbeat messages.
/**
 * It allocates the memory for the array and calls
 * skyhunter_msgs__msg__SwarmHeartbeat__Sequence__init().
 * \param[in] size The size / capacity of the array.
 * \return The pointer to the initialized array if successful, otherwise NULL
 */
ROSIDL_GENERATOR_C_PUBLIC_skyhunter_msgs
skyhunter_msgs__msg__SwarmHeartbeat__Sequence *
skyhunter_msgs__msg__SwarmHeartbeat__Sequence__create(size_t size);

/// Destroy array of msg/SwarmHeartbeat messages.
/**
 * It calls
 * skyhunter_msgs__msg__SwarmHeartbeat__Sequence__fini()
 * on the array,
 * and frees the memory of the array.
 * \param[in,out] array The initialized array pointer.
 */
ROSIDL_GENERATOR_C_PUBLIC_skyhunter_msgs
void
skyhunter_msgs__msg__SwarmHeartbeat__Sequence__destroy(skyhunter_msgs__msg__SwarmHeartbeat__Sequence * array);

/// Check for msg/SwarmHeartbeat message array equality.
/**
 * \param[in] lhs The message array on the left hand size of the equality operator.
 * \param[in] rhs The message array on the right hand size of the equality operator.
 * \return true if message arrays are equal in size and content, otherwise false.
 */
ROSIDL_GENERATOR_C_PUBLIC_skyhunter_msgs
bool
skyhunter_msgs__msg__SwarmHeartbeat__Sequence__are_equal(const skyhunter_msgs__msg__SwarmHeartbeat__Sequence * lhs, const skyhunter_msgs__msg__SwarmHeartbeat__Sequence * rhs);

/// Copy an array of msg/SwarmHeartbeat messages.
/**
 * This functions performs a deep copy, as opposed to the shallow copy that
 * plain assignment yields.
 *
 * \param[in] input The source array pointer.
 * \param[out] output The target array pointer, which must
 *   have been initialized before calling this function.
 * \return true if successful, or false if either pointer
 *   is null or memory allocation fails.
 */
ROSIDL_GENERATOR_C_PUBLIC_skyhunter_msgs
bool
skyhunter_msgs__msg__SwarmHeartbeat__Sequence__copy(
  const skyhunter_msgs__msg__SwarmHeartbeat__Sequence * input,
  skyhunter_msgs__msg__SwarmHeartbeat__Sequence * output);

#ifdef __cplusplus
}
#endif

#endif  // SKYHUNTER_MSGS__MSG__DETAIL__SWARM_HEARTBEAT__FUNCTIONS_H_
