/***
 * Name: pycc::driver::WriteFileOrReport
 * Purpose: Write a file and standardize error reporting to stderr.
 * Inputs:
 *   - path: destination path
 *   - data: content to write
 *   - err: receives detailed error text on failure
 * Outputs:
 *   - bool: true on success, false on failure (message printed)
 * Theory of Operation: Wraps support::WriteFile and prints a 'pycc: ' prefixed message.
 */
#include "pycc/driver/app.h"
#include "pycc/support/fs.h"

#include <iostream>

namespace pycc::driver {

auto WriteFileOrReport(const std::string& path, const std::string& data, std::string& err) -> bool {
  if (!support::WriteFile(path, data, err)) {
    std::cerr << "pycc: " << err << '\n';
    return false;
  }
  return true;
}

}  // namespace pycc::driver
