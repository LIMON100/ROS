// generated from rosidl_generator_c/resource/idl__struct.h.em
// with input from skyhunter_msgs:msg/ElectionVote.idl
// generated code does not contain a copyright notice

#ifndef SKYHUNTER_MSGS__MSG__DETAIL__ELECTION_VOTE__STRUCT_H_
#define SKYHUNTER_MSGS__MSG__DETAIL__ELECTION_VOTE__STRUCT_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


// Constants defined in the message

// Include directives for member types
// Member 'candidate_id'
// Member 'voter_id'
#include "rosidl_runtime_c/string.h"

/// Struct defined in msg/ElectionVote in the package skyhunter_msgs.
typedef struct skyhunter_msgs__msg__ElectionVote
{
  uint32_t term;
  rosidl_runtime_c__String candidate_id;
  rosidl_runtime_c__String voter_id;
  float fitness_score;
} skyhunter_msgs__msg__ElectionVote;

// Struct for a sequence of skyhunter_msgs__msg__ElectionVote.
typedef struct skyhunter_msgs__msg__ElectionVote__Sequence
{
  skyhunter_msgs__msg__ElectionVote * data;
  /// The number of valid items in data
  size_t size;
  /// The number of allocated items in data
  size_t capacity;
} skyhunter_msgs__msg__ElectionVote__Sequence;

#ifdef __cplusplus
}
#endif

#endif  // SKYHUNTER_MSGS__MSG__DETAIL__ELECTION_VOTE__STRUCT_H_
