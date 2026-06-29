"""Modified Raft election + DDS Liveliness QoS (SDD Rev.A.6 §6, §6.5).

Priority 4-tuple, weighted score in [0, 1]:
  [Battery 0.4 | RTK 0.3 | Sensor health 0.2 | RobotID 0.1]

DDS Liveliness:
  Lease duration: 2.0 s  (LIVELINESS_LOST after this without heartbeat)
  Deadline:       0.2 s  (heartbeat cadence)
  3 missed deadlines → CANDIDATE state
  Election timeout: 1.0 s (CANDIDATE waits this long for vote majority)

KPP §2.1.1 「reconfiguration ≤ 10 s」 target.
"""
from __future__ import annotations

import struct
import time
from dataclasses import dataclass
from enum import IntEnum
from typing import Dict, Optional


class ElectionState(IntEnum):
    FOLLOWER = 0
    CANDIDATE = 1
    LEADER = 2


@dataclass
class ElectionPriority:
    """4-tuple weighted priority for leader election."""
    battery_pct: float        # 0.0 .. 1.0  (weight 0.4)
    rtk_quality: float        # 0.0 (none) .. 1.0 (FIXED)  (weight 0.3)
    sensor_health: float      # 0.0 .. 1.0  (weight 0.2)
    robot_id: int             # uint32, smaller = preferred (weight 0.1)

    def score(self) -> float:
        """Weighted scalar in [0, 1]. Higher is more preferred."""
        # robot_id contribution: smaller IDs get a small bias (max 0.1).
        id_score = 1.0 - min(self.robot_id / 10000.0, 1.0)
        return (0.4 * self.battery_pct
                + 0.3 * self.rtk_quality
                + 0.2 * self.sensor_health
                + 0.1 * id_score)

    def to_bytes(self) -> bytes:
        """Pack as 32 bytes (256 bit) for DDS broadcast."""
        head = struct.pack(
            "<ffffI",
            self.battery_pct, self.rtk_quality,
            self.sensor_health, 0.0,           # padding float
            self.robot_id,
        )
        return head + b"\x00" * (32 - len(head))

    @staticmethod
    def from_bytes(data: bytes) -> "ElectionPriority":
        b, r, s, _, rid = struct.unpack("<ffffI", data[:20])
        return ElectionPriority(b, r, s, rid)


@dataclass
class ElectionVote:
    term: int
    candidate_id: int
    voter_id: int
    granted: bool


class RaftElection:
    """Modified Raft election state machine.

    Triggered by DDS Liveliness LIVELINESS_LOST event from the leader_pose
    topic — the lease (LEASE_DURATION_S) is the wall-clock window after
    which a missing leader heartbeat counts as a fault.
    """

    LEASE_DURATION_S = 2.0
    DEADLINE_S = 0.2
    MAX_MISSED = 3
    ELECTION_TIMEOUT_S = 1.0

    def __init__(self, robot_id: int):
        self.robot_id = robot_id
        self.state = ElectionState.FOLLOWER
        self.current_term = 0
        self.voted_for: Optional[int] = None
        self.leader_id: Optional[int] = None
        self.last_leader_seen: float = time.monotonic()
        self.votes_received: Dict[int, ElectionVote] = {}
        self.peer_priorities: Dict[int, ElectionPriority] = {}
        self.my_priority: Optional[ElectionPriority] = None

    def update_my_priority(self, priority: ElectionPriority) -> None:
        self.my_priority = priority

    def receive_priority(self, robot_id: int,
                         priority: ElectionPriority) -> None:
        self.peer_priorities[robot_id] = priority

    def heartbeat_received_from_leader(self, leader_id: int,
                                       term: int) -> None:
        """Leader heartbeat — reset to FOLLOWER if term is at least ours."""
        if term >= self.current_term:
            self.current_term = term
            self.leader_id = leader_id
            self.state = ElectionState.FOLLOWER
            self.last_leader_seen = time.monotonic()
            self.voted_for = None

    def check_liveliness(self) -> bool:
        """Return True if leader liveliness lost (start election)."""
        elapsed = time.monotonic() - self.last_leader_seen
        return elapsed > self.LEASE_DURATION_S

    def start_election(self) -> None:
        """Transition to CANDIDATE, increment term, vote for self."""
        self.state = ElectionState.CANDIDATE
        self.current_term += 1
        self.voted_for = self.robot_id
        self.votes_received.clear()
        self.votes_received[self.robot_id] = ElectionVote(
            term=self.current_term,
            candidate_id=self.robot_id,
            voter_id=self.robot_id,
            granted=True,
        )

    def receive_vote_request(self, term: int, candidate_id: int,
                             candidate_priority: ElectionPriority) -> bool:
        """Reply granted=True iff we can vote for this candidate."""
        if term < self.current_term:
            return False
        if term > self.current_term:
            self.current_term = term
            self.voted_for = None
            self.state = ElectionState.FOLLOWER

        if self.voted_for is not None and self.voted_for != candidate_id:
            return False

        # Compare priorities: only abstain if I'm a strictly better candidate
        # AND I haven't already kicked off my own election.
        if self.my_priority is not None:
            if self.my_priority.score() > candidate_priority.score():
                if self.state != ElectionState.CANDIDATE:
                    return False

        self.voted_for = candidate_id
        return True

    def receive_vote(self, vote: ElectionVote, total_peers: int) -> bool:
        """Process incoming vote. Return True iff majority achieved."""
        if vote.term != self.current_term:
            return False
        if not vote.granted:
            return False
        self.votes_received[vote.voter_id] = vote
        majority = (total_peers // 2) + 1
        return len(self.votes_received) >= majority

    def become_leader(self) -> None:
        self.state = ElectionState.LEADER
        self.leader_id = self.robot_id
        self.last_leader_seen = time.monotonic()
