/**
 * @file
 * @brief Internal argparse helpers to keep public wrapper thin.
 */
#pragma once

#include <cstddef>
#include <string>

namespace pycc::rt::detail {

struct OptVal {
  std::string opt; // option token (e.g., "--name")
  std::string val; // attached value when present
  bool has_val{false};
};

// Split a token like "--name=value" into option and value.
OptVal argparse_split_optval(const std::string& token);

// Lookup canonical option name in the parser's option map (by string contents).
// Returns nullptr if not found; otherwise returns the canonical key object.
void* argparse_lookup_canon(void* optmap, const std::string& opt);

// Apply the action for a single recognized option.
// Updates `idx` when consuming a following argument, and mutates `result` dict.
// Returns false if an error was raised.
bool argparse_apply_action(const std::string& action_name,
                          void* canon_key,
                          const OptVal& ov,
                          void* args_list,
                          std::size_t& idx,
                          void*& result);

} // namespace pycc::rt::detail

