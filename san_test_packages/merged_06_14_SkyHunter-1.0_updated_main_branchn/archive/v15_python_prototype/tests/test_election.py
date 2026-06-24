"""Tests for Modified Raft election (P1-13, SDD Rev.A.6 §6, §6.5)."""
import time

from swarm.election import (
    ElectionPriority,
    ElectionState,
    ElectionVote,
    RaftElection,
)


def test_initial_state_is_follower():
    e = RaftElection(robot_id=1)
    assert e.state == ElectionState.FOLLOWER
    assert e.current_term == 0


def test_priority_score_calculation():
    p = ElectionPriority(battery_pct=1.0, rtk_quality=1.0,
                         sensor_health=1.0, robot_id=1)
    assert 0.95 < p.score() <= 1.0


def test_priority_score_battery_dominant():
    p_high = ElectionPriority(battery_pct=1.0, rtk_quality=0.5,
                              sensor_health=0.5, robot_id=1)
    p_low = ElectionPriority(battery_pct=0.2, rtk_quality=0.5,
                             sensor_health=0.5, robot_id=1)
    assert p_high.score() > p_low.score()


def test_priority_serialization_roundtrip():
    p = ElectionPriority(0.85, 0.9, 0.95, robot_id=42)
    b = p.to_bytes()
    assert len(b) == 32                 # 256-bit fixed payload
    p2 = ElectionPriority.from_bytes(b)
    assert abs(p.battery_pct - p2.battery_pct) < 1e-6
    assert abs(p.rtk_quality - p2.rtk_quality) < 1e-6
    assert abs(p.sensor_health - p2.sensor_health) < 1e-6
    assert p.robot_id == p2.robot_id


def test_check_liveliness_lost_after_lease():
    e = RaftElection(robot_id=1)
    e.last_leader_seen = time.monotonic() - 3.0   # 3 s ago — past lease
    assert e.check_liveliness() is True


def test_check_liveliness_ok_within_lease():
    e = RaftElection(robot_id=1)
    e.last_leader_seen = time.monotonic() - 0.5   # 0.5 s ago
    assert e.check_liveliness() is False


def test_start_election_increments_term():
    e = RaftElection(robot_id=1)
    e.start_election()
    assert e.state == ElectionState.CANDIDATE
    assert e.current_term == 1
    assert e.voted_for == 1
    assert len(e.votes_received) == 1   # self-vote


def test_become_leader_after_majority():
    e = RaftElection(robot_id=1)
    e.start_election()
    # 5 peers total → majority = 3 (incl self-vote)
    vote_a = ElectionVote(term=1, candidate_id=1, voter_id=2, granted=True)
    vote_b = ElectionVote(term=1, candidate_id=1, voter_id=3, granted=True)
    assert e.receive_vote(vote_a, total_peers=5) is False
    assert e.receive_vote(vote_b, total_peers=5) is True   # majority


def test_heartbeat_resets_to_follower():
    e = RaftElection(robot_id=2)
    e.start_election()                   # → CANDIDATE
    e.heartbeat_received_from_leader(leader_id=1, term=2)
    assert e.state == ElectionState.FOLLOWER
    assert e.leader_id == 1
    assert e.current_term == 2


def test_vote_request_higher_term_rolls_over():
    e = RaftElection(robot_id=2)
    e.current_term = 1
    e.voted_for = 99
    p = ElectionPriority(0.8, 0.8, 0.8, 3)
    granted = e.receive_vote_request(
        term=5, candidate_id=3, candidate_priority=p)
    assert e.current_term == 5
    assert granted is True


def test_lower_term_vote_rejected():
    e = RaftElection(robot_id=2)
    e.current_term = 5
    p = ElectionPriority(0.8, 0.8, 0.8, 3)
    granted = e.receive_vote_request(
        term=3, candidate_id=3, candidate_priority=p)
    assert granted is False
