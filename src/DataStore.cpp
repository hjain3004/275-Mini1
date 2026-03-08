#include "DataStore.h"

void DataStore::addRecord(ServiceRequest &&record) {
  records_.push_back(std::move(record));
}

void DataStore::addRecord(const ServiceRequest &record) {
  records_.push_back(record);
}

void DataStore::reserve(size_t n) { records_.reserve(n); }

size_t DataStore::size() const { return records_.size(); }

const ServiceRequest &DataStore::getRecord(size_t index) const {
  return records_[index];
}

ServiceRequest &DataStore::getRecord(size_t index) { return records_[index]; }

const std::vector<ServiceRequest> &DataStore::getRecords() const {
  return records_;
}

std::vector<ServiceRequest> &DataStore::getRecords() { return records_; }

size_t DataStore::memoryFootprint() const {
  size_t total = sizeof(DataStore);
  total +=
      records_.capacity() * sizeof(ServiceRequest); // vector backing storage

  // Add string heap overhead for each record
  for (const auto &r : records_) {
    total += r.memoryFootprint() -
             sizeof(ServiceRequest); // avoid double-counting sizeof
  }

  return total;
}

void DataStore::clear() {
  records_.clear();
  records_.shrink_to_fit();
}
