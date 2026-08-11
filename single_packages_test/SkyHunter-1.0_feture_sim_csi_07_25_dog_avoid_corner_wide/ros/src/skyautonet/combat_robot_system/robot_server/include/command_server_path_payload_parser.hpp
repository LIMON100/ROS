#ifndef COMMAND_SERVER_PATH_PAYLOAD_PARSER_HPP
#define COMMAND_SERVER_PATH_PAYLOAD_PARSER_HPP

#include <cstddef>
#include <string>
#include <vector>

namespace command_server {

struct ParsedWaypoint {
  double latitude;
  double longitude;
};

std::size_t extractWaypointCountFromPayload(const std::string& t_payload_json);
std::string extractJsonStringField(const std::string& t_json_text, const char* t_field_name);
bool extractParsedPathFromPayload(
  const std::string& t_payload_json,
  std::vector<ParsedWaypoint>* t_waypoints_out,
  std::string* t_source_format_out);

}  // namespace command_server

#endif  // COMMAND_SERVER_PATH_PAYLOAD_PARSER_HPP
