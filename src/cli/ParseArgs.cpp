/***
 * Name: pycc::cli::ParseArgs
 * Purpose: Minimal GCC-like CLI argument parser for pycc.
 */
#include "cli/Options.h"
#include <cstddef>
#include <iostream>
#include <string_view>

namespace pycc::cli {

static bool isFlag(std::string_view arg, std::string_view flag) { return arg == flag; }

// NOLINTNEXTLINE(readability-function-size,readability-function-cognitive-complexity,misc-use-internal-linkage)
bool ParseArgs(const int argc, char** argv, Options& out) {
  for (int i = 1; i < argc; ++i) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    const std::string_view arg{argv[i]};
    if (isFlag(arg, "-h") || isFlag(arg, "--help")) {
      out.showHelp = true;
      continue;
    }
    if (isFlag(arg, "--")) {
      // Treat everything after as positional
      for (int j = i + 1; j < argc; ++j) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        out.inputs.emplace_back(argv[j]);
      }
      break;
    }
    if (isFlag(arg, "-o")) {
      if (i + 1 >= argc) { return false; }
      // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      out.outputFile = argv[++i];
      continue;
    }
    if (isFlag(arg, "-S")) {
      out.emitAssemblyOnly = true;
      continue;
    }
    if (isFlag(arg, "-c")) {
      out.compileOnly = true;
      continue;
    }
    if (isFlag(arg, "--metrics")) {
      out.metrics = true;
      continue;
    }
    if (isFlag(arg, "--metrics-json")) {
      out.metricsJson = true;
      continue;
    }
    if (isFlag(arg, "--opt-const-fold")) {
      out.optConstFold = true;
      continue;
    }
    if (isFlag(arg, "--opt-algebraic")) {
      out.optAlgebraic = true;
      continue;
    }
    if (isFlag(arg, "--opt-dce")) {
      out.optDCE = true;
      continue;
    }
    if (isFlag(arg, "--ast-log")) {
      out.astLog = pycc::cli::AstLogMode::Before;
      continue;
    }
    {
      constexpr std::string_view kPrefix{"--ast-log="};
      if (arg.rfind(kPrefix, 0) == 0) {
      using enum pycc::cli::AstLogMode;
      const auto value = arg.substr(kPrefix.size());
      out.astLog = Before;
      if (value == "after") { out.astLog = After; }
      else if (value == "both") { out.astLog = Both; }
      continue;
      }
    }
    {
      constexpr std::string_view kPrefix{"--log-path="};
      if (arg.rfind(kPrefix, 0) == 0) {
        out.logPath = std::string(arg.substr(kPrefix.size()));
        continue;
      }
    }
    if (isFlag(arg, "--log-lexer")) { out.logLexer = true; continue; }
    if (isFlag(arg, "--log-ast")) { out.logAst = true; continue; }
    if (isFlag(arg, "--log-codegen")) { out.logCodegen = true; continue; }
    {
      constexpr std::string_view kPrefix{"--color="};
      if (arg.rfind(kPrefix, 0) == 0) {
      using enum pycc::cli::ColorMode;
      const auto value = arg.substr(kPrefix.size());
      if (value == "always") {
        out.color = Always;
      } else if (value == "never") {
        out.color = Never;
      } else {
        out.color = Auto;
      }
      continue;
      }
    }
    {
      constexpr std::string_view kPrefix{"--diag-context="};
      if (arg.rfind(kPrefix, 0) == 0) {
        const auto value = std::string(arg.substr(kPrefix.size()));
        int numLines = 0;
        try {
          numLines = std::stoi(value);
        } catch (...) { numLines = 0; }
        out.diagContext = std::max(numLines, 0);
        continue;
      }
    }

    // Positional
    if (!arg.empty() && arg[0] == '-') {
      std::cerr << "pycc: unknown option '" << arg << "'\n";
      return false;
    }
    out.inputs.emplace_back(std::string(arg));
  }

  if (out.emitAssemblyOnly && out.compileOnly) { // NOLINT(readability-simplify-boolean-expr)
    std::cerr << "pycc: cannot use -S and -c together\n";
    return false; // NOLINT(readability-simplify-boolean-expr)
  }

  return true;
}

} // namespace pycc::cli
