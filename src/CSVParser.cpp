#include "CSVParser.h"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>

#if defined(HAS_OPENMP)
#include <omp.h>
#endif

// ─── RFC-4180 CSV Line Splitting (state machine) ────────────────────────────

std::vector<std::string> CSVParser::splitCSVLine(const std::string &line) {
  std::vector<std::string> fields;
  std::string current;
  bool inQuotes = false;

  for (size_t i = 0; i < line.size(); ++i) {
    char c = line[i];

    if (inQuotes) {
      if (c == '"') {
        // Check for escaped quote ""
        if (i + 1 < line.size() && line[i + 1] == '"') {
          current += '"';
          ++i; // skip the second quote
        } else {
          inQuotes = false; // end of quoted field
        }
      } else {
        current += c;
      }
    } else {
      if (c == '"') {
        inQuotes = true;
      } else if (c == ',') {
        fields.push_back(current);
        current.clear();
      } else {
        current += c;
      }
    }
  }
  fields.push_back(current); // last field
  return fields;
}

// ─── Date Parsing ────────────────────────────────────────────────────────────

time_t CSVParser::parseDate(const std::string &dateStr) {
  if (dateStr.empty())
    return 0;

  struct tm tm = {};

  // Try ISO 8601 format: "2026-03-06T02:53:58.000"
  if (dateStr.size() >= 19 && dateStr[4] == '-' && dateStr[10] == 'T') {
    tm.tm_year = std::stoi(dateStr.substr(0, 4)) - 1900;
    tm.tm_mon = std::stoi(dateStr.substr(5, 2)) - 1;
    tm.tm_mday = std::stoi(dateStr.substr(8, 2));
    tm.tm_hour = std::stoi(dateStr.substr(11, 2));
    tm.tm_min = std::stoi(dateStr.substr(14, 2));
    tm.tm_sec = std::stoi(dateStr.substr(17, 2));
    tm.tm_isdst = -1;
    return mktime(&tm);
  }

  // Try MM/DD/YYYY HH:MM:SS AM/PM format
  if (dateStr.size() >= 10 && dateStr[2] == '/') {
    tm.tm_mon = std::stoi(dateStr.substr(0, 2)) - 1;
    tm.tm_mday = std::stoi(dateStr.substr(3, 2));
    tm.tm_year = std::stoi(dateStr.substr(6, 4)) - 1900;

    if (dateStr.size() >= 19) {
      tm.tm_hour = std::stoi(dateStr.substr(11, 2));
      tm.tm_min = std::stoi(dateStr.substr(14, 2));
      tm.tm_sec = std::stoi(dateStr.substr(17, 2));

      // Handle AM/PM
      if (dateStr.size() >= 22) {
        std::string ampm = dateStr.substr(20, 2);
        if (ampm == "PM" && tm.tm_hour != 12)
          tm.tm_hour += 12;
        if (ampm == "AM" && tm.tm_hour == 12)
          tm.tm_hour = 0;
      }
    }

    tm.tm_isdst = -1;
    return mktime(&tm);
  }

  // Fallback: try strptime-style parsing
  return 0;
}

// ─── Safe type parsers ───────────────────────────────────────────────────────

int64_t CSVParser::safeParseInt64(const std::string &s) {
  if (s.empty())
    return 0;
  try {
    return std::stoll(s);
  } catch (...) {
    return 0;
  }
}

int32_t CSVParser::safeParseInt32(const std::string &s) {
  if (s.empty())
    return 0;
  try {
    return static_cast<int32_t>(std::stol(s));
  } catch (...) {
    return 0;
  }
}

double CSVParser::safeParseDouble(const std::string &s) {
  if (s.empty())
    return 0.0;
  try {
    return std::stod(s);
  } catch (...) {
    return 0.0;
  }
}

// ─── Parse a single row ─────────────────────────────────────────────────────

