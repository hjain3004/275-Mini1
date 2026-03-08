#pragma once

#include "ServiceRequest.h"
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

// Forward declaration
class DataStore;

// ═══════════════════════════════════════════════════════════════════════════════
// FlatStringColumn — contiguous string storage for improved data locality
// ═══════════════════════════════════════════════════════════════════════════════
//
// Problem with vector<string>:
//   Each std::string allocates its own heap buffer. For 40M records, that's
//   40M separate malloc() calls, resulting in scattered memory pages.
//   The CPU prefetcher cannot predict these random jumps.
//
// Solution: Store ALL string data for a column in ONE contiguous char buffer.
//   offsets[i] = start position of string i in the buffer
//   offsets[i+1] - offsets[i] = length of string i
//   The CPU prefetcher can stride through the offset array efficiently,
//   and string data is physically adjacent in memory.
//
// Memory layout:
//   offsets:  [0, 3, 8, 15, ...]     ← contiguous uint32_t array
//   buffer:   "DOTBROOKLYNQUEENS..." ← one contiguous char block
//
// ═══════════════════════════════════════════════════════════════════════════════

struct FlatStringColumn {
  std::vector<uint32_t> offsets; // offsets[i] = start of string i
  std::vector<char> buffer;      // contiguous character data

  /** Reserve space for n strings with estimated avg length */
  void reserve(size_t n, size_t avgLen = 16) {
    offsets.reserve(n + 1);
    buffer.reserve(n * avgLen);
    if (offsets.empty()) {
      offsets.push_back(0); // sentinel: first string starts at 0
    }
  }

  /** Append a string to the column */
  void append(const std::string &s) {
    if (offsets.empty()) {
      offsets.push_back(0);
    }
    buffer.insert(buffer.end(), s.begin(), s.end());
    offsets.push_back(static_cast<uint32_t>(buffer.size()));
  }

  /** Get string at index i (returns a string_view-like result) */
  std::string get(size_t i) const {
    uint32_t start = offsets[i];
    uint32_t len = offsets[i + 1] - start;
    return std::string(buffer.data() + start, len);
  }

  /** Get raw pointer + length at index i (zero-copy for comparisons) */
  const char *data(size_t i) const { return buffer.data() + offsets[i]; }
  uint32_t length(size_t i) const { return offsets[i + 1] - offsets[i]; }

  /** Compare string at index i against a target (avoids constructing
   * std::string) */
  bool equals(size_t i, const std::string &target) const {
    uint32_t len = offsets[i + 1] - offsets[i];
    if (len != target.size())
      return false;
    return std::memcmp(buffer.data() + offsets[i], target.data(), len) == 0;
  }

  /** Number of strings stored */
  size_t size() const { return offsets.empty() ? 0 : offsets.size() - 1; }

  /** Approximate memory footprint in bytes */
  size_t memoryFootprint() const {
    return sizeof(FlatStringColumn) + offsets.capacity() * sizeof(uint32_t) +
           buffer.capacity() * sizeof(char);
  }

  /** Clear all data */
  void clear() {
    offsets.clear();
    buffer.clear();
  }
};

// ═══════════════════════════════════════════════════════════════════════════════
// DataStoreSoA — Structure of Arrays (columnar) layout for 311 data
// ═══════════════════════════════════════════════════════════════════════════════
//
// Data locality strategy:
//
// 1. NUMERIC COLUMNS: Each stored in its own std::vector<T>.
//    vector<time_t> created_dates is one contiguous block of 8-byte values.
//    Scanning 40M dates touches 40M×8B = 320MB of tightly packed data.
//    The CPU prefetcher excels at sequential stride access.
//
// 2. ENUM-ENCODED COLUMNS: vector<uint8_t> for Borough, Status, ChannelType.
//    6 possible boroughs stored as 1 byte each instead of strings.
//    40M × 1B = 40MB vs 40M × (32+avg9) = 1.6GB — 40x memory savings.
//
// 3. FLAT STRING COLUMNS: FlatStringColumn (contiguous buffer + offsets).
//    All string data for a column in ONE malloc, not 40M separate ones.
//    The offset array enables O(1) indexing without pointer chasing.
//
// 4. STRING INTERNING: For low-cardinality fields (~200 complaint types),
//    store a uint16_t index instead of the full string. Queries compare
//    2-byte integers instead of variable-length strings.
//
// Contrast with AoS (DataStore):
//    vector<ServiceRequest> is contiguous for the STRUCTS, but each struct
//    contains 25 std::string members → 25 separate heap allocations per row.
//    40M rows × 25 strings = 1 BILLION scattered heap allocations.
//    A date-range query loads 680-byte structs into cache but only reads
//    8 bytes (the date field) — 98.8% cache waste.
//
// ═══════════════════════════════════════════════════════════════════════════════

