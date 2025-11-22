/**
 * @file
 * @brief Declarations for pycc CLI argument parsing helpers.
 */
#pragma once

#include <cstddef>
#include <string>
#include <string_view>

#include "cli/Options.h"
#include "cli/ColorMode.h"

namespace pycc::cli::detail {

/** Return true if `arg` exactly matches the `flag`. */
bool isFlag(std::string_view arg, std::string_view flag);

/** Parse `--ast-log=<value>` to AstLogMode with default fallback. */
AstLogMode parseAstLogValue(std::string_view value);

/** Parse `--color=<value>` to ColorMode with default fallback. */
ColorMode parseColorValue(std::string_view value);

/** Collect remaining argv items as input paths starting at index. */
void collectRemainingAsInputs(std::size_t startIndex, int argc, char** argv, Options& out);

/** Detect unknown option-like arguments beginning with '-' that aren't supported. */
bool isUnknownOptionArg(std::string_view arg);

/** Validate incompatible modes (e.g., -S and -c used together). */
bool hasConflictingModes(const Options& opts);

/** Handle boolean, flag-only options like -h, -S, --metrics, etc. */
bool applySimpleBoolFlags(std::string_view arg, Options& out);

/** Handle `--key=value` style options (ast-log, log-path, color, diag-context). */
bool applyPrefixedOptions(std::string_view arg, Options& out);

/** Handle `-o <file>` output flag by consuming the next argv item. */
bool handleOutputFileFlag(int& idx, int argc, char** argv, Options& out);

} // namespace pycc::cli::detail

