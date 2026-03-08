# Mini 1: Memory Overload — Research Implementation Plan

**CMPE 275 · Spring 2026 · Professor Gash · Due: March 9, 2026**

---

## Guiding Philosophy

This is NOT a software project. It is a **research investigation into memory behavior** within a single process. Every line of code exists to answer a measurable question. Every design decision generates data for the report. Gash grades on:

1. **Depth of research and validation** — not features, not LoC
2. **The journey** — "the journey rather than the result is the important part" (Gash, Feb 2 lecture)
3. **Data-backed conclusions** — 10+ trial benchmarks, mean/stddev, graphs
4. **Failed attempts** — documented with analysis of WHY they failed
5. **Burden of proof** — yours to win

**The progression is intentional**: start with clean OO (Phase 1), discover its limitations through parallelization (Phase 2), then redesign for performance (Phase 3). Skipping steps or "de-tuning" loses points.

---

## Environment

- **Compiler**: LLVM Clang 17 via Homebrew (NOT Apple Xcode clang)
  - `CC=/opt/homebrew/opt/llvm/bin/clang` and `CXX=/opt/homebrew/opt/llvm/bin/clang++` set in `~/.zshrc`
  - OpenMP verified working (14 threads)
- **Build**: CMake 4.2.3, C++17 standard
- **Python**: 3.x with matplotlib, pandas, numpy for graphing
- **Do NOT**: run from an IDE or VM. Use the terminal.

---

## Dataset

