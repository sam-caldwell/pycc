/***
 * Name: test_optimization_equivalence
 * Purpose: Compile programs with and without optimization flags and assert identical exit codes.
 */
#include <gtest/gtest.h>
#include <fstream>
#include <cstdlib>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

static int run_cmd(const std::string& cmd) { return std::system(cmd.c_str()); }

static int run_and_status(const std::string& bin) {
  int rc = std::system(("./" + bin + " > /dev/null 2>&1").c_str());
#ifdef WIFEXITED
  if (WIFEXITED(rc)) { return WEXITSTATUS(rc); }
  return rc >> 8; // best-effort
#else
  return rc >> 8;
#endif
}

static void write_file(const std::string& path, const std::string& s) {
  std::ofstream out(path); out << s;
}

static void ensure_run_cwd() {
  // For direct invocation (outside of CTest), create a run directory under build/ and chdir into it.
  // When invoked by CTest, we honor PYCC_TEST_STAY_CWD=1 and keep the working directory untouched.
  const char* stay = std::getenv("PYCC_TEST_STAY_CWD");
  if (stay && std::string(stay) == "1") return;
  const std::string dir = "build/run_local";
  (void)std::system((std::string("mkdir -p ") + dir).c_str());
  (void)chdir(dir.c_str());
}

static void check_equiv(const std::string& fname, const std::string& body) {
  ensure_run_cwd();
  // If the compiler binary is not available in the expected location, skip e2e.
  if (access("../pycc", X_OK) != 0) {
    GTEST_SKIP() << "Skipping e2e: ../pycc not accessible";
  }
  write_file(fname, body);
  const std::string base = fname + ".base";
  const std::string cf = fname + ".cf";
  const std::string alg = fname + ".alg";
  const std::string dce = fname + ".dce";
  const std::string cfg = fname + ".cfg";
  const std::string all = fname + ".all";
  const std::string allcfg = fname + ".allcfg";

  ASSERT_EQ(run_cmd("../pycc -o " + base + " " + fname + " > /dev/null 2>&1"), 0) << "baseline compile failed";
  ASSERT_EQ(run_cmd("../pycc --opt-const-fold -o " + cf + " " + fname + " > /dev/null 2>&1"), 0) << "const-fold compile failed";
  ASSERT_EQ(run_cmd("../pycc --opt-algebraic -o " + alg + " " + fname + " > /dev/null 2>&1"), 0) << "algebraic compile failed";
  ASSERT_EQ(run_cmd("../pycc --opt-dce -o " + dce + " " + fname + " > /dev/null 2>&1"), 0) << "dce compile failed";
  ASSERT_EQ(run_cmd("../pycc --opt-cfg -o " + cfg + " " + fname + " > /dev/null 2>&1"), 0) << "cfg compile failed";
  ASSERT_EQ(run_cmd("../pycc --opt-const-fold --opt-algebraic --opt-dce -o " + all + " " + fname + " > /dev/null 2>&1"), 0) << "all-opts compile failed";
  ASSERT_EQ(run_cmd("../pycc --opt-const-fold --opt-algebraic --opt-dce --opt-cfg -o " + allcfg + " " + fname + " > /dev/null 2>&1"), 0) << "all-opts+cfg compile failed";

  const int baseCode = run_and_status(base);
  EXPECT_EQ(run_and_status(cf), baseCode);
  EXPECT_EQ(run_and_status(alg), baseCode);
  EXPECT_EQ(run_and_status(dce), baseCode);
  EXPECT_EQ(run_and_status(all), baseCode);
  EXPECT_EQ(run_and_status(cfg), baseCode);
  EXPECT_EQ(run_and_status(allcfg), baseCode);
}

TEST(OptimizationEquivalence, Arithmetic) {
  const std::string fname = "opt_arith.py";
  const std::string src =
      "def main() -> int:\n"
      "  y = (2 + 3) * 4\n"
      "  return y\n";
  check_equiv(fname, src);
}

TEST(OptimizationEquivalence, BooleanShortCircuit) {
  const std::string fname = "opt_bool.py";
  const std::string src =
      "def main() -> int:\n"
      "  a = True\n"
      "  b = False\n"
      "  c = (a and b) or (not b)\n"
      "  return 1 if c else 0\n";
  check_equiv(fname, src);
}

TEST(OptimizationEquivalence, Recursion) {
  const std::string fname = "opt_recur.py";
  const std::string src =
      "def fact(n: int) -> int:\n"
      "  if n == 0:\n"
      "    return 1\n"
      "  else:\n"
      "    return n * fact(n - 1)\n"
      "def main() -> int:\n"
      "  return fact(5)\n"; // 120
  check_equiv(fname, src);
}

TEST(OptimizationEquivalence, CollectionsLen) {
  const std::string fname = "opt_len.py";
  const std::string src =
      "def main() -> int:\n"
      "  a = [1,2,3]\n"
      "  return len(a)\n";
  check_equiv(fname, src);
}
