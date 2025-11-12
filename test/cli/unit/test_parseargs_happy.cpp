/***
 * Name: test_parseargs_happy
 * Purpose: Exercise happy-path CLI parsing for all supported flags.
 */
#include <gtest/gtest.h>
#include "cli/CLI.h"

using namespace pycc::cli;

TEST(CLI_Happy, DefaultsAndOutput) {
  const char* argv[] = {"pycc", "file.py"};
  Options o;
  ASSERT_TRUE(ParseArgs(2, const_cast<char**>(argv), o));
  EXPECT_EQ(o.outputFile, "a.out");
  ASSERT_EQ(o.inputs.size(), 1u);
  EXPECT_EQ(o.inputs[0], "file.py");
}

TEST(CLI_Happy, OutputFlag) {
  const char* argv[] = {"pycc", "-o", "out.bin", "main.py"};
  Options o;
  ASSERT_TRUE(ParseArgs(4, const_cast<char**>(argv), o));
  EXPECT_EQ(o.outputFile, "out.bin");
  ASSERT_EQ(o.inputs.size(), 1u);
  EXPECT_EQ(o.inputs[0], "main.py");
}

TEST(CLI_Happy, CompileAndAssembleFlags) {
  const char* argv1[] = {"pycc", "-S", "m.py"};
  Options o1; ASSERT_TRUE(ParseArgs(3, const_cast<char**>(argv1), o1));
  EXPECT_TRUE(o1.emitAssemblyOnly);

  const char* argv2[] = {"pycc", "-c", "m.py"};
  Options o2; ASSERT_TRUE(ParseArgs(3, const_cast<char**>(argv2), o2));
  EXPECT_TRUE(o2.compileOnly);
}

TEST(CLI_Happy, MetricsFlags) {
  const char* argv1[] = {"pycc", "--metrics", "file.py"};
  Options o1; ASSERT_TRUE(ParseArgs(3, const_cast<char**>(argv1), o1));
  EXPECT_TRUE(o1.metrics);

  const char* argv2[] = {"pycc", "--metrics-json", "file.py"};
  Options o2; ASSERT_TRUE(ParseArgs(3, const_cast<char**>(argv2), o2));
  EXPECT_TRUE(o2.metricsJson);
}

TEST(CLI_Happy, ColorModes) {
  const char* argv1[] = {"pycc", "--color=always", "file.py"};
  Options o1; ASSERT_TRUE(ParseArgs(3, const_cast<char**>(argv1), o1));
  EXPECT_EQ(o1.color, ColorMode::Always);

  const char* argv2[] = {"pycc", "--color=never", "file.py"};
  Options o2; ASSERT_TRUE(ParseArgs(3, const_cast<char**>(argv2), o2));
  EXPECT_EQ(o2.color, ColorMode::Never);

  const char* argv3[] = {"pycc", "--color=auto", "file.py"};
  Options o3; ASSERT_TRUE(ParseArgs(3, const_cast<char**>(argv3), o3));
  EXPECT_EQ(o3.color, ColorMode::Auto);
}

TEST(CLI_Happy, DiagContext) {
  const char* argv1[] = {"pycc", "--diag-context=2", "file.py"};
  Options o1; ASSERT_TRUE(ParseArgs(3, const_cast<char**>(argv1), o1));
  EXPECT_EQ(o1.diagContext, 2);

  const char* argv2[] = {"pycc", "--diag-context=-5", "file.py"};
  Options o2; ASSERT_TRUE(ParseArgs(3, const_cast<char**>(argv2), o2));
  EXPECT_EQ(o2.diagContext, 0);
}

TEST(CLI_Happy, EndOfOptionsMarker) {
  const char* argv[] = {"pycc", "--", "-strange-name.py"};
  Options o; ASSERT_TRUE(ParseArgs(3, const_cast<char**>(argv), o));
  ASSERT_EQ(o.inputs.size(), 1u);
  EXPECT_EQ(o.inputs[0], "-strange-name.py");
}

TEST(CLI_Happy, MultipleInputs) {
  const char* argv[] = {"pycc", "a.py", "b.py"};
  Options o; ASSERT_TRUE(ParseArgs(3, const_cast<char**>(argv), o));
  ASSERT_EQ(o.inputs.size(), 2u);
  EXPECT_EQ(o.inputs[0], "a.py");
  EXPECT_EQ(o.inputs[1], "b.py");
}

TEST(CLI_Happy, OptConstFoldFlag) {
  const char* argv[] = {"pycc", "--opt-const-fold", "m.py"};
  Options o; ASSERT_TRUE(ParseArgs(3, const_cast<char**>(argv), o));
  EXPECT_TRUE(o.optConstFold);
}

TEST(CLI_Happy, AstLogFlagDefaultBefore) {
  const char* argv[] = {"pycc", "--ast-log", "m.py"};
  Options o; ASSERT_TRUE(ParseArgs(3, const_cast<char**>(argv), o));
  EXPECT_EQ(o.astLog, AstLogMode::Before);
}

TEST(CLI_Happy, AstLogModes) {
  const char* argv1[] = {"pycc", "--ast-log=before", "m.py"};
  Options o1; ASSERT_TRUE(ParseArgs(3, const_cast<char**>(argv1), o1));
  EXPECT_EQ(o1.astLog, AstLogMode::Before);

  const char* argv2[] = {"pycc", "--ast-log=after", "m.py"};
  Options o2; ASSERT_TRUE(ParseArgs(3, const_cast<char**>(argv2), o2));
  EXPECT_EQ(o2.astLog, AstLogMode::After);

  const char* argv3[] = {"pycc", "--ast-log=both", "m.py"};
  Options o3; ASSERT_TRUE(ParseArgs(3, const_cast<char**>(argv3), o3));
  EXPECT_EQ(o3.astLog, AstLogMode::Both);
}

TEST(CLI_Happy, OptFlagsAndLogs) {
  const char* argv[] = {"pycc", "--opt-algebraic", "--opt-dce", "--log-path=logs", "--log-lexer", "--log-ast", "--log-codegen", "m.py"};
  Options o; ASSERT_TRUE(ParseArgs(8, const_cast<char**>(argv), o));
  EXPECT_TRUE(o.optAlgebraic);
  EXPECT_TRUE(o.optDCE);
  EXPECT_EQ(o.logPath, std::string("logs"));
  EXPECT_TRUE(o.logLexer);
  EXPECT_TRUE(o.logAst);
  EXPECT_TRUE(o.logCodegen);
}
