#include "BenchmarkHarness.h"
#include "CSVParser.h"
#include "DataStore.h"
#include "DataStoreSoA.h"
#include "QueryEngine.h"

#include <cstring>
#include <fstream>
#include <iostream>
#include <string>

#if defined(HAS_OPENMP)
#include <omp.h>
#endif

// ─── Command-line argument parsing ──────────────────────────────────────────

struct Config {
  std::string dataFile;
  int trials = 10;
  int parseTrials =
      0;         // 0 = use trials; separate count for slow parse experiments
  int phase = 0; // 0 = all, 1/2/3 = specific
  size_t maxRows = 0; // 0 = no limit; cap for AoS experiments on huge datasets
  bool skipParse = false; // skip timed parse benchmarks, only run query exps
  std::string csvOutput;  // optional CSV output file

  int getParseTrials() const { return parseTrials > 0 ? parseTrials : trials; }
};

Config parseArgs(int argc, char *argv[]) {
  Config cfg;
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0]
              << " <csv_file> [--trials N] [--parse-trials N] [--max-rows N] "
                 "[--phase "
                 "1|2|3|all] [--csv output.csv]\n";
    exit(1);
  }
  cfg.dataFile = argv[1];

  for (int i = 2; i < argc; ++i) {
    if (std::strcmp(argv[i], "--trials") == 0 && i + 1 < argc) {
      cfg.trials = std::stoi(argv[++i]);
    } else if (std::strcmp(argv[i], "--parse-trials") == 0 && i + 1 < argc) {
      cfg.parseTrials = std::stoi(argv[++i]);
    } else if (std::strcmp(argv[i], "--max-rows") == 0 && i + 1 < argc) {
      cfg.maxRows = static_cast<size_t>(std::stoll(argv[++i]));
    } else if (std::strcmp(argv[i], "--skip-parse") == 0) {
      cfg.skipParse = true;
    } else if (std::strcmp(argv[i], "--phase") == 0 && i + 1 < argc) {
      ++i;
      if (std::strcmp(argv[i], "all") == 0)
        cfg.phase = 0;
      else
        cfg.phase = std::stoi(argv[i]);
    } else if (std::strcmp(argv[i], "--csv") == 0 && i + 1 < argc) {
      cfg.csvOutput = argv[++i];
    }
  }
  return cfg;
}

// ─── Experiment helper: find date range for queries ─────────────────────────

struct DateBounds {
  time_t earliest = 0;
  time_t latest = 0;
  time_t oneMonthStart = 0;
  time_t oneMonthEnd = 0;
};

DateBounds findDateBounds(const DataStore &store) {
  DateBounds db;
  if (store.size() == 0)
    return db;

  db.earliest = store.getRecord(0).created_date;
  db.latest = store.getRecord(0).created_date;

  for (size_t i = 1; i < store.size(); ++i) {
    time_t t = store.getRecord(i).created_date;
    if (t > 0 && (t < db.earliest || db.earliest == 0))
      db.earliest = t;
    if (t > db.latest)
      db.latest = t;
  }

  // One month window near the middle
  time_t mid = db.earliest + (db.latest - db.earliest) / 2;
  db.oneMonthStart = mid;
  db.oneMonthEnd = mid + 30 * 24 * 3600; // 30 days

  return db;
}

// ─── Phase 1: Serial Baseline Experiments ───────────────────────────────────