**NYC 311 Service Requests** from NYC OpenData (https://opendata.cityofnewyork.us)

- 2020–Present: `curl -o 311_2020_present.csv "https://data.cityofnewyork.us/api/views/erm2-nwe9/rows.csv?accessType=DOWNLOAD"`
- 2010–2019: `curl -o 311_2010_2019.csv "https://data.cityofnewyork.us/api/views/dpz2-n643/rows.csv?accessType=DOWNLOAD"`
- Combined: 40M+ rows, 12GB+, 41–44 columns
- Development sample: `curl -o 311_sample.csv "https://data.cityofnewyork.us/resource/erm2-nwe9.csv?\$limit=1000"`

---

## Project Structure

```
mini1-teamname/
  CMakeLists.txt
  README.md                    # Build instructions, dependencies
  decision_journal.md          # Every design choice logged with rationale
  src/
    CMakeLists.txt
    ServiceRequest.h/.cpp      # Phase 1: AoS data class
    DataStore.h/.cpp            # Phase 1: vector<ServiceRequest> container
    DataStoreSoA.h/.cpp         # Phase 3: columnar layout
    CSVParser.h/.cpp            # RFC-4180 parser
    QueryEngine.h/.cpp          # Range search APIs
    BenchmarkHarness.h/.cpp     # Timing framework (build FIRST)
    main.cpp
  test/
    CMakeLists.txt
    test_parser.cpp
    test_queries.cpp
  scripts/
    run_benchmarks.sh           # Automated 10+ trial runner
    plot_benchmarks.py          # Generates all report graphs
  report/
    teamname-mini1-report.pdf
    poster.pptx                 # Exactly ONE slide
```

---

## CMake Design

Two build configurations from the **same codebase** (follows hello-omp2.cpp pattern from labs):

```cmake
cmake_minimum_required(VERSION 4.1)
project(mini1)

set(CMAKE_CXX_STANDARD 17)

# OpenMP is optional — code compiles with or without it
find_package(OpenMP)

add_library(mini1_lib src/ServiceRequest.cpp src/DataStore.cpp
            src/DataStoreSoA.cpp src/CSVParser.cpp
            src/QueryEngine.cpp src/BenchmarkHarness.cpp)

if(OpenMP_CXX_FOUND)
    target_link_libraries(mini1_lib PUBLIC OpenMP::OpenMP_CXX)
    target_compile_definitions(mini1_lib PUBLIC HAS_OPENMP)
endif()

add_executable(mini1 src/main.cpp)
target_link_libraries(mini1 mini1_lib)
```

Build serial: `cmake .. -DCMAKE_CXX_COMPILER=$CXX && make`
Build parallel: Same (OpenMP auto-detected from LLVM clang)
Build WITHOUT OpenMP: `cmake .. -DCMAKE_CXX_COMPILER=$CXX -DCMAKE_DISABLE_FIND_PACKAGE_OpenMP=TRUE && make`

Use `#if defined(_OPENMP)` guards around all OpenMP includes and calls (as shown in hello-omp2.cpp).

---

## Phase 1: Serial OO Library + Baseline

### Research Questions (what you're investigating, not just building)

- **RQ1**: What is the memory overhead of OO representation vs raw CSV size?
- **RQ2**: Where does time go in the parse pipeline — I/O, tokenization, type conversion, or allocation?
- **RQ3**: Does pre-reserving vector capacity affect parse performance?
- **RQ4**: What is the cost of std::string for low-cardinality fields (Borough has only 6 values)?
- **RQ5**: How do different query patterns perform on an AoS layout?

### Code: ServiceRequest.h

One row. Fields as primitive types (assignment requirement).

```
Enums (uint8_t):
  Borough:     MANHATTAN=0, BRONX=1, BROOKLYN=2, QUEENS=3, STATEN_ISLAND=4, UNSPECIFIED=5
  Status:      OPEN=0, CLOSED=1, PENDING=2, IN_PROGRESS=3, ASSIGNED=4
  ChannelType: PHONE=0, ONLINE=1, MOBILE=2, OTHER=3

Fixed-size fields:
  int64_t  unique_key
  time_t   created_date, closed_date, due_date, resolution_updated_date
  double   latitude, longitude
  int32_t  incident_zip
  int32_t  x_coordinate, y_coordinate
  uint8_t  borough, status, channel_type

Variable-length fields:
  std::string  agency, complaint_type, descriptor, incident_address,
               street_name, city, resolution_description, facility_type,
               community_board, open_data_channel_type_raw
```

Design requirements: virtual base class (e.g., `IRecord`), facade pattern for the library API, templates for the benchmark harness.

### Code: CSVParser.h

- RFC-4180 compliant state machine: handles `""` escaping, commas inside quoted fields
- `Resolution Description` field has commas and special characters — this is a known parsing challenge (document it)
- Date parsing: `"03/06/2026 02:53:58 AM"` → `time_t` via `strptime` or manual parsing
- Handle empty/null fields: default to 0 for numerics, empty string for strings, UNSPECIFIED for enums

### Code: DataStore.h

- `std::vector<ServiceRequest> records_`
- `reserve(n)` method for pre-allocation experiments
- `memoryFootprint()` — compute approximate bytes: `sizeof(ServiceRequest) * size() + string heap overhead`
- Compare reported footprint to raw CSV file size

### Code: QueryEngine.h

All queries return `std::vector<size_t>` (indices, not pointers — works for both AoS and SoA).

```
queryByDateRange(time_t start, time_t end)
queryByBorough(Borough b)
queryByGeoBoundingBox(double minLat, double maxLat, double minLon, double maxLon)
queryByComplaintType(const std::string& type)
compositeQuery(time_t start, time_t end, Borough b, const std::string& type)
```

Hint from assignment: "What types of searches do you expect?" — range queries on dates and coordinates, categorical filters on borough/complaint type, and composite combinations.

### Code: BenchmarkHarness.h (BUILD THIS FIRST)

```cpp
template<typename Callable>
BenchmarkResult benchmark(const std::string& name, Callable fn, int nTrials = 10);

struct BenchmarkResult {
    std::string name;
    double mean_ms, stddev_ms, min_ms, max_ms;
    std::vector<double> all_times_ms;
    void printTable() const;
    void writeCSV(std::ostream& out) const;
};
```

Uses `std::chrono::high_resolution_clock`. Outputs to both terminal (human-readable table) and CSV (for Python graphing).

### Phase 1 Experiments (10+ trials each)

| Experiment | What to Measure | Why |
|---|---|---|
| E1.1: Full CSV parse | Total time to parse 12GB+ | Baseline for Phase 2/3 comparison |
| E1.2: Parse pipeline breakdown | Time for I/O vs tokenization vs type conversion vs allocation separately | Identify bottleneck (RQ2) |
| E1.3: reserve() vs no reserve | Parse with `reserve(N)` vs dynamic growth | Quantify reallocation cost (RQ3) |
| E1.4: Memory footprint | sizeof(ServiceRequest) * N vs actual RSS vs CSV file size | Measure OO overhead (RQ1) |
| E1.5: String overhead | Count total bytes in string heap vs useful chars | Quantify std::string overhead (RQ4) |
| E1.6: Query — date range | Time to filter by 1-month window | Baseline for Phase 2/3 |
| E1.7: Query — borough filter | Time to filter by borough enum | Baseline, should be fast (uint8_t compare) |
| E1.8: Query — geo bounding box | Time to filter by lat/lon range | Two-column scan baseline |
| E1.9: Query — complaint type | Time to filter by string match | String comparison cost |
| E1.10: Query — composite | Date + borough + complaint type | Multi-field scan baseline |

**Log every result AND every failed attempt in decision_journal.md.**

---

## Phase 2: OpenMP Parallelization

### Research Questions

- **RQ6**: Does threading improve parse performance, or is parsing I/O-bound?
- **RQ7**: At what thread count do queries saturate? Why?
- **RQ8**: Does threading HURT performance for small workloads? (Gash showed 5.8x degradation)
- **RQ9**: Where does the scatter-gather pattern (from Gash's lectures) apply?

### Approach: Follow Gash's Iteration Pattern (from Feb 4 lecture slides)

**Do NOT parallelize everything at once.** Follow the lecture's progression:

**Iteration 1 — Thread the queries only.**
Apply `#pragma omp parallel for` to query loops. Each thread scans its chunk of the data array, collects local results, then merge (scatter-gather pattern). Benchmark.

```cpp
// Scatter-gather pattern from lecture
#pragma omp parallel
{
    std::vector<size_t> local_results;
    #pragma omp for nowait
    for (size_t i = 0; i < n; i++) {
        if (matches(records[i], criteria))
            local_results.push_back(i);
    }
    #pragma omp critical
    global_results.insert(global_results.end(),
                          local_results.begin(), local_results.end());
}
```

**Iteration 2 — Thread the parsing (interleave read-parse).**
Read entire file into memory first, find line boundaries, then chunk lines across threads. Each thread parses independently into a local vector. Merge at the end. Document whether this helps or hurts vs serial (I/O is likely the bottleneck, not parsing CPU work — this is a valuable finding).

**Iteration 3 — Try concurrent independent queries.**
Run multiple different queries simultaneously using `#pragma omp parallel sections`. Measure if this helps when you have many queries to execute.

### Phase 2 Experiments

| Experiment | What to Measure | Threads |
|---|---|---|
| E2.1: Query scaling curve | Each query type with 1, 2, 4, 8, 14 threads | All |
| E2.2: Speedup + efficiency | Speedup = T_serial / T_parallel; Efficiency = Speedup / N_threads | All |
| E2.3: Parse — serial vs parallel | Parse time with threading vs without | 1, 2, 4, 8, 14 |
| E2.4: Small dataset threading cost | Run queries on 1000-row sample with 14 threads | 1 vs 14 |
| E2.5: Thread overhead | Measure thread creation/join overhead with near-zero work | 1, 14 |
| E2.6: Critical section impact | Compare `#pragma omp critical` vs thread-local + merge | 14 |

**Expected finding worth documenting**: Threading the parser may show minimal improvement or even degradation because the workload is I/O-bound, not CPU-bound. This mirrors Gash's 5.8x degradation example — parallel does not always mean faster. **This is a valuable research finding, not a failure.**

Plot: Speedup curve (threads vs speedup factor), Parallel efficiency curve (threads vs efficiency %).

---

## Phase 3: SoA Optimization + Cache Analysis

### Research Questions

- **RQ10**: How much does SoA improve single-column query performance vs AoS?
- **RQ11**: Does SoA still win for multi-column (composite) queries?
- **RQ12**: How much memory does enum encoding save vs storing strings?
- **RQ13**: Does SoA + OpenMP compound the improvement, or is the benefit redundant?
- **RQ14**: "Do queries shape your design?" — how does the access pattern determine optimal layout?

### Code: DataStoreSoA.h

```cpp
struct DataStoreSoA {
    // Fixed-size numeric columns — contiguous in memory
    std::vector<int64_t>  unique_keys;
    std::vector<time_t>   created_dates;
    std::vector<time_t>   closed_dates;
    std::vector<double>   latitudes;
    std::vector<double>   longitudes;
    std::vector<int32_t>  incident_zips;
    std::vector<uint8_t>  boroughs;       // enum-encoded
    std::vector<uint8_t>  statuses;       // enum-encoded
    std::vector<uint8_t>  channel_types;  // enum-encoded

    // String columns — still variable-length but isolated
    std::vector<std::string> agencies;
    std::vector<std::string> complaint_types;
    std::vector<std::string> descriptors;
    // ... etc

    // Build directly from CSV (not just convert from AoS)
    void parseFromCSV(const std::string& filename);

    // Also convert from AoS for comparison
    void convertFromAoS(const DataStore& aos);

    // SoA-specific queries — scan only relevant columns
    std::vector<size_t> queryByDateRange(time_t start, time_t end) const;
    std::vector<size_t> queryByBorough(uint8_t b) const;
    // ... etc
};
```

**Key insight for report**: A date range query on SoA touches ONLY the `created_dates` vector. On AoS, it loads entire `ServiceRequest` structs into cache, wasting cache lines on 40+ fields you don't need. The CPU prefetcher works optimally on contiguous arrays but thrashes on strided access through structs.

**Build a direct SoA parser**, not just a converter from AoS. This lets you fairly compare parse-time performance between layouts.

### Additional Phase 3 Optimizations to Investigate

1. **String interning for medium-cardinality fields**: `complaint_type` has ~200 unique values across 40M rows. Store an index into a lookup table instead of 40M duplicate strings. Measure memory savings.

2. **Enum encoding**: Borough as `uint8_t` (1 byte) vs `std::string` ("MANHATTAN" = 9+ bytes + 32 byte std::string overhead). Calculate savings across 40M rows.

3. **SoA + OpenMP combined**: Apply Phase 2 threading to Phase 3 SoA queries. Does the benefit compound? Or does SoA's cache efficiency already saturate memory bandwidth?

### Phase 3 Experiments

| Experiment | What to Measure | Comparison |
|---|---|---|
| E3.1: Date range query | AoS serial vs SoA serial | Cache effect of layout |
| E3.2: Borough filter | AoS serial vs SoA serial | uint8_t scan on contiguous array |
| E3.3: Geo bounding box | AoS serial vs SoA serial | Two-column (lat+lon) SoA scan |
| E3.4: Composite query | AoS serial vs SoA serial | Multi-column — SoA advantage smaller? |
| E3.5: SoA + OpenMP | SoA serial vs SoA parallel (1/2/4/8/14 threads) | Compound effect |
| E3.6: Memory footprint | AoS vs SoA with enum encoding vs SoA with string interning | Memory savings |
| E3.7: Parse time | AoS parse vs SoA direct parse | Layout affects parse? |
| E3.8: Full pipeline | Best of Phase 1 vs best of Phase 2 vs best of Phase 3 | Summary comparison |

---

## Decision Journal Format

Every entry follows this template:

```markdown
### [Date] Decision: [Title]

**Context**: What problem were we trying to solve?
**Options considered**: What alternatives did we evaluate?
**Decision**: What did we choose?
**Rationale**: Why? Cite data if available.
**Result**: What happened? Include benchmark numbers.
**Status**: Succeeded / Failed / Partially succeeded
```

Failed attempts get the SAME treatment — Gash explicitly values them.

---

## Report Structure

Paragraph-formatted prose (not bullet points). Include:

1. Introduction — research questions, dataset description
2. Phase 1 — OO design decisions, baseline benchmarks (tables + graphs), parse pipeline analysis
3. Phase 2 — parallelization strategy following Gash's iteration model, scaling curves, cases where threading hurt
4. Phase 3 — SoA design, cache analysis, memory savings, query performance comparison
5. Conclusions — data-backed answers to each research question
6. Failed attempts — what didn't work and why (important!)
7. Citations/references
8. Team contributions (appendix)

**Graphs to include** (generated by `scripts/plot_benchmarks.py`):
- Parse time comparison across phases (bar chart)
- Thread scaling curve (line chart: threads vs speedup)
- Parallel efficiency (line chart: threads vs efficiency %)
- AoS vs SoA query performance (grouped bar chart per query type)
- Memory footprint comparison (stacked bar: fixed fields + string overhead)

---

## Poster Slide

**Exactly ONE slide. Not a summary. Not a class diagram. Not a tutorial.**

Choose the single most surprising or significant finding. Examples:
- "Threading made our parser 3x SLOWER — here's why memory bandwidth is the bottleneck"
- "SoA layout reduced date-range query time by 8x due to 94% fewer L1 cache misses"
- "Encoding Borough as uint8_t instead of std::string saved 1.2GB across 40M rows"

The poster should have: one key finding, supporting data (graph or table), and a brief explanation.

---

## Submission Checklist

- [ ] Code: all three phases, compiles with `cmake .. && make`
- [ ] Report: paragraph-formatted PDF with tables + graphs
- [ ] Poster: exactly one PPTX slide
- [ ] decision_journal.md with timestamped entries
- [ ] README.md with build instructions and dependency list
- [ ] Archive as `teamname-mini1.tar.gz`
- [ ] Dataset files NOT included in archive
- [ ] Submitted to Canvas before presentations start March 9
