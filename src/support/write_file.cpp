/***
 * Name: pycc::support::WriteFile
 * Purpose: Write the full contents of a string into a file.
 * Inputs:
 *   - path: filesystem path to write
 *   - data: content to write
 * Outputs:
 *   - err: error message on failure
 * Theory of Operation: Uses std::ofstream and checks .good().
 */
// NOLINTNEXTLINE(misc-include-cleaner) - include interface to ensure signature stays in sync
#include "pycc/support/fs.h"

#include <fstream>
#include <ios>
#include <string>

namespace pycc {
namespace support {

bool WriteFile(const std::string& path, const std::string& data, std::string& err) {
  std::ofstream file_stream(path, std::ios::binary);
  if (!file_stream.good()) {
    err = "failed to open file for write: " + path;
    return false;
  }
  file_stream << data;
  if (!file_stream.good()) {
    err = "failed to write file: " + path;
    return false;
  }
  return true;
}

}  // namespace support
}  // namespace pycc
