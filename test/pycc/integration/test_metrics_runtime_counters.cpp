/***
 * Name: test_metrics_runtime_counters
 * Purpose: Ensure runtime perf counters are captured in JSON metrics.
 */
#include <gtest/gtest.h>
#include <fstream>
#include <cstdlib>
#include <string>

static std::string readAll(const std::string& p) {
  std::ifstream in(p);
  std::string s, line; while (std::getline(in, line)) { s += line; s += '\n'; } return s;
}

TEST(RuntimeMetrics, JsonContainsRtCounters) {
  const char* src = "metrics_rt.py";
  {
    std::ofstream out(src);
    out << "def main() -> int:\n";
    out << "  return 1\n";
  }
  int rc = std::system("./pycc --metrics-json -o out_rt metrics_rt.py > metrics_rt.json 2>/dev/null");
  ASSERT_EQ(rc, 0);
  auto js = readAll("metrics_rt.json");
  ASSERT_NE(js.find("\"rt.bytes_live\""), std::string::npos);
  ASSERT_NE(js.find("\"rt.collections\""), std::string::npos);
}

