/***
 * Name: pycc::obs::IRDiff
 * Purpose: Lightweight IR text diff utility for diagnostics and tests.
 */
#pragma once

#include <string>

namespace pycc::obs {

// Returns a simple unified diff between two LLVM IR text blobs.
// - ignoreComments: drops lines starting with ';'
// - ignoreDebug: drops metadata/lines containing '!dbg' or starting with '!'
std::string diffIR(const std::string& a, const std::string& b,
                   bool ignoreComments = true,
                   bool ignoreDebug = true);

} // namespace pycc::obs

