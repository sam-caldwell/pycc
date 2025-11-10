/***
 * Name: pycc::driver::DeriveOutputs
 * Purpose: Compute artifact names from the requested binary path.
 * Inputs:
 *   - out_bin: Requested binary path
 * Outputs:
 *   - Outputs: Structure with bin, ll, s, o fields
 * Theory of Operation: Simple string concatenation of extensions.
 */
#include "pycc/driver/app.h"

#include <string>

namespace pycc::driver {

auto DeriveOutputs(const std::string& out_bin) -> Outputs {
  return Outputs{out_bin, out_bin + ".ll", out_bin + ".s", out_bin + ".o"};
}

}  // namespace pycc::driver
