#!/bin/bash
# run_benchmarks.sh — runs all benchmark phases on the 12GB dataset
# Designed to run to completion without being killed by a background job runner.
# Usage: bash scripts/run_benchmarks.sh [dataset_path]

set -e
DATASET="${1:-dataset/311_2020.csv}"
BINARY="build/mini1"
TRIALS=10
PARSE_TRIALS=3
MAX_ROWS_AOS=5000000   # 5M rows for AoS = ~3.4GB + string heap, safe on 24GB RAM
DATE=$(date +%Y%m%d_%H%M%S)

echo "=================================================="
echo "  NYC 311 Benchmark Suite — Full Run"
echo "  Dataset : $DATASET"
echo "  AoS cap : ${MAX_ROWS_AOS} rows"
echo "  Query trials  : $TRIALS"
echo "  Parse trials  : $PARSE_TRIALS"
echo "  Started : $(date)"
echo "=================================================="

# Phase 1: Serial baseline (AoS, capped at MAX_ROWS_AOS for memory)
echo ""
echo ">>> Phase 1: Serial baseline (capped at ${MAX_ROWS_AOS} rows)"
$BINARY "$DATASET" \
  --phase 1 \
  --trials $TRIALS \
  --parse-trials $PARSE_TRIALS \
  --max-rows $MAX_ROWS_AOS \
  --csv results_12g_p1.csv

echo ""
echo "Phase 1 complete. Results in results_12g_p1.csv"

# Phase 2: OpenMP parallelization (same AoS cap)
echo ""
echo ">>> Phase 2: OpenMP parallelization"
$BINARY "$DATASET" \
  --phase 2 \
  --trials $TRIALS \
  --parse-trials $PARSE_TRIALS \
  --max-rows $MAX_ROWS_AOS \
  --csv results_12g_p2.csv

echo ""
echo "Phase 2 complete. Results in results_12g_p2.csv"

# Phase 3: SoA (runs without AoS - no row cap needed for SoA experiments)
echo ""
echo ">>> Phase 3: SoA optimization"
$BINARY "$DATASET" \
  --phase 3 \
  --trials $TRIALS \
  --parse-trials $PARSE_TRIALS \
  --max-rows $MAX_ROWS_AOS \
  --csv results_12g_p3.csv

echo ""
echo "Phase 3 complete. Results in results_12g_p3.csv"

echo ""
echo "=================================================="
echo "  All phases complete: $(date)"
echo "=================================================="
echo "Generating graphs..."
python3 scripts/plot_benchmarks.py --dir . --output report/graphs
echo "Done!"
