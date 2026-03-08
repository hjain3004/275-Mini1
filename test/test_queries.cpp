#include "CSVParser.h"
#include "DataStore.h"
#include "DataStoreSoA.h"
#include "QueryEngine.h"

#include <cassert>
#include <cstring>
#include <iostream>

// Shared test framework (declared in test_parser.cpp)
extern int runParserTests(int argc, char *argv[]);

static int tests_passed = 0;
static int tests_failed = 0;

#define QTEST(name)                                                            \
  std::cout << "  TEST: " << name << "... ";                                   \
  try {

#define QEND_TEST                                                              \
  std::cout << "PASSED\n";                                                     \
  tests_passed++;                                                              \
  }                                                                            \
  catch (const std::exception &e) {                                            \
    std::cout << "FAILED: " << e.what() << "\n";                               \
    tests_failed++;                                                            \
  }

#define QASSERT_TRUE(x)                                                        \
  if (!(x)) {                                                                  \
    throw std::runtime_error("Assertion failed: " #x);                         \
  }

#define QASSERT_EQ(a, b)                                                       \
  if ((a) != (b)) {                                                            \
    throw std::runtime_error("Expected " + std::to_string(b) + " but got " +   \
                             std::to_string(a));                               \
  }

void testQueries(const std::string &filename) {
  std::cout
      << "═══════════════════════════════════════════════════════════════\n";
  std::cout << "  Query Test Suite\n";
  std::cout
      << "═══════════════════════════════════════════════════════════════\n";

  CSVParser parser;
  DataStore store = parser.parseFile(filename);

  if (store.size() == 0) {
    std::cout << "  SKIPPED — no data loaded\n";
    return;
  }

  std::cout << "  Loaded " << store.size() << " records\n";

  // ─── AoS Query Tests ────────────────────────────────────────────────

  std::cout << "\n── AoS Queries ──\n";

  QTEST("Borough query returns non-empty for BROOKLYN") {
    auto res = QueryEngine::queryByBorough(store, Borough::BROOKLYN);
    std::cout << "(" << res.size() << " matches) ";
    QASSERT_TRUE(res.size() > 0);
    // Verify all results actually have BROOKLYN borough
    for (size_t idx : res) {
      QASSERT_TRUE(store.getRecord(idx).borough == Borough::BROOKLYN);
    }
  }
  QEND_TEST

  QTEST("Borough query — all boroughs sum to total") {
    size_t total = 0;
    total += QueryEngine::queryByBorough(store, Borough::MANHATTAN).size();
    total += QueryEngine::queryByBorough(store, Borough::BRONX).size();
    total += QueryEngine::queryByBorough(store, Borough::BROOKLYN).size();
    total += QueryEngine::queryByBorough(store, Borough::QUEENS).size();
    total += QueryEngine::queryByBorough(store, Borough::STATEN_ISLAND).size();
    total += QueryEngine::queryByBorough(store, Borough::UNSPECIFIED).size();
    std::cout << "(" << total << " vs " << store.size() << ") ";
    QASSERT_EQ(total, store.size());
  }
  QEND_TEST

  QTEST("Geo bounding box query returns valid coordinates") {
    double minLat = 40.70, maxLat = 40.82, minLon = -74.02, maxLon = -73.93;
    auto res = QueryEngine::queryByGeoBoundingBox(store, minLat, maxLat, minLon,
                                                  maxLon);
    std::cout << "(" << res.size() << " matches) ";
    for (size_t idx : res) {
      const auto &r = store.getRecord(idx);
      QASSERT_TRUE(r.latitude >= minLat && r.latitude <= maxLat);
      QASSERT_TRUE(r.longitude >= minLon && r.longitude <= maxLon);
    }
  }
  QEND_TEST

  QTEST("Complaint type query correctness") {
    auto res = QueryEngine::queryByComplaintType(store, "Noise - Residential");
    std::cout << "(" << res.size() << " matches) ";
    for (size_t idx : res) {
      QASSERT_TRUE(store.getRecord(idx).complaint_type ==
                   "Noise - Residential");
    }
  }
  QEND_TEST

  QTEST("Date range query returns valid dates") {
    // Find a reasonable date range
    time_t earliest = 0, latest = 0;
    for (size_t i = 0; i < store.size(); ++i) {
      time_t t = store.getRecord(i).created_date;
      if (t > 0 && (t < earliest || earliest == 0))
        earliest = t;
      if (t > latest)
        latest = t;
    }
    time_t mid = earliest + (latest - earliest) / 2;
    time_t end = mid + 30 * 24 * 3600;

    auto res = QueryEngine::queryByDateRange(store, mid, end);
    std::cout << "(" << res.size() << " matches in range) ";
    for (size_t idx : res) {
      time_t t = store.getRecord(idx).created_date;
      QASSERT_TRUE(t >= mid && t <= end);
    }
  }
  QEND_TEST

  QTEST("Composite query is subset of individual queries") {
    time_t earliest = 0, latest = 0;
    for (size_t i = 0; i < store.size(); ++i) {
      time_t t = store.getRecord(i).created_date;
      if (t > 0 && (t < earliest || earliest == 0))
        earliest = t;
      if (t > latest)
        latest = t;
    }

    auto boroughRes = QueryEngine::queryByBorough(store, Borough::BROOKLYN);
    auto compositeRes = QueryEngine::compositeQuery(
        store, earliest, latest, Borough::BROOKLYN, "Noise - Residential");

    std::cout << "(" << compositeRes.size()
              << " composite <= " << boroughRes.size() << " borough) ";
    QASSERT_TRUE(compositeRes.size() <= boroughRes.size());
  }
  QEND_TEST

  // ─── SoA Query Tests ────────────────────────────────────────────────

  std::cout << "\n── SoA Queries ──\n";

  DataStoreSoA soaStore;
  soaStore.convertFromAoS(store);

  QTEST("SoA record count matches AoS") {
    QASSERT_EQ(soaStore.size(), store.size());
  }
  QEND_TEST

  QTEST("SoA borough query matches AoS") {
    auto aosRes = QueryEngine::queryByBorough(store, Borough::BROOKLYN);
    auto soaRes = QueryEngine::queryByBorough(
        soaStore, static_cast<uint8_t>(Borough::BROOKLYN));
    std::cout << "(AoS=" << aosRes.size() << " SoA=" << soaRes.size() << ") ";
    QASSERT_EQ(aosRes.size(), soaRes.size());
  }
  QEND_TEST

  QTEST("SoA direct parse matches AoS parse") {
    DataStoreSoA directSoA;
    directSoA.parseFromCSV(filename);
    QASSERT_EQ(directSoA.size(), store.size());
  }
  QEND_TEST

  QTEST("String interning builds correctly") {
    soaStore.buildComplaintTypeInterning();
    QASSERT_TRUE(soaStore.complaint_type_lookup.size() > 0);
    QASSERT_EQ(soaStore.complaint_type_indices.size(), soaStore.size());
    std::cout << "(" << soaStore.complaint_type_lookup.size()
              << " unique types) ";
  }
  QEND_TEST

  // ─── Summary ────────────────────────────────────────────────────────────

  std::cout
      << "\n═══════════════════════════════════════════════════════════════\n";
  std::cout << "  Query Results: " << tests_passed << " passed, "
            << tests_failed << " failed\n";
  std::cout
      << "═══════════════════════════════════════════════════════════════\n";
}

// ─── Main ───────────────────────────────────────────────────────────────────

int main(int argc, char *argv[]) {
  // Run parser tests first
  int parserFailed = runParserTests(argc, argv);

  // Then run query tests (need a CSV file)
  std::string filename;
  for (int i = 1; i < argc; ++i) {
    if (std::strstr(argv[i], ".csv")) {
      filename = argv[i];
      break;
    }
  }

  if (!filename.empty()) {
    testQueries(filename);
  } else {
    std::cout << "\n  Query tests SKIPPED — provide a CSV file as argument.\n";
  }

  return parserFailed + tests_failed;
}
