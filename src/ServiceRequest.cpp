#include "ServiceRequest.h"
#include <algorithm>
#include <cctype>

// ─── Helper: uppercase a string for case-insensitive matching
// ─────────────────

static std::string toUpper(const std::string &s) {
  std::string result = s;
  std::transform(result.begin(), result.end(), result.begin(),
                 [](unsigned char c) { return std::toupper(c); });
  return result;
}

// ─── Borough conversions
// ──────────────────────────────────────────────────────

Borough stringToBorough(const std::string &s) {
  std::string u = toUpper(s);
  if (u == "MANHATTAN")
    return Borough::MANHATTAN;
  if (u == "BRONX")
    return Borough::BRONX;
  if (u == "BROOKLYN")
    return Borough::BROOKLYN;
  if (u == "QUEENS")
    return Borough::QUEENS;
  if (u == "STATEN ISLAND")
    return Borough::STATEN_ISLAND;
  return Borough::UNSPECIFIED;
}

std::string boroughToString(Borough b) {
  switch (b) {
  case Borough::MANHATTAN:
    return "MANHATTAN";
  case Borough::BRONX:
    return "BRONX";
  case Borough::BROOKLYN:
    return "BROOKLYN";
  case Borough::QUEENS:
    return "QUEENS";
  case Borough::STATEN_ISLAND:
    return "STATEN ISLAND";
  case Borough::UNSPECIFIED:
    return "UNSPECIFIED";
  }
  return "UNSPECIFIED";
}

// ─── Status conversions ──────────────────────────────────────────────────────

Status stringToStatus(const std::string &s) {
  std::string u = toUpper(s);
  if (u == "OPEN")
    return Status::OPEN;
  if (u == "CLOSED")
    return Status::CLOSED;
  if (u == "PENDING")
    return Status::PENDING;
  if (u == "IN PROGRESS")
    return Status::IN_PROGRESS;
  if (u == "ASSIGNED")
    return Status::ASSIGNED;
  return Status::OPEN;
}

std::string statusToString(Status s) {
  switch (s) {
  case Status::OPEN:
    return "Open";
  case Status::CLOSED:
    return "Closed";
  case Status::PENDING:
    return "Pending";
  case Status::IN_PROGRESS:
    return "In Progress";
  case Status::ASSIGNED:
    return "Assigned";
  }
  return "Open";
}

// ─── ChannelType conversions ─────────────────────────────────────────────────

ChannelType stringToChannelType(const std::string &s) {
  std::string u = toUpper(s);
  if (u == "PHONE")
    return ChannelType::PHONE;
  if (u == "ONLINE")
    return ChannelType::ONLINE;
  if (u == "MOBILE")
    return ChannelType::MOBILE;
  if (u == "OTHER" || u == "UNKNOWN")
    return ChannelType::OTHER;
  return ChannelType::OTHER;
}

std::string channelTypeToString(ChannelType c) {
  switch (c) {
  case ChannelType::PHONE:
    return "PHONE";
  case ChannelType::ONLINE:
    return "ONLINE";
  case ChannelType::MOBILE:
    return "MOBILE";
  case ChannelType::OTHER:
    return "OTHER";
  }
  return "OTHER";
}

// ─── Memory footprint ────────────────────────────────────────────────────────

static size_t stringHeapBytes(const std::string &s) {
  // Small string optimization: strings <= ~22 chars (libstdc++/libc++) are
  // stored inline. Only count heap allocation for longer strings.
  if (s.capacity() > sizeof(std::string)) {
    return s.capacity();
  }
  return 0;
}

size_t ServiceRequest::memoryFootprint() const {
  size_t total = sizeof(ServiceRequest);

  // Add heap-allocated string bytes
  total += stringHeapBytes(agency);
  total += stringHeapBytes(agency_name);
  total += stringHeapBytes(complaint_type);
  total += stringHeapBytes(descriptor);
  total += stringHeapBytes(location_type);
  total += stringHeapBytes(incident_address);
  total += stringHeapBytes(street_name);
  total += stringHeapBytes(cross_street_1);
  total += stringHeapBytes(cross_street_2);
  total += stringHeapBytes(intersection_street_1);
  total += stringHeapBytes(intersection_street_2);
  total += stringHeapBytes(address_type);
  total += stringHeapBytes(city);
  total += stringHeapBytes(landmark);
  total += stringHeapBytes(facility_type);
  total += stringHeapBytes(resolution_description);
  total += stringHeapBytes(community_board);
  total += stringHeapBytes(park_facility_name);
  total += stringHeapBytes(vehicle_type);
  total += stringHeapBytes(taxi_company_borough);
  total += stringHeapBytes(taxi_pick_up_location);
  total += stringHeapBytes(bridge_highway_name);
  total += stringHeapBytes(bridge_highway_direction);
  total += stringHeapBytes(road_ramp);
  total += stringHeapBytes(bridge_highway_segment);

  return total;
}
