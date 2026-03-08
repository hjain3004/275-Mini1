#include "DataStoreSoA.h"
#include "CSVParser.h"
#include "DataStore.h"
#include <fstream>
#include <numeric>

// ─── Direct CSV parsing into SoA layout ──────────────────────────────────────

void DataStoreSoA::parseFromCSV(const std::string &filename) {
  clear();

  std::ifstream file(filename);
  if (!file.is_open()) {
    throw std::runtime_error("Cannot open file: " + filename);
  }

  std::string line;
  // Skip header
  if (!std::getline(file, line))
    return;

  // Initialize flat string column sentinels
  agencies_flat.reserve(1000);
  agency_names_flat.reserve(1000, 30);
  complaint_types_flat.reserve(1000, 20);
  descriptors_flat.reserve(1000, 20);
  location_types_flat.reserve(1000, 15);
  incident_addresses_flat.reserve(1000, 20);
  street_names_flat.reserve(1000, 15);
  cross_streets_1_flat.reserve(1000, 15);
  cross_streets_2_flat.reserve(1000, 15);
  cities_flat.reserve(1000, 12);
  resolution_descriptions_flat.reserve(1000, 60);
  community_boards_flat.reserve(1000, 10);
  facility_types_flat.reserve(1000, 5);

  while (std::getline(file, line)) {
    if (line.empty())
      continue;
    auto fields = CSVParser::splitCSVLine(line);

    auto getField = [&fields](size_t idx) -> const std::string & {
      static const std::string empty;
      return (idx < fields.size()) ? fields[idx] : empty;
    };

    unique_keys.push_back(CSVParser::safeParseInt64(getField(0)));
    created_dates.push_back(CSVParser::parseDate(getField(1)));
    closed_dates.push_back(CSVParser::parseDate(getField(2)));

    // Legacy string columns (for comparison benchmarks)
    agencies.push_back(getField(3));
    agency_names.push_back(getField(4));
    complaint_types.push_back(getField(5));
    descriptors.push_back(getField(6));
    location_types.push_back(getField(8));

    // Flat string columns (contiguous memory)
    agencies_flat.append(getField(3));
    agency_names_flat.append(getField(4));
    complaint_types_flat.append(getField(5));
    descriptors_flat.append(getField(6));
    location_types_flat.append(getField(8));

    incident_zips.push_back(CSVParser::safeParseInt32(getField(9)));

    // Legacy strings
    incident_addresses.push_back(getField(10));
    street_names.push_back(getField(11));
    cross_streets_1.push_back(getField(12));
    cross_streets_2.push_back(getField(13));
    cities.push_back(getField(17));
    facility_types.push_back(getField(19));

    // Flat strings
    incident_addresses_flat.append(getField(10));
    street_names_flat.append(getField(11));
    cross_streets_1_flat.append(getField(12));
    cross_streets_2_flat.append(getField(13));
    cities_flat.append(getField(17));
    facility_types_flat.append(getField(19));

    statuses.push_back(static_cast<uint8_t>(stringToStatus(getField(20))));
    due_dates.push_back(CSVParser::parseDate(getField(21)));

    resolution_descriptions.push_back(getField(22));
    resolution_descriptions_flat.append(getField(22));

    resolution_updated_dates.push_back(CSVParser::parseDate(getField(23)));

    community_boards.push_back(getField(24));
    community_boards_flat.append(getField(24));

    boroughs.push_back(static_cast<uint8_t>(stringToBorough(getField(28))));
    x_coordinates.push_back(CSVParser::safeParseInt32(getField(29)));
    y_coordinates.push_back(CSVParser::safeParseInt32(getField(30)));
    channel_types.push_back(
        static_cast<uint8_t>(stringToChannelType(getField(31))));
    latitudes.push_back(CSVParser::safeParseDouble(getField(41)));
    longitudes.push_back(CSVParser::safeParseDouble(getField(42)));
  }
}

// ─── Convert from AoS ────────────────────────────────────────────────────────

