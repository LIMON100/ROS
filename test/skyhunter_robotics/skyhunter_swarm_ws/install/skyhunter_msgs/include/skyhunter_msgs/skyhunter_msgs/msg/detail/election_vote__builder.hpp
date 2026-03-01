// generated from rosidl_generator_cpp/resource/idl__builder.hpp.em
// with input from skyhunter_msgs:msg/ElectionVote.idl
// generated code does not contain a copyright notice

#ifndef SKYHUNTER_MSGS__MSG__DETAIL__ELECTION_VOTE__BUILDER_HPP_
#define SKYHUNTER_MSGS__MSG__DETAIL__ELECTION_VOTE__BUILDER_HPP_

#include <algorithm>
#include <utility>

#include "skyhunter_msgs/msg/detail/election_vote__struct.hpp"
#include "rosidl_runtime_cpp/message_initialization.hpp"


namespace skyhunter_msgs
{

namespace msg
{

namespace builder
{

class Init_ElectionVote_fitness_score
{
public:
  explicit Init_ElectionVote_fitness_score(::skyhunter_msgs::msg::ElectionVote & msg)
  : msg_(msg)
  {}
  ::skyhunter_msgs::msg::ElectionVote fitness_score(::skyhunter_msgs::msg::ElectionVote::_fitness_score_type arg)
  {
    msg_.fitness_score = std::move(arg);
    return std::move(msg_);
  }

private:
  ::skyhunter_msgs::msg::ElectionVote msg_;
};

class Init_ElectionVote_voter_id
{
public:
  explicit Init_ElectionVote_voter_id(::skyhunter_msgs::msg::ElectionVote & msg)
  : msg_(msg)
  {}
  Init_ElectionVote_fitness_score voter_id(::skyhunter_msgs::msg::ElectionVote::_voter_id_type arg)
  {
    msg_.voter_id = std::move(arg);
    return Init_ElectionVote_fitness_score(msg_);
  }

private:
  ::skyhunter_msgs::msg::ElectionVote msg_;
};

class Init_ElectionVote_candidate_id
{
public:
  explicit Init_ElectionVote_candidate_id(::skyhunter_msgs::msg::ElectionVote & msg)
  : msg_(msg)
  {}
  Init_ElectionVote_voter_id candidate_id(::skyhunter_msgs::msg::ElectionVote::_candidate_id_type arg)
  {
    msg_.candidate_id = std::move(arg);
    return Init_ElectionVote_voter_id(msg_);
  }

private:
  ::skyhunter_msgs::msg::ElectionVote msg_;
};

class Init_ElectionVote_term
{
public:
  Init_ElectionVote_term()
  : msg_(::rosidl_runtime_cpp::MessageInitialization::SKIP)
  {}
  Init_ElectionVote_candidate_id term(::skyhunter_msgs::msg::ElectionVote::_term_type arg)
  {
    msg_.term = std::move(arg);
    return Init_ElectionVote_candidate_id(msg_);
  }

private:
  ::skyhunter_msgs::msg::ElectionVote msg_;
};

}  // namespace builder

}  // namespace msg

template<typename MessageType>
auto build();

template<>
inline
auto build<::skyhunter_msgs::msg::ElectionVote>()
{
  return skyhunter_msgs::msg::builder::Init_ElectionVote_term();
}

}  // namespace skyhunter_msgs

#endif  // SKYHUNTER_MSGS__MSG__DETAIL__ELECTION_VOTE__BUILDER_HPP_
