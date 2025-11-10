/***
 * Name: pycc::frontend (simple_return_int)
 * Purpose: Parse a minimal Python subset: def main() -> int: return <int>
 * Inputs: Source text
 * Outputs: Parsed integer constant (return value)
 * Theory of Operation: Scans for a line starting with 'return ' followed by a base-10 integer.
 */
#pragma once

#include <string>

namespace pycc::frontend {

    /*** ParseReturnInt: Extract integer literal from a 'return <int>' statement. */
    bool ParseReturnInt(const std::string& source, int& out_value, std::string& err);

}