void DataStoreSoA::convertFromAoS(const DataStore &aos) {
  clear();
  size_t n = aos.size();

  // Reserve all numeric columns
  unique_keys.reserve(n);
  created_dates.reserve(n);
  closed_dates.reserve(n);
  due_dates.reserve(n);
  resolution_updated_dates.reserve(n);
  latitudes.reserve(n);
  longitudes.reserve(n);
  incident_zips.reserve(n);
  x_coordinates.reserve(n);
  y_coordinates.reserve(n);
  boroughs.reserve(n);
  statuses.reserve(n);
  channel_types.reserve(n);

  // Reserve legacy string columns
  agencies.reserve(n);
  agency_names.reserve(n);
  complaint_types.reserve(n);
  descriptors.reserve(n);
  location_types.reserve(n);
  incident_addresses.reserve(n);
  street_names.reserve(n);
  cross_streets_1.reserve(n);
  cross_streets_2.reserve(n);
  cities.reserve(n);
  resolution_descriptions.reserve(n);
  community_boards.reserve(n);
  facility_types.reserve(n);

  // Reserve flat string columns
  agencies_flat.reserve(n, 5);
  agency_names_flat.reserve(n, 30);
  complaint_types_flat.reserve(n, 20);
  descriptors_flat.reserve(n, 20);
  location_types_flat.reserve(n, 15);
  incident_addresses_flat.reserve(n, 20);
  street_names_flat.reserve(n, 15);
  cross_streets_1_flat.reserve(n, 15);
  cross_streets_2_flat.reserve(n, 15);
  cities_flat.reserve(n, 12);
  resolution_descriptions_flat.reserve(n, 60);
  community_boards_flat.reserve(n, 10);
  facility_types_flat.reserve(n, 5);

  for (size_t i = 0; i < n; ++i) {
    const auto &r = aos.getRecord(i);

    // Numeric columns
    unique_keys.push_back(r.unique_key);
    created_dates.push_back(r.created_date);
    closed_dates.push_back(r.closed_date);
    due_dates.push_back(r.due_date);
    resolution_updated_dates.push_back(r.resolution_updated_date);
    latitudes.push_back(r.latitude);
    longitudes.push_back(r.longitude);
    incident_zips.push_back(r.incident_zip);
    x_coordinates.push_back(r.x_coordinate);
    y_coordinates.push_back(r.y_coordinate);
    boroughs.push_back(static_cast<uint8_t>(r.borough));
    statuses.push_back(static_cast<uint8_t>(r.status));
    channel_types.push_back(static_cast<uint8_t>(r.channel_type));

    // Legacy string columns
    agencies.push_back(r.agency);
    agency_names.push_back(r.agency_name);
    complaint_types.push_back(r.complaint_type);
    descriptors.push_back(r.descriptor);
    location_types.push_back(r.location_type);
    incident_addresses.push_back(r.incident_address);
    street_names.push_back(r.street_name);
    cross_streets_1.push_back(r.cross_street_1);
    cross_streets_2.push_back(r.cross_street_2);
    cities.push_back(r.city);
    resolution_descriptions.push_back(r.resolution_description);
    community_boards.push_back(r.community_board);
    facility_types.push_back(r.facility_type);

    // Flat string columns
    agencies_flat.append(r.agency);
    agency_names_flat.append(r.agency_name);
    complaint_types_flat.append(r.complaint_type);
    descriptors_flat.append(r.descriptor);
    location_types_flat.append(r.location_type);
    incident_addresses_flat.append(r.incident_address);
    street_names_flat.append(r.street_name);
    cross_streets_1_flat.append(r.cross_street_1);
    cross_streets_2_flat.append(r.cross_street_2);
    cities_flat.append(r.city);
    resolution_descriptions_flat.append(r.resolution_description);
    community_boards_flat.append(r.community_board);
    facility_types_flat.append(r.facility_type);
  }
}

// ─── Size ────────────────────────────────────────────────────────────────────

size_t DataStoreSoA::size() const { return unique_keys.size(); }

// ─── Memory footprint (legacy vector<string>) ───────────────────────────────

size_t DataStoreSoA::memoryFootprint() const {
  size_t total = sizeof(DataStoreSoA);

  // Fixed-size vectors: capacity * element_size
  total += unique_keys.capacity() * sizeof(int64_t);
  total += created_dates.capacity() * sizeof(time_t);
  total += closed_dates.capacity() * sizeof(time_t);
  total += due_dates.capacity() * sizeof(time_t);
  total += resolution_updated_dates.capacity() * sizeof(time_t);
  total += latitudes.capacity() * sizeof(double);
  total += longitudes.capacity() * sizeof(double);
  total += incident_zips.capacity() * sizeof(int32_t);
  total += x_coordinates.capacity() * sizeof(int32_t);
  total += y_coordinates.capacity() * sizeof(int32_t);
  total += boroughs.capacity() * sizeof(uint8_t);
  total += statuses.capacity() * sizeof(uint8_t);
  total += channel_types.capacity() * sizeof(uint8_t);

  // String vectors: capacity * sizeof(string) + heap allocations
  auto stringVecBytes = [](const std::vector<std::string> &v) -> size_t {
    size_t bytes = v.capacity() * sizeof(std::string);
    for (const auto &s : v) {
      if (s.capacity() > sizeof(std::string)) {
        bytes += s.capacity();
      }
    }
    return bytes;
  };

  total += stringVecBytes(agencies);
  total += stringVecBytes(agency_names);
  total += stringVecBytes(complaint_types);
  total += stringVecBytes(descriptors);
  total += stringVecBytes(location_types);
  total += stringVecBytes(incident_addresses);
  total += stringVecBytes(street_names);
  total += stringVecBytes(cross_streets_1);
  total += stringVecBytes(cross_streets_2);
  total += stringVecBytes(cities);
  total += stringVecBytes(resolution_descriptions);
  total += stringVecBytes(community_boards);
  total += stringVecBytes(facility_types);

  return total;
}

