#include "QueryEngine.h"
#include "DataStoreSoA.h"

#if defined(HAS_OPENMP)
#include <omp.h>
#endif

// ═══════════════════════════════════════════════════════════════════════════════
// Phase 1: AoS Serial Queries
// ═══════════════════════════════════════════════════════════════════════════════

std::vector<size_t> QueryEngine::queryByDateRange(const DataStore &store,
                                                  time_t start, time_t end) {
  std::vector<size_t> results;
  const auto &records = store.getRecords();
  for (size_t i = 0; i < records.size(); ++i) {
    if (records[i].created_date >= start && records[i].created_date <= end) {
      results.push_back(i);
    }
  }
  return results;
}

std::vector<size_t> QueryEngine::queryByBorough(const DataStore &store,
                                                Borough borough) {
  std::vector<size_t> results;
  const auto &records = store.getRecords();
  for (size_t i = 0; i < records.size(); ++i) {
    if (records[i].borough == borough) {
      results.push_back(i);
    }
  }
  return results;
}

std::vector<size_t> QueryEngine::queryByGeoBoundingBox(const DataStore &store,
                                                       double minLat,
                                                       double maxLat,
                                                       double minLon,
                                                       double maxLon) {
  std::vector<size_t> results;
  const auto &records = store.getRecords();
  for (size_t i = 0; i < records.size(); ++i) {
    double lat = records[i].latitude;
    double lon = records[i].longitude;
    if (lat >= minLat && lat <= maxLat && lon >= minLon && lon <= maxLon) {
      results.push_back(i);
    }
  }
  return results;
}

std::vector<size_t> QueryEngine::queryByComplaintType(const DataStore &store,
                                                      const std::string &type) {
  std::vector<size_t> results;
  const auto &records = store.getRecords();
  for (size_t i = 0; i < records.size(); ++i) {
    if (records[i].complaint_type == type) {
      results.push_back(i);
    }
  }
  return results;
}

