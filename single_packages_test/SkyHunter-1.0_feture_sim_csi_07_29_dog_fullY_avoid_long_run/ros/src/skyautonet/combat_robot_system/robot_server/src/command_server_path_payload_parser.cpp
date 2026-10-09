#include "command_server_path_payload_parser.hpp"

#include <cctype>
#include <cstdlib>

namespace command_server {
namespace {

enum class WaypointPayloadFormat {
  OBJECT_ARRAY,
  GEOJSON_COORDINATES
};

std::size_t findMatchingDelimiter(
  const std::string& t_text,
  std::size_t t_open_pos,
  char t_open_char,
  char t_close_char)
{
  if (t_open_pos >= t_text.size() || t_text[t_open_pos] != t_open_char) {
    return std::string::npos;
  }

  int depth = 0;
  for (std::size_t i = t_open_pos; i < t_text.size(); ++i) {
    if (t_text[i] == t_open_char) {
      ++depth;
    } else if (t_text[i] == t_close_char) {
      --depth;
      if (depth == 0) {
        return i;
      }
    }
  }

  return std::string::npos;
}

bool extractJsonNumberField(
  const std::string& t_object_text,
  const char* t_field_name,
  double* t_value)
{
  if (t_value == nullptr) {
    return false;
  }

  const std::string key = "\"" + std::string(t_field_name) + "\"";
  const std::size_t key_pos = t_object_text.find(key);
  if (key_pos == std::string::npos) {
    return false;
  }

  const std::size_t colon_pos = t_object_text.find(':', key_pos + key.size());
  if (colon_pos == std::string::npos) {
    return false;
  }

  std::size_t value_start = colon_pos + 1;
  while (value_start < t_object_text.size() &&
         std::isspace(static_cast<unsigned char>(t_object_text[value_start])))
  {
    ++value_start;
  }
  if (value_start >= t_object_text.size()) {
    return false;
  }

  char* parse_end = nullptr;
  *t_value = std::strtod(t_object_text.c_str() + value_start, &parse_end);
  return parse_end != t_object_text.c_str() + value_start;
}

bool findWaypointArrayBounds(
  const std::string& t_payload_json,
  std::size_t* t_array_start,
  std::size_t* t_array_end,
  WaypointPayloadFormat* t_payload_format)
{
  if (t_array_start == nullptr || t_array_end == nullptr || t_payload_format == nullptr) {
    return false;
  }

  std::size_t key_pos = t_payload_json.find("\"waypoints\"");
  if (key_pos != std::string::npos) {
    *t_array_start = t_payload_json.find('[', key_pos);
    if (*t_array_start == std::string::npos) {
      return false;
    }

    *t_array_end = findMatchingDelimiter(t_payload_json, *t_array_start, '[', ']');
    if (*t_array_end == std::string::npos) {
      return false;
    }

    *t_payload_format = WaypointPayloadFormat::OBJECT_ARRAY;
    return true;
  }

  key_pos = t_payload_json.find("\"coordinates\"");
  if (key_pos != std::string::npos) {
    *t_array_start = t_payload_json.find('[', key_pos);
    if (*t_array_start == std::string::npos) {
      return false;
    }

    *t_array_end = findMatchingDelimiter(t_payload_json, *t_array_start, '[', ']');
    if (*t_array_end == std::string::npos) {
      return false;
    }

    *t_payload_format = WaypointPayloadFormat::GEOJSON_COORDINATES;
    return true;
  }

  const std::size_t first_non_ws = t_payload_json.find_first_not_of(" \t\r\n");
  if (first_non_ws != std::string::npos && t_payload_json[first_non_ws] == '[') {
    *t_array_start = first_non_ws;
    *t_array_end = findMatchingDelimiter(t_payload_json, *t_array_start, '[', ']');
    if (*t_array_end == std::string::npos) {
      return false;
    }

    *t_payload_format = WaypointPayloadFormat::OBJECT_ARRAY;
    return true;
  }

  return false;
}

bool extractWaypointLatLon(const std::string& t_object_text, double* t_lat, double* t_lon)
{
  if (t_lat == nullptr || t_lon == nullptr) {
    return false;
  }

  double parsed_lat = 0.0;
  double parsed_lon = 0.0;
  const bool has_lat =
    extractJsonNumberField(t_object_text, "lat", &parsed_lat) ||
    extractJsonNumberField(t_object_text, "latitude", &parsed_lat);
  const bool has_lon =
    extractJsonNumberField(t_object_text, "lon", &parsed_lon) ||
    extractJsonNumberField(t_object_text, "lng", &parsed_lon) ||
    extractJsonNumberField(t_object_text, "longitude", &parsed_lon);

  if (!has_lat || !has_lon) {
    return false;
  }

  *t_lat = parsed_lat;
  *t_lon = parsed_lon;
  return true;
}

bool extractNextJsonNumber(const std::string& t_text, std::size_t* t_scan_pos, double* t_value)
{
  if (t_scan_pos == nullptr || t_value == nullptr) {
    return false;
  }

  std::size_t value_start = *t_scan_pos;
  while (value_start < t_text.size() &&
         !std::isdigit(static_cast<unsigned char>(t_text[value_start])) &&
         t_text[value_start] != '-')
  {
    ++value_start;
  }
  if (value_start >= t_text.size()) {
    return false;
  }

  char* parse_end = nullptr;
  *t_value = std::strtod(t_text.c_str() + value_start, &parse_end);
  if (parse_end == t_text.c_str() + value_start) {
    return false;
  }

  *t_scan_pos = static_cast<std::size_t>(parse_end - t_text.c_str());
  return true;
}

bool extractGeoJsonCoordinateLatLon(
  const std::string& t_coordinate_text,
  double* t_lat,
  double* t_lon)
{
  if (t_lat == nullptr || t_lon == nullptr) {
    return false;
  }

  std::size_t scan_pos = 0;
  double parsed_lon = 0.0;
  double parsed_lat = 0.0;
  if (!extractNextJsonNumber(t_coordinate_text, &scan_pos, &parsed_lon) ||
      !extractNextJsonNumber(t_coordinate_text, &scan_pos, &parsed_lat))
  {
    return false;
  }

  *t_lat = parsed_lat;
  *t_lon = parsed_lon;
  return true;
}

}  // namespace

std::size_t extractWaypointCountFromPayload(const std::string& t_payload_json)
{
  std::size_t array_start = 0;
  std::size_t array_end = 0;
  WaypointPayloadFormat payload_format = WaypointPayloadFormat::OBJECT_ARRAY;
  if (!findWaypointArrayBounds(
        t_payload_json, &array_start, &array_end, &payload_format))
  {
    return 0;
  }

  std::size_t waypoint_count = 0;
  std::size_t scan_pos = array_start + 1;
  while (scan_pos < array_end) {
    double lat = 0.0;
    double lon = 0.0;
    bool has_lat_lon = false;

    if (payload_format == WaypointPayloadFormat::OBJECT_ARRAY) {
      const std::size_t object_start = t_payload_json.find('{', scan_pos);
      if (object_start == std::string::npos || object_start > array_end) {
        break;
      }

      const std::size_t object_end =
        findMatchingDelimiter(t_payload_json, object_start, '{', '}');
      if (object_end == std::string::npos || object_end > array_end) {
        break;
      }

      has_lat_lon = extractWaypointLatLon(
        t_payload_json.substr(object_start, object_end - object_start + 1),
        &lat,
        &lon);
      scan_pos = object_end + 1;
    } else {
      const std::size_t coordinate_start = t_payload_json.find('[', scan_pos);
      if (coordinate_start == std::string::npos || coordinate_start > array_end) {
        break;
      }

      const std::size_t coordinate_end =
        findMatchingDelimiter(t_payload_json, coordinate_start, '[', ']');
      if (coordinate_end == std::string::npos || coordinate_end > array_end) {
        break;
      }

      has_lat_lon = extractGeoJsonCoordinateLatLon(
        t_payload_json.substr(coordinate_start, coordinate_end - coordinate_start + 1),
        &lat,
        &lon);
      scan_pos = coordinate_end + 1;
    }

    if (has_lat_lon) {
      ++waypoint_count;
    }
  }

  return waypoint_count;
}

bool extractParsedPathFromPayload(
  const std::string& t_payload_json,
  std::vector<ParsedWaypoint>* t_waypoints_out,
  std::string* t_source_format_out)
{
  if (t_waypoints_out == nullptr || t_source_format_out == nullptr) {
    return false;
  }

  t_waypoints_out->clear();
  t_source_format_out->clear();

  std::size_t array_start = 0;
  std::size_t array_end = 0;
  WaypointPayloadFormat payload_format = WaypointPayloadFormat::OBJECT_ARRAY;
  if (!findWaypointArrayBounds(
        t_payload_json, &array_start, &array_end, &payload_format))
  {
    return false;
  }

  *t_source_format_out =
    (payload_format == WaypointPayloadFormat::OBJECT_ARRAY)
      ? "waypoints"
      : "coordinates";

  std::size_t scan_pos = array_start + 1;
  while (scan_pos < array_end) {
    double lat = 0.0;
    double lon = 0.0;
    bool has_lat_lon = false;

    if (payload_format == WaypointPayloadFormat::OBJECT_ARRAY) {
      const std::size_t object_start = t_payload_json.find('{', scan_pos);
      if (object_start == std::string::npos || object_start > array_end) {
        break;
      }

      const std::size_t object_end =
        findMatchingDelimiter(t_payload_json, object_start, '{', '}');
      if (object_end == std::string::npos || object_end > array_end) {
        break;
      }

      has_lat_lon = extractWaypointLatLon(
        t_payload_json.substr(object_start, object_end - object_start + 1),
        &lat,
        &lon);
      scan_pos = object_end + 1;
    } else {
      const std::size_t coordinate_start = t_payload_json.find('[', scan_pos);
      if (coordinate_start == std::string::npos || coordinate_start > array_end) {
        break;
      }

      const std::size_t coordinate_end =
        findMatchingDelimiter(t_payload_json, coordinate_start, '[', ']');
      if (coordinate_end == std::string::npos || coordinate_end > array_end) {
        break;
      }

      has_lat_lon = extractGeoJsonCoordinateLatLon(
        t_payload_json.substr(coordinate_start, coordinate_end - coordinate_start + 1),
        &lat,
        &lon);
      scan_pos = coordinate_end + 1;
    }

    if (has_lat_lon) {
      t_waypoints_out->push_back(ParsedWaypoint{lat, lon});
    }
  }

  return !t_waypoints_out->empty();
}

std::string extractJsonStringField(const std::string& t_json_text, const char* t_field_name)
{
  const std::string key = "\"" + std::string(t_field_name) + "\"";
  const std::size_t key_pos = t_json_text.find(key);
  if (key_pos == std::string::npos) {
    return "";
  }

  const std::size_t colon_pos = t_json_text.find(':', key_pos + key.size());
  if (colon_pos == std::string::npos) {
    return "";
  }

  const std::size_t value_open = t_json_text.find('"', colon_pos + 1);
  if (value_open == std::string::npos) {
    return "";
  }

  std::size_t value_close = value_open + 1;
  while (value_close < t_json_text.size()) {
    if (t_json_text[value_close] == '"' && t_json_text[value_close - 1] != '\\') {
      break;
    }
    ++value_close;
  }

  if (value_close >= t_json_text.size()) {
    return "";
  }

  return t_json_text.substr(value_open + 1, value_close - value_open - 1);
}

}  // namespace command_server
