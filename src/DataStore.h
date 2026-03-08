#pragma once

#include "ServiceRequest.h"
#include <cstddef>
#include <vector>

/**
 * DataStore — AoS (Array of Structures) container for ServiceRequest records.
 * Phase 1 data layout: std::vector<ServiceRequest>.
 */
class DataStore {
public:
  DataStore() = default;

  /** Add a record to the store */
  void addRecord(ServiceRequest &&record);
  void addRecord(const ServiceRequest &record);

  /** Pre-allocate capacity (for reserve() vs. dynamic growth experiments) */
  void reserve(size_t n);

  /** Number of records */
  size_t size() const;

  /** Access individual records */
  const ServiceRequest &getRecord(size_t index) const;
  ServiceRequest &getRecord(size_t index);

  /** Access the underlying vector (for iterating in queries) */
  const std::vector<ServiceRequest> &getRecords() const;
  std::vector<ServiceRequest> &getRecords();

  /**
   * Approximate total memory footprint in bytes.
   * Includes sizeof(ServiceRequest) * N + string heap overhead.
   */
  size_t memoryFootprint() const;

  /** Clear all records */
  void clear();

private:
  std::vector<ServiceRequest> records_;
};
