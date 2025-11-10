/***
 * Name: pycc::support::ReadFile
 * Purpose: Read the full contents of a text file into a string.
 * Inputs:
 *   - path: filesystem path to read
 * Outputs:
 *   - out: populated with file contents on success
 *   - err: error message on failure
 * Theory of Operation: Uses std::ifstream with exceptions disabled; checks .good().
 */
// NOLINTNEXTLINE(misc-include-cleaner) - include interface to ensure signature stays in sync
#include "pycc/support/fs.h"

#include <fstream>
#include <sstream>
#include <string>

namespace pycc {
namespace support {

bool ReadFile(const std::string& path, std::string& out, std::string& err) {
  const std::ifstream file_stream(path);
  if (!file_stream.good()) {
    err = "failed to open file: " + path;
    return false;
  }
  std::ostringstream stream;
  stream << file_stream.rdbuf();
  out = stream.str();
  return true;
}

}  // namespace support
}  // namespace pycc
