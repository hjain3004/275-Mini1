#include "CSVParser.h"
#include "DataStoreSoA.h"
#include <chrono>
#include <iostream>

int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <dataset.csv>\n";
    return 1;
  }

  std::string filename = argv[1];
  std::cout << "Starting isolated SoA parse on: " << filename << "\n";

  DataStoreSoA soaStore;

  auto start = std::chrono::high_resolution_clock::now();

  try {
    soaStore.parseFromCSV(filename);
  } catch (const std::exception &e) {
    std::cerr << "Error parsing: " << e.what() << "\n";
    return 1;
  }

  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> diff = end - start;

  std::cout << "\nParse completed successfully!\n";
  std::cout << "  -> Records loaded: " << soaStore.size() << "\n";
  std::cout << "  -> Time elapsed:   " << diff.count() << " seconds\n";

  // Print memory footprint
  size_t memVector = soaStore.memoryFootprint();
  size_t memFlat = soaStore.memoryFootprintFlat();
  std::cout << "  -> Memory footprint (vector<string>): "
            << memVector / (1024.0 * 1024.0) << " MB\n";
  std::cout << "  -> Memory footprint (flat buffer):    "
            << memFlat / (1024.0 * 1024.0) << " MB\n";

  return 0;
}