void runPhase1(const Config &cfg, std::ofstream *csvOut) {
  std::cout << "\n╔════════════════════════════════════════════════════════════"
               "═══╗\n";
  std::cout
      << "║              PHASE 1: Serial OO Library + Baseline           ║\n";
  std::cout << "╚══════════════════════════════════════════════════════════════"
               "═╝\n\n";

  CSVParser parser;
  DataStore store;

  // Load data (always needed for query benchmarks)
  store = parser.parseFile(cfg.dataFile, cfg.maxRows);
  std::cout << "  [Loaded " << store.size() << " records]\n\n";

  if (!cfg.skipParse) {
    // E1.1: Full CSV parse (serial)
    auto r1 = BenchmarkHarness::benchmark(
        "E1.1: CSV Parse (serial)",
        [&]() { store = parser.parseFile(cfg.dataFile, cfg.maxRows); },
        cfg.getParseTrials());
    r1.printTable();
    if (csvOut)
      r1.writeCSV(*csvOut);

    std::cout << "  → Records loaded: " << store.size() << "\n\n";

    // E1.3: reserve() vs no reserve
    auto r3a = BenchmarkHarness::benchmark(
        "E1.3a: Parse WITHOUT reserve()",
        [&]() {
          CSVParser p;
          auto s = p.parseFile(cfg.dataFile, cfg.maxRows);
        },
        cfg.getParseTrials());
    r3a.printTable();
    if (csvOut)
      r3a.writeCSV(*csvOut);

    auto r3b = BenchmarkHarness::benchmark(
        "E1.3b: Parse WITH reserve(N)",
        [&]() {
          CSVParser p;
          auto s =
              p.parseFileWithReserve(cfg.dataFile, store.size(), cfg.maxRows);
        },
        cfg.getParseTrials());
    r3b.printTable();
    if (csvOut)
      r3b.writeCSV(*csvOut);
  } // end !skipParse

  // E1.4: Memory footprint
  size_t memBytes = store.memoryFootprint();
  std::ifstream fileCheck(cfg.dataFile, std::ios::ate);
  size_t fileBytes = fileCheck.tellg();
  fileCheck.close();

  std::cout
      << "┌─────────────────────────────────────────────────────────────┐\n";
  std::cout
      << "│ E1.4: Memory Footprint                                     │\n";
  std::cout
      << "├──────────────────────────┬──────────────────────────────────┤\n";
  std::cout << "│ sizeof(ServiceRequest)   │ " << std::setw(12)
            << sizeof(ServiceRequest) << " bytes             │\n";
  std::cout << "│ AoS memory footprint     │ " << std::setw(12) << memBytes
            << " bytes             │\n";
  std::cout << "│ Raw CSV file size        │ " << std::setw(12) << fileBytes
            << " bytes             │\n";
  std::cout << "│ Overhead ratio           │ " << std::setw(12) << std::fixed
            << std::setprecision(2) << (double)memBytes / fileBytes
            << "x                  │\n";
  std::cout << "│ Records                  │ " << std::setw(12) << store.size()
            << "                    │\n";
  std::cout
      << "└──────────────────────────┴──────────────────────────────────┘\n\n";

  // E1.5: String overhead analysis
  size_t totalStringCapacity = 0;
  size_t totalStringLength = 0;
  size_t totalStringCount = 0;
  for (const auto &r : store.getRecords()) {
    auto addString = [&](const std::string &s) {
      totalStringCapacity += s.capacity();
      totalStringLength += s.length();
      ++totalStringCount;
    };
    addString(r.agency);
    addString(r.agency_name);
    addString(r.complaint_type);
    addString(r.descriptor);
    addString(r.location_type);
    addString(r.incident_address);
    addString(r.street_name);
    addString(r.cross_street_1);
    addString(r.cross_street_2);
    addString(r.city);
    addString(r.resolution_description);
    addString(r.community_board);
    addString(r.facility_type);
    addString(r.landmark);
    addString(r.intersection_street_1);
    addString(r.intersection_street_2);
    addString(r.address_type);
    addString(r.park_facility_name);
    addString(r.vehicle_type);
    addString(r.taxi_company_borough);
    addString(r.taxi_pick_up_location);
    addString(r.bridge_highway_name);
    addString(r.bridge_highway_direction);
    addString(r.road_ramp);
    addString(r.bridge_highway_segment);
  }

  std::cout
      << "┌─────────────────────────────────────────────────────────────┐\n";
  std::cout
      << "│ E1.5: String Overhead Analysis                             │\n";
  std::cout
      << "├──────────────────────────┬──────────────────────────────────┤\n";
  std::cout << "│ Total strings            │ " << std::setw(12)
            << totalStringCount << "                    │\n";
  std::cout << "│ Total useful chars       │ " << std::setw(12)
            << totalStringLength << " bytes             │\n";
  std::cout << "│ Total capacity allocated │ " << std::setw(12)
            << totalStringCapacity << " bytes             │\n";
  std::cout << "│ Overhead (cap - len)     │ " << std::setw(12)
            << (totalStringCapacity - totalStringLength)
            << " bytes             │\n";
  std::cout << "│ Overhead ratio           │ " << std::setw(12) << std::fixed
            << std::setprecision(2)
            << (totalStringLength > 0
                    ? (double)totalStringCapacity / totalStringLength
                    : 0.0)
            << "x                  │\n";
  std::cout
      << "└──────────────────────────┴──────────────────────────────────┘\n\n";

  // Find date bounds for queries
  DateBounds db = findDateBounds(store);

  // E1.6: Query — date range (1 month)
  auto r6 = BenchmarkHarness::benchmark(
      "E1.6: Query date range (1 month)",
      [&]() {
        auto res = QueryEngine::queryByDateRange(store, db.oneMonthStart,
                                                 db.oneMonthEnd);
      },
      cfg.trials);
  r6.printTable();
  if (csvOut)
    r6.writeCSV(*csvOut);
  {
    auto res =
        QueryEngine::queryByDateRange(store, db.oneMonthStart, db.oneMonthEnd);
    std::cout << "  → Matches: " << res.size() << "\n\n";
  }

  // E1.7: Query — borough filter (BROOKLYN)
  auto r7 = BenchmarkHarness::benchmark(
      "E1.7: Query borough (BROOKLYN)",
      [&]() {
        auto res = QueryEngine::queryByBorough(store, Borough::BROOKLYN);
      },
      cfg.trials);
  r7.printTable();
  if (csvOut)
    r7.writeCSV(*csvOut);
  {
    auto res = QueryEngine::queryByBorough(store, Borough::BROOKLYN);
    std::cout << "  → Matches: " << res.size() << "\n\n";
  }

  // E1.8: Query — geo bounding box (Manhattan approx)
  double minLat = 40.70, maxLat = 40.82, minLon = -74.02, maxLon = -73.93;
  auto r8 = BenchmarkHarness::benchmark(
      "E1.8: Query geo bounding box (Manhattan)",
      [&]() {
        auto res = QueryEngine::queryByGeoBoundingBox(store, minLat, maxLat,
                                                      minLon, maxLon);
      },
      cfg.trials);
  r8.printTable();
  if (csvOut)
    r8.writeCSV(*csvOut);
  {
    auto res = QueryEngine::queryByGeoBoundingBox(store, minLat, maxLat, minLon,
                                                  maxLon);
    std::cout << "  → Matches: " << res.size() << "\n\n";
  }

  // E1.9: Query — complaint type
  auto r9 = BenchmarkHarness::benchmark(
      "E1.9: Query complaint type (Noise - Residential)",
      [&]() {
        auto res =
            QueryEngine::queryByComplaintType(store, "Noise - Residential");
      },
      cfg.trials);
  r9.printTable();
  if (csvOut)
    r9.writeCSV(*csvOut);
  {
    auto res = QueryEngine::queryByComplaintType(store, "Noise - Residential");
    std::cout << "  → Matches: " << res.size() << "\n\n";
  }

  // E1.10: Query — composite
  auto r10 = BenchmarkHarness::benchmark(
      "E1.10: Composite query (date+borough+type)",
      [&]() {
        auto res = QueryEngine::compositeQuery(
            store, db.oneMonthStart, db.oneMonthEnd, Borough::BROOKLYN,
            "Noise - Residential");
      },
      cfg.trials);
  r10.printTable();
  if (csvOut)
    r10.writeCSV(*csvOut);
  {
    auto res =
        QueryEngine::compositeQuery(store, db.oneMonthStart, db.oneMonthEnd,
                                    Borough::BROOKLYN, "Noise - Residential");
    std::cout << "  → Matches: " << res.size() << "\n\n";
  }
}

