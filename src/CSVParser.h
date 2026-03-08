#pragma once

#include "DataStore.h"
#include "ServiceRequest.h"
#include <string>
#include <vector>

// Forward declaration for Phase 3
class DataStoreSoA;

/**
 * CSVParser — RFC-4180 compliant CSV parser for NYC 311 data.
 *
 * Handles:
 *   - Quoted fields with embedded commas (e.g., Resolution Description)
 *   - Escaped quotes ("" inside quoted fields)
 *   - Empty/null fields → default values
 *   - Dual date format: ISO 8601 and MM/DD/YYYY
 *
 * Design note: State-machine approach for correctness over speed.
 * Phase 2 adds parseFileParallel() for OpenMP parallelization.
 */
class CSVParser {
public:
  CSVParser() = default;

  /** Parse CSV file into AoS DataStore */
  DataStore parseFile(const std::string &filename);

  /** Parse CSV file with pre-reserved capacity (for E1.3 experiment) */
  DataStore parseFileWithReserve(const std::string &filename,
                                 size_t reserveCount);

#if defined(HAS_OPENMP)
  /** Phase 2: Parallel CSV parsing — read file, chunk, parallel parse, merge */
  DataStore parseFileParallel(const std::string &filename);
#endif

  /** Parse a single CSV line into a ServiceRequest */
  ServiceRequest parseLine(const std::vector<std::string> &fields);

  /** Split a CSV line into fields using RFC-4180 state machine */
  static std::vector<std::string> splitCSVLine(const std::string &line);

  /** Parse date string to time_t (handles ISO 8601 and MM/DD/YYYY formats) */
  static time_t parseDate(const std::string &dateStr);

  /** Safe integer parsing (returns 0 on failure) */
  static int64_t safeParseInt64(const std::string &s);
  static int32_t safeParseInt32(const std::string &s);

  /** Safe double parsing (returns 0.0 on failure) */
  static double safeParseDouble(const std::string &s);

  /** Get the last recorded number of lines parsed */
  size_t getLastLineCount() const { return lastLineCount_; }

private:
  size_t lastLineCount_ = 0;
};
