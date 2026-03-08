cmake_minimum_required(VERSION 4.1)
project(mini1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# OpenMP is optional — code compiles with or without it
find_package(OpenMP)

add_library(mini1_lib
    src/ServiceRequest.cpp
    src/DataStore.cpp
    src/DataStoreSoA.cpp
    src/CSVParser.cpp
    src/QueryEngine.cpp
    src/BenchmarkHarness.cpp
)

target_include_directories(mini1_lib PUBLIC src)

if(OpenMP_CXX_FOUND)
    target_link_libraries(mini1_lib PUBLIC OpenMP::OpenMP_CXX)
    target_compile_definitions(mini1_lib PUBLIC HAS_OPENMP)
endif()

add_executable(mini1 src/main.cpp)
target_link_libraries(mini1 mini1_lib)

# Test executable
add_executable(test_mini1 test/test_parser.cpp test/test_queries.cpp)
target_link_libraries(test_mini1 mini1_lib)