// ─── Phase 2: OpenMP Parallelization Experiments ────────────────────────────

void runPhase2(const Config &cfg, std::ofstream *csvOut) {
  std::cout << "\n╔════════════════════════════════════════════════════════════"
               "═══╗\n";
  std::cout
      << "║           PHASE 2: OpenMP Parallelization                    ║\n";
  std::cout << "╚══════════════════════════════════════════════════════════════"
               "═╝\n\n";

#if defined(HAS_OPENMP)
  int maxThreads = omp_get_max_threads();
  std::cout << "  OpenMP enabled — max threads: " << maxThreads << "\n\n";

  // Parse data first (serial) for query benchmarks
  CSVParser parser;
  DataStore store = parser.parseFile(cfg.dataFile, cfg.maxRows);
  DateBounds db = findDateBounds(store);

  // E2.1 + E2.2: Query scaling curve
  std::vector<int> threadCounts = {1, 2, 4, 8};
  if (maxThreads >= 14)
    threadCounts.push_back(14);

  // Also get serial baseline for speedup calculation
  auto serialBaseline = BenchmarkHarness::benchmark(
      "E2.1: Date query SERIAL baseline",
      [&]() {
        auto res = QueryEngine::queryByDateRange(store, db.oneMonthStart,
                                                 db.oneMonthEnd);
      },
      cfg.trials);
  serialBaseline.printTable();
  if (csvOut)
    serialBaseline.writeCSV(*csvOut);

  for (int t : threadCounts) {
    omp_set_num_threads(t);
    std::string name = "E2.1: Date query " + std::to_string(t) + " threads";
    auto result = BenchmarkHarness::benchmark(
        name,
        [&]() {
          auto res = QueryEngine::queryByDateRangeParallel(
              store, db.oneMonthStart, db.oneMonthEnd);
        },
        cfg.trials);
    result.printTable();
    if (csvOut)
      result.writeCSV(*csvOut);

    double speedup = serialBaseline.mean_ms / result.mean_ms;
    double efficiency = speedup / t * 100.0;
    std::cout << "  → Speedup: " << std::fixed << std::setprecision(2)
              << speedup << "x  |  Efficiency: " << efficiency << "%\n\n";
  }

  // E2.1 continued: Borough query scaling
  auto boroughSerial = BenchmarkHarness::benchmark(
      "E2.1: Borough query SERIAL",
      [&]() {
        auto res = QueryEngine::queryByBorough(store, Borough::BROOKLYN);
      },
      cfg.trials);
  boroughSerial.printTable();
  if (csvOut)
    boroughSerial.writeCSV(*csvOut);

  for (int t : threadCounts) {
    omp_set_num_threads(t);
    std::string name = "E2.1: Borough query " + std::to_string(t) + " threads";
    auto result = BenchmarkHarness::benchmark(
        name,
        [&]() {
          auto res =
              QueryEngine::queryByBoroughParallel(store, Borough::BROOKLYN);
        },
        cfg.trials);
    result.printTable();
    if (csvOut)
      result.writeCSV(*csvOut);

    double speedup = boroughSerial.mean_ms / result.mean_ms;
    std::cout << "  → Speedup: " << std::fixed << std::setprecision(2)
              << speedup << "x\n\n";
  }

  // E2.3: Parse — serial vs parallel (skipped with --skip-parse)
  if (!cfg.skipParse) {
    auto parseSerial = BenchmarkHarness::benchmark(
        "E2.3: Parse SERIAL",
        [&]() {
          CSVParser p;
          auto s = p.parseFile(cfg.dataFile, cfg.maxRows);
        },
        cfg.getParseTrials());
    parseSerial.printTable();
    if (csvOut)
      parseSerial.writeCSV(*csvOut);

    for (int t : threadCounts) {
      omp_set_num_threads(t);
      std::string name =
          "E2.3: Parse PARALLEL " + std::to_string(t) + " threads";
      auto result = BenchmarkHarness::benchmark(
          name,
          [&]() {
            CSVParser p;
            auto s = p.parseFileParallel(cfg.dataFile, cfg.maxRows);
          },
          cfg.getParseTrials());
      result.printTable();
      if (csvOut)
        result.writeCSV(*csvOut);

      double speedup = parseSerial.mean_ms / result.mean_ms;
      std::cout << "  → Parse speedup: " << std::fixed << std::setprecision(2)
                << speedup << "x\n\n";
    }
  } // end !skipParse

  // E2.4: Threading overhead test (with warm-up to avoid cold-cache outliers)
  omp_set_num_threads(maxThreads);
  // Warm-up: run one query to populate cache, then benchmark
  {
    auto warmup = QueryEngine::queryByBorough(store, Borough::BROOKLYN);
  }
  auto smallSerial = BenchmarkHarness::benchmark(
      "E2.4: Query SERIAL (warmed)",
      [&]() {
        auto res = QueryEngine::queryByBorough(store, Borough::BROOKLYN);
      },
      cfg.trials);
  smallSerial.printTable();
  if (csvOut)
    smallSerial.writeCSV(*csvOut);

  {
    auto warmup = QueryEngine::queryByBoroughParallel(store, Borough::BROOKLYN);
  }
  auto smallParallel = BenchmarkHarness::benchmark(
      "E2.4: Query PARALLEL " + std::to_string(maxThreads) +
          " threads (warmed)",
      [&]() {
        auto res =
            QueryEngine::queryByBoroughParallel(store, Borough::BROOKLYN);
      },
      cfg.trials);
  smallParallel.printTable();
  if (csvOut)
    smallParallel.writeCSV(*csvOut);

  double overhead = smallParallel.mean_ms / smallSerial.mean_ms;
  std::cout << "  → Threading overhead ratio: " << std::fixed
            << std::setprecision(2) << overhead
            << "x (>1.0 means threading HURT)\n\n";

#else
  std::cout << "  OpenMP NOT available — skipping Phase 2.\n";
  std::cout << "  Build with LLVM Clang to enable OpenMP.\n\n";
#endif
}

