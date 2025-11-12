/***
 * Name: pycc::cli::ParseArgs
 * Purpose: Minimal GCC-like CLI argument parser for pycc.
 */
#include "cli/Options.h"
#include <iostream>

namespace pycc::cli {

static bool isFlag(const char* a, const char* s) {
  return std::strcmp(a, s) == 0;
}

bool ParseArgs(const int argc, char** argv, Options& out) {
  for (int i = 1; i < argc; ++i) {
    const char* a = argv[i];
    if (isFlag(a, "-h") || isFlag(a, "--help")) {
      out.showHelp = true;
      continue;
    }
    if (isFlag(a, "--")) {
      // Treat everything after as positional
      for (int j = i + 1; j < argc; ++j) out.inputs.emplace_back(argv[j]);
      break;
    }
    if (isFlag(a, "-o")) {
      if (i + 1 >= argc) return false;
      out.outputFile = argv[++i];
      continue;
    }
    if (isFlag(a, "-S")) {
      out.emitAssemblyOnly = true; continue;
    }
    if (isFlag(a, "-c")) {
      out.compileOnly = true; continue;
    }
    if (isFlag(a, "--metrics")) {
      out.metrics = true; continue;
    }
    if (isFlag(a, "--metrics-json")) {
      out.metricsJson = true; continue;
    }
    if (isFlag(a, "--opt-const-fold")) {
      out.optConstFold = true; continue;
    }
    if (isFlag(a, "--opt-algebraic")) {
      out.optAlgebraic = true; continue;
    }
    if (isFlag(a, "--opt-dce")) {
      out.optDCE = true; continue;
    }
    if (isFlag(a, "--ast-log")) {
      out.astLog = pycc::cli::AstLogMode::Before; continue;
    }
    if (std::strncmp(a, "--ast-log=", 10) == 0) {
      using enum pycc::cli::AstLogMode;
      const char* v = a + 10;
      if (std::strcmp(v, "before") == 0) out.astLog = Before;
      else if (std::strcmp(v, "after") == 0) out.astLog = After;
      else if (std::strcmp(v, "both") == 0) out.astLog = Both;
      else out.astLog = Before;
      continue;
    }
    if (std::strncmp(a, "--log-path=", 11) == 0) {
      out.logPath = std::string(a + 11);
      continue;
    }
    if (isFlag(a, "--log-lexer")) { out.logLexer = true; continue; }
    if (isFlag(a, "--log-ast")) { out.logAst = true; continue; }
    if (isFlag(a, "--log-codegen")) { out.logCodegen = true; continue; }
    if (std::strncmp(a, "--color=", 8) == 0) {
      using enum pycc::cli::ColorMode;
      if (const char* v = a + 8; std::strcmp(v, "always") == 0)
        out.color = Always;
      else if (std::strcmp(v, "never") == 0)
        out.color = Never;
      else
        out.color = Auto;
      continue;
    }
    if (std::strncmp(a, "--diag-context=", 15) == 0) {
      const char* v = a + 15;
      int n = std::atoi(v);
      if (n < 0) {
        n = 0;
      }
      out.diagContext = n;
      continue;
    }

    // Positional
    if (a[0] == '-') {
      std::cerr << "pycc: unknown option '" << a << "'\n";
      return false;
    }
    out.inputs.emplace_back(a);
  }

  if (out.emitAssemblyOnly && out.compileOnly) {
    std::cerr << "pycc: cannot use -S and -c together\n";
    return false;
  }

  return true;
}

} // namespace pycc::cli
