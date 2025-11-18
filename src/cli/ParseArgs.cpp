/***
 * Name: pycc::cli::ParseArgs
 * Purpose: Minimal GCC-like CLI argument parser for pycc.
 */
#include "cli/ParseArgs.h"
#include "cli/ColorMode.h"
#include "cli/Options.h"
#include <algorithm>
#include <cstddef>
#include <iostream>
#include <string>
#include <string_view>

namespace pycc::cli {

static bool isFlag(std::string_view arg, std::string_view flag) { return arg == flag; }

static inline AstLogMode parseAstLogValue(std::string_view value) {
  using enum pycc::cli::AstLogMode;
  if (value == "after") { return After; }
  if (value == "both") { return Both; }
  return Before;
}

static inline ColorMode parseColorValue(std::string_view value) {
  using enum pycc::cli::ColorMode;
  if (value == "always") { return Always; }
  if (value == "never") { return Never; }
  return Auto;
}

static inline void collectRemainingAsInputs(std::size_t startIndex, int argc, char** argv, Options& out) {
  for (int j = static_cast<int>(startIndex); j < argc; ++j) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    out.inputs.emplace_back(argv[j]);
  }
}

static inline bool isUnknownOptionArg(std::string_view arg) { return !arg.empty() && arg[0] == '-'; }

static inline bool hasConflictingModes(const Options& opts) { return opts.emitAssemblyOnly && opts.compileOnly; }

static bool applySimpleBoolFlags(std::string_view arg, Options& out) {
  if (isFlag(arg, "-h")) { out.showHelp = true; return true; }
  if (isFlag(arg, "--help")) { out.showHelp = true; return true; }
  if (isFlag(arg, "-S")) { out.emitAssemblyOnly = true; return true; }
  if (isFlag(arg, "-c")) { out.compileOnly = true; return true; }
  if (isFlag(arg, "--metrics")) { out.metrics = true; return true; }
  if (isFlag(arg, "--metrics-json")) { out.metricsJson = true; return true; }
  if (isFlag(arg, "--opt-const-fold")) { out.optConstFold = true; return true; }
  if (isFlag(arg, "--opt-algebraic")) { out.optAlgebraic = true; return true; }
  if (isFlag(arg, "--opt-dce")) { out.optDCE = true; return true; }
  if (isFlag(arg, "--opt-cfg")) { out.optCFG = true; return true; }
  if (isFlag(arg, "--log-lexer")) { out.logLexer = true; return true; }
  if (isFlag(arg, "--log-ast")) { out.logAst = true; return true; }
  if (isFlag(arg, "--log-codegen")) { out.logCodegen = true; return true; }
  if (isFlag(arg, "--ast-log")) { out.astLog = AstLogMode::Before; return true; }
  return false;
}

static bool applyPrefixedOptions(std::string_view arg, Options& out) {
  if (constexpr std::string_view astLogPrefix{"--ast-log="}; arg.rfind(astLogPrefix, 0) == 0) {
    out.astLog = parseAstLogValue(arg.substr(astLogPrefix.size())); return true;
  }

  constexpr std::string_view logPathPrefix{"--log-path="};
  if (arg.rfind(logPathPrefix, 0) == 0) { out.logPath = std::string(arg.substr(logPathPrefix.size())); return true; }

  constexpr std::string_view colorPrefix{"--color="};
  if (arg.rfind(colorPrefix, 0) == 0) { out.color = parseColorValue(arg.substr(colorPrefix.size())); return true; }

  constexpr std::string_view diagPrefix{"--diag-context="};
  if (arg.rfind(diagPrefix, 0) == 0) {
    int numLines = 0;
    try { numLines = std::stoi(std::string(arg.substr(diagPrefix.size()))); }
    catch (...) { numLines = 0; }
    out.diagContext = std::max(numLines, 0);
    return true;
  }
  return false;
}

static bool handleOutputFileFlag(int& idx, int argc, char** argv, Options& out) {
  // -o <file>
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  const std::string_view arg{argv[idx]};
  if (!isFlag(arg, "-o")) { return false; }
  if (idx + 1 >= argc) { return false; }
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  out.outputFile = argv[++idx];
  return true;
}

bool ParseArgs(const int argc, char** argv, Options& out) {
  for (int i = 1; i < argc; ++i) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    const std::string_view arg{argv[i]};
    if (isFlag(arg, "--")) { collectRemainingAsInputs(i + 1, argc, argv, out); break; }
    if (handleOutputFileFlag(i, argc, argv, out)) { continue; }
    if (applySimpleBoolFlags(arg, out)) { continue; }
    if (applyPrefixedOptions(arg, out)) { continue; }

    // Positional
    if (isUnknownOptionArg(arg)) {
      std::cerr << "pycc: unknown option '" << arg << "'\n";
      return false;
    }
    out.inputs.emplace_back(std::string(arg));
  }

  if (hasConflictingModes(out)) {
    std::cerr << "pycc: cannot use -S and -c together\n";
    return false;
  }

  return true;
}

} // namespace pycc::cli
