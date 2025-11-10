/***
 * Name: pycc::tests::Cli
 * Purpose: Validate CLI parsing for -h/--help and -o behavior.
 * Inputs: none
 * Outputs: Pass/fail test results.
 * Theory of Operation: Feed synthetic argv arrays into ParseCli and assert
 *   on populated CliOptions and error handling.
 */
#include <gtest/gtest.h>

#include <sstream>
#include <string>
#include <vector>

#include "pycc/driver/cli.h"

using namespace pycc::driver;

static std::vector<const char*> MakeArgv(const std::vector<std::string>& args) {
  static std::vector<std::string> storage;  // keeps c_str alive for the test duration
  storage = args;
  std::vector<const char*> argv;
  argv.reserve(storage.size());
  for (auto& s : storage) argv.push_back(s.c_str());
  return argv;
}

TEST(Cli, HelpFlag) {
  CliOptions opts;
  std::ostringstream err;
  auto argv_vec = MakeArgv({"pycc", "-h"});
  EXPECT_TRUE(ParseCli(static_cast<int>(argv_vec.size()), argv_vec.data(), opts, err))
      << err.str();
  EXPECT_TRUE(opts.show_help);

  std::ostringstream out;
  PrintUsage(out, argv_vec[0]);
  EXPECT_NE(std::string::npos, out.str().find("Usage:"));
}

TEST(Cli, OutputFlagAndInput) {
  CliOptions opts;
  std::ostringstream err;
  auto argv_vec = MakeArgv({"pycc", "-o", "out.bin", "main.py"});
  EXPECT_TRUE(ParseCli(static_cast<int>(argv_vec.size()), argv_vec.data(), opts, err))
      << err.str();
  EXPECT_EQ("out.bin", opts.output);
  ASSERT_EQ(1u, opts.inputs.size());
  EXPECT_EQ("main.py", opts.inputs[0]);
}

TEST(Cli, UnknownFlag) {
  CliOptions opts;
  std::ostringstream err;
  auto argv_vec = MakeArgv({"pycc", "-unknown"});
  EXPECT_FALSE(ParseCli(static_cast<int>(argv_vec.size()), argv_vec.data(), opts, err));
  EXPECT_NE(std::string::npos, err.str().find("unknown option"));
}

TEST(Cli, MetricsFlag) {
  CliOptions opts;
  std::ostringstream err;
  auto argv_vec = MakeArgv({"pycc", "--metrics", "main.py"});
  EXPECT_TRUE(ParseCli(static_cast<int>(argv_vec.size()), argv_vec.data(), opts, err))
      << err.str();
  EXPECT_TRUE(opts.metrics);
  ASSERT_EQ(1u, opts.inputs.size());
}

TEST(Cli, MetricsJson) {
  CliOptions opts;
  std::ostringstream err;
  auto argv_vec = MakeArgv({"pycc", "--metrics=json", "main.py"});
  EXPECT_TRUE(ParseCli(static_cast<int>(argv_vec.size()), argv_vec.data(), opts, err))
      << err.str();
  EXPECT_TRUE(opts.metrics);
  // Verify format JSON
  EXPECT_EQ(CliOptions::MetricsFormat::Json, opts.metrics_format);
}

TEST(Cli, IncludeDirShortAndSpaced) {
  CliOptions opts; std::ostringstream err;
  auto argv_vec = MakeArgv({"pycc", "-Iinc1", "-I", "inc2", "main.py"});
  EXPECT_TRUE(ParseCli(static_cast<int>(argv_vec.size()), argv_vec.data(), opts, err)) << err.str();
  ASSERT_EQ(2u, opts.include_dirs.size());
  EXPECT_EQ("inc1", opts.include_dirs[0]);
  EXPECT_EQ("inc2", opts.include_dirs[1]);
}

TEST(Cli, LinkDirAndLibs) {
  CliOptions opts; std::ostringstream err;
  auto argv_vec = MakeArgv({"pycc", "-Llibpath", "-l", "m", "-lssl", "main.py"});
  EXPECT_TRUE(ParseCli(static_cast<int>(argv_vec.size()), argv_vec.data(), opts, err)) << err.str();
  ASSERT_EQ(1u, opts.link_dirs.size());
  EXPECT_EQ("libpath", opts.link_dirs[0]);
  ASSERT_EQ(2u, opts.link_libs.size());
  EXPECT_EQ("m", opts.link_libs[0]);
  EXPECT_EQ("ssl", opts.link_libs[1]);
}
