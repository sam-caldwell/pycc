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
// NOLINTNEXTLINE(misc-include-cleaner) - interface for declarations
#include "pycc/driver/app.h"
#include "pycc/support/fs.h"

#include <iostream>
#include <string>

namespace pycc::driver {

auto WriteFileOrReport(const std::string& path, const std::string& data, std::string& err) -> bool {
  const bool is_ok = support::WriteFile(path, data, err);
  if (!is_ok) {
    std::cerr << "pycc: " << err << '\n';
  }
  return is_ok;
}

}  // namespace pycc::driver
