# Mini 1: Memory Overload — NYC 311 Service Request Analysis

**CMPE 275 · Spring 2026**

## Dependencies

- LLVM Clang 17+ with OpenMP (`/opt/homebrew/opt/llvm/bin/clang++`)
- CMake 4.1+
- Python 3.x with matplotlib, pandas, numpy (for graphing)

## Build Instructions

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_C_COMPILER=/opt/homebrew/opt/llvm/bin/clang \
         -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++
make
```

Build WITHOUT OpenMP (serial-only):
```bash
cmake .. -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++ \
         -DCMAKE_DISABLE_FIND_PACKAGE_OpenMP=TRUE
make
```

## Usage

```bash
# Phase 1: serial baseline
./mini1 ../311_sample.csv --phase 1 --trials 10

# Phase 2: OpenMP parallel (set thread count)
OMP_NUM_THREADS=4 ./mini1 ../311_sample.csv --phase 2 --trials 10

# Phase 3: SoA optimization
./mini1 ../311_sample.csv --phase 3 --trials 10

# All phases
./mini1 ../311_sample.csv --phase all --trials 10
```

## Dataset

Download from NYC OpenData (do NOT include in submission):
```bash
curl -o 311_2020_present.csv "https://data.cityofnewyork.us/api/views/erm2-nwe9/rows.csv?accessType=DOWNLOAD"
curl -o 311_sample.csv "https://data.cityofnewyork.us/resource/erm2-nwe9.csv?\$limit=1000"
```
