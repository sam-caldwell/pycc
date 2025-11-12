/***
 * Name: test_optimizer_metrics
 * Purpose: Verify optimizer metrics JSON includes folds and breakdown with -S.
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

TEST(OptimizerMetrics, FoldsReported) {
  const char* src = "metrics_tmp_cf.py";
  {
    std::ofstream out(src);
    out << "def main() -> int:\n";
    out << "  return 2 + 3\n";
  }
  const char* cmd = "./pycc --opt-const-fold --metrics-json -S -o out_cf metrics_tmp_cf.py > metrics_cf.json 2>/dev/null";
  int rc = std::system(cmd);
  ASSERT_EQ(rc, 0);
  auto js = readFileAll("metrics_cf.json");
  ASSERT_NE(js.find("\"optimizer\""), std::string::npos);
  ASSERT_NE(js.find("\"folds\""), std::string::npos);
}

