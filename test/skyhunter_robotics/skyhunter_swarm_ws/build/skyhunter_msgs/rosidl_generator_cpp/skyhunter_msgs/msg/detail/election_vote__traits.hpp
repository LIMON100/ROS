// generated from rosidl_generator_cpp/resource/idl__traits.hpp.em
// with input from skyhunter_msgs:msg/ElectionVote.idl
// generated code does not contain a copyright notice

#ifndef SKYHUNTER_MSGS__MSG__DETAIL__ELECTION_VOTE__TRAITS_HPP_
#define SKYHUNTER_MSGS__MSG__DETAIL__ELECTION_VOTE__TRAITS_HPP_

#include <stdint.h>

#include <sstream>
#include <string>
#include <type_traits>

#include "skyhunter_msgs/msg/detail/election_vote__struct.hpp"
#include "rosidl_runtime_cpp/traits.hpp"

namespace skyhunter_msgs
{

namespace msg
{

inline void to_flow_style_yaml(
  const ElectionVote & msg,
  std::ostream & out)
{
  out << "{";
  // member: term
  {
    out << "term: ";
    rosidl_generator_traits::value_to_yaml(msg.term, out);
    out << ", ";
  }

  // member: candidate_id
  {
    out << "candidate_id: ";
    rosidl_generator_traits::value_to_yaml(msg.candidate_id, out);
    out << ", ";
  }

  // member: voter_id
  {
    out << "voter_id: ";
    rosidl_generator_traits::value_to_yaml(msg.voter_id, out);
    out << ", ";
  }

  // member: fitness_score
  {
    out << "fitness_score: ";
    rosidl_generator_traits::value_to_yaml(msg.fitness_score, out);
  }
  out << "}";
}  // NOLINT(readability/fn_size)

inline void to_block_style_yaml(
  const ElectionVote & msg,
  std::ostream & out, size_t indentation = 0)
{
  // member: term
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "term: ";
    rosidl_generator_traits::value_to_yaml(msg.term, out);
    out << "\n";
  }

  // member: candidate_id
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "candidate_id: ";
    rosidl_generator_traits::value_to_yaml(msg.candidate_id, out);
    out << "\n";
  }

  // member: voter_id
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "voter_id: ";
    rosidl_generator_traits::value_to_yaml(msg.voter_id, out);
    out << "\n";
  }

  // member: fitness_score
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "fitness_score: ";
    rosidl_generator_traits::value_to_yaml(msg.fitness_score, out);
    out << "\n";
  }
}  // NOLINT(readability/fn_size)

inline std::string to_yaml(const ElectionVote & msg, bool use_flow_style = false)
{
  std::ostringstream out;
  if (use_flow_style) {
    to_flow_style_yaml(msg, out);
  } else {
    to_block_style_yaml(msg, out);
  }
  return out.str();
}

}  // namespace msg

}  // namespace skyhunter_msgs

namespace rosidl_generator_traits
{

[[deprecated("use skyhunter_msgs::msg::to_block_style_yaml() instead")]]
inline void to_yaml(
  const skyhunter_msgs::msg::ElectionVote & msg,
  std::ostream & out, size_t indentation = 0)
{
  skyhunter_msgs::msg::to_block_style_yaml(msg, out, indentation);
}

[[deprecated("use skyhunter_msgs::msg::to_yaml() instead")]]
inline std::string to_yaml(const skyhunter_msgs::msg::ElectionVote & msg)
{
  return skyhunter_msgs::msg::to_yaml(msg);
}

template<>
inline const char * data_type<skyhunter_msgs::msg::ElectionVote>()
{
  return "skyhunter_msgs::msg::ElectionVote";
}

template<>
inline const char * name<skyhunter_msgs::msg::ElectionVote>()
{
  return "skyhunter_msgs/msg/ElectionVote";
}

template<>
struct has_fixed_size<skyhunter_msgs::msg::ElectionVote>
  : std::integral_constant<bool, false> {};

template<>
struct has_bounded_size<skyhunter_msgs::msg::ElectionVote>
  : std::integral_constant<bool, false> {};

template<>
struct is_message<skyhunter_msgs::msg::ElectionVote>
  : std::true_type {};

}  // namespace rosidl_generator_traits

#endif  // SKYHUNTER_MSGS__MSG__DETAIL__ELECTION_VOTE__TRAITS_HPP_