// ─── Phase 3: SoA Optimization Experiments ──────────────────────────────────

void runPhase3(const Config &cfg, std::ofstream *csvOut) {
  std::cout << "\n╔════════════════════════════════════════════════════════════"
               "═══╗\n";
  std::cout
      << "║          PHASE 3: SoA Optimization + Cache Analysis          ║\n";
  std::cout << "╚══════════════════════════════════════════════════════════════"
               "═╝\n\n";

  CSVParser parser;
  DataStore aosStore;
  DataStoreSoA soaStore;

  // Always load data for query experiments
  aosStore = parser.parseFile(cfg.dataFile, cfg.maxRows);
  soaStore.convertFromAoS(aosStore);
  std::cout << "  [Loaded " << aosStore.size()
            << " records into AoS + SoA]\n\n";

  // E3.7: Parse time comparison — AoS vs SoA
  if (!cfg.skipParse) {
    auto aosParseResult = BenchmarkHarness::benchmark(
        "E3.7a: AoS CSV parse",
        [&]() { aosStore = parser.parseFile(cfg.dataFile, cfg.maxRows); },
        cfg.getParseTrials());
    aosParseResult.printTable();
    if (csvOut)
      aosParseResult.writeCSV(*csvOut);

    auto soaParseResult = BenchmarkHarness::benchmark(
        "E3.7b: SoA direct CSV parse",
        [&]() { soaStore.parseFromCSV(cfg.dataFile); }, cfg.getParseTrials());
    soaParseResult.printTable();
    if (csvOut)
      soaParseResult.writeCSV(*csvOut);

    std::cout << "  \u2192 AoS records: " << aosStore.size()
              << " | SoA records: " << soaStore.size() << "\n\n";
  } // end !skipParse

  // E3.6: Memory footprint comparison
  size_t aosMem = aosStore.memoryFootprint();
  size_t soaMem = soaStore.memoryFootprint();
  soaStore.buildComplaintTypeInterning();
  size_t soaInternMem = soaStore.memoryFootprintWithInterning();

  std::cout
      << "┌─────────────────────────────────────────────────────────────┐\n";
  std::cout
      << "│ E3.6: Memory Footprint Comparison                          │\n";
  std::cout
      << "├──────────────────────────┬──────────────────────────────────┤\n";
  std::cout << "│ AoS footprint            │ " << std::setw(12) << aosMem
            << " bytes             │\n";
  std::cout << "│ SoA footprint            │ " << std::setw(12) << soaMem
            << " bytes             │\n";
  std::cout << "│ SoA + interning          │ " << std::setw(12) << soaInternMem
            << " bytes             │\n";
  std::cout << "│ SoA savings vs AoS       │ " << std::setw(12) << std::fixed
            << std::setprecision(1) << (1.0 - (double)soaMem / aosMem) * 100
            << "%                 │\n";
  std::cout << "│ Unique complaint types    │ " << std::setw(12)
            << soaStore.complaint_type_lookup.size()
            << "                    │\n";
  std::cout << "└──────────────────────────┴─────────────────────────────────"
               "─┘\n\n";

  // E3.6b: Flat string buffer memory comparison
  size_t soaFlatMem = soaStore.memoryFootprintFlat();

  std::cout
      << "┌─────────────────────────────────────────────────────────────┐\n";
  std::cout
      << "│ E3.6b: Data Locality — Memory Layout Comparison            │\n";
  std::cout
      << "├──────────────────────────┬──────────────────────────────────┤\n";
  std::cout << "│ AoS (scattered strings)  │ " << std::setw(12) << aosMem
            << " bytes             │\n";
  std::cout << "│ SoA (vector<string>)     │ " << std::setw(12) << soaMem
            << " bytes             │\n";
  std::cout << "│ SoA (flat buffer)        │ " << std::setw(12) << soaFlatMem
            << " bytes             │\n";
  std::cout << "│ SoA (flat + interning)   │ " << std::setw(12) << soaInternMem
            << " bytes             │\n";
  std::cout << "│ Flat vs vector<string>   │ " << std::setw(12) << std::fixed
            << std::setprecision(1) << (1.0 - (double)soaFlatMem / soaMem) * 100
            << "% savings          │\n";
  std::cout << "│ Flat buffer mallocs      │ " << std::setw(12) << "13"
            << " (1 per column)    │\n";
  std::cout << "│ vector<string> mallocs   │ " << std::setw(12)
            << soaStore.size() * 13 << " (N per column)    │\n";
  std::cout << "└──────────────────────────┴─────────────────────────────────"
               "─┘\n\n";

  // E3.9: Complaint type query — scattered vs contiguous strings
  auto soaScatteredQuery = BenchmarkHarness::benchmark(
      "E3.9a: Complaint type query (vector<string>)",
      [&]() {
        auto res =
            QueryEngine::queryByComplaintType(soaStore, "Noise - Residential");
      },
      cfg.trials);
  soaScatteredQuery.printTable();
  if (csvOut)
    soaScatteredQuery.writeCSV(*csvOut);

  auto soaFlatQuery = BenchmarkHarness::benchmark(
      "E3.9b: Complaint type query (flat buffer)",
      [&]() {
        auto res = soaStore.queryByComplaintTypeFlat("Noise - Residential");
      },
      cfg.trials);
  soaFlatQuery.printTable();
  if (csvOut)
    soaFlatQuery.writeCSV(*csvOut);

  std::cout << "  → Flat vs scattered speedup: " << std::fixed
            << std::setprecision(2)
            << soaScatteredQuery.mean_ms / soaFlatQuery.mean_ms << "x\n\n";

  // Find date bounds using AoS store
  DateBounds db = findDateBounds(aosStore);

  // E3.1: Date range query — AoS vs SoA
  auto aosDateResult = BenchmarkHarness::benchmark(
      "E3.1a: AoS date range query",
      [&]() {
        auto res = QueryEngine::queryByDateRange(aosStore, db.oneMonthStart,
                                                 db.oneMonthEnd);
      },
      cfg.trials);
  aosDateResult.printTable();
  if (csvOut)
    aosDateResult.writeCSV(*csvOut);

  auto soaDateResult = BenchmarkHarness::benchmark(
      "E3.1b: SoA date range query",
      [&]() {
        auto res = QueryEngine::queryByDateRange(soaStore, db.oneMonthStart,
                                                 db.oneMonthEnd);
      },
      cfg.trials);
  soaDateResult.printTable();
  if (csvOut)
    soaDateResult.writeCSV(*csvOut);

  std::cout << "  → AoS/SoA speedup: " << std::fixed << std::setprecision(2)
            << aosDateResult.mean_ms / soaDateResult.mean_ms << "x\n\n";

  // E3.2: Borough filter — AoS vs SoA
  auto aosBoroughResult = BenchmarkHarness::benchmark(
      "E3.2a: AoS borough query",
      [&]() {
        auto res = QueryEngine::queryByBorough(aosStore, Borough::BROOKLYN);
      },
      cfg.trials);
  aosBoroughResult.printTable();
  if (csvOut)
    aosBoroughResult.writeCSV(*csvOut);

  auto soaBoroughResult = BenchmarkHarness::benchmark(
      "E3.2b: SoA borough query",
      [&]() {
        auto res = QueryEngine::queryByBorough(
            soaStore, static_cast<uint8_t>(Borough::BROOKLYN));
      },
      cfg.trials);
  soaBoroughResult.printTable();
  if (csvOut)
    soaBoroughResult.writeCSV(*csvOut);

  std::cout << "  → AoS/SoA speedup: " << std::fixed << std::setprecision(2)
            << aosBoroughResult.mean_ms / soaBoroughResult.mean_ms << "x\n\n";

  // E3.3: Geo bounding box — AoS vs SoA
  double minLat = 40.70, maxLat = 40.82, minLon = -74.02, maxLon = -73.93;
  auto aosGeoResult = BenchmarkHarness::benchmark(
      "E3.3a: AoS geo bounding box",
      [&]() {
        auto res = QueryEngine::queryByGeoBoundingBox(aosStore, minLat, maxLat,
                                                      minLon, maxLon);
      },
      cfg.trials);
  aosGeoResult.printTable();
  if (csvOut)
    aosGeoResult.writeCSV(*csvOut);

  auto soaGeoResult = BenchmarkHarness::benchmark(
      "E3.3b: SoA geo bounding box",
      [&]() {
        auto res = QueryEngine::queryByGeoBoundingBox(soaStore, minLat, maxLat,
                                                      minLon, maxLon);
      },
      cfg.trials);
  soaGeoResult.printTable();
  if (csvOut)
    soaGeoResult.writeCSV(*csvOut);

  std::cout << "  → AoS/SoA speedup: " << std::fixed << std::setprecision(2)
            << aosGeoResult.mean_ms / soaGeoResult.mean_ms << "x\n\n";

  // E3.4: Composite query — AoS vs SoA
  auto aosCompResult = BenchmarkHarness::benchmark(
      "E3.4a: AoS composite query",
      [&]() {
        auto res = QueryEngine::compositeQuery(
            aosStore, db.oneMonthStart, db.oneMonthEnd, Borough::BROOKLYN,
            "Noise - Residential");
      },
      cfg.trials);
  aosCompResult.printTable();
  if (csvOut)
    aosCompResult.writeCSV(*csvOut);

  auto soaCompResult = BenchmarkHarness::benchmark(
      "E3.4b: SoA composite query",
      [&]() {
        auto res = QueryEngine::compositeQuery(
            soaStore, db.oneMonthStart, db.oneMonthEnd,
            static_cast<uint8_t>(Borough::BROOKLYN), "Noise - Residential");
      },
      cfg.trials);
  soaCompResult.printTable();
  if (csvOut)
    soaCompResult.writeCSV(*csvOut);

  std::cout << "  → AoS/SoA speedup: " << std::fixed << std::setprecision(2)
            << aosCompResult.mean_ms / soaCompResult.mean_ms << "x\n\n";

  // E3.5: SoA + OpenMP combined
#if defined(HAS_OPENMP)
  std::vector<int> threadCounts = {1, 2, 4, 8};
  if (omp_get_max_threads() >= 14)
    threadCounts.push_back(14);

  auto soaSerialDate = BenchmarkHarness::benchmark(
      "E3.5: SoA date SERIAL",
      [&]() {
        auto res = QueryEngine::queryByDateRange(soaStore, db.oneMonthStart,
                                                 db.oneMonthEnd);
      },
      cfg.trials);
  soaSerialDate.printTable();
  if (csvOut)
    soaSerialDate.writeCSV(*csvOut);

  for (int t : threadCounts) {
    omp_set_num_threads(t);
    std::string name = "E3.5: SoA date " + std::to_string(t) + " threads";
    auto result = BenchmarkHarness::benchmark(
        name,
        [&]() {
          auto res = QueryEngine::queryByDateRangeParallel(
              soaStore, db.oneMonthStart, db.oneMonthEnd);
        },
        cfg.trials);
    result.printTable();
    if (csvOut)
      result.writeCSV(*csvOut);

    double speedup = soaSerialDate.mean_ms / result.mean_ms;
    std::cout << "  → SoA+OMP speedup: " << std::fixed << std::setprecision(2)
              << speedup << "x\n\n";
  }
#endif

  // E3.8: Full pipeline summary
  std::cout
      << "┌─────────────────────────────────────────────────────────────┐\n";
  std::cout
      << "│ E3.8: Full Pipeline Summary                                │\n";
  std::cout
      << "├──────────────────────────┬──────────────────────────────────┤\n";
  std::cout << "│ AoS date query (serial)  │ " << std::setw(12) << std::fixed
            << std::setprecision(3) << aosDateResult.mean_ms
            << " ms             │\n";
  std::cout << "│ SoA date query (serial)  │ " << std::setw(12)
            << soaDateResult.mean_ms << " ms             │\n";
  std::cout << "│ Layout improvement       │ " << std::setw(12)
            << std::setprecision(2)
            << aosDateResult.mean_ms / soaDateResult.mean_ms
            << "x                  │\n";
  std::cout << "└──────────────────────────┴─────────────────────────────────"
               "─┘\n\n";
}

