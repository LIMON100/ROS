// generated from rosidl_generator_cpp/resource/idl__struct.hpp.em
// with input from skyhunter_msgs:msg/ElectionVote.idl
// generated code does not contain a copyright notice

#ifndef SKYHUNTER_MSGS__MSG__DETAIL__ELECTION_VOTE__STRUCT_HPP_
#define SKYHUNTER_MSGS__MSG__DETAIL__ELECTION_VOTE__STRUCT_HPP_

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "rosidl_runtime_cpp/bounded_vector.hpp"
#include "rosidl_runtime_cpp/message_initialization.hpp"


#ifndef _WIN32
# define DEPRECATED__skyhunter_msgs__msg__ElectionVote __attribute__((deprecated))
#else
# define DEPRECATED__skyhunter_msgs__msg__ElectionVote __declspec(deprecated)
#endif

namespace skyhunter_msgs
{

namespace msg
{

// message struct
template<class ContainerAllocator>
struct ElectionVote_
{
  using Type = ElectionVote_<ContainerAllocator>;

  explicit ElectionVote_(rosidl_runtime_cpp::MessageInitialization _init = rosidl_runtime_cpp::MessageInitialization::ALL)
  {
    if (rosidl_runtime_cpp::MessageInitialization::ALL == _init ||
      rosidl_runtime_cpp::MessageInitialization::ZERO == _init)
    {
      this->term = 0ul;
      this->candidate_id = "";
      this->voter_id = "";
      this->fitness_score = 0.0f;
    }
  }

  explicit ElectionVote_(const ContainerAllocator & _alloc, rosidl_runtime_cpp::MessageInitialization _init = rosidl_runtime_cpp::MessageInitialization::ALL)
  : candidate_id(_alloc),
    voter_id(_alloc)
  {
    if (rosidl_runtime_cpp::MessageInitialization::ALL == _init ||
      rosidl_runtime_cpp::MessageInitialization::ZERO == _init)
    {
      this->term = 0ul;
      this->candidate_id = "";
      this->voter_id = "";
      this->fitness_score = 0.0f;
    }
  }

  // field types and members
  using _term_type =
    uint32_t;
  _term_type term;
  using _candidate_id_type =
    std::basic_string<char, std::char_traits<char>, typename std::allocator_traits<ContainerAllocator>::template rebind_alloc<char>>;
  _candidate_id_type candidate_id;
  using _voter_id_type =
    std::basic_string<char, std::char_traits<char>, typename std::allocator_traits<ContainerAllocator>::template rebind_alloc<char>>;
  _voter_id_type voter_id;
  using _fitness_score_type =
    float;
  _fitness_score_type fitness_score;

  // setters for named parameter idiom
  Type & set__term(
    const uint32_t & _arg)
  {
    this->term = _arg;
    return *this;
  }
  Type & set__candidate_id(
    const std::basic_string<char, std::char_traits<char>, typename std::allocator_traits<ContainerAllocator>::template rebind_alloc<char>> & _arg)
  {
    this->candidate_id = _arg;
    return *this;
  }
  Type & set__voter_id(
    const std::basic_string<char, std::char_traits<char>, typename std::allocator_traits<ContainerAllocator>::template rebind_alloc<char>> & _arg)
  {
    this->voter_id = _arg;
    return *this;
  }
  Type & set__fitness_score(
    const float & _arg)
  {
    this->fitness_score = _arg;
    return *this;
  }

  // constant declarations

  // pointer types
  using RawPtr =
    skyhunter_msgs::msg::ElectionVote_<ContainerAllocator> *;
  using ConstRawPtr =
    const skyhunter_msgs::msg::ElectionVote_<ContainerAllocator> *;
  using SharedPtr =
    std::shared_ptr<skyhunter_msgs::msg::ElectionVote_<ContainerAllocator>>;
  using ConstSharedPtr =
    std::shared_ptr<skyhunter_msgs::msg::ElectionVote_<ContainerAllocator> const>;

  template<typename Deleter = std::default_delete<
      skyhunter_msgs::msg::ElectionVote_<ContainerAllocator>>>
  using UniquePtrWithDeleter =
    std::unique_ptr<skyhunter_msgs::msg::ElectionVote_<ContainerAllocator>, Deleter>;

  using UniquePtr = UniquePtrWithDeleter<>;

  template<typename Deleter = std::default_delete<
      skyhunter_msgs::msg::ElectionVote_<ContainerAllocator>>>
  using ConstUniquePtrWithDeleter =
    std::unique_ptr<skyhunter_msgs::msg::ElectionVote_<ContainerAllocator> const, Deleter>;
  using ConstUniquePtr = ConstUniquePtrWithDeleter<>;

  using WeakPtr =
    std::weak_ptr<skyhunter_msgs::msg::ElectionVote_<ContainerAllocator>>;
  using ConstWeakPtr =
    std::weak_ptr<skyhunter_msgs::msg::ElectionVote_<ContainerAllocator> const>;

  // pointer types similar to ROS 1, use SharedPtr / ConstSharedPtr instead
  // NOTE: Can't use 'using' here because GNU C++ can't parse attributes properly
  typedef DEPRECATED__skyhunter_msgs__msg__ElectionVote
    std::shared_ptr<skyhunter_msgs::msg::ElectionVote_<ContainerAllocator>>
    Ptr;
  typedef DEPRECATED__skyhunter_msgs__msg__ElectionVote
    std::shared_ptr<skyhunter_msgs::msg::ElectionVote_<ContainerAllocator> const>
    ConstPtr;

  // comparison operators
  bool operator==(const ElectionVote_ & other) const
  {
    if (this->term != other.term) {
      return false;
    }
    if (this->candidate_id != other.candidate_id) {
      return false;
    }
    if (this->voter_id != other.voter_id) {
      return false;
    }
    if (this->fitness_score != other.fitness_score) {
      return false;
    }
    return true;
  }
  bool operator!=(const ElectionVote_ & other) const
  {
    return !this->operator==(other);
  }
};  // struct ElectionVote_

// alias to use template instance with default allocator
using ElectionVote =
  skyhunter_msgs::msg::ElectionVote_<std::allocator<void>>;

// constant definitions

}  // namespace msg

}  // namespace skyhunter_msgs

#endif  // SKYHUNTER_MSGS__MSG__DETAIL__ELECTION_VOTE__STRUCT_HPP_
