#pragma once

#include "DataStore.h"
#include "ServiceRequest.h"
#include <ctime>
#include <string>
#include <vector>

// Forward declaration
class DataStoreSoA;

/**
 * QueryEngine — range search APIs for 311 data.
 *
 * All queries return std::vector<size_t> (indices into the data store).
 * This design works for both AoS (DataStore) and SoA (DataStoreSoA) layouts.
 *
 * Phase 1: serial scan
 * Phase 2: parallel scan with OpenMP (scatter-gather pattern)
 */
class QueryEngine {
public:
  // ─── AoS (DataStore) queries — Phase 1 serial ──────────────────────────

  static std::vector<size_t> queryByDateRange(const DataStore &store,
                                              time_t start, time_t end);

  static std::vector<size_t> queryByBorough(const DataStore &store,
                                            Borough borough);

  static std::vector<size_t> queryByGeoBoundingBox(const DataStore &store,
                                                   double minLat, double maxLat,
                                                   double minLon,
                                                   double maxLon);

  static std::vector<size_t> queryByComplaintType(const DataStore &store,
                                                  const std::string &type);

  static std::vector<size_t> compositeQuery(const DataStore &store,
                                            time_t startDate, time_t endDate,
                                            Borough borough,
                                            const std::string &complaintType);

#if defined(HAS_OPENMP)
  // ─── AoS (DataStore) queries — Phase 2 parallel (scatter-gather) ───────

  static std::vector<size_t> queryByDateRangeParallel(const DataStore &store,
                                                      time_t start, time_t end);

  static std::vector<size_t> queryByBoroughParallel(const DataStore &store,
                                                    Borough borough);

  static std::vector<size_t>
  queryByGeoBoundingBoxParallel(const DataStore &store, double minLat,
                                double maxLat, double minLon, double maxLon);

  static std::vector<size_t>
  queryByComplaintTypeParallel(const DataStore &store, const std::string &type);

  static std::vector<size_t>
  compositeQueryParallel(const DataStore &store, time_t startDate,
                         time_t endDate, Borough borough,
                         const std::string &complaintType);
#endif

  // ─── SoA (DataStoreSoA) queries — Phase 3 ─────────────────────────────

  static std::vector<size_t> queryByDateRange(const DataStoreSoA &store,
                                              time_t start, time_t end);

  static std::vector<size_t> queryByBorough(const DataStoreSoA &store,
                                            uint8_t borough);

  static std::vector<size_t> queryByGeoBoundingBox(const DataStoreSoA &store,
                                                   double minLat, double maxLat,
                                                   double minLon,
                                                   double maxLon);

  static std::vector<size_t> queryByComplaintType(const DataStoreSoA &store,
                                                  const std::string &type);

  static std::vector<size_t> compositeQuery(const DataStoreSoA &store,
                                            time_t startDate, time_t endDate,
                                            uint8_t borough,
                                            const std::string &complaintType);

#if defined(HAS_OPENMP)
  // ─── SoA parallel queries ──────────────────────────────────────────────

  static std::vector<size_t> queryByDateRangeParallel(const DataStoreSoA &store,
                                                      time_t start, time_t end);

  static std::vector<size_t> queryByBoroughParallel(const DataStoreSoA &store,
                                                    uint8_t borough);

  static std::vector<size_t>
  queryByGeoBoundingBoxParallel(const DataStoreSoA &store, double minLat,
                                double maxLat, double minLon, double maxLon);

  static std::vector<size_t>
  queryByComplaintTypeParallel(const DataStoreSoA &store,
                               const std::string &type);

  static std::vector<size_t>
  compositeQueryParallel(const DataStoreSoA &store, time_t startDate,
                         time_t endDate, uint8_t borough,
                         const std::string &complaintType);
#endif
};
