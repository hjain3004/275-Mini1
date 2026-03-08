#include "CSVParser.h"
#include "DataStore.h"
#include "ServiceRequest.h"

#include <cassert>
#include <cstring>
#include <iostream>

// Simple test framework
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name)                                                             \
  std::cout << "  TEST: " << name << "... ";                                   \
  try {

#define END_TEST                                                               \
  std::cout << "PASSED\n";                                                     \
  tests_passed++;                                                              \
  }                                                                            \
  catch (const std::exception &e) {                                            \
    std::cout << "FAILED: " << e.what() << "\n";                               \
    tests_failed++;                                                            \
  }

#define ASSERT_EQ(a, b)                                                        \
  if ((a) != (b)) {                                                            \
    throw std::runtime_error("Expected " + std::to_string(b) + " but got " +   \
                             std::to_string(a));                               \
  }

#define ASSERT_STR_EQ(a, b)                                                    \
  if ((a) != (b)) {                                                            \
    throw std::runtime_error("Expected '" + std::string(b) + "' but got '" +   \
                             std::string(a) + "'");                            \
  }

#define ASSERT_TRUE(x)                                                         \
  if (!(x)) {                                                                  \
    throw std::runtime_error("Assertion failed: " #x);                         \
  }

// ─── CSV Line Splitting Tests ───────────────────────────────────────────────

void testCSVSplitting() {
  std::cout << "\n── CSV Line Splitting ──\n";

  TEST("Simple comma-separated") {
    auto fields = CSVParser::splitCSVLine("a,b,c");
    ASSERT_EQ(fields.size(), 3u);
    ASSERT_STR_EQ(fields[0], "a");
    ASSERT_STR_EQ(fields[1], "b");
    ASSERT_STR_EQ(fields[2], "c");
  }
  END_TEST

  TEST("Quoted fields") {
    auto fields = CSVParser::splitCSVLine("\"hello\",\"world\"");
    ASSERT_EQ(fields.size(), 2u);
    ASSERT_STR_EQ(fields[0], "hello");
    ASSERT_STR_EQ(fields[1], "world");
  }
  END_TEST

  TEST("Commas inside quotes") {
    auto fields = CSVParser::splitCSVLine("\"hello, world\",test");
    ASSERT_EQ(fields.size(), 2u);
    ASSERT_STR_EQ(fields[0], "hello, world");
    ASSERT_STR_EQ(fields[1], "test");
  }
  END_TEST

  TEST("Escaped quotes") {
    auto fields = CSVParser::splitCSVLine("\"say \"\"hello\"\"\",done");
    ASSERT_EQ(fields.size(), 2u);
    ASSERT_STR_EQ(fields[0], "say \"hello\"");
    ASSERT_STR_EQ(fields[1], "done");
  }
  END_TEST

  TEST("Empty fields") {
    auto fields = CSVParser::splitCSVLine("a,,c,,e");
    ASSERT_EQ(fields.size(), 5u);
    ASSERT_STR_EQ(fields[0], "a");
    ASSERT_STR_EQ(fields[1], "");
    ASSERT_STR_EQ(fields[2], "c");
    ASSERT_STR_EQ(fields[3], "");
    ASSERT_STR_EQ(fields[4], "e");
  }
  END_TEST

  TEST("Resolution description with commas") {
    auto fields = CSVParser::splitCSVLine(
        "123,\"The Department of Transportation determined that this complaint "
        "is a duplicate of a previously filed complaint. The original "
        "complaint is being addressed.\",done");
    ASSERT_EQ(fields.size(), 3u);
    ASSERT_TRUE(fields[1].find("Department of Transportation") !=
                std::string::npos);
  }
  END_TEST
}

// ─── Date Parsing Tests ─────────────────────────────────────────────────────

void testDateParsing() {
  std::cout << "\n── Date Parsing ──\n";

  TEST("ISO 8601 format") {
    time_t t = CSVParser::parseDate("2026-03-06T02:53:58.000");
    ASSERT_TRUE(t > 0);
    struct tm *tm = localtime(&t);
    ASSERT_EQ(tm->tm_year + 1900, 2026);
    ASSERT_EQ(tm->tm_mon + 1, 3);
    ASSERT_EQ(tm->tm_mday, 6);
  }
  END_TEST

  TEST("MM/DD/YYYY format") {
    time_t t = CSVParser::parseDate("03/06/2026 02:53:58 AM");
    ASSERT_TRUE(t > 0);
    struct tm *tm = localtime(&t);
    ASSERT_EQ(tm->tm_year + 1900, 2026);
    ASSERT_EQ(tm->tm_mon + 1, 3);
    ASSERT_EQ(tm->tm_mday, 6);
  }
  END_TEST

  TEST("Empty date string") {
    time_t t = CSVParser::parseDate("");
    ASSERT_EQ(t, 0);
  }
  END_TEST
}

// ─── Enum Conversion Tests ──────────────────────────────────────────────────

void testEnumConversions() {
  std::cout << "\n── Enum Conversions ──\n";

  TEST("Borough conversions") {
    ASSERT_TRUE(stringToBorough("MANHATTAN") == Borough::MANHATTAN);
    ASSERT_TRUE(stringToBorough("BROOKLYN") == Borough::BROOKLYN);
    ASSERT_TRUE(stringToBorough("QUEENS") == Borough::QUEENS);
    ASSERT_TRUE(stringToBorough("BRONX") == Borough::BRONX);
    ASSERT_TRUE(stringToBorough("STATEN ISLAND") == Borough::STATEN_ISLAND);
    ASSERT_TRUE(stringToBorough("Unspecified") == Borough::UNSPECIFIED);
    ASSERT_TRUE(stringToBorough("") == Borough::UNSPECIFIED);
  }
  END_TEST

  TEST("Status conversions") {
    ASSERT_TRUE(stringToStatus("Closed") == Status::CLOSED);
    ASSERT_TRUE(stringToStatus("In Progress") == Status::IN_PROGRESS);
    ASSERT_TRUE(stringToStatus("Pending") == Status::PENDING);
  }
  END_TEST

  TEST("ChannelType conversions") {
    ASSERT_TRUE(stringToChannelType("PHONE") == ChannelType::PHONE);
    ASSERT_TRUE(stringToChannelType("MOBILE") == ChannelType::MOBILE);
    ASSERT_TRUE(stringToChannelType("ONLINE") == ChannelType::ONLINE);
    ASSERT_TRUE(stringToChannelType("UNKNOWN") == ChannelType::OTHER);
  }
  END_TEST
}

// ─── Full file parse test ────────────────────────────────────────────────────

void testFullParse(const std::string &filename) {
  std::cout << "\n── Full File Parse ──\n";

  TEST("Parse sample CSV file") {
    CSVParser parser;
    DataStore store = parser.parseFile(filename);
    std::cout << "(loaded " << store.size() << " records) ";
    ASSERT_TRUE(store.size() > 0);
  }
  END_TEST

  TEST("Verify first record fields are populated") {
    CSVParser parser;
    DataStore store = parser.parseFile(filename);
    const auto &r = store.getRecord(0);
    ASSERT_TRUE(r.unique_key != 0);
    ASSERT_TRUE(r.created_date != 0);
    ASSERT_TRUE(!r.agency.empty());
    ASSERT_TRUE(!r.complaint_type.empty());
  }
  END_TEST

  TEST("Memory footprint is reasonable") {
    CSVParser parser;
    DataStore store = parser.parseFile(filename);
    size_t footprint = store.memoryFootprint();
    std::cout << "(" << footprint << " bytes for " << store.size()
              << " records) ";
    ASSERT_TRUE(footprint > 0);
    ASSERT_TRUE(footprint >
                store.size() *
                    sizeof(ServiceRequest)); // should include string overhead
  }
  END_TEST
}

// ─── Main ───────────────────────────────────────────────────────────────────

int runParserTests(int argc, char *argv[]) {
  std::cout
      << "═══════════════════════════════════════════════════════════════\n";
  std::cout << "  Parser Test Suite\n";
  std::cout
      << "═══════════════════════════════════════════════════════════════\n";

  testCSVSplitting();
  testDateParsing();
  testEnumConversions();

  // If a CSV file is provided, test full parsing
  for (int i = 1; i < argc; ++i) {
    if (std::strstr(argv[i], ".csv")) {
      testFullParse(argv[i]);
      break;
    }
  }

  std::cout
      << "\n═══════════════════════════════════════════════════════════════\n";
  std::cout << "  Results: " << tests_passed << " passed, " << tests_failed
            << " failed\n";
  std::cout
      << "═══════════════════════════════════════════════════════════════\n\n";

  return tests_failed;
}