std::vector<size_t>
QueryEngine::compositeQuery(const DataStore &store, time_t startDate,
                            time_t endDate, Borough borough,
                            const std::string &complaintType) {
  std::vector<size_t> results;
  const auto &records = store.getRecords();
  for (size_t i = 0; i < records.size(); ++i) {
    const auto &r = records[i];
    if (r.created_date >= startDate && r.created_date <= endDate &&
        r.borough == borough && r.complaint_type == complaintType) {
      results.push_back(i);
    }
  }
  return results;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Phase 2: AoS Parallel Queries (scatter-gather pattern from Gash lectures)
// ═══════════════════════════════════════════════════════════════════════════════

#if defined(HAS_OPENMP)

std::vector<size_t>
QueryEngine::queryByDateRangeParallel(const DataStore &store, time_t start,
                                      time_t end) {
  std::vector<size_t> results;
  const auto &records = store.getRecords();
  size_t n = records.size();

#pragma omp parallel
  {
    std::vector<size_t> local_results;
#pragma omp for nowait
    for (size_t i = 0; i < n; ++i) {
      if (records[i].created_date >= start && records[i].created_date <= end) {
        local_results.push_back(i);
      }
    }
#pragma omp critical
    results.insert(results.end(), local_results.begin(), local_results.end());
  }
  return results;
}

std::vector<size_t> QueryEngine::queryByBoroughParallel(const DataStore &store,
                                                        Borough borough) {
  std::vector<size_t> results;
  const auto &records = store.getRecords();
  size_t n = records.size();

#pragma omp parallel
  {
    std::vector<size_t> local_results;
#pragma omp for nowait
    for (size_t i = 0; i < n; ++i) {
      if (records[i].borough == borough) {
        local_results.push_back(i);
      }
    }
#pragma omp critical
    results.insert(results.end(), local_results.begin(), local_results.end());
  }
  return results;
}

std::vector<size_t>
QueryEngine::queryByGeoBoundingBoxParallel(const DataStore &store,
                                           double minLat, double maxLat,
                                           double minLon, double maxLon) {
  std::vector<size_t> results;
  const auto &records = store.getRecords();
  size_t n = records.size();

#pragma omp parallel
  {
    std::vector<size_t> local_results;
#pragma omp for nowait
    for (size_t i = 0; i < n; ++i) {
      double lat = records[i].latitude;
      double lon = records[i].longitude;
      if (lat >= minLat && lat <= maxLat && lon >= minLon && lon <= maxLon) {
        local_results.push_back(i);
      }
    }
#pragma omp critical
    results.insert(results.end(), local_results.begin(), local_results.end());
  }
  return results;
}

std::vector<size_t>
QueryEngine::queryByComplaintTypeParallel(const DataStore &store,
                                          const std::string &type) {
  std::vector<size_t> results;
  const auto &records = store.getRecords();
  size_t n = records.size();

#pragma omp parallel
  {
    std::vector<size_t> local_results;
#pragma omp for nowait
    for (size_t i = 0; i < n; ++i) {
      if (records[i].complaint_type == type) {
        local_results.push_back(i);
      }
    }
#pragma omp critical
    results.insert(results.end(), local_results.begin(), local_results.end());
  }
  return results;
}

std::vector<size_t>
QueryEngine::compositeQueryParallel(const DataStore &store, time_t startDate,
                                    time_t endDate, Borough borough,
                                    const std::string &complaintType) {
  std::vector<size_t> results;
  const auto &records = store.getRecords();
  size_t n = records.size();

#pragma omp parallel
  {
    std::vector<size_t> local_results;
#pragma omp for nowait
    for (size_t i = 0; i < n; ++i) {
      const auto &r = records[i];
      if (r.created_date >= startDate && r.created_date <= endDate &&
          r.borough == borough && r.complaint_type == complaintType) {
        local_results.push_back(i);
      }
    }
#pragma omp critical
    results.insert(results.end(), local_results.begin(), local_results.end());
  }
  return results;
}

#endif // HAS_OPENMP

// ═══════════════════════════════════════════════════════════════════════════════
// Phase 3: SoA Serial Queries — scan ONLY relevant columns
// ═══════════════════════════════════════════════════════════════════════════════

std::vector<size_t> QueryEngine::queryByDateRange(const DataStoreSoA &store,
                                                  time_t start, time_t end) {
  std::vector<size_t> results;
  for (size_t i = 0; i < store.created_dates.size(); ++i) {
    if (store.created_dates[i] >= start && store.created_dates[i] <= end) {
      results.push_back(i);
    }
  }
  return results;
}

std::vector<size_t> QueryEngine::queryByBorough(const DataStoreSoA &store,
                                                uint8_t borough) {
  std::vector<size_t> results;
  for (size_t i = 0; i < store.boroughs.size(); ++i) {
    if (store.boroughs[i] == borough) {
      results.push_back(i);
    }
  }
  return results;
}

std::vector<size_t>
QueryEngine::queryByGeoBoundingBox(const DataStoreSoA &store, double minLat,
                                   double maxLat, double minLon,
                                   double maxLon) {
  std::vector<size_t> results;
  for (size_t i = 0; i < store.latitudes.size(); ++i) {
    if (store.latitudes[i] >= minLat && store.latitudes[i] <= maxLat &&
        store.longitudes[i] >= minLon && store.longitudes[i] <= maxLon) {
      results.push_back(i);
    }
  }
  return results;
}

std::vector<size_t> QueryEngine::queryByComplaintType(const DataStoreSoA &store,
                                                      const std::string &type) {
  std::vector<size_t> results;
  for (size_t i = 0; i < store.complaint_types.size(); ++i) {
    if (store.complaint_types[i] == type) {
      results.push_back(i);
    }
  }
  return results;
}

std::vector<size_t>
QueryEngine::compositeQuery(const DataStoreSoA &store, time_t startDate,
                            time_t endDate, uint8_t borough,
                            const std::string &complaintType) {
  std::vector<size_t> results;
  for (size_t i = 0; i < store.created_dates.size(); ++i) {
    if (store.created_dates[i] >= startDate &&
        store.created_dates[i] <= endDate && store.boroughs[i] == borough &&
        store.complaint_types[i] == complaintType) {
      results.push_back(i);
    }
  }
  return results;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Phase 3 + Phase 2: SoA Parallel Queries
// ═══════════════════════════════════════════════════════════════════════════════

#if defined(HAS_OPENMP)

std::vector<size_t>
QueryEngine::queryByDateRangeParallel(const DataStoreSoA &store, time_t start,
                                      time_t end) {
  std::vector<size_t> results;
  size_t n = store.created_dates.size();

#pragma omp parallel
  {
    std::vector<size_t> local_results;
#pragma omp for nowait
    for (size_t i = 0; i < n; ++i) {
      if (store.created_dates[i] >= start && store.created_dates[i] <= end) {
        local_results.push_back(i);
      }
    }
#pragma omp critical
    results.insert(results.end(), local_results.begin(), local_results.end());
  }
  return results;
}

std::vector<size_t>
QueryEngine::queryByBoroughParallel(const DataStoreSoA &store,
                                    uint8_t borough) {
  std::vector<size_t> results;
  size_t n = store.boroughs.size();

#pragma omp parallel
  {
    std::vector<size_t> local_results;
#pragma omp for nowait
    for (size_t i = 0; i < n; ++i) {
      if (store.boroughs[i] == borough) {
        local_results.push_back(i);
      }
    }
#pragma omp critical
    results.insert(results.end(), local_results.begin(), local_results.end());
  }
  return results;
}

std::vector<size_t>
QueryEngine::queryByGeoBoundingBoxParallel(const DataStoreSoA &store,
                                           double minLat, double maxLat,
                                           double minLon, double maxLon) {
  std::vector<size_t> results;
  size_t n = store.latitudes.size();

#pragma omp parallel
  {
    std::vector<size_t> local_results;
#pragma omp for nowait
    for (size_t i = 0; i < n; ++i) {
      if (store.latitudes[i] >= minLat && store.latitudes[i] <= maxLat &&
          store.longitudes[i] >= minLon && store.longitudes[i] <= maxLon) {
        local_results.push_back(i);
      }
    }
#pragma omp critical
    results.insert(results.end(), local_results.begin(), local_results.end());
  }
  return results;
}

std::vector<size_t>
QueryEngine::queryByComplaintTypeParallel(const DataStoreSoA &store,
                                          const std::string &type) {
  std::vector<size_t> results;
  size_t n = store.complaint_types.size();

#pragma omp parallel
  {
    std::vector<size_t> local_results;
#pragma omp for nowait
    for (size_t i = 0; i < n; ++i) {
      if (store.complaint_types[i] == type) {
        local_results.push_back(i);
      }
    }
#pragma omp critical
    results.insert(results.end(), local_results.begin(), local_results.end());
  }
  return results;
}

std::vector<size_t>
QueryEngine::compositeQueryParallel(const DataStoreSoA &store, time_t startDate,
                                    time_t endDate, uint8_t borough,
                                    const std::string &complaintType) {
  std::vector<size_t> results;
  size_t n = store.created_dates.size();

#pragma omp parallel
  {
    std::vector<size_t> local_results;
#pragma omp for nowait
    for (size_t i = 0; i < n; ++i) {
      if (store.created_dates[i] >= startDate &&
          store.created_dates[i] <= endDate && store.boroughs[i] == borough &&
          store.complaint_types[i] == complaintType) {
        local_results.push_back(i);
      }
    }
#pragma omp critical
    results.insert(results.end(), local_results.begin(), local_results.end());
  }
  return results;
}

#endif // HAS_OPENMP
