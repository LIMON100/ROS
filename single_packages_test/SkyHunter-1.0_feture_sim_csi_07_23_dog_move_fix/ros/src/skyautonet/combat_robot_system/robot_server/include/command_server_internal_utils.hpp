#ifndef COMMAND_SERVER_INTERNAL_UTILS_HPP
#define COMMAND_SERVER_INTERNAL_UTILS_HPP

#include <algorithm>

#include "command_server_protocol.hpp"
#include "combat_robot_msgs/msg/mission_control_command.hpp"
#include "combat_robot_msgs/msg/operation_state.hpp"
#include "combat_robot_msgs/msg/user_command.hpp"

namespace command_server::detail {

inline int robotIndexFromId(uint32_t t_robot_id)
{
  if (t_robot_id == 0 || t_robot_id > MAX_SWARM_ROBOTS) {
    return -1;
  }
  return static_cast<int>(t_robot_id - 1);
}

inline bool isValidRobotId(uint32_t t_robot_id)
{
  return robotIndexFromId(t_robot_id) >= 0;
}

inline bool isStateCommandTargetingRobot(const StateCommand& t_command, uint32_t t_robot_id)
{
  if (!isValidRobotId(t_robot_id)) {
    return false;
  }

  const uint8_t selected_count =
    std::min<uint8_t>(t_command.selected_robot_count, static_cast<uint8_t>(MAX_SWARM_ROBOTS));
  if (selected_count == 0) {
    return true;
  }

  return std::find(
           t_command.selected_robot_ids,
           t_command.selected_robot_ids + selected_count,
           t_robot_id) != (t_command.selected_robot_ids + selected_count);
}

inline bool isValidFormationSelection(uint8_t t_formation_type, uint8_t t_formation_number)
{
  switch (static_cast<FormationType>(t_formation_type)) {
    case FormationType::NONE:
      return t_formation_number == 0;
    case FormationType::RECON:
    case FormationType::PROTECT:
    case FormationType::ASSAULT:
      return t_formation_number >= 1 && t_formation_number <= MAX_FORMATION_PRESET_NUMBER;
    default:
      return false;
  }
}

inline uint8_t normalizeFormationType(uint8_t t_formation_type, uint8_t t_formation_number)
{
  return isValidFormationSelection(t_formation_type, t_formation_number) ?
           t_formation_type :
           static_cast<uint8_t>(FormationType::NONE);
}

inline uint8_t normalizeFormationNumber(uint8_t t_formation_type, uint8_t t_formation_number)
{
  return isValidFormationSelection(t_formation_type, t_formation_number) ? t_formation_number : 0;
}

inline uint8_t normalizeAttackPermission(uint8_t t_attack_permission)
{
  switch (static_cast<AttackPermission>(t_attack_permission)) {
    case AttackPermission::NONE:
    case AttackPermission::APPROVE:
    case AttackPermission::DENY:
      return t_attack_permission;
    default:
      return static_cast<uint8_t>(AttackPermission::NONE);
  }
}

inline uint8_t mapIncomingCommandToMissionCommand(uint8_t t_incoming_command_id)
{
  using MissionControlCommand = combat_robot_msgs::msg::MissionControlCommand;

  // 태블릿 앱 enum 기준으로 변환. 서버 UserCommand 는 4=DEBUG_ATTACK,5=DEBUG_TRACKING 이
  // 끼어 있어 ASSAULT=6,RTH=7 인데, **태블릿엔 DEBUG 모드가 없어 ASSAULT=4,RTH=5 로 들어옴**
  // (raw 캡처로 실측 확정). 그래서 태블릿 ASSAULT 가 서버에서 DEBUG_ATTACK 으로 오해석돼
  // ATTACKING 으로 빠지던 문제 → 입력 코드를 태블릿 enum 으로 직접 변환해 해소.
  //   태블릿: 0=IDLE 1=RECON 2=PROTECT_G 3=PROTECT_D 4=ASSAULT 5=RTH (ESTOP 은 e_stop_command 플래그)
  switch (t_incoming_command_id) {
    case 0:
      return MissionControlCommand::IDLE;
    case 1:
      return MissionControlCommand::RECON;
    case 2:
      return MissionControlCommand::PROTECT_GENERAL;
    case 3:
      return MissionControlCommand::PROTECT_DRONE;
    case 4:
      return MissionControlCommand::ASSAULT;          // 태블릿 ASSAULT (서버 4=DEBUG_ATTACK 충돌 해소)
    case 5:
      return MissionControlCommand::RETURN_TO_HOME;   // 태블릿 RTH (서버 5=DEBUG_TRACKING 충돌 해소)
    default:
      return MissionControlCommand::IDLE;
  }
}

inline bool isIncomingEstopCommand(const StateCommand& t_command)
{
  using UserCommand = combat_robot_msgs::msg::UserCommand;

  return t_command.e_stop_command != 0 || t_command.command_id == UserCommand::ESTOP;
}

inline bool isModeChangeAllowed(uint8_t t_current_operation_state, uint8_t t_requested_command_id)
{
  using MissionControlCommand = combat_robot_msgs::msg::MissionControlCommand;
  using OperationState = combat_robot_msgs::msg::OperationState;

  if (t_requested_command_id == MissionControlCommand::RETURN_TO_HOME ||
      t_requested_command_id == MissionControlCommand::IDLE)
  {
    return true;
  }

  return t_current_operation_state == OperationState::IDLE;
}

inline uint8_t commandIdForCurrentState(uint8_t t_current_operation_state)
{
  using MissionControlCommand = combat_robot_msgs::msg::MissionControlCommand;
  using OperationState = combat_robot_msgs::msg::OperationState;

  switch (t_current_operation_state) {
    case OperationState::MOVE:
      return MissionControlCommand::RECON;
    case OperationState::SURVEILLANCE:
      return MissionControlCommand::PROTECT_GENERAL;
    case OperationState::DRONE_SURVEILLANCE:
      return MissionControlCommand::PROTECT_DRONE;
    case OperationState::MANUAL_ATTACK:
      return MissionControlCommand::DEBUG_ATTACK;
    case OperationState::ASSAULT:
      return MissionControlCommand::ASSAULT;
    case OperationState::TRACKING:
      return MissionControlCommand::DEBUG_TRACKING;
    case OperationState::EMERGENCY_STOP:
      return MissionControlCommand::IDLE;
    case OperationState::IDLE:
    case OperationState::INIT:
    case OperationState::ERROR:
    default:
      return MissionControlCommand::IDLE;
  }
}

inline uint8_t commandIdForCurrentContext(
  uint8_t t_current_operation_state,
  uint8_t t_current_active_mode_id)
{
  using MissionControlCommand = combat_robot_msgs::msg::MissionControlCommand;
  using OperationState = combat_robot_msgs::msg::OperationState;

  switch (t_current_active_mode_id) {
    case OperationState::ACTIVE_MODE_RECON:
      return MissionControlCommand::RECON;
    case OperationState::ACTIVE_MODE_PROTECT_GENERAL:
      return MissionControlCommand::PROTECT_GENERAL;
    case OperationState::ACTIVE_MODE_PROTECT_DRONE:
      return MissionControlCommand::PROTECT_DRONE;
    case OperationState::ACTIVE_MODE_ASSAULT:
      return MissionControlCommand::ASSAULT;
    case OperationState::ACTIVE_MODE_RETURN_TO_HOME:
      return MissionControlCommand::RETURN_TO_HOME;
    default:
      return commandIdForCurrentState(t_current_operation_state);
  }
}

}  // namespace command_server::detail

#endif  // COMMAND_SERVER_INTERNAL_UTILS_HPP
