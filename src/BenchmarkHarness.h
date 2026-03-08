#pragma once

#include <string>
#include <vector>
#include <functional>
#include <chrono>
#include <iostream>
#include <fstream>
#include <cmath>
#include <numeric>
#include <algorithm>
#include <iomanip>

/**
 * BenchmarkResult — stores timing data from N-trial benchmark runs.
 */
struct BenchmarkResult {
    std::string name;
    double mean_ms = 0.0;
    double stddev_ms = 0.0;
    double min_ms = 0.0;
    double max_ms = 0.0;
    std::vector<double> all_times_ms;

    void compute() {
        if (all_times_ms.empty()) return;
        min_ms = *std::min_element(all_times_ms.begin(), all_times_ms.end());
        max_ms = *std::max_element(all_times_ms.begin(), all_times_ms.end());
        mean_ms = std::accumulate(all_times_ms.begin(), all_times_ms.end(), 0.0) 
                  / all_times_ms.size();
        double sq_sum = 0.0;
        for (double t : all_times_ms) {
            sq_sum += (t - mean_ms) * (t - mean_ms);
        }
        stddev_ms = std::sqrt(sq_sum / all_times_ms.size());
    }

    void printTable() const {
        std::cout << std::fixed << std::setprecision(3);
        std::cout << "┌─────────────────────────────────────────────────┐\n";
        std::cout << "│ " << std::left << std::setw(48) << name << "│\n";
        std::cout << "├──────────────┬──────────────────────────────────┤\n";
        std::cout << "│ Mean         │ " << std::right << std::setw(14) << mean_ms << " ms             │\n";
        std::cout << "│ Stddev       │ " << std::right << std::setw(14) << stddev_ms << " ms             │\n";
        std::cout << "│ Min          │ " << std::right << std::setw(14) << min_ms << " ms             │\n";
        std::cout << "│ Max          │ " << std::right << std::setw(14) << max_ms << " ms             │\n";
        std::cout << "│ Trials       │ " << std::right << std::setw(14) << all_times_ms.size() << "                │\n";
        std::cout << "└──────────────┴──────────────────────────────────┘\n";
    }

    void writeCSV(std::ostream& out) const {
        out << name << "," << mean_ms << "," << stddev_ms << "," 
            << min_ms << "," << max_ms << "," << all_times_ms.size();
        for (double t : all_times_ms) {
            out << "," << t;
        }
        out << "\n";
    }
};

/**
 * BenchmarkHarness — template-based N-trial benchmarking framework.
 * Uses std::chrono::high_resolution_clock for precision timing.
 */
class BenchmarkHarness {
public:
    /**
     * Run a callable N times and collect timing data.
     * @param name   Human-readable benchmark label
     * @param fn     Callable to benchmark (void return)
     * @param nTrials Number of trials (default 10)
     * @return BenchmarkResult with all timing data
     */
    template<typename Callable>
    static BenchmarkResult benchmark(const std::string& name, Callable fn, int nTrials = 10) {
        BenchmarkResult result;
        result.name = name;
        result.all_times_ms.reserve(nTrials);

        for (int i = 0; i < nTrials; ++i) {
            auto start = std::chrono::high_resolution_clock::now();
            fn();
            auto end = std::chrono::high_resolution_clock::now();
            double elapsed = std::chrono::duration<double, std::milli>(end - start).count();
            result.all_times_ms.push_back(elapsed);
        }

        result.compute();
        return result;
    }

    /**
     * Run a callable that returns a value — captures the result from the last trial.
     */
    template<typename Callable>
    static BenchmarkResult benchmarkWithResult(const std::string& name, Callable fn, 
                                                int nTrials, 
                                                typename std::invoke_result<Callable>::type* outResult) {
        BenchmarkResult result;
        result.name = name;
        result.all_times_ms.reserve(nTrials);

        for (int i = 0; i < nTrials; ++i) {
            auto start = std::chrono::high_resolution_clock::now();
            auto val = fn();
            auto end = std::chrono::high_resolution_clock::now();
            double elapsed = std::chrono::duration<double, std::milli>(end - start).count();
            result.all_times_ms.push_back(elapsed);
            if (outResult && i == nTrials - 1) {
                *outResult = std::move(val);
            }
        }

        result.compute();
        return result;
    }

    /** Write CSV header for benchmark output files */
    static void writeCSVHeader(std::ostream& out) {
        out << "benchmark,mean_ms,stddev_ms,min_ms,max_ms,trials,individual_times...\n";
    }

    /** Print a separator line between benchmark results */
    static void printSeparator() {
        std::cout << "\n";
    }
};