// ─── Memory footprint (flat string columns) ──────────────────────────────────

size_t DataStoreSoA::memoryFootprintFlat() const {
  size_t total = sizeof(DataStoreSoA);

  // Fixed-size numeric vectors (same as above)
  total += unique_keys.capacity() * sizeof(int64_t);
  total += created_dates.capacity() * sizeof(time_t);
  total += closed_dates.capacity() * sizeof(time_t);
  total += due_dates.capacity() * sizeof(time_t);
  total += resolution_updated_dates.capacity() * sizeof(time_t);
  total += latitudes.capacity() * sizeof(double);
  total += longitudes.capacity() * sizeof(double);
  total += incident_zips.capacity() * sizeof(int32_t);
  total += x_coordinates.capacity() * sizeof(int32_t);
  total += y_coordinates.capacity() * sizeof(int32_t);
  total += boroughs.capacity() * sizeof(uint8_t);
  total += statuses.capacity() * sizeof(uint8_t);
  total += channel_types.capacity() * sizeof(uint8_t);

  // Flat string columns: offsets array + contiguous buffer
  total += agencies_flat.memoryFootprint();
  total += agency_names_flat.memoryFootprint();
  total += complaint_types_flat.memoryFootprint();
  total += descriptors_flat.memoryFootprint();
  total += location_types_flat.memoryFootprint();
  total += incident_addresses_flat.memoryFootprint();
  total += street_names_flat.memoryFootprint();
  total += cross_streets_1_flat.memoryFootprint();
  total += cross_streets_2_flat.memoryFootprint();
  total += cities_flat.memoryFootprint();
  total += resolution_descriptions_flat.memoryFootprint();
  total += community_boards_flat.memoryFootprint();
  total += facility_types_flat.memoryFootprint();

  return total;
}

// ─── Memory footprint with string interning ──────────────────────────────────

size_t DataStoreSoA::memoryFootprintWithInterning() const {
  size_t total = memoryFootprintFlat();

  // Add interned representation: vector<uint16_t> + lookup table
  total += complaint_type_indices.capacity() * sizeof(uint16_t);
  for (const auto &s : complaint_type_lookup) {
    total += sizeof(std::string);
    if (s.capacity() > sizeof(std::string)) {
      total += s.capacity();
    }
  }

  return total;
}

// ─── String interning ────────────────────────────────────────────────────────

void DataStoreSoA::buildComplaintTypeInterning() {
  complaint_type_intern.clear();
  complaint_type_lookup.clear();
  complaint_type_indices.clear();
  complaint_type_indices.reserve(complaint_types.size());

  for (const auto &ct : complaint_types) {
    auto it = complaint_type_intern.find(ct);
    if (it == complaint_type_intern.end()) {
      uint16_t idx = static_cast<uint16_t>(complaint_type_lookup.size());
      complaint_type_intern[ct] = idx;
      complaint_type_lookup.push_back(ct);
      complaint_type_indices.push_back(idx);
    } else {
      complaint_type_indices.push_back(it->second);
    }
  }
}

// ─── Flat string query (contiguous buffer scan) ──────────────────────────────

std::vector<size_t>
DataStoreSoA::queryByComplaintTypeFlat(const std::string &type) const {
  std::vector<size_t> results;
  size_t n = complaint_types_flat.size();
  for (size_t i = 0; i < n; ++i) {
    if (complaint_types_flat.equals(i, type)) {
      results.push_back(i);
    }
  }
  return results;
}

// ─── Clear ───────────────────────────────────────────────────────────────────

void DataStoreSoA::clear() {
  unique_keys.clear();
  created_dates.clear();
  closed_dates.clear();
  due_dates.clear();
  resolution_updated_dates.clear();
  latitudes.clear();
  longitudes.clear();
  incident_zips.clear();
  x_coordinates.clear();
  y_coordinates.clear();
  boroughs.clear();
  statuses.clear();
  channel_types.clear();

  agencies.clear();
  agency_names.clear();
  complaint_types.clear();
  descriptors.clear();
  location_types.clear();
  incident_addresses.clear();
  street_names.clear();
  cross_streets_1.clear();
  cross_streets_2.clear();
  cities.clear();
  resolution_descriptions.clear();
  community_boards.clear();
  facility_types.clear();

  agencies_flat.clear();
  agency_names_flat.clear();
  complaint_types_flat.clear();
  descriptors_flat.clear();
  location_types_flat.clear();
  incident_addresses_flat.clear();
  street_names_flat.clear();
  cross_streets_1_flat.clear();
  cross_streets_2_flat.clear();
  cities_flat.clear();
  resolution_descriptions_flat.clear();
  community_boards_flat.clear();
  facility_types_flat.clear();

  complaint_type_intern.clear();
  complaint_type_lookup.clear();
  complaint_type_indices.clear();
}