class DataStoreSoA {
public:
  // ─── Fixed-size numeric columns (contiguous in memory) ────────────────

  std::vector<int64_t> unique_keys;
  std::vector<time_t> created_dates;
  std::vector<time_t> closed_dates;
  std::vector<time_t> due_dates;
  std::vector<time_t> resolution_updated_dates;
  std::vector<double> latitudes;
  std::vector<double> longitudes;
  std::vector<int32_t> incident_zips;
  std::vector<int32_t> x_coordinates;
  std::vector<int32_t> y_coordinates;
  std::vector<uint8_t> boroughs;      // enum-encoded → 1 byte, not 9+ chars
  std::vector<uint8_t> statuses;      // enum-encoded
  std::vector<uint8_t> channel_types; // enum-encoded

  // ─── Flat string columns (contiguous buffer, zero-copy compare) ───────

  FlatStringColumn agencies_flat;
  FlatStringColumn agency_names_flat;
  FlatStringColumn complaint_types_flat;
  FlatStringColumn descriptors_flat;
  FlatStringColumn location_types_flat;
  FlatStringColumn incident_addresses_flat;
  FlatStringColumn street_names_flat;
  FlatStringColumn cross_streets_1_flat;
  FlatStringColumn cross_streets_2_flat;
  FlatStringColumn cities_flat;
  FlatStringColumn resolution_descriptions_flat;
  FlatStringColumn community_boards_flat;
  FlatStringColumn facility_types_flat;

  // ─── Legacy string columns (kept for comparison experiments) ──────────

  std::vector<std::string> agencies;
  std::vector<std::string> agency_names;
  std::vector<std::string> complaint_types;
  std::vector<std::string> descriptors;
  std::vector<std::string> location_types;
  std::vector<std::string> incident_addresses;
  std::vector<std::string> street_names;
  std::vector<std::string> cross_streets_1;
  std::vector<std::string> cross_streets_2;
  std::vector<std::string> cities;
  std::vector<std::string> resolution_descriptions;
  std::vector<std::string> community_boards;
  std::vector<std::string> facility_types;

  // ─── String interning (Phase 3 optimization) ──────────────────────────
  // Maps string → index for medium-cardinality fields
  std::unordered_map<std::string, uint16_t> complaint_type_intern;
  std::vector<std::string> complaint_type_lookup;
  std::vector<uint16_t> complaint_type_indices; // interned indices

  // ─── Core methods ─────────────────────────────────────────────────────

  /** Build directly from CSV file (fair comparison with AoS parse time) */
  void parseFromCSV(const std::string &filename);

  /** Convert from AoS layout (for comparison experiments) */
  void convertFromAoS(const DataStore &aos);

  /** Number of records */
  size_t size() const;

  /** Approximate memory footprint in bytes (using vector<string>) */
  size_t memoryFootprint() const;

  /** Memory footprint using flat string columns */
  size_t memoryFootprintFlat() const;

  /** Memory footprint with string interning applied */
  size_t memoryFootprintWithInterning() const;

  /** Build the string intern table for complaint_types */
  void buildComplaintTypeInterning();

  // ─── Flat-string-specific query (scans contiguous buffer) ─────────────

  std::vector<size_t> queryByComplaintTypeFlat(const std::string &type) const;

  /** Clear all data */
  void clear();
};
