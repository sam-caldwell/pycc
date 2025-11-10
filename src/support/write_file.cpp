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
#include "pycc/support/fs.h"

#include <fstream>

namespace pycc {
namespace support {

bool WriteFile(const std::string& path, const std::string& data, std::string& err) {
  std::ofstream f(path, std::ios::binary);
  if (!f.good()) {
    err = "failed to open file for write: " + path;
    return false;
  }
  f << data;
  if (!f.good()) {
    err = "failed to write file: " + path;
    return false;
  }
  return true;
}

}  // namespace support
}  // namespace pycc

