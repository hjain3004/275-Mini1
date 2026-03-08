#pragma once

#include <cstdint>
#include <ctime>
#include <string>

// ─── Enums (uint8_t underlying)
// ───────────────────────────────────────────────

enum class Borough : uint8_t {
  MANHATTAN = 0,
  BRONX = 1,
  BROOKLYN = 2,
  QUEENS = 3,
  STATEN_ISLAND = 4,
  UNSPECIFIED = 5
};

enum class Status : uint8_t {
  OPEN = 0,
  CLOSED = 1,
  PENDING = 2,
  IN_PROGRESS = 3,
  ASSIGNED = 4
};

enum class ChannelType : uint8_t {
  PHONE = 0,
  ONLINE = 1,
  MOBILE = 2,
  OTHER = 3
};

// ─── String ↔ Enum conversion helpers
// ─────────────────────────────────────────

Borough stringToBorough(const std::string &s);
std::string boroughToString(Borough b);

Status stringToStatus(const std::string &s);
std::string statusToString(Status s);

ChannelType stringToChannelType(const std::string &s);
std::string channelTypeToString(ChannelType c);

// ─── IRecord — virtual base class (OO design requirement)
// ─────────────────────

class IRecord {
public:
  virtual ~IRecord() = default;
  virtual int64_t getUniqueKey() const = 0;
  virtual time_t getCreatedDate() const = 0;
  virtual Borough getBorough() const = 0;
  virtual const std::string &getComplaintType() const = 0;
  virtual double getLatitude() const = 0;
  virtual double getLongitude() const = 0;
};

// ─── ServiceRequest — one row, all fields as primitive types
// ──────────────────

class ServiceRequest : public IRecord {
public:
  // Fixed-size fields
  int64_t unique_key = 0;
  time_t created_date = 0;
  time_t closed_date = 0;
  time_t due_date = 0;
  time_t resolution_updated_date = 0;
  double latitude = 0.0;
  double longitude = 0.0;
  int32_t incident_zip = 0;
  int32_t x_coordinate = 0;
  int32_t y_coordinate = 0;
  Borough borough = Borough::UNSPECIFIED;
  Status status = Status::OPEN;
  ChannelType channel_type = ChannelType::OTHER;

  // Variable-length fields
  std::string agency;
  std::string agency_name;
  std::string complaint_type;
  std::string descriptor;
  std::string location_type;
  std::string incident_address;
  std::string street_name;
  std::string cross_street_1;
  std::string cross_street_2;
  std::string intersection_street_1;
  std::string intersection_street_2;
  std::string address_type;
  std::string city;
  std::string landmark;
  std::string facility_type;
  std::string resolution_description;
  std::string community_board;
  std::string park_facility_name;
  std::string vehicle_type;
  std::string taxi_company_borough;
  std::string taxi_pick_up_location;
  std::string bridge_highway_name;
  std::string bridge_highway_direction;
  std::string road_ramp;
  std::string bridge_highway_segment;

  // IRecord interface
  int64_t getUniqueKey() const override { return unique_key; }
  time_t getCreatedDate() const override { return created_date; }
  Borough getBorough() const override { return borough; }
  const std::string &getComplaintType() const override {
    return complaint_type;
  }
  double getLatitude() const override { return latitude; }
  double getLongitude() const override { return longitude; }

  /**
   * Approximate memory footprint of this object in bytes.
   * Counts sizeof(ServiceRequest) + string heap allocations.
   */
  size_t memoryFootprint() const;
};
