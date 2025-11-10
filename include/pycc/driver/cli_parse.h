/***
 * Name: pycc::driver (cli_parse helpers)
 * Purpose: Declarations for small, single-purpose CLI option handlers used by ParseCli.
 * Inputs: Argument string(s), index into args, CLI options destination, error stream
 * Outputs: detail::OptResult (NotMatched, Handled, Error)
 * Theory of Operation: Each function recognizes one category of options, mutates state,
 *   and advances the index where necessary, keeping ParseCli simple and low complexity.
 */
#pragma once

#include <ostream>
#include <string>
#include <vector>

#include "pycc/driver/cli.h"

namespace pycc {
namespace driver {
namespace detail {

/*** HandleMetricsArg: Parse --metrics and --metrics=.. variants. */
OptResult HandleMetricsArg(const std::string& arg, CliOptions& dst, std::ostream& err);

/*** HandlePathListArg: Handle -X<val> or -X <val> and append to a string list. */
struct PathListParams {
  const std::string& short_opt;
  const std::vector<std::string>& args;
  int& index;
  int argc;
  std::vector<std::string>& out;
  const char* missing_msg;
  std::ostream& err;
};

OptResult HandlePathListArg(const std::string& arg, const PathListParams& p);

/*** HandleOutputArg: Handle -o <file>. */
OptResult HandleOutputArg(const std::vector<std::string>& args,
                          int& index,
                          int argc,
                          CliOptions& dst,
                          std::ostream& err);

/*** HandleSwitch: Handle booleans -S and -c. */
OptResult HandleSwitch(const std::string& arg, CliOptions& dst);

/*** HandleEndOfOptions: Handle "--" and push remaining inputs. */
OptResult HandleEndOfOptions(const std::vector<std::string>& args,
                             int& index,
                             int argc,
                             CliOptions& dst);

/*** HandleUnknownOrPositional: Error on unknown '-' options; otherwise record input. */
OptResult HandleUnknownOrPositional(const std::string& arg, CliOptions& dst, std::ostream& err);

/*** HandleHelpArg: Recognize -h/--help and set flag. */
OptResult HandleHelpArg(const std::string& arg, CliOptions& dst);

/*** NormalizeArgv: Convert argv into vector<string> with null safety. */
void NormalizeArgv(int argc, const char* const* argv, std::vector<std::string>& out);

/*** RunHandlers: Execute ordered handlers for current arg index. */
OptResult RunHandlers(const std::vector<std::string>& args,
                      int& index,
                      int argc,
                      CliOptions& dst,
                      std::ostream& err);

}  // namespace detail
}  // namespace driver
}  // namespace pycc
