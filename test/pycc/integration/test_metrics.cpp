/***
 * Name: test_metrics
 * Purpose: Verify --metrics-json prints valid-looking JSON with durations and AST.
 */
#include <gtest/gtest.h>
#include <fstream>
#include <cstdlib>
#include <string>

static std::string readFileAll(const std::string& p) {
  std::ifstream in(p);
  std::string all, line;
  while (std::getline(in, line)) { all += line; all += "\n"; }
  return all;
}

TEST(Metrics, JsonOutput) {
  const char* src = "metrics_tmp.py";
  {
    std::ofstream out(src);
    out << "def main() -> int:\n";
    out << "  return 5\n";
  }
  const char* cmd = "./pycc --metrics-json -o out metrics_tmp.py > metrics.json 2>/dev/null";
  int rc = std::system(cmd);
  ASSERT_EQ(rc, 0);
  auto js = readFileAll("metrics.json");
  // Minimal checks for JSON structure and keys
  ASSERT_NE(js.find("\"durations_ms\""), std::string::npos);
  ASSERT_NE(js.find("\"ast\""), std::string::npos);
}