// ─── Main entry point
// ───────────────────────────────────────────────────────

int main(int argc, char *argv[]) {
  Config cfg = parseArgs(argc, argv);

  std::cout
      << "═══════════════════════════════════════════════════════════════\n";
  std::cout << "  Mini 1: Memory Overload — NYC 311 Benchmark Suite\n";
  std::cout << "  Data:   " << cfg.dataFile << "\n";
  std::cout << "  Trials: " << cfg.trials << "\n";
#if defined(HAS_OPENMP)
  std::cout << "  OpenMP: enabled (max " << omp_get_max_threads()
            << " threads)\n";
#else
  std::cout << "  OpenMP: disabled\n";
#endif
  std::cout
      << "═══════════════════════════════════════════════════════════════\n";

  std::ofstream csvFile;
  std::ofstream *csvOut = nullptr;
  if (!cfg.csvOutput.empty()) {
    csvFile.open(cfg.csvOutput);
    BenchmarkHarness::writeCSVHeader(csvFile);
    csvOut = &csvFile;
    std::cout << "  CSV output: " << cfg.csvOutput << "\n";
  }

  if (cfg.phase == 0 || cfg.phase == 1) {
    runPhase1(cfg, csvOut);
  }
  if (cfg.phase == 0 || cfg.phase == 2) {
    runPhase2(cfg, csvOut);
  }
  if (cfg.phase == 0 || cfg.phase == 3) {
    runPhase3(cfg, csvOut);
  }

  if (csvOut) {
    csvFile.close();
    std::cout << "\n  Benchmark data written to: " << cfg.csvOutput << "\n";
  }

  std::cout << "\n  Done.\n";
  return 0;
}
