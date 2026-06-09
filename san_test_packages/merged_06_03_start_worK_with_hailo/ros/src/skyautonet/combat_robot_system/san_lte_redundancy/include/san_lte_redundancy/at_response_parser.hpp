// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 Phase 2-E Turn 3 — AT modem response parser.
//
// Pure-logic parsers for modem AT response lines. NO ROS, NO serial
// dependency — fully testable standalone. The LteModemNode delegates
// all string-to-field extraction here so the parsing logic can be
// unit-tested via gtest without a modem or rclcpp environment.
//
// Supported response formats:
//   +CREG: <n>,<stat>[,...]               registration
//   +COPS: <mode>,<format>,"<oper>"[,<rat>]  operator + rat
//   +QCSQ: "<rat>",<rssi>,<rsrp>,<sinr>,<rsrq>  Quectel proprietary
//   +CESQ: <rxlev>,<ber>,<rscp>,<ecno>,<rsrq>,<rsrp>  3GPP standard
//   +CGPADDR: <cid>,"<ip>"                 PDP IP address

#ifndef SAN_LTE_REDUNDANCY__AT_RESPONSE_PARSER_HPP_
#define SAN_LTE_REDUNDANCY__AT_RESPONSE_PARSER_HPP_

#include <cstdint>
#include <optional>
#include <string>

namespace san_lte_redundancy
{

/// Mirrors the LteModemStatus.msg REG_* constants.
enum class CregStatus : uint8_t
{
  NotRegistered = 0,
  Home          = 1,
  Searching     = 2,
  Denied        = 3,
  Unknown       = 4,
  Roaming       = 5,
};

struct QcsqResult
{
  std::string rat;           // "LTE", "WCDMA", "NR5G", ...
  int32_t rssi_dbm = INT32_MIN;
  int32_t rsrp_dbm = INT32_MIN;
  int32_t sinr_db = INT32_MIN;        // some firmware reports in 0.5 dB
  int32_t rsrq_db = INT32_MIN;
};

struct CesqResult
{
  int32_t rsrq_db = INT32_MIN;
  int32_t rsrp_dbm = INT32_MIN;
};

/// Parse "+CREG: <n>,<stat>[,<lac>,<ci>]" or unsolicited "+CREG: <stat>"
/// Returns std::nullopt on parse failure.
std::optional<CregStatus> parseCreg(const std::string & line);

/// Parse "+COPS: <mode>,<format>,\"<operator>\"[,<act>]"
/// Returns the operator string (between the first pair of quotes) on
/// success, std::nullopt otherwise.
std::optional<std::string> parseCops(const std::string & line);

/// Parse Quectel "+QCSQ: \"<rat>\",<rssi>,<rsrp>,<sinr>,<rsrq>"
/// All numeric fields optional; missing fields stay INT32_MIN.
std::optional<QcsqResult> parseQcsq(const std::string & line);

/// Parse 3GPP "+CESQ: <rxlev>,<ber>,<rscp>,<ecno>,<rsrq>,<rsrp>"
/// Note: CESQ rsrp is encoded as (rsrp_dbm + 141) mapped to 0..97;
/// we convert back to dBm.
std::optional<CesqResult> parseCesq(const std::string & line);

/// Parse "+CGPADDR: <cid>,\"<ip>\""
/// Returns the IPv4 dotted-quad on success.
std::optional<std::string> parseCgpaddr(const std::string & line);

}  // namespace san_lte_redundancy

#endif  // SAN_LTE_REDUNDANCY__AT_RESPONSE_PARSER_HPP_
