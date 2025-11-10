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
#include "pycc/support/fs.h"

#include <fstream>
#include <sstream>

namespace pycc {
namespace support {

bool ReadFile(const std::string& path, std::string& out, std::string& err) {
  std::ifstream f(path);
  if (!f.good()) {
    err = "failed to open file: " + path;
    return false;
  }
  std::ostringstream ss;
  ss << f.rdbuf();
  out = ss.str();
  return true;
}

}  // namespace support
}  // namespace pycc