ServiceRequest CSVParser::parseLine(const std::vector<std::string> &fields) {
  ServiceRequest sr;

  // Safely access fields by index (CSV may have varying field counts)
  auto getField = [&fields](size_t idx) -> const std::string & {
    static const std::string empty;
    return (idx < fields.size()) ? fields[idx] : empty;
  };

  // Column mapping (0-indexed, matching the 44-column CSV):
  //  0: unique_key
  //  1: created_date
  //  2: closed_date
  //  3: agency
  //  4: agency_name
  //  5: complaint_type
  //  6: descriptor
  //  7: descriptor_2         (skip — not in primary mapping)
  //  8: location_type
  //  9: incident_zip
  // 10: incident_address
  // 11: street_name
  // 12: cross_street_1
  // 13: cross_street_2
  // 14: intersection_street_1
  // 15: intersection_street_2
  // 16: address_type
  // 17: city
  // 18: landmark
  // 19: facility_type
  // 20: status
  // 21: due_date
  // 22: resolution_description
  // 23: resolution_action_updated_date
  // 24: community_board
  // 25: council_district      (skip — stored only in SoA if needed)
  // 26: police_precinct        (skip)
  // 27: bbl                    (skip)
  // 28: borough
  // 29: x_coordinate_state_plane
  // 30: y_coordinate_state_plane
  // 31: open_data_channel_type
  // 32: park_facility_name
  // 33: park_borough           (skip — duplicate of borough)
  // 34: vehicle_type
  // 35: taxi_company_borough
  // 36: taxi_pick_up_location
  // 37: bridge_highway_name
  // 38: bridge_highway_direction
  // 39: road_ramp
  // 40: bridge_highway_segment
  // 41: latitude
  // 42: longitude
  // 43: location              (skip — duplicate lat/lon)

  sr.unique_key = safeParseInt64(getField(0));
  sr.created_date = parseDate(getField(1));
  sr.closed_date = parseDate(getField(2));
  sr.agency = getField(3);
  sr.agency_name = getField(4);
  sr.complaint_type = getField(5);
  sr.descriptor = getField(6);
  // field 7 = descriptor_2, skipped
  sr.location_type = getField(8);
  sr.incident_zip = safeParseInt32(getField(9));
  sr.incident_address = getField(10);
  sr.street_name = getField(11);
  sr.cross_street_1 = getField(12);
  sr.cross_street_2 = getField(13);
  sr.intersection_street_1 = getField(14);
  sr.intersection_street_2 = getField(15);
  sr.address_type = getField(16);
  sr.city = getField(17);
  sr.landmark = getField(18);
  sr.facility_type = getField(19);
  sr.status = stringToStatus(getField(20));
  sr.due_date = parseDate(getField(21));
  sr.resolution_description = getField(22);
  sr.resolution_updated_date = parseDate(getField(23));
  sr.community_board = getField(24);
  // fields 25-27 skipped (council_district, police_precinct, bbl)
  sr.borough = stringToBorough(getField(28));
  sr.x_coordinate = safeParseInt32(getField(29));
  sr.y_coordinate = safeParseInt32(getField(30));
  sr.channel_type = stringToChannelType(getField(31));
  sr.park_facility_name = getField(32);
  // field 33 = park_borough, skipped
  sr.vehicle_type = getField(34);
  sr.taxi_company_borough = getField(35);
  sr.taxi_pick_up_location = getField(36);
  sr.bridge_highway_name = getField(37);
  sr.bridge_highway_direction = getField(38);
  sr.road_ramp = getField(39);
  sr.bridge_highway_segment = getField(40);
  sr.latitude = safeParseDouble(getField(41));
  sr.longitude = safeParseDouble(getField(42));
  // field 43 = location point string, skipped

  return sr;
}

// ─── Parse entire file (serial) ──────────────────────────────────────────────

DataStore CSVParser::parseFile(const std::string &filename, size_t maxRows) {
  DataStore store;
  std::ifstream file(filename);
  if (!file.is_open()) {
    throw std::runtime_error("Cannot open file: " + filename);
  }

  std::string line;

  // Skip header line
  if (!std::getline(file, line)) {
    return store;
  }

  lastLineCount_ = 0;
  while (std::getline(file, line)) {
    if (line.empty())
      continue;
    if (maxRows > 0 && lastLineCount_ >= maxRows)
      break;
    auto fields = splitCSVLine(line);
    store.addRecord(parseLine(fields));
    ++lastLineCount_;
  }

  return store;
}

DataStore CSVParser::parseFileWithReserve(const std::string &filename,
                                          size_t reserveCount, size_t maxRows) {
  DataStore store;
  store.reserve(reserveCount);

  std::ifstream file(filename);
  if (!file.is_open()) {
    throw std::runtime_error("Cannot open file: " + filename);
  }

  std::string line;

  // Skip header line
  if (!std::getline(file, line)) {
    return store;
  }

  lastLineCount_ = 0;
  while (std::getline(file, line)) {
    if (line.empty())
      continue;
    if (maxRows > 0 && lastLineCount_ >= maxRows)
      break;
    auto fields = splitCSVLine(line);
    store.addRecord(parseLine(fields));
    ++lastLineCount_;
  }

  return store;
}

// ─── Phase 2: Parallel CSV Parsing ──────────────────────────────────────────

#if defined(HAS_OPENMP)
DataStore CSVParser::parseFileParallel(const std::string &filename,
                                       size_t maxRows) {
  // Chunk-based streaming parallel parse.
  // Instead of loading the entire file into memory (which OOMs on 12GB+),
  // we read CHUNK_SIZE lines at a time, parse each chunk in parallel, then
  // append to the result store. This bounds RAM to ~chunk overhead only.

  static const size_t CHUNK_SIZE = 100000; // 100K lines per batch

  std::ifstream file(filename);
  if (!file.is_open()) {
    throw std::runtime_error("Cannot open file: " + filename);
  }

  // Skip header
  std::string header;
  if (!std::getline(file, header)) {
    return DataStore();
  }

  DataStore store;
  lastLineCount_ = 0;
  bool done = false;

  while (!done) {
    // Read a chunk of lines
    std::vector<std::string> chunk;
    chunk.reserve(CHUNK_SIZE);
    std::string line;
    while (chunk.size() < CHUNK_SIZE && std::getline(file, line)) {
      if (!line.empty()) {
        chunk.push_back(std::move(line));
        if (maxRows > 0 && lastLineCount_ + chunk.size() >= maxRows) {
          done = true;
          break;
        }
      }
    }
    if (chunk.empty())
      break;
    if (file.eof())
      done = true;

    size_t n = chunk.size();
    std::vector<ServiceRequest> parsed(n);

    // Parse lines in parallel
#pragma omp parallel for schedule(static)
    for (size_t i = 0; i < n; ++i) {
      auto fields = splitCSVLine(chunk[i]);
      parsed[i] = parseLine(fields);
    }

    // Append to store (serial merge)
    store.reserve(store.size() + n);
    for (auto &sr : parsed) {
      store.addRecord(std::move(sr));
    }
    lastLineCount_ += n;

    if (maxRows > 0 && lastLineCount_ >= maxRows)
      break;
  }

  return store;
}
#endif
